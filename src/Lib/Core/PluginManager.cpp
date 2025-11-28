/**
 * @file PluginManager.cpp
 * @brief High-performance plugin manager implementation
 * @author Draconis++ Team
 * @version 1.0.0
 */

#include <Drac++/Core/PluginManager.hpp>

#if DRAC_ENABLE_PLUGINS

  #include <format>   // std::format
  #include <optional> // std::optional
  #include <string>   // std::string

  #include <Drac++/Utils/CacheManager.hpp>
  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Logging.hpp>

  #include "../../CLI/Config/Config.hpp"

  // Include static plugins header when using precompiled config
  #if DRAC_PRECOMPILED_CONFIG
    #include <Drac++/Core/StaticPlugins.hpp>
  #endif

  #ifdef _WIN32
    #include <windows.h>
  #else
    #include <dlfcn.h> // dlopen, dlsym, dlclose
  #endif

namespace draconis::core::plugin {
  // Proper cache wrapper that uses the full CacheManager infrastructure with TTL support
  class CacheWrapper : public IPluginCache {
   public:
    explicit CacheWrapper(utils::cache::CacheManager& manager) : m_manager(manager) {}

    fn get(const String& key) -> Option<String> override {
      // Use a fetcher that always fails - we only want cached data, not to fetch fresh data
      fn fetcher = []() -> utils::types::Result<String> {
        return utils::types::Err(utils::error::DracError(utils::error::DracErrorCode::Other, "Cache miss - no fetcher provided"));
      };

      // Try to get from cache. If it's a cache miss, the fetcher will fail and we'll return nullopt
      Result<String> result = m_manager.getOrSet<String>(key, fetcher);

      if (result)
        return *result;

      return std::nullopt;
    }

    fn set(const String& key, const String& value, utils::types::u32 ttlSeconds) -> void override {
      // First invalidate any existing cache entry to ensure we store fresh data
      m_manager.invalidate(key);

      // Create a fetcher that returns the provided value
      fn fetcher = [&value]() -> utils::types::Result<String> {
        return value;
      };

      // Set cache policy with the specified TTL
      using namespace std::chrono;
      utils::cache::CachePolicy policy {
        .location = utils::cache::CacheLocation::Persistent, // Use persistent cache for plugins
        .ttl      = seconds(ttlSeconds)
      };

      // Store the value with TTL - we don't use the return value since we just want to cache it
      [[maybe_unused]] auto cacheResult = m_manager.getOrSet<String>(key, policy, fetcher);
    }

   private:
    utils::cache::CacheManager& m_manager; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  namespace {
    using utils::error::DracErrorCode;
    using utils::types::StringView;
    using enum DracErrorCode;

    // Platform-specific plugin file extension
  #ifdef _WIN32
    constexpr StringView PLUGIN_EXTENSION = ".dll";
  #elif defined(__APPLE__)
    constexpr StringView PLUGIN_EXTENSION = ".dylib";
  #else
    constexpr StringView PLUGIN_EXTENSION = ".so";
  #endif

    // Default search paths for plugins
    fn GetDefaultPluginPaths() -> const Vec<fs::path>& {
      static const Vec<fs::path> DEFAULT_PLUGIN_PATHS = []() {
        Vec<fs::path> paths;
  #ifdef _WIN32
        using draconis::utils::env::GetEnv;

        if (auto result = GetEnv("LOCALAPPDATA"))
          paths.push_back(fs::path(*result) / "draconis++" / "plugins");

        if (auto result = GetEnv("APPDATA"))
          paths.push_back(fs::path(*result) / "draconis++" / "plugins");

        if (auto result = GetEnv("USERPROFILE"))
          paths.push_back(fs::path(*result) / ".config" / "draconis++" / "plugins");

        paths.push_back(fs::current_path() / "plugins");
  #else
        paths.push_back(fs::path("/usr/local/lib/draconis++/plugins"));
        paths.push_back(fs::path("/usr/lib/draconis++/plugins"));
        paths.push_back(fs::path(getenv("HOME") ? getenv("HOME") : "") / ".local/lib/draconis++/plugins");
        paths.push_back(fs::current_path() / "plugins");
  #endif
        return paths;
      }();

      return DEFAULT_PLUGIN_PATHS;
    }
  } // namespace

  PluginManager::~PluginManager() {
    shutdown();
  }

  fn PluginManager::getInstance() -> PluginManager& {
    static PluginManager Instance;
    return Instance;
  }

