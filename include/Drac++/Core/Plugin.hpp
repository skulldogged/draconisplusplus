/**
 * @file Plugin.hpp
 * @brief Core plugin system interfaces for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin system is designed for maximum performance:
 * - Zero-cost abstractions when plugins are disabled
 * - Lazy loading with efficient caching
 * - Minimal memory allocations
 * - RAII-based resource management
 * - Lock-free plugin access after initialization
 */

#pragma once

#include "../Utils/Types.hpp"

// Forward declaration to avoid including CacheManager.hpp
namespace draconis::utils::cache {
  class CacheManager;
}

// Simple cache interface for plugins to avoid heavy dependencies
class IPluginCache {
 public:
  IPluginCache()                                                                                                                                         = default;
  IPluginCache(const IPluginCache&)                                                                                                                      = default;
  IPluginCache(IPluginCache&&)                                                                                                                           = delete;
  fn operator=(const IPluginCache&)->IPluginCache&                                                                                                       = default;
  fn operator=(IPluginCache&&)->IPluginCache&                                                                                                            = delete;
  virtual ~IPluginCache()                                                                                                                                = default;
  virtual fn get(const draconis::utils::types::String& key) -> draconis::utils::types::Option<draconis::utils::types::String>                            = 0;
  virtual fn set(const draconis::utils::types::String& key, const draconis::utils::types::String& value, draconis::utils::types::u32 ttlSeconds) -> void = 0;
};

namespace draconis::core::plugin {
  /**
   * @enum PluginType
   * @brief Categorizes plugins for efficient lookup and filtering
   */
  enum class PluginType : utils::types::u8 {
    SystemInfo,   // Adds new system information fields
    OutputFormat, // Adds new output formats (beyond JSON/Markdown/ASCII)
  };

  struct PluginDependencies {
    bool requiresNetwork    = false;
    bool requiresFilesystem = false;
    bool requiresAdmin      = false;
    bool requiresCaching    = false;
  };

  struct PluginMetadata {
    utils::types::String name;
    utils::types::String version;
    utils::types::String author;
    utils::types::String description;
    PluginType           type;
    PluginDependencies   dependencies;
  };

  class IPlugin {
   public:
    IPlugin()                                                             = default;
    IPlugin(const IPlugin&)                                               = default;
    IPlugin(IPlugin&&)                                                    = delete;
    fn operator=(const IPlugin&)->IPlugin&                                = default;
    fn operator=(IPlugin&&)->IPlugin&                                     = delete;
    virtual ~IPlugin()                                                    = default;
    [[nodiscard]] virtual fn getMetadata() const -> const PluginMetadata& = 0;

    virtual fn initialize(::IPluginCache& cache) -> utils::types::Result<utils::types::Unit> = 0;

    virtual fn shutdown() -> utils::types::Unit = 0;

    [[nodiscard]] virtual fn isReady() const -> bool = 0;
  };

  // Specific plugin interfaces
  class ISystemInfoPlugin : public IPlugin {
   public:
    virtual fn collectInfo(::IPluginCache& cache) -> utils::types::Result<utils::types::Map<utils::types::String, utils::types::String>> = 0;

    [[nodiscard]] virtual fn getFieldNames() const -> utils::types::Vec<utils::types::String> = 0;
  };

  class IOutputFormatPlugin : public IPlugin {
   public:
    /**
     * @brief Format the data using the specified format variant
     * @param formatName The format name to use (must be one returned by getFormatNames())
     * @param data The data to format
     * @return Formatted output string or error
     */
    virtual fn formatOutput(const utils::types::String& formatName, const utils::types::Map<utils::types::String, utils::types::String>& data) const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get all format names this plugin supports
     * @return Vector of supported format names (e.g., {"json", "json-pretty"})
     */
    [[nodiscard]] virtual fn getFormatNames() const -> utils::types::Vec<utils::types::String> = 0;

    /**
     * @brief Get file extension for a given format
     * @param formatName The format name
     * @return File extension (without dot)
     */
    [[nodiscard]] virtual fn getFileExtension(const utils::types::String& formatName) const -> utils::types::String = 0;
  };
} // namespace draconis::core::plugin

#if defined(_WIN32)
  #if defined(DRAC_PLUGIN_BUILD)
    #define DRAC_PLUGIN_API __declspec(dllexport)
  #else
    #define DRAC_PLUGIN_API __declspec(dllimport)
  #endif
#else
  #define DRAC_PLUGIN_API __attribute__((visibility("default")))
#endif

/**
 * @def DRAC_PLUGIN
 * @brief Generates plugin factory functions with default create/destroy behavior
 *
 * @param PluginClass The plugin class to instantiate (must be default-constructible)
 *
 * For static builds, creates factory functions and self-registers the plugin at startup.
 * For dynamic builds, creates extern "C" exports for dynamic loading.
 *
 * @example
 * @code
 * DRAC_PLUGIN(WindowsInfoPlugin)
 * @endcode
 */
#ifdef DRAC_STATIC_PLUGIN_BUILD
  #include <Drac++/Core/StaticPlugins.hpp>
  #define DRAC_PLUGIN(PluginClass)                                                \
    static fn Create_##PluginClass() -> draconis::core::plugin::IPlugin* {        \
      return new PluginClass();                                                   \
    }                                                                             \
    static fn Destroy_##PluginClass(draconis::core::plugin::IPlugin* p) -> void { \
      delete p;                                                                   \
    }                                                                             \
    static const bool g_##PluginClass##_registered =                              \
      draconis::core::plugin::RegisterStaticPlugin({ #PluginClass, Create_##PluginClass, Destroy_##PluginClass });
#else
  #define DRAC_PLUGIN(PluginClass)                                                                 \
    extern "C" DRAC_PLUGIN_API fn CreatePlugin() -> draconis::core::plugin::IPlugin* {             \
      return new PluginClass();                                                                    \
    }                                                                                              \
    extern "C" DRAC_PLUGIN_API fn DestroyPlugin(draconis::core::plugin::IPlugin* plugin) -> void { \
      delete plugin;                                                                               \
    }
#endif