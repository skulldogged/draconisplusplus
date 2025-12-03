/**
 * @file Plugin.hpp
 * @brief Core plugin system interfaces for Draconis++
 * @author Draconis++ Team
 * @version 2.0.0
 *
 * @details This plugin system is designed for maximum performance:
 * - Zero-cost abstractions when plugins are disabled
 * - Lazy loading with efficient caching
 * - Minimal memory allocations
 * - RAII-based resource management
 * - Lock-free plugin access after initialization
 *
 * Plugin Types:
 * - InfoProvider: Provides structured data (weather, media, docker stats, etc.)
 * - OutputFormat: Formats output (JSON, YAML, Markdown, etc.)
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <glaze/glaze.hpp>

#include "../Utils/Types.hpp"

// Forward declaration to avoid including CacheManager.hpp
namespace draconis::utils::cache {
  class CacheManager;
}

/**
 * @class PluginCache
 * @brief Cache interface for plugins - provides efficient BEVE-based caching for any serializable type
 *
 * @details This class wraps a cache directory and provides template-based caching that stores
 * data in BEVE format (glaze's binary encoding) for maximum efficiency. Plugins can cache
 * any type that has glaze metadata defined.
 *
 * Cache entries include an expiry timestamp, and expired entries are automatically ignored.
 */
class PluginCache {
  using String = draconis::utils::types::String;
  template <typename T>
  using Option = draconis::utils::types::Option<T>;
  using u32    = draconis::utils::types::u32;
  using u64    = draconis::utils::types::u64;
  template <typename K, typename V>
  using UnorderedMap = draconis::utils::types::UnorderedMap<K, V>;
  template <typename A, typename B>
  using Pair                 = draconis::utils::types::Pair<A, B>;
  static constexpr auto None = draconis::utils::types::None;

 public:
  template <typename T>
  struct CacheEntry {
    T           data;
    Option<u64> expires; // UNIX timestamp, None = no expiry
  };

  explicit PluginCache(const std::filesystem::path& cacheDir) : m_cacheDir(cacheDir) {
    std::error_code errc;
    std::filesystem::create_directories(m_cacheDir, errc);
  }

  /**
   * @brief Get a cached value
   * @tparam T The type to retrieve (must have glaze metadata)
   * @param key Cache key
   * @return The cached value if found and not expired, None otherwise
   */
  template <typename T>
  [[nodiscard]] fn get(const String& key) const -> Option<T> {
    // Check in-memory cache first
    if (auto iter = m_cache.find(key); iter != m_cache.end()) {
      const auto& [data, expiryTp] = iter->second;
      if (std::chrono::system_clock::now() < expiryTp) {
        CacheEntry<T> entry;
        if (glz::read_beve(entry, data) == glz::error_code::none)
          return entry.data;
      }
    }

    // Check filesystem
    const std::filesystem::path filePath = m_cacheDir / key;
    if (!std::filesystem::exists(filePath))
      return None;

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs)
      return None;

    String        fileContents((std::istreambuf_iterator<char>(ifs)), {});
    CacheEntry<T> entry;

    if (glz::read_beve(entry, fileContents) != glz::error_code::none)
      return None;

    // Check expiry
    if (entry.expires.has_value()) {
      auto expiryTp = std::chrono::system_clock::time_point(std::chrono::seconds(*entry.expires));
      if (std::chrono::system_clock::now() >= expiryTp)
        return None;
    }

    // Store in memory cache for faster subsequent access
    auto expiryTp = entry.expires.has_value()
      ? std::chrono::system_clock::time_point(std::chrono::seconds(*entry.expires))
      : std::chrono::system_clock::time_point::max();
    m_cache[key]  = { fileContents, expiryTp };

    return entry.data;
  }

  /**
   * @brief Set a cached value
   * @tparam T The type to store (must have glaze metadata)
   * @param key Cache key
   * @param value The value to cache
   * @param ttlSeconds Time-to-live in seconds (0 = no expiry)
   */
  template <typename T>
  fn set(const String& key, const T& value, u32 ttlSeconds = 0) -> void {
    using namespace std::chrono;

    Option<u64>              expiryTs = None;
    system_clock::time_point expiryTp = system_clock::time_point::max();

    if (ttlSeconds > 0) {
      expiryTp = system_clock::now() + seconds(ttlSeconds);
      expiryTs = duration_cast<seconds>(expiryTp.time_since_epoch()).count();
    }

    CacheEntry<T> entry {
      .data    = value,
      .expires = expiryTs
    };

    String binaryBuffer;
    glz::write_beve(entry, binaryBuffer);

    // Store in memory
    m_cache[key] = { binaryBuffer, expiryTp };

    // Store to filesystem
    const std::filesystem::path filePath = m_cacheDir / key;
    std::error_code             errc;
    std::filesystem::create_directories(filePath.parent_path(), errc);

    if (std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc); ofs.is_open())
      ofs.write(binaryBuffer.data(), static_cast<std::streamsize>(binaryBuffer.size()));
  }

  /**
   * @brief Invalidate a cached entry
   * @param key Cache key to invalidate
   */
  fn invalidate(const String& key) -> void {
    m_cache.erase(key);
    std::error_code errc;
    std::filesystem::remove(m_cacheDir / key, errc);
  }

 private:
  std::filesystem::path                                                             m_cacheDir;
  mutable UnorderedMap<String, Pair<String, std::chrono::system_clock::time_point>> m_cache;
};

// Glaze metadata for CacheEntry (in global glz namespace)
namespace glz {
  template <typename T>
  struct meta<PluginCache::CacheEntry<T>> {
    using Entry                 = PluginCache::CacheEntry<T>;
    static constexpr auto value = object("data", &Entry::data, "expires", &Entry::expires);
  };
} // namespace glz

namespace draconis::core::plugin {
  /**
   * @enum PluginType
   * @brief Categorizes plugins for efficient lookup and filtering
   */
  enum class PluginType : utils::types::u8 {
    InfoProvider, ///< Provides structured information (weather, media, etc.)
    OutputFormat, ///< Adds new output formats (JSON, YAML, Markdown, etc.)
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

  /**
   * @struct PluginContext
   * @brief Context passed to plugins during initialization
   * @details Contains paths and configuration needed by plugins
   */
  struct PluginContext {
    std::filesystem::path configDir; ///< Directory where plugin configs live (e.g., ~/.config/draconis++/plugins/)
    std::filesystem::path cacheDir;  ///< Directory for plugin cache files
    std::filesystem::path dataDir;   ///< Directory for plugin data files
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

    /**
     * @brief Initialize the plugin with context and cache
     * @param ctx Plugin context with paths for config, cache, and data
     * @param cache Cache interface for persistent storage
     * @return Success or error
     */
    virtual fn initialize(const PluginContext& ctx, ::PluginCache& cache) -> utils::types::Result<utils::types::Unit> = 0;

    virtual fn shutdown() -> utils::types::Unit = 0;

    [[nodiscard]] virtual fn isReady() const -> bool = 0;
  };

  /**
   * @class IInfoProviderPlugin
   * @brief Plugin interface for providing structured information
   *
   * @details Info providers supply structured data that can be:
   * - Serialized to JSON for --json output
   * - Converted to key-value pairs for compact format templates
   * - Displayed in the UI
   *
   * Each plugin manages its own configuration file in the plugins config directory.
   * The plugin is responsible for reading its config during initialize().
   *
   * Example providers: weather, now playing, docker stats, system monitors
   */
  class IInfoProviderPlugin : public IPlugin {
   public:
    /**
     * @brief Unique identifier for this provider
     * @return Provider ID (e.g., "weather", "media", "docker")
     * @details Used as the key in JSON output and for compact format placeholders
     */
    [[nodiscard]] virtual fn getProviderId() const -> utils::types::String = 0;

    /**
     * @brief Collect/refresh data from this provider
     * @param cache Cache interface for data persistence
     * @return Success or error (errors are passed up for doctor mode)
     * @details Called each time the app runs. Plugin should use cache for efficiency.
     */
    virtual fn collectData(::PluginCache& cache) -> utils::types::Result<utils::types::Unit> = 0;

    /**
     * @brief Serialize collected data to JSON
     * @return JSON string representation of the data
     * @details Used for --json output. Plugin defines its own JSON structure.
     */
    [[nodiscard]] virtual fn toJson() const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get data as key-value string pairs
     * @return Map of field names to string values
     * @details Used for compact format templates (e.g., {weather_temp}, {weather_desc})
     *          Keys should be prefixed with provider ID (e.g., "weather_temp", "weather_desc")
     */
    [[nodiscard]] virtual fn getFields() const -> utils::types::Map<utils::types::String, utils::types::String> = 0;

    /**
     * @brief Get a single-line display string for UI
     * @return Formatted string for display (e.g., "72°F, Clear sky")
     * @details Used in the main UI output
     */
    [[nodiscard]] virtual fn getDisplayValue() const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get the icon for UI display (Nerd Font icon with spacing)
     * @return Icon string (e.g., "   " for weather)
     * @details Used in the main UI output. Should include trailing space for alignment.
     */
    [[nodiscard]] virtual fn getDisplayIcon() const -> utils::types::String = 0;

    /**
     * @brief Get the label for UI display
     * @return Label string (e.g., "Weather")
     * @details Used in the main UI output
     */
    [[nodiscard]] virtual fn getDisplayLabel() const -> utils::types::String = 0;

    /**
     * @brief Get the last error from data collection, if any
     * @return Error if collectData() failed, None otherwise
     * @details Used for doctor mode to report failures
     */
    [[nodiscard]] virtual fn getLastError() const -> utils::types::Option<utils::types::String> = 0;

    /**
     * @brief Check if this provider is enabled in its config
     * @return True if enabled, false if disabled
     * @details Plugins can be installed but disabled in config
     */
    [[nodiscard]] virtual fn isEnabled() const -> bool = 0;
  };

  /**
   * @class IOutputFormatPlugin
   * @brief Plugin interface for output formatting
   */
  class IOutputFormatPlugin : public IPlugin {
   public:
    /**
     * @brief Format the data using the specified format variant
     * @param formatName The format name to use (must be one returned by getFormatNames())
     * @param data The core system data to format
     * @param pluginData Plugin-contributed data organized by plugin ID
     * @return Formatted output string or error
     */
    virtual fn formatOutput(
      const utils::types::String&                                                                                   formatName,
      const utils::types::Map<utils::types::String, utils::types::String>&                                          data,
      const utils::types::Map<utils::types::String, utils::types::Map<utils::types::String, utils::types::String>>& pluginData
    ) const -> utils::types::Result<utils::types::String> = 0;

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

  // Legacy alias for backward compatibility during migration
  using ISystemInfoPlugin = IInfoProviderPlugin;
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