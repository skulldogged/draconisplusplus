/**
 * @file yaml_format.cpp
 * @brief YAML output format plugin for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin provides YAML output formatting for system information
 * using the RapidYAML library (single-header amalgamation) for proper YAML generation.
 * It supports a single output mode:
 * - "yaml": Human-readable YAML output
 *
 * This file supports both dynamic (shared library) and static compilation.
 * When compiled as a static plugin (DRAC_STATIC_PLUGIN_BUILD defined),
 * it exports factory functions in a namespace instead of extern "C".
 */

// ryml uses 'fn' as a parameter name,
// but we use 'fn' as a macro for 'auto'
#ifdef fn
  #undef fn
#endif

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "ryml_all.hpp"

#ifndef fn
  #define fn auto
#endif

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace {

  using namespace draconis::utils::types;

  class YamlFormatPlugin : public draconis::core::plugin::IOutputFormatPlugin {
   private:
    draconis::core::plugin::PluginMetadata m_metadata;
    bool                                   m_ready = false;

    static constexpr auto FORMAT_YAML = "yaml";

    /**
     * @brief Helper to get value from data map, returning pointer to the string if found
     * @note Returns pointer to the map's string which remains valid during formatOutput
     */
    static fn getValue(const Map<String, String>& data, const String& key) -> const String* {
      if (auto iter = data.find(key); iter != data.end() && !iter->second.empty())
        return &iter->second;
      return nullptr;
    }

    /**
     * @brief Add a key-value pair to a YAML node if value exists
     * @note The value pointer must remain valid for the lifetime of the tree
     */
    static fn addIfPresent(ryml::NodeRef node, const char* key, const String* value) -> void {
      if (value)
        node[ryml::to_csubstr(key)] = ryml::to_csubstr(*value);
    }

   public:
    YamlFormatPlugin() {
      m_metadata = {
        .name         = "YAML Format",
        .version      = "1.0.0",
        .author       = "Draconis++ Team",
        .description  = "Provides YAML output formatting for system information using RapidYAML",
        .type         = draconis::core::plugin::PluginType::OutputFormat,
        .dependencies = {}
      };
    }

    [[nodiscard]] fn getMetadata() const -> const draconis::core::plugin::PluginMetadata& override {
      return m_metadata;
    }

    fn initialize(const draconis::core::plugin::PluginContext& /*ctx*/, ::PluginCache& /*cache*/) -> Result<Unit> override {
      m_ready = true;
      return {};
    }

    fn shutdown() -> Unit override {
      m_ready = false;
    }

    [[nodiscard]] fn isReady() const -> bool override {
      return m_ready;
    }

    fn formatOutput(
      const String& /*formatName*/,
      const Map<String, String>&              data,
      const Map<String, Map<String, String>>& pluginData
    ) const -> Result<String> override {
      if (!m_ready)
        return Err(draconis::utils::error::DracError { draconis::utils::error::DracErrorCode::Other, "YamlFormatPlugin is not ready." });

      ryml::Tree    tree;
      ryml::NodeRef root = tree.rootref();
      root |= ryml::MAP;

      // General section
      if (getValue(data, "date")) {
        ryml::NodeRef general = root["general"];
        general |= ryml::MAP;
        addIfPresent(general, "date", getValue(data, "date"));
      }

      // Weather section
      if (getValue(data, "weather_temperature")) {
        ryml::NodeRef weather = root["weather"];
        weather |= ryml::MAP;
        addIfPresent(weather, "temperature", getValue(data, "weather_temperature"));
        addIfPresent(weather, "town", getValue(data, "weather_town"));
        addIfPresent(weather, "description", getValue(data, "weather_description"));
      }

      // System section
      if (getValue(data, "host") || getValue(data, "os") || getValue(data, "kernel")) {
        ryml::NodeRef system = root["system"];
        system |= ryml::MAP;
        addIfPresent(system, "host", getValue(data, "host"));
        addIfPresent(system, "operating_system", getValue(data, "os"));
        addIfPresent(system, "os_name", getValue(data, "os_name"));
        addIfPresent(system, "os_version", getValue(data, "os_version"));
        addIfPresent(system, "os_id", getValue(data, "os_id"));
        addIfPresent(system, "kernel", getValue(data, "kernel"));
      }

      // Hardware section
      if (getValue(data, "ram") || getValue(data, "disk") || getValue(data, "cpu") ||
          getValue(data, "gpu") || getValue(data, "uptime")) {
        ryml::NodeRef hardware = root["hardware"];
        hardware |= ryml::MAP;

        // Memory subsection
        if (getValue(data, "ram")) {
          ryml::NodeRef memory = hardware["memory"];
          memory |= ryml::MAP;
          addIfPresent(memory, "info", getValue(data, "ram"));
          addIfPresent(memory, "used_bytes", getValue(data, "memory_used_bytes"));
          addIfPresent(memory, "total_bytes", getValue(data, "memory_total_bytes"));
        }

        // Disk subsection
        if (getValue(data, "disk")) {
          ryml::NodeRef disk = hardware["disk"];
          disk |= ryml::MAP;
          addIfPresent(disk, "info", getValue(data, "disk"));
          addIfPresent(disk, "used_bytes", getValue(data, "disk_used_bytes"));
          addIfPresent(disk, "total_bytes", getValue(data, "disk_total_bytes"));
        }

        // CPU subsection
        if (getValue(data, "cpu")) {
          ryml::NodeRef cpu = hardware["cpu"];
          cpu |= ryml::MAP;
          addIfPresent(cpu, "model", getValue(data, "cpu"));
          addIfPresent(cpu, "cores_physical", getValue(data, "cpu_cores_physical"));
          addIfPresent(cpu, "cores_logical", getValue(data, "cpu_cores_logical"));
        }

        // GPU
        addIfPresent(hardware, "gpu", getValue(data, "gpu"));

        // Uptime subsection
        if (getValue(data, "uptime")) {
          ryml::NodeRef uptime = hardware["uptime"];
          uptime |= ryml::MAP;
          addIfPresent(uptime, "formatted", getValue(data, "uptime"));
          addIfPresent(uptime, "seconds", getValue(data, "uptime_seconds"));
        }
      }

      // Software section
      if (getValue(data, "shell") || getValue(data, "packages")) {
        ryml::NodeRef software = root["software"];
        software |= ryml::MAP;
        addIfPresent(software, "shell", getValue(data, "shell"));
        addIfPresent(software, "package_count", getValue(data, "packages"));
      }

      // Environment section
      if (getValue(data, "de") || getValue(data, "wm")) {
        ryml::NodeRef environment = root["environment"];
        environment |= ryml::MAP;
        addIfPresent(environment, "desktop_environment", getValue(data, "de"));
        addIfPresent(environment, "window_manager", getValue(data, "wm"));
      }

      // Media section (Now Playing)
      if (getValue(data, "playing") || getValue(data, "playing_artist") || getValue(data, "playing_title")) {
        ryml::NodeRef media = root["media"];
        media |= ryml::MAP;
        ryml::NodeRef nowPlaying = media["now_playing"];
        nowPlaying |= ryml::MAP;

        const auto* artist   = getValue(data, "playing_artist");
        nowPlaying["artist"] = artist ? ryml::to_csubstr(*artist) : ryml::csubstr("Unknown Artist");

        const auto* title   = getValue(data, "playing_title");
        nowPlaying["title"] = title ? ryml::to_csubstr(*title) : ryml::csubstr("Unknown Title");
      }

      // Plugin data section - use pluginData directly
      if (!pluginData.empty()) {
        ryml::NodeRef pluginsNode = root["plugins"];
        pluginsNode |= ryml::MAP;

        for (const auto& [pluginId, fields] : pluginData) {
          // Copy plugin ID to arena so it outlives the loop
          ryml::csubstr arenaPluginId = tree.copy_to_arena(ryml::to_csubstr(pluginId));
          pluginsNode[arenaPluginId] |= ryml::MAP;

          for (const auto& [fieldName, value] : fields) {
            ryml::csubstr arenaFieldName               = tree.copy_to_arena(ryml::to_csubstr(fieldName));
            pluginsNode[arenaPluginId][arenaFieldName] = ryml::to_csubstr(value);
          }
        }
      }

      // Emit YAML with document start marker
      String yaml = "---\n";
      yaml += ryml::emitrs_yaml<String>(tree);

      return yaml;
    }

    [[nodiscard]] fn getFormatNames() const -> Vec<String> override {
      return { FORMAT_YAML };
    }

    [[nodiscard]] fn getFileExtension(const String& /*formatName*/) const -> String override {
      return "yaml";
    }
  };

} // anonymous namespace

DRAC_PLUGIN(YamlFormatPlugin)