  fn PluginManager::initialize(const draconis::config::Config* config) -> Result<Unit> {
    if (m_initialized)
      return {};

    debug_log("Initializing PluginManager...");

    // Use the provided config or get from singleton
    std::optional<draconis::config::Config> singletonConfig;
    const draconis::config::Config&         effectiveConfig = config ? *config : (singletonConfig = draconis::config::Config::getInstance(), *singletonConfig);

    // Check if plugins are enabled in config
  #if DRAC_ENABLE_PLUGINS
    if (!effectiveConfig.plugins.enabled) {
      debug_log("Plugin system disabled in configuration");
      m_initialized = true;
      return {};
    }
  #endif

    // Add default search paths
    for (const fs::path& path : GetDefaultPluginPaths())
      addSearchPath(path);

    {
      std::unique_lock<std::shared_mutex> lock(m_mutex);
      // Scan for plugins in all search paths
      if (auto scanResult = scanForPlugins(); !scanResult) {
        warn_log("Failed to scan for plugins: {}", scanResult.error().message);
        // Continue initialization even if scan fails, as plugins might be loaded explicitly
      }
    }

    // Auto-load plugins from config
  #if DRAC_ENABLE_PLUGINS
    CacheManager cache;
    for (const auto& pluginName : effectiveConfig.plugins.autoLoad) {
      debug_log("Auto-loading plugin '{}' from config", pluginName);
      if (auto loadResult = loadPlugin(pluginName, cache); !loadResult) {
        warn_log("Failed to auto-load plugin '{}': {}", pluginName, loadResult.error().message);
        // Continue with other plugins even if one fails
      }
    }
  #endif

    m_initialized = true;
    debug_log("PluginManager initialized. Found {} discovered plugins.", listDiscoveredPlugins().size());
    return {};
  }

  fn PluginManager::shutdown() -> Unit {
    if (!m_initialized) {
      return;
    }

    debug_log("Shutting down PluginManager...");

    // Unload all loaded plugins
    Vec<String> pluginNamesToUnload;

    {
      std::shared_lock<std::shared_mutex> lock(m_mutex);
      for (const auto& [name, loadedPlugin] : m_plugins)
        if (loadedPlugin.isLoaded)
          pluginNamesToUnload.push_back(name);
    }

    for (const auto& name : pluginNamesToUnload)
      if (auto result = unloadPlugin(name); !result)
        error_log("Failed to unload plugin '{}': {}", name, result.error().message);

    m_plugins.clear();
    m_initialized = false;
    debug_log("PluginManager shut down.");
  }

  fn PluginManager::addSearchPath(const fs::path& path) -> Unit {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Add only if not already present
    if (std::ranges::find(m_pluginSearchPaths, path) == m_pluginSearchPaths.end()) {
      m_pluginSearchPaths.push_back(path);
      debug_log("Added plugin search path: {}", path.string());
    }
  }

  fn PluginManager::getSearchPaths() const -> Vec<fs::path> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_pluginSearchPaths;
  }

  fn PluginManager::scanForPlugins() -> Result<Unit> {
    m_discoveredPlugins.clear();

    for (const auto& searchPath : m_pluginSearchPaths) {
      if (!fs::exists(searchPath) || !fs::is_directory(searchPath))
        continue;

      for (const auto& entry : fs::directory_iterator(searchPath))
        if (entry.is_regular_file() && entry.path().extension() == PLUGIN_EXTENSION) {
          String pluginName = entry.path().stem().string();
          // The first discovery of a plugin with a given name wins
          if (!m_discoveredPlugins.contains(pluginName))
            m_discoveredPlugins.emplace(pluginName, entry.path());
        }
    }

    return {};
  }

