#pragma once

#if DRAC_PRECOMPILED_CONFIG

  #include <array>
  #include <span>

  #include <Drac++/Utils/Logging.hpp>

namespace draconis::config {
  using draconis::utils::logging::LogColor;

  /**
   * @brief Compile-time UI layout row used by precompiled configs.
   */
  struct PrecompiledLayoutRow {
    const char* key;                        ///< e.g. "cpu", "plugin.weather.temp", "playing"
    const char* label;                      ///< Optional label override, nullptr/empty = default
    const char* icon;                       ///< Optional icon override, nullptr/empty = default
    bool        autoWrap = false;           ///< Enable automatic word wrapping
    LogColor    color    = LogColor::White; ///< Value foreground color
  };

  /**
   * @brief Compile-time UI layout group used by precompiled configs.
   */
  struct PrecompiledLayoutGroup {
    const char*                           name; ///< Group label (not rendered, for readability)
    std::span<const PrecompiledLayoutRow> rows;
  };

  /**
   * @brief Helper to build a layout row.
   * @param key The data key (e.g., "cpu", "plugin.weather")
   * @param autoWrap Enable automatic word wrapping
   * @param color Value foreground color (default: White)
   * @param label Optional label override (nullptr = default)
   * @param icon Optional icon override (nullptr = default)
   */
  [[nodiscard]] constexpr auto Row(
    const char* key,
    bool        autoWrap = false,
    LogColor    color    = LogColor::White,
    const char* label    = nullptr,
    const char* icon     = nullptr
  ) -> PrecompiledLayoutRow {
    return { .key = key, .label = label, .icon = icon, .autoWrap = autoWrap, .color = color };
  }

  /**
   * @brief Helper to build a layout group from a std::array of rows.
   */
  template <std::size_t n>
  [[nodiscard]] constexpr auto Group(const char* name, const std::array<PrecompiledLayoutRow, n>& rows) -> PrecompiledLayoutGroup {
    return { .name = name, .rows = std::span<const PrecompiledLayoutRow>(rows) };
  }

  /**
   * @brief Compile-time logo configuration used by precompiled configs.
   * All fields are optional (nullptr/0 means not set).
   */
  struct PrecompiledLogo {
    const char* path     = nullptr; ///< Path to image file, nullptr = not set
    const char* protocol = nullptr; ///< "kitty", "kitty-direct", or "iterm2", nullptr = default (kitty)
    unsigned    width    = 0;       ///< Width in pixels, 0 = not set
    unsigned    height   = 0;       ///< Height in pixels, 0 = not set
  };
} // namespace draconis::config

#endif // DRAC_PRECOMPILED_CONFIG
