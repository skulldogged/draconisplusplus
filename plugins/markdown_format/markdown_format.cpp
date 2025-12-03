/**
 * @file markdown_format.cpp
 * @brief Markdown output format plugin for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin provides markdown output formatting for system information.
 * It extracts the markdown formatting logic from the main application into a plugin.
 *
 * This file supports both dynamic (shared library) and static compilation.
 * When compiled as a static plugin (DRAC_STATIC_PLUGIN_BUILD defined),
 * it exports factory functions in a namespace instead of extern "C".
 */

#include <cmath>
#include <format>

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace {

  using namespace draconis::utils::types;

  class MarkdownFormatPlugin : public draconis::core::plugin::IOutputFormatPlugin {
   private:
    draconis::core::plugin::PluginMetadata m_metadata;
    bool                                   m_ready = false;

    static constexpr auto FORMAT_MARKDOWN = "markdown";

   public:
    MarkdownFormatPlugin() {
      m_metadata = {
        .name         = "Markdown Format",
        .version      = "1.0.0",
        .author       = "Draconis++ Team",
        .description  = "Provides markdown output formatting for system information",
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
        return Err(draconis::utils::error::DracError { draconis::utils::error::DracErrorCode::Other, "MarkdownFormatPlugin is not ready." });

      String markdown;
      markdown.reserve(2048);

      // Title
      markdown += "# System Information\n\n";

      // Date section
      if (auto date = data.find("date"); date != data.end() && !date->second.empty()) {
        markdown += "## General\n\n";
        markdown += std::format("- **Date**: {}\n", date->second);
      }

      // Weather section
      if (auto weatherTemp = data.find("weather_temperature"); weatherTemp != data.end()) {
        if (!weatherTemp->second.empty()) {
          String weatherValue;
          double temperature = 0.0;
          try {
            temperature = std::stod(weatherTemp->second);
          } catch (...) {
            temperature = 0.0;
          }

          if (auto townName = data.find("weather_town"); townName != data.end() && !townName->second.empty())
            weatherValue = std::format("{}° in {}", std::lround(temperature), townName->second);
          else if (auto description = data.find("weather_description"); description != data.end())
            weatherValue = std::format("{}°, {}", std::lround(temperature), description->second);
          else
            weatherValue = std::format("{}°", std::lround(temperature));

          if (!weatherValue.empty()) {
            if (markdown.find("## General\n\n") == std::string::npos)
              markdown += "## General\n\n";

            markdown += std::format("- **Weather**: {}\n", weatherValue);
          }
        }
      }

      markdown += "\n";

      // System Information section
      bool   hasSystemInfo = false;
      String systemSection;

      if (auto host = data.find("host"); host != data.end() && !host->second.empty()) {
        systemSection += std::format("- **Host**: {}\n", host->second);
        hasSystemInfo = true;
      }

      if (auto operSys = data.find("os"); operSys != data.end() && !operSys->second.empty()) {
        systemSection += std::format("- **OS**: {}\n", operSys->second);
        hasSystemInfo = true;
      }

      if (auto kernel = data.find("kernel"); kernel != data.end() && !kernel->second.empty()) {
        systemSection += std::format("- **Kernel**: {}\n", kernel->second);
        hasSystemInfo = true;
      }

      if (hasSystemInfo) {
        markdown += "## System\n\n";
        markdown += systemSection;
        markdown += "\n";
      }

      // Hardware section
      bool   hasHardwareInfo = false;
      String hardwareSection;

      if (auto ram = data.find("ram"); ram != data.end() && !ram->second.empty()) {
        hardwareSection += std::format("- **RAM**: {}\n", ram->second);
        hasHardwareInfo = true;
      }

      if (auto disk = data.find("disk"); disk != data.end() && !disk->second.empty()) {
        hardwareSection += std::format("- **Disk**: {}\n", disk->second);
        hasHardwareInfo = true;
      }

      if (auto cpu = data.find("cpu"); cpu != data.end() && !cpu->second.empty()) {
        hardwareSection += std::format("- **CPU**: {}\n", cpu->second);
        hasHardwareInfo = true;
      }

      if (auto gpu = data.find("gpu"); gpu != data.end() && !gpu->second.empty()) {
        hardwareSection += std::format("- **GPU**: {}\n", gpu->second);
        hasHardwareInfo = true;
      }

      if (auto uptime = data.find("uptime"); uptime != data.end() && !uptime->second.empty()) {
        hardwareSection += std::format("- **Uptime**: {}\n", uptime->second);
        hasHardwareInfo = true;
      }

      if (hasHardwareInfo) {
        markdown += "## Hardware\n\n";
        markdown += hardwareSection;
        markdown += "\n";
      }

      // Software section
      bool   hasSoftwareInfo = false;
      String softwareSection;

      if (auto shell = data.find("shell"); shell != data.end() && !shell->second.empty()) {
        softwareSection += std::format("- **Shell**: {}\n", shell->second);
        hasSoftwareInfo = true;
      }

      if (auto packages = data.find("packages"); packages != data.end() && !packages->second.empty()) {
        try {
          unsigned long long packageCount = std::stoull(packages->second);
          if (packageCount > 0) {
            softwareSection += std::format("- **Packages**: {}\n", packageCount);
            hasSoftwareInfo = true;
          }
        } catch (const std::exception& ex) {
          (void)ex;
        }
      }

      if (hasSoftwareInfo) {
        markdown += "## Software\n\n";
        markdown += softwareSection;
        markdown += "\n";
      }

      // Environment section
      bool   hasEnvironmentInfo = false;
      String environmentSection;

      if (auto desktop = data.find("de"); desktop != data.end() && !desktop->second.empty()) {
        environmentSection += std::format("- **Desktop Environment**: {}\n", desktop->second);
        hasEnvironmentInfo = true;
      }

      if (auto winMgr = data.find("wm"); winMgr != data.end() && !winMgr->second.empty()) {
        environmentSection += std::format("- **Window Manager**: {}\n", winMgr->second);
        hasEnvironmentInfo = true;
      }

      if (hasEnvironmentInfo) {
        markdown += "## Environment\n\n";
        markdown += environmentSection;
        markdown += "\n";
      }

      // Now Playing section (always show if data present)
      if (auto artist = data.find("playing_artist"); artist != data.end())
        if (auto title = data.find("playing_title"); title != data.end()) {
          String artistStr = artist->second.empty() ? "Unknown Artist" : artist->second;
          String titleStr  = title->second.empty() ? "Unknown Title" : title->second;
          markdown += "## Media\n\n";
          markdown += std::format("- **Now Playing**: {} - {}\n", artistStr, titleStr);
          markdown += "\n";
        }

      // Plugin data section - use pluginData directly
      if (!pluginData.empty()) {
        markdown += "## Plugin Data\n\n";
        for (const auto& [pluginId, fields] : pluginData) {
          markdown += std::format("### {}\n\n", pluginId);
          for (const auto& [fieldName, value] : fields)
            markdown += std::format("- **{}**: {}\n", fieldName, value);
          markdown += "\n";
        }
      }

      return markdown;
    }

    [[nodiscard]] fn getFormatNames() const -> Vec<String> override {
      return { FORMAT_MARKDOWN };
    }

    [[nodiscard]] fn getFileExtension(const String& /*formatName*/) const -> String override {
      return "md";
    }
  };

} // anonymous namespace

DRAC_PLUGIN(MarkdownFormatPlugin)
