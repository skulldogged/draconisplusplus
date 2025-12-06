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

  /**
   * @brief Zero-overhead builder for generating Markdown documents
   *
   * Handles the "only print header if data exists" logic automatically.
   * Sections are buffered and only committed if they contain content.
   */
  class MarkdownBuilder {
    String m_fullDoc;
    String m_currentSectionBuffer;
    String m_currentHeader;

   public:
    MarkdownBuilder() {
      m_fullDoc.reserve(2048);
    }

    /**
     * @brief Start a new section (e.g., "Hardware")
     * @param title The section title
     *
     * Won't be written to the main doc until commit() or a new section starts
     */
    auto section(std::string_view title) -> void {
      commit();
      m_currentHeader = std::format("## {}\n\n", title);
    }

    /**
     * @brief Add a line entry "- **Label**: Value"
     * @param label The label text
     * @param value The value text (skipped if empty)
     */
    auto line(std::string_view label, std::string_view value) -> void {
      if (!value.empty())
        std::format_to(std::back_inserter(m_currentSectionBuffer), "- **{}**: {}\n", label, value);
    }

    /**
     * @brief Look up map key, check existence and empty, add line if valid
     * @param data The data map to search
     * @param key The key to look up
     * @param label The display label for the line
     */
    auto mapEntry(const Map<String, String>& data, const String& key, std::string_view label) -> void {
      if (auto iter = data.find(key); iter != data.end() && !iter->second.empty())
        line(label, iter->second);
    }

    /**
     * @brief Finalize and return the document
     * @return The complete markdown string
     */
    auto build() -> String {
      commit();
      return std::move(m_fullDoc);
    }

    /**
     * @brief Add raw markdown directly (for titles, etc)
     * @param text Raw markdown text to append
     */
    auto raw(std::string_view text) -> void {
      commit();
      m_fullDoc += text;
    }

   private:
    /**
     * @brief Only appends header + section if section has content
     */
    auto commit() -> void {
      if (!m_currentSectionBuffer.empty()) {
        m_fullDoc += m_currentHeader;
        m_fullDoc += m_currentSectionBuffer;
        m_fullDoc += '\n';
        m_currentSectionBuffer.clear();
      }
    }
  };

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

    auto formatOutput(
      const String& /*formatName*/,
      const Map<String, String>&              data,
      const Map<String, Map<String, String>>& pluginData
    ) const -> Result<String> override {
      if (!m_ready)
        return Err(
          draconis::utils::error::DracError { draconis::utils::error::DracErrorCode::Other, "MarkdownFormatPlugin is not ready." }
        );

      MarkdownBuilder builder;

      // 1. Title
      builder.raw("# System Information\n\n");

      // 2. General Section
      builder.section("General");
      builder.mapEntry(data, "date", "Date");

      // Weather requires special handling due to formatting logic
      if (auto iter = data.find("weather_temperature"); iter != data.end() && !iter->second.empty()) {
        try {
          double temperature = std::stod(iter->second);
          String suffix;

          if (auto town = data.find("weather_town"); town != data.end() && !town->second.empty())
            suffix = std::format(" in {}", town->second);
          else if (auto desc = data.find("weather_description"); desc != data.end() && !desc->second.empty())
            suffix = std::format(", {}", desc->second);

          builder.line("Weather", std::format("{}°{}", std::lround(temperature), suffix));
        } catch (...) {
          (void)0; // Ignore invalid temperature values
        }
      }

      // 3. System Section
      builder.section("System");
      builder.mapEntry(data, "host", "Host");
      builder.mapEntry(data, "os", "OS");
      builder.mapEntry(data, "kernel", "Kernel");

      // 4. Hardware Section
      builder.section("Hardware");
      builder.mapEntry(data, "ram", "RAM");
      builder.mapEntry(data, "disk", "Disk");
      builder.mapEntry(data, "cpu", "CPU");
      builder.mapEntry(data, "gpu", "GPU");
      builder.mapEntry(data, "uptime", "Uptime");

      // 5. Software Section
      builder.section("Software");
      builder.mapEntry(data, "shell", "Shell");

      // Packages requires validation (skip if zero)
      if (auto iter = data.find("packages"); iter != data.end() && !iter->second.empty()) {
        try {
          if (u64 count = std::stoull(iter->second); count > 0)
            builder.line("Packages", std::to_string(count));
        } catch (...) {
          (void)0; // Ignore invalid package count values
        }
      }

      // 6. Environment Section
      builder.section("Environment");
      builder.mapEntry(data, "de", "Desktop Environment");
      builder.mapEntry(data, "wm", "Window Manager");

      // 7. Dynamic Plugin Data
      if (!pluginData.empty()) {
        builder.raw("## Plugin Data\n\n");
        for (const auto& [pluginId, fields] : pluginData) {
          builder.raw(std::format("### {}\n\n", pluginId));
          for (const auto& [fieldName, value] : fields)
            builder.raw(std::format("- **{}**: {}\n", fieldName, value));
          builder.raw("\n");
        }
      }

      return builder.build();
    }

    [[nodiscard]] auto getFormatNames() const -> Vec<String> override {
      return { FORMAT_MARKDOWN };
    }

    [[nodiscard]] auto getFileExtension(const String& /*formatName*/) const -> String override {
      return "md";
    }
  };

} // anonymous namespace

DRAC_PLUGIN(MarkdownFormatPlugin)
