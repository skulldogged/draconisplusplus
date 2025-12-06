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

    // Get the base config directory for draconis++
    fn GetConfigDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++";
      if (auto result = GetEnv("USERPROFILE"))
        return fs::path(*result) / ".config" / "draconis++";
      return fs::current_path();
  #else
      if (const char* xdgConfig = getenv("XDG_CONFIG_HOME"))
        return fs::path(xdgConfig) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".config" / "draconis++";
      return fs::current_path();
  #endif
    }

    // Get the cache directory for draconis++
    fn GetCacheDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++" / "cache";
      return GetConfigDir() / "cache";
  #else
      if (const char* xdgCache = getenv("XDG_CACHE_HOME"))
        return fs::path(xdgCache) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".cache" / "draconis++";
      return GetConfigDir() / "cache";
  #endif
    }

    // Get the data directory for draconis++
    fn GetDataDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++" / "data";
      return GetConfigDir() / "data";
  #else
      if (const char* xdgData = getenv("XDG_DATA_HOME"))
        return fs::path(xdgData) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".local" / "share" / "draconis++";
      return GetConfigDir() / "data";
  #endif
    }
  } // namespace

  fn GetPluginContext() -> PluginContext {
    return PluginContext {
      .configDir = GetConfigDir() / "plugins",
      .cacheDir  = GetCacheDir() / "plugins",
      .dataDir   = GetDataDir() / "plugins",
    };
  }

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
      if (auto loadResult = loadPlugin(pluginName, cache); !loadResult)
        warn_log("Failed to auto-load plugin '{}': {}", pluginName, loadResult.error().message);
    }
  #endif

    m_initialized = true;
    debug_log("PluginManager initialized. Found {} discovered plugins.", listDiscoveredPlugins().size());
    return {};
  }

  fn PluginManager::shutdown() -> Unit {
    if (!m_initialized)
      return;

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
    // Check if there's already a static plugin loaded with the same provider ID
    // This prevents loading a dynamic plugin when a static version is already active
    for (const auto& [loadedName, loadedPlugin] : m_plugins) {
      if (!loadedPlugin.isLoaded || loadedPlugin.handle != nullptr)
        continue; // Skip unloaded plugins or dynamic plugins

      // Check if this is an info provider plugin with matching provider ID
      if (loadedPlugin.metadata.type == PluginType::InfoProvider) {
        if (auto* infoProvider = dynamic_cast<IInfoProviderPlugin*>(loadedPlugin.instance.get())) {
          // Compare provider IDs - if they match, skip loading the dynamic version
          if (infoProvider->getProviderId() == pluginName) {
            debug_log(
              "Skipping dynamic plugin '{}' - static plugin '{}' with same provider ID is already loaded.",
              pluginName,
              loadedName
            );
            return {};
          }
        }
      }
    }

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
          case PluginType::InfoProvider:
            if (auto* plugin = dynamic_cast<IInfoProviderPlugin*>(loadedPlugin.instance.get()))
              m_infoProviderPlugins.push_back(plugin);
            break;
          case PluginType::OutputFormat:
            if (auto* plugin = dynamic_cast<IOutputFormatPlugin*>(loadedPlugin.instance.get()))
              m_outputFormatPlugins.push_back(plugin);
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

    // Sync log level with the plugin before creating the instance
    syncPluginLogLevel(loadedPlugin.handle);

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
        case PluginType::InfoProvider:
          if (auto* plugin = dynamic_cast<IInfoProviderPlugin*>(loadedPlugin.instance.get()))
            m_infoProviderPlugins.push_back(plugin);
          break;
        case PluginType::OutputFormat:
          if (auto* plugin = dynamic_cast<IOutputFormatPlugin*>(loadedPlugin.instance.get()))
            m_outputFormatPlugins.push_back(plugin);
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
      case PluginType::InfoProvider:
        std::erase_if(m_infoProviderPlugins, [&](const IInfoProviderPlugin* plugin) {
          return plugin == loadedPlugin.instance.get();
        });
        break;
      case PluginType::OutputFormat:
        std::erase_if(m_outputFormatPlugins, [&](const IOutputFormatPlugin* plugin) {
          return plugin == loadedPlugin.instance.get();
        });
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

  fn PluginManager::getInfoProviderPlugins() const -> Vec<IInfoProviderPlugin*> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_infoProviderPlugins;
  }

  fn PluginManager::getInfoProviderByName(const String& providerId) const -> Option<IInfoProviderPlugin*> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    for (auto* plugin : m_infoProviderPlugins) {
      if (plugin->getProviderId() == providerId)
        return plugin;
    }
    return std::nullopt;
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

  fn PluginManager::syncPluginLogLevel(DynamicLibraryHandle handle) -> void {
    using utils::logging::GetLogLevelPtr;
    using utils::logging::LogLevel;

    using SetLogLevelFunc = void (*)(LogLevel*);

  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "SetPluginLogLevel");
  #else
    void* func = dlsym(handle, "SetPluginLogLevel");
  #endif
    if (func) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      auto setLogLevel = reinterpret_cast<SetLogLevelFunc>(func);
      setLogLevel(GetLogLevelPtr());
      debug_log("Synchronized log level with plugin");
    } else {
      debug_log("SetPluginLogLevel function not found in plugin (may be an older plugin)");
    }
  }

  fn PluginManager::initializePluginInstance(LoadedPlugin& loadedPlugin, CacheManager& /*cache*/) -> Result<Unit> {
    if (loadedPlugin.isInitialized) {
      debug_log("Plugin '{}' is already initialized", loadedPlugin.metadata.name);
      return {};
    }

    debug_log("Initializing plugin instance '{}'", loadedPlugin.metadata.name);

    // Create plugin context with paths
    PluginContext ctx = GetPluginContext();

    // Ensure directories exist
    std::error_code errc;
    fs::create_directories(ctx.configDir, errc);
    fs::create_directories(ctx.cacheDir, errc);
    fs::create_directories(ctx.dataDir, errc);

    // Create a PluginCache using the plugin's cache directory
    PluginCache pluginCache(ctx.cacheDir);

    if (auto initResult = loadedPlugin.instance->initialize(ctx, pluginCache); !initResult) {
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