  fn PluginManager::loadPlugin(const String& pluginName, CacheManager& cache) -> Result<Unit> {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (m_plugins.contains(pluginName) && m_plugins.at(pluginName).isLoaded) {
      debug_log("Plugin '{}' is already loaded.", pluginName);
      return {};
    }

  #if DRAC_PRECOMPILED_CONFIG
    // Try to load as a static plugin first (for precompiled config mode)
    if (IsStaticPlugin(pluginName)) {
      debug_log("Loading static plugin '{}'", pluginName);

      LoadedPlugin loadedPlugin;
      loadedPlugin.path   = fs::path("<static>");
      loadedPlugin.handle = nullptr; // No dynamic library handle for static plugins

      IPlugin* instance = CreateStaticPlugin(pluginName);
      if (!instance)
        ERR_FMT(InternalError, "Failed to create static plugin instance for '{}'", pluginName);

      loadedPlugin.instance.reset(instance);
      loadedPlugin.metadata = loadedPlugin.instance->getMetadata();
      loadedPlugin.isLoaded = true;

      if (auto initResult = initializePluginInstance(loadedPlugin, cache); !initResult) {
        warn_log("Static plugin '{}' failed to initialize: {}", pluginName, initResult.error().message);
        m_plugins.emplace(pluginName, std::move(loadedPlugin));
        return initResult;
      }

      // Add to type-safe caches if ready
      if (loadedPlugin.isReady) {
        switch (loadedPlugin.metadata.type) {
          case PluginType::SystemInfo:
            if (auto* plugin = dynamic_cast<ISystemInfoPlugin*>(loadedPlugin.instance.get()))
              m_systemInfoPlugins.push_back(plugin);
            break;
          case PluginType::OutputFormat:
            if (auto* plugin = dynamic_cast<IOutputFormatPlugin*>(loadedPlugin.instance.get()))
              m_outputFormatPlugins.push_back(plugin);
            break;
          default:
            break;
        }
      }

      m_plugins.emplace(pluginName, std::move(loadedPlugin));
      debug_log("Static plugin '{}' loaded and initialized successfully.", pluginName);
      return {};
    }
  #endif

    // Fall back to dynamic loading
    if (!m_discoveredPlugins.contains(pluginName))
      ERR_FMT(NotFound, "Plugin '{}' not found in search paths.", pluginName);

    const fs::path& pluginPath = m_discoveredPlugins.at(pluginName);

    debug_log("Loading plugin '{}' from '{}'", pluginName, pluginPath.string());

    LoadedPlugin loadedPlugin;
    loadedPlugin.path = pluginPath;

    if (Result<DynamicLibraryHandle> handleResult = loadDynamicLibrary(pluginPath); !handleResult)
      return std::unexpected(handleResult.error());
    else
      loadedPlugin.handle = *handleResult;

    if (Result<IPlugin* (*)()> createFuncResult = getCreatePluginFunc(loadedPlugin.handle); !createFuncResult) {
      unloadDynamicLibrary(loadedPlugin.handle);
      return std::unexpected(createFuncResult.error());
    } else
      loadedPlugin.instance.reset((*createFuncResult)());

    if (!loadedPlugin.instance) {
      unloadDynamicLibrary(loadedPlugin.handle);
      ERR_FMT(InternalError, "Failed to create instance for plugin '{}'", pluginName);
    }

    loadedPlugin.metadata = loadedPlugin.instance->getMetadata();
    loadedPlugin.isLoaded = true;

    if (auto initResult = initializePluginInstance(loadedPlugin, cache); !initResult) {
      warn_log("Plugin '{}' failed to initialize: {}", pluginName, initResult.error().message);
      m_plugins.emplace(pluginName, std::move(loadedPlugin));
      return initResult;
    }

    // Add to type-safe caches if ready
    if (loadedPlugin.isReady) {
      switch (loadedPlugin.metadata.type) {
        case PluginType::SystemInfo:
          if (auto* plugin = dynamic_cast<ISystemInfoPlugin*>(loadedPlugin.instance.get()))
            m_systemInfoPlugins.push_back(plugin);
          break;
        case PluginType::OutputFormat:
          if (auto* plugin = dynamic_cast<IOutputFormatPlugin*>(loadedPlugin.instance.get()))
            m_outputFormatPlugins.push_back(plugin);
          break;
        default:
          // Other plugin types are not cached in type-safe lists
          break;
      }
    }

    m_plugins.emplace(pluginName, std::move(loadedPlugin));
    debug_log("Plugin '{}' loaded and initialized successfully.", pluginName);
    return {};
  }

  fn PluginManager::unloadPlugin(const String& pluginName) -> Result<Unit> {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (!m_plugins.contains(pluginName))
      ERR_FMT(NotFound, "Plugin '{}' is not loaded.", pluginName);

    LoadedPlugin& loadedPlugin = m_plugins.at(pluginName);

    if (loadedPlugin.isReady) {
      debug_log("Shutting down plugin instance '{}'", pluginName);
      loadedPlugin.instance->shutdown();
      loadedPlugin.isReady = false;
    }

    // Remove from type-safe caches
    switch (loadedPlugin.metadata.type) {
      case PluginType::SystemInfo:
        std::erase_if(m_systemInfoPlugins, [&](const ISystemInfoPlugin* plugin) {
          return plugin == loadedPlugin.instance.get();
        });

        break;
      case PluginType::OutputFormat:
        std::erase_if(m_outputFormatPlugins, [&](const IOutputFormatPlugin* plugin) {
          return plugin == loadedPlugin.instance.get();
        });

        break;
      default:
        break;
    }

    debug_log("Destroying plugin instance '{}'", pluginName);

  #if DRAC_PRECOMPILED_CONFIG
    // Handle static plugins (identified by nullptr handle)
    if (loadedPlugin.handle == nullptr) {
      DestroyStaticPlugin(pluginName, loadedPlugin.instance.release());
      m_plugins.erase(pluginName);
      debug_log("Static plugin '{}' unloaded successfully.", pluginName);
      return {};
    }
  #endif

    // Handle dynamic plugins
    if (auto destroyFuncResult = getDestroyPluginFunc(loadedPlugin.handle); destroyFuncResult) {
      (*destroyFuncResult)(loadedPlugin.instance.release());
    } else {
      error_log("Failed to get destroyPlugin function for '{}': {}", pluginName, destroyFuncResult.error().message);
      delete loadedPlugin.instance.release();
    }

    debug_log("Unloading dynamic library for plugin '{}'", pluginName);
    unloadDynamicLibrary(loadedPlugin.handle);

    m_plugins.erase(pluginName);
    debug_log("Plugin '{}' unloaded successfully.", pluginName);
    return {};
  }

