/**
 * @file json_format.cpp
 * @brief JSON output format plugin for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin provides JSON output formatting for system information.
 * It supports multiple output modes:
 * - "json": Compact JSON output
 * - "json-pretty": Pretty-printed JSON output
 *
 * This file supports both dynamic (shared library) and static compilation.
 * When compiled as a static plugin (DRAC_STATIC_PLUGIN_BUILD defined),
 * it exports factory functions in a namespace instead of extern "C".
 */

#include <glaze/glaze.hpp>

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace {
  using namespace draconis::utils::types;

  /**
   * @brief JSON output structure for system information
   * @details This structure mirrors the data map keys and provides
   * proper JSON serialization via glaze
   */
  struct JsonOutput {
    Option<String> date;
    Option<String> host;
    Option<String> kernelVersion;
    Option<String> operatingSystem;
    Option<String> osName;
    Option<String> osVersion;
    Option<String> osId;
    Option<String> memInfo;
    Option<u64>    memUsedBytes;
    Option<u64>    memTotalBytes;
    Option<String> desktopEnv;
    Option<String> windowMgr;
    Option<String> diskUsage;
    Option<u64>    diskUsedBytes;
    Option<u64>    diskTotalBytes;
    Option<String> shell;
    Option<String> cpuModel;
    Option<u32>    cpuCoresPhysical;
    Option<u32>    cpuCoresLogical;
    Option<String> gpuModel;
    Option<String> uptime;
    Option<i64>    uptimeSeconds;
    Option<u64>    packageCount;
    Option<String> weatherTemperature;
    Option<String> weatherDescription;
    Option<String> weatherTown;

    // Plugin-contributed fields organized by plugin ID
    Map<String, Map<String, String>> pluginFields;
  };

} // anonymous namespace

namespace glz {
  template <>
  struct meta<JsonOutput> {
    using T = JsonOutput;

    // clang-format off
    static constexpr detail::Object value = object(
      "date",              &T::date,
      "host",              &T::host,
      "kernelVersion",     &T::kernelVersion,
      "operatingSystem",   &T::operatingSystem,
      "osName",            &T::osName,
      "osVersion",         &T::osVersion,
      "osId",              &T::osId,
      "memInfo",           &T::memInfo,
      "memUsedBytes",      &T::memUsedBytes,
      "memTotalBytes",     &T::memTotalBytes,
      "desktopEnv",        &T::desktopEnv,
      "windowMgr",         &T::windowMgr,
      "diskUsage",         &T::diskUsage,
      "diskUsedBytes",     &T::diskUsedBytes,
      "diskTotalBytes",    &T::diskTotalBytes,
      "shell",             &T::shell,
      "cpuModel",          &T::cpuModel,
      "cpuCoresPhysical",  &T::cpuCoresPhysical,
      "cpuCoresLogical",   &T::cpuCoresLogical,
      "gpuModel",          &T::gpuModel,
      "uptime",            &T::uptime,
      "uptimeSeconds",     &T::uptimeSeconds,
      "packageCount",      &T::packageCount,
      "weatherTemperature",&T::weatherTemperature,
      "weatherDescription",&T::weatherDescription,
      "weatherTown",       &T::weatherTown,
      "pluginFields",      &T::pluginFields
    );
    // clang-format on
  };
} // namespace glz

namespace {

  class JsonFormatPlugin : public draconis::core::plugin::IOutputFormatPlugin {
   private:
    draconis::core::plugin::PluginMetadata m_metadata;
    bool                                   m_ready = false;

    static constexpr auto FORMAT_JSON        = "json";
    static constexpr auto FORMAT_JSON_PRETTY = "json-pretty";

   public:
    JsonFormatPlugin() {
      m_metadata = {
        .name         = "JSON Format",
        .version      = "1.0.0",
        .author       = "Draconis++ Team",
        .description  = "Provides JSON output formatting (compact and pretty-printed)",
        .type         = draconis::core::plugin::PluginType::OutputFormat,
        .dependencies = {}
      };
    }

    [[nodiscard]] auto getMetadata() const -> const draconis::core::plugin::PluginMetadata& override {
      return m_metadata;
    }

    auto initialize(const draconis::core::plugin::PluginContext& /*ctx*/, ::PluginCache& /*cache*/) -> Result<Unit> override {
      m_ready = true;
      return {};
    }

    auto shutdown() -> Unit override {
      m_ready = false;
    }

    [[nodiscard]] auto isReady() const -> bool override {
      return m_ready;
    }

    [[nodiscard]] auto formatOutput(
      const String&                           formatName,
      const Map<String, String>&              data,
      const Map<String, Map<String, String>>& pluginData
    ) const -> Result<String> override {
      if (!m_ready)
        return Err(draconis::utils::error::DracError { draconis::utils::error::DracErrorCode::Other, "JsonFormatPlugin is not ready." });

      // Determine if pretty printing based on format name
      bool prettyPrint = (formatName == FORMAT_JSON_PRETTY);

      // Build the JSON output structure
      JsonOutput output;

      // Helper lambda to get optional string value
      auto getOptional = [&data](const String& key) -> Option<String> {
        if (auto iter = data.find(key); iter != data.end() && !iter->second.empty())
          return iter->second;
        return std::nullopt;
      };

      // Helper lambda to get optional numeric value
      auto getOptionalU64 = [&data](const String& key) -> Option<u64> {
        if (auto iter = data.find(key); iter != data.end() && !iter->second.empty()) {
          try {
            return std::stoull(iter->second);
          } catch (...) {
            return std::nullopt;
          }
        }
        return std::nullopt;
      };

      auto getOptionalU32 = [&data](const String& key) -> Option<u32> {
        if (auto iter = data.find(key); iter != data.end() && !iter->second.empty()) {
          try {
            return static_cast<u32>(std::stoul(iter->second));
          } catch (...) {
            return std::nullopt;
          }
        }
        return std::nullopt;
      };

      auto getOptionalI64 = [&data](const String& key) -> Option<i64> {
        if (auto iter = data.find(key); iter != data.end() && !iter->second.empty()) {
          try {
            return std::stoll(iter->second);
          } catch (...) {
            return std::nullopt;
          }
        }
        return std::nullopt;
      };

      // Map data to JSON output structure
      output.date               = getOptional("date");
      output.host               = getOptional("host");
      output.kernelVersion      = getOptional("kernel");
      output.operatingSystem    = getOptional("os");
      output.osName             = getOptional("os_name");
      output.osVersion          = getOptional("os_version");
      output.osId               = getOptional("os_id");
      output.memInfo            = getOptional("ram");
      output.memUsedBytes       = getOptionalU64("memory_used_bytes");
      output.memTotalBytes      = getOptionalU64("memory_total_bytes");
      output.desktopEnv         = getOptional("de");
      output.windowMgr          = getOptional("wm");
      output.diskUsage          = getOptional("disk");
      output.diskUsedBytes      = getOptionalU64("disk_used_bytes");
      output.diskTotalBytes     = getOptionalU64("disk_total_bytes");
      output.shell              = getOptional("shell");
      output.cpuModel           = getOptional("cpu");
      output.cpuCoresPhysical   = getOptionalU32("cpu_cores_physical");
      output.cpuCoresLogical    = getOptionalU32("cpu_cores_logical");
      output.gpuModel           = getOptional("gpu");
      output.uptime             = getOptional("uptime");
      output.uptimeSeconds      = getOptionalI64("uptime_seconds");
      output.packageCount       = getOptionalU64("packages");
      output.weatherTemperature = getOptional("weather_temperature");
      output.weatherDescription = getOptional("weather_description");
      output.weatherTown        = getOptional("weather_town");

      // Use plugin data directly (already organized by plugin ID)
      output.pluginFields = pluginData;

      // Serialize to JSON
      String jsonStr;

      glz::error_ctx errorContext = prettyPrint
        ? glz::write<glz::opts { .skip_null_members = true, .prettify = true }>(output, jsonStr)
        : glz::write<glz::opts { .skip_null_members = true }>(output, jsonStr);

      if (errorContext)
        return Err(draconis::utils::error::DracError { draconis::utils::error::DracErrorCode::ParseError, std::format("Failed to write JSON output: {}", glz::format_error(errorContext, jsonStr)) });

      return jsonStr;
    }

    [[nodiscard]] auto getFormatNames() const -> Vec<String> override {
      return { FORMAT_JSON, FORMAT_JSON_PRETTY };
    }

    [[nodiscard]] auto getFileExtension(const String& /*formatName*/) const -> String override {
      return "json";
    }
  };

} // anonymous namespace

DRAC_PLUGIN(JsonFormatPlugin)
