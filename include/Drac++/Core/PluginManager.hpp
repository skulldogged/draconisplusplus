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

  #ifdef _WIN32
    #include <windows.h>
  #endif

// Forward declaration to avoid circular includes
namespace draconis::config {
  struct Config;
}

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

    fn operator=(const LoadedPlugin&)->LoadedPlugin& = delete;
    fn operator=(LoadedPlugin&&)->LoadedPlugin&      = default;
  };

  /**
   * @brief Get the plugin context with standard paths
   * @return PluginContext with config, cache, and data directories
   */
  fn GetPluginContext() -> PluginContext;

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

    static fn loadDynamicLibrary(const fs::path& path) -> Result<DynamicLibraryHandle>;
    static fn unloadDynamicLibrary(DynamicLibraryHandle handle) -> Unit;

    fn scanForPlugins() -> Result<Unit>;

    static fn getCreatePluginFunc(DynamicLibraryHandle handle) -> Result<IPlugin* (*)()>;
    static fn getDestroyPluginFunc(DynamicLibraryHandle handle) -> Result<void (*)(IPlugin*)>;

    static fn initializePluginInstance(LoadedPlugin& loadedPlugin, CacheManager& cache) -> Result<Unit>;

   public:
    PluginManager(const PluginManager&)                = delete;
    PluginManager(PluginManager&&)                     = delete;
    fn operator=(const PluginManager&)->PluginManager& = delete;
    fn operator=(PluginManager&&)->PluginManager&      = delete;

    ~PluginManager(); // Destructor to unload all plugins

    static fn getInstance() -> PluginManager&;

    // Initialization and shutdown
    fn initialize(const draconis::config::Config* config) -> Result<Unit>;
    fn shutdown() -> Unit;
    fn isInitialized() const -> bool {
      return m_initialized;
    }

    // Plugin discovery and loading
    fn addSearchPath(const fs::path& path) -> Unit;
    fn getSearchPaths() const -> Vec<fs::path>;
    fn loadPlugin(const String& pluginName, CacheManager& cache) -> Result<Unit>;
    fn unloadPlugin(const String& pluginName) -> Result<Unit>;

    // Plugin access (read-only, thread-safe)
    fn getPlugin(const String& pluginName) const -> Option<IPlugin*>;
    fn getInfoProviderPlugins() const -> Vec<IInfoProviderPlugin*>;
    fn getOutputFormatPlugins() const -> Vec<IOutputFormatPlugin*>;
    fn getInfoProviderByName(const String& providerId) const -> Option<IInfoProviderPlugin*>;

    // Legacy alias
    fn getSystemInfoPlugins() const -> Vec<IInfoProviderPlugin*> {
      return getInfoProviderPlugins();
    }

    // Plugin metadata
    fn listLoadedPlugins() const -> Vec<PluginMetadata>;
    fn listDiscoveredPlugins() const -> Vec<String>; // Lists all .so/.dll files found
    fn isPluginLoaded(const String& pluginName) const -> bool;
  };

  inline fn GetPluginManager() -> PluginManager& {
    return PluginManager::getInstance();
  }
} // namespace draconis::core::plugin

#else  // DRAC_ENABLE_PLUGINS is 0
// Zero-cost abstraction when plugins are disabled
namespace draconis::core::plugin {
  class PluginManager {
   public:
    static fn getInstance() -> PluginManager& {
      static PluginManager instance;
      return instance;
    }

    fn initialize() -> Result<Unit> {
      return Ok({});
    }

    fn shutdown() -> Unit {}

    fn isInitialized() const -> bool {
      return false;
    }

    fn addSearchPath(const std::filesystem::path&) -> Unit {}

    fn getSearchPaths() const -> Vec<fs::path> {
      return {};
    }

    fn scanForPlugins() -> Result<Vec<String> > {
      return Ok({});
    }

    fn loadPlugin(const String&, CacheManager&) -> Result<Unit> {
      return Ok({});
    }

    fn unloadPlugin(const String&) -> Result<Unit> {
      return Ok({});
    }

    fn getPlugin(const String&) const -> Option<IPlugin*> {
      return None;
    }

    fn getSystemInfoPlugins() const -> Vec<ISystemInfoPlugin*> {
      return {};
    }

    fn getOutputFormatPlugins() const -> Vec<IOutputFormatPlugin*> {
      return {};
    }

    fn listLoadedPlugins() const -> Vec<PluginMetadata> {
      return {};
    }

    fn listDiscoveredPlugins() const -> Vec<String> {
      return {};
    }

    fn isPluginLoaded(const String&) const -> bool {
      return false;
    }
  };

  inline fn getPluginManager() -> PluginManager& {
    return PluginManager::getInstance();
  }
} // namespace draconis::core::plugin
#endif // DRAC_ENABLE_PLUGINS