  fn PluginManager::getPlugin(const String& pluginName) const -> Option<IPlugin*> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_plugins.contains(pluginName))
      return m_plugins.at(pluginName).instance.get();

    return std::nullopt;
  }

  fn PluginManager::getSystemInfoPlugins() const -> Vec<ISystemInfoPlugin*> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_systemInfoPlugins;
  }

  fn PluginManager::getOutputFormatPlugins() const -> Vec<IOutputFormatPlugin*> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_outputFormatPlugins;
  }

  fn PluginManager::listLoadedPlugins() const -> Vec<PluginMetadata> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    Vec<PluginMetadata>                 loadedMetadata;

    for (const auto& [name, loadedPlugin] : m_plugins)
      if (loadedPlugin.isLoaded)
        loadedMetadata.push_back(loadedPlugin.metadata);

    std::ranges::sort(loadedMetadata, [](const PluginMetadata& metaA, const PluginMetadata& metaB) {
      return metaA.name < metaB.name;
    });

    return loadedMetadata;
  }

  fn PluginManager::listDiscoveredPlugins() const -> Vec<String> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    Vec<String>                         discoveredNames;

    for (const auto& [name, path] : m_discoveredPlugins)
      discoveredNames.push_back(name);

    std::ranges::sort(discoveredNames);
    return discoveredNames;
  }

  fn PluginManager::isPluginLoaded(const String& pluginName) const -> bool {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_plugins.contains(pluginName) && m_plugins.at(pluginName).isLoaded;
  }

  fn PluginManager::loadDynamicLibrary(const fs::path& path) -> Result<DynamicLibraryHandle> {
  #ifdef _WIN32
    HMODULE handle = LoadLibraryA(path.string().c_str());
    if (!handle)
      ERR_FMT(InternalError, "Failed to load DLL '{}': Error Code {}", path.string(), GetLastError());
  #else
    void* handle = dlopen(path.string().c_str(), RTLD_LAZY);
    if (!handle)
      ERR_FMT(InternalError, "Failed to load shared library '{}': {}", path.string(), dlerror());
  #endif

    return handle;
  }

  fn PluginManager::unloadDynamicLibrary(DynamicLibraryHandle handle) -> Unit {
    if (handle)
  #ifdef _WIN32
      FreeLibrary(handle);
  #else
      dlclose(handle);
  #endif
  }

  fn PluginManager::getCreatePluginFunc(DynamicLibraryHandle handle) -> Result<IPlugin* (*)()> {
  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "CreatePlugin");
  #else
    void* func = dlsym(handle, "CreatePlugin");
  #endif
    if (!func)
      ERR(InternalError, "Failed to find 'CreatePlugin' function in plugin.");

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<IPlugin* (*)()>(func);
  }

  fn PluginManager::getDestroyPluginFunc(DynamicLibraryHandle handle) -> Result<void (*)(IPlugin*)> {
  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "DestroyPlugin");
  #else
    void* func = dlsym(handle, "DestroyPlugin");
  #endif
    if (!func)
      ERR(InternalError, "Failed to find 'DestroyPlugin' function in plugin.");

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<void (*)(IPlugin*)>(func);
  }

  fn PluginManager::initializePluginInstance(LoadedPlugin& loadedPlugin, CacheManager& cache) -> Result<Unit> {
    if (loadedPlugin.isInitialized) {
      debug_log("Plugin '{}' is already initialized", loadedPlugin.metadata.name);
      return {};
    }

    debug_log("Initializing plugin instance '{}'", loadedPlugin.metadata.name);
    CacheWrapper cacheWrapper(cache);

    if (auto initResult = loadedPlugin.instance->initialize(cacheWrapper); !initResult) {
      debug_log("Plugin '{}' initialization failed: {}", loadedPlugin.metadata.name, initResult.error().message);
      loadedPlugin.isReady = false;
      return initResult;
    }

    debug_log("Plugin '{}' initialized successfully", loadedPlugin.metadata.name);
    loadedPlugin.isInitialized = true;
    loadedPlugin.isReady       = loadedPlugin.instance->isReady();

    if (!loadedPlugin.isReady)
      warn_log("Plugin '{}' initialized but is not ready", loadedPlugin.metadata.name);

    return {};
  }
} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS
