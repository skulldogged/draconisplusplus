#pragma once

#include <glaze/glaze.hpp>

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace types = ::draconis::utils::types;

  using config::Config;

  using std::chrono::seconds;

#if DRAC_ENABLE_PLUGINS
  using plugin::GetPluginManager;
  using plugin::ISystemInfoPlugin;
#endif

  /**
   * @brief Utility struct for storing system information.
   *
   * @details Performance optimizations:
   * - Plugin data stored in a flat map for O(1) access
   * - Lazy plugin loading only when accessed
   * - Minimal memory allocations
   */
  struct SystemInfo {
    types::Result<types::String>        date;
    types::Result<types::String>        host;
    types::Result<types::String>        kernelVersion;
    types::Result<types::OSInfo>        operatingSystem;
    types::Result<types::ResourceUsage> memInfo;
    types::Result<types::String>        desktopEnv;
    types::Result<types::String>        windowMgr;
    types::Result<types::ResourceUsage> diskUsage;
    types::Result<types::String>        shell;
    types::Result<types::String>        cpuModel;
    types::Result<types::CPUCores>      cpuCores;
    types::Result<types::String>        gpuModel;
    types::Result<seconds>              uptime;
#if DRAC_ENABLE_PACKAGECOUNT
    types::Result<types::u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    types::Result<types::MediaInfo> nowPlaying;
#endif

#if DRAC_ENABLE_PLUGINS
    // Plugin-contributed data (high-performance flat map)
    types::Map<types::String, types::String> pluginData;

    /**
     * @brief Get plugin-contributed field value
     * @param fieldName Name of the field to retrieve
     * @return Field value or empty string if not found
     */
    [[nodiscard]] fn getPluginField(const types::String& fieldName) const noexcept -> types::String {
      if (auto iter = pluginData.find(fieldName); iter != pluginData.end())
        return iter->second;

      return {};
    }

    /**
     * @brief Check if plugin field exists
     * @param fieldName Name of the field to check
     * @return True if field exists
     */
    [[nodiscard]] fn hasPluginField(const types::String& fieldName) const noexcept -> bool {
      return pluginData.contains(fieldName);
    }

    /**
     * @brief Get all plugin field names (for iteration)
     * @return Vector of field names
     */
    [[nodiscard]] fn getPluginFieldNames() const -> types::Vec<types::String> {
      types::Vec<types::String> names;
      names.reserve(pluginData.size());

      for (const auto& [name, value] : pluginData)
        names.push_back(name);

      return names;
    }
#endif

    /**
     * @brief Convert all system info to a flat key-value map
     * @details This is the single source of truth for all system info data.
     * Used by compact output, plugins, and any other consumers that need
     * a generic map representation. Adding new fields here automatically
     * makes them available everywhere.
     * @return Map of field names to their string values
     */
    [[nodiscard]] fn toMap() const -> types::Map<types::String, types::String>;

    explicit SystemInfo(utils::cache::CacheManager& cache, const Config& config);

   private:
#if DRAC_ENABLE_PLUGINS
    /**
     * @brief Collect data from all system info plugins efficiently
     * @param cache Cache manager for plugin data persistence
     */
    fn collectPluginData(utils::cache::CacheManager& cache) -> types::Unit;
#endif
  };

  struct JsonInfo {
    types::Option<types::String>        date;
    types::Option<types::String>        host;
    types::Option<types::String>        kernelVersion;
    types::Option<types::OSInfo>        operatingSystem;
    types::Option<types::ResourceUsage> memInfo;
    types::Option<types::String>        desktopEnv;
    types::Option<types::String>        windowMgr;
    types::Option<types::ResourceUsage> diskUsage;
    types::Option<types::String>        shell;
    types::Option<types::String>        cpuModel;
    types::Option<types::CPUCores>      cpuCores;
    types::Option<types::String>        gpuModel;
    types::Option<types::i64>           uptimeSeconds;
#if DRAC_ENABLE_PACKAGECOUNT
    types::Option<types::u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
    types::Option<types::MediaInfo> nowPlaying;
#endif
#if DRAC_ENABLE_WEATHER
    types::Option<services::weather::Report> weather;
#endif
#if DRAC_ENABLE_PLUGINS
    // Plugin-contributed fields (dynamic JSON object)
    types::Map<types::String, types::String> pluginFields;
#endif
  };

} // namespace draconis::core::system

namespace glz {
  template <>
  struct meta<draconis::utils::types::ResourceUsage> {
    using T = draconis::utils::types::ResourceUsage;

    static constexpr detail::Object value = object("usedBytes", &T::usedBytes, "totalBytes", &T::totalBytes);
  };

#if DRAC_ENABLE_NOWPLAYING
  template <>
  struct meta<draconis::utils::types::MediaInfo> {
    using T = draconis::utils::types::MediaInfo;

    static constexpr detail::Object value = object("title", &T::title, "artist", &T::artist);
  };
#endif

  template <>
  struct meta<draconis::core::system::JsonInfo> {
    using T = draconis::core::system::JsonInfo;

    // clang-format off
    static constexpr detail::Object value = object(
#if DRAC_ENABLE_PACKAGECOUNT
      "packageCount",    &T::packageCount,
#endif
#if DRAC_ENABLE_NOWPLAYING
      "nowPlaying",      &T::nowPlaying,
#endif
#if DRAC_ENABLE_WEATHER
      "weather",         &T::weather,
#endif
#if DRAC_ENABLE_PLUGINS
      "pluginFields",    &T::pluginFields,
#endif
      "date",            &T::date,
      "host",            &T::host,
      "kernelVersion",   &T::kernelVersion,
      "operatingSystem", &T::operatingSystem,
      "memInfo",         &T::memInfo,
      "desktopEnv",      &T::desktopEnv,
      "windowMgr",       &T::windowMgr,
      "diskUsage",       &T::diskUsage,
      "shell",           &T::shell,
      "cpuModel",        &T::cpuModel,
      "cpuCores",        &T::cpuCores,
      "gpuModel",        &T::gpuModel,
      "uptimeSeconds",   &T::uptimeSeconds
    );
    // clang-format on
  };
} // namespace glz