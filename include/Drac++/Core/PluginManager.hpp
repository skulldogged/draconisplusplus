/**
 * @file PluginManager.hpp
 * @brief High-performance plugin manager with lazy loading and efficient resource management
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details Performance optimizations:
 * - Lazy loading: Plugins loaded only when first accessed
 * - Memory pooling: Minimal allocations during runtime
 * - Cache-friendly data structures: Arrays and contiguous memory
 * - Lock-free access: Thread-safe reads without mutexes after init
 * - RAII resource management: Automatic cleanup
 */

#pragma once

#if DRAC_ENABLE_PLUGINS

  #include <filesystem> // std::filesystem::path
  #include <shared_mutex>

  #include "../Utils/CacheManager.hpp"
  #include "../Utils/Types.hpp"
  #include "Plugin.hpp"
  #include "PluginConfig.hpp"

  #ifdef _WIN32
    #include <windows.h>
  #endif

namespace draconis::core::plugin {
  namespace fs = std::filesystem;

  using utils::cache::CacheManager;
  using utils::types::Map;
  using utils::types::Option;
  using utils::types::Result;
  using utils::types::String;
  using utils::types::UniquePointer;
  using utils::types::Unit;
  using utils::types::Vec;

  // Platform-specific dynamic library handle
  #ifdef _WIN32
  using DynamicLibraryHandle = HMODULE;
  #else
  using DynamicLibraryHandle = void*;
  #endif

  struct LoadedPlugin {
    UniquePointer<IPlugin> instance;
    DynamicLibraryHandle   handle;
    fs::path               path;
    PluginMetadata         metadata;
    bool                   isInitialized = false;
    bool                   isReady       = false;
    bool                   isLoaded      = false;

    LoadedPlugin() = default;
    LoadedPlugin(
      UniquePointer<IPlugin> instance,
      DynamicLibraryHandle   handle,
      fs::path               path,
      PluginMetadata         metadata
    ) : instance(std::move(instance)),
        handle(handle),
        path(std::move(path)),
        metadata(std::move(metadata)) {}

    ~LoadedPlugin() = default;

    LoadedPlugin(const LoadedPlugin&) = delete;
    LoadedPlugin(LoadedPlugin&&)      = default;

    auto operator=(const LoadedPlugin&) -> LoadedPlugin& = delete;
    auto operator=(LoadedPlugin&&) -> LoadedPlugin&      = default;
  };

  /**
   * @brief Get the plugin context with standard paths
   * @return PluginContext with config, cache, and data directories
   */
  auto GetPluginContext() -> PluginContext;

  class PluginManager {
   private:
    Map<String, LoadedPlugin> m_plugins;
    Map<String, fs::path>     m_discoveredPlugins;
    Vec<fs::path>             m_pluginSearchPaths;

    // Type-safe, sorted plugin caches for fast access
    Vec<IInfoProviderPlugin*> m_infoProviderPlugins;
    Vec<IOutputFormatPlugin*> m_outputFormatPlugins;

    // Plugin context (paths for config, cache, data)
    PluginContext m_context;

    mutable std::shared_mutex m_mutex;
    std::atomic<bool>         m_initialized = false;

    PluginManager() = default;

    static auto loadDynamicLibrary(const fs::path& path) -> Result<DynamicLibraryHandle>;
    static auto unloadDynamicLibrary(DynamicLibraryHandle handle) -> Unit;

    auto scanForPlugins() -> Result<Unit>;

    static auto getCreatePluginFunc(DynamicLibraryHandle handle) -> Result<IPlugin* (*)()>;
    static auto getDestroyPluginFunc(DynamicLibraryHandle handle) -> Result<void (*)(IPlugin*)>;
    static auto syncPluginLogLevel(DynamicLibraryHandle handle) -> void;

    static auto initializePluginInstance(LoadedPlugin& loadedPlugin, CacheManager& cache) -> Result<Unit>;

   public:
    PluginManager(const PluginManager&)                    = delete;
    PluginManager(PluginManager&&)                         = delete;
    auto operator=(const PluginManager&) -> PluginManager& = delete;
    auto operator=(PluginManager&&) -> PluginManager&      = delete;

    ~PluginManager(); // Destructor to unload all plugins

    static auto getInstance() -> PluginManager&;

    // Initialization and shutdown
    auto initialize(const PluginConfig& config = {}) -> Result<Unit>;
    auto shutdown() -> Unit;
    auto isInitialized() const -> bool {
      return m_initialized;
    }

    // Plugin discovery and loading
    auto addSearchPath(const fs::path& path) -> Unit;
    auto getSearchPaths() const -> Vec<fs::path>;
    auto loadPlugin(const String& pluginName, CacheManager& cache) -> Result<Unit>;
    auto unloadPlugin(const String& pluginName) -> Result<Unit>;

    // Plugin access (read-only, thread-safe)
    auto getPlugin(const String& pluginName) const -> Option<IPlugin*>;
    auto getInfoProviderPlugins() const -> Vec<IInfoProviderPlugin*>;
    auto getOutputFormatPlugins() const -> Vec<IOutputFormatPlugin*>;
    auto getInfoProviderByName(const String& providerId) const -> Option<IInfoProviderPlugin*>;

    // Legacy alias
    auto getSystemInfoPlugins() const -> Vec<IInfoProviderPlugin*> {
      return getInfoProviderPlugins();
    }

    // Plugin metadata
    auto listLoadedPlugins() const -> Vec<PluginMetadata>;
    auto listDiscoveredPlugins() const -> Vec<String>; // Lists all .so/.dll files found
    auto isPluginLoaded(const String& pluginName) const -> bool;
  };

  inline auto GetPluginManager() -> PluginManager& {
    return PluginManager::getInstance();
  }
} // namespace draconis::core::plugin

#else  // DRAC_ENABLE_PLUGINS is 0
// Zero-cost abstraction when plugins are disabled
namespace draconis::core::plugin {
  class PluginManager {
   public:
    static auto getInstance() -> PluginManager& {
      static PluginManager instance;
      return instance;
    }

    auto initialize() -> Result<Unit> {
      return Ok({});
    }

    auto shutdown() {}

    auto isInitialized() const -> bool {
      return false;
    }

    auto addSearchPath(const std::filesystem::path&) {}

    auto getSearchPaths() const -> Vec<fs::path> {
      return {};
    }

    auto scanForPlugins() -> Result<Vec<String> > {
      return Ok({});
    }

    auto loadPlugin(const String&, CacheManager&) -> Result<Unit> {
      return Ok({});
    }

    auto unloadPlugin(const String&) -> Result<Unit> {
      return Ok({});
    }

    auto getPlugin(const String&) const -> Option<IPlugin*> {
      return None;
    }

    auto getSystemInfoPlugins() const -> Vec<ISystemInfoPlugin*> {
      return {};
    }

    auto getOutputFormatPlugins() const -> Vec<IOutputFormatPlugin*> {
      return {};
    }

    auto listLoadedPlugins() const -> Vec<PluginMetadata> {
      return {};
    }

    auto listDiscoveredPlugins() const -> Vec<String> {
      return {};
    }

    auto isPluginLoaded(const String&) const -> bool {
      return false;
    }
  };

  inline auto getPluginManager() -> PluginManager& {
    return PluginManager::getInstance();
  }
} // namespace draconis::core::plugin
#endif // DRAC_ENABLE_PLUGINS