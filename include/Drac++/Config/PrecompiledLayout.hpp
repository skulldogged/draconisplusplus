#pragma once

#if DRAC_PRECOMPILED_CONFIG

  #include <array>
  #include <span>

namespace draconis::config {
  /**
   * @brief Compile-time UI layout row used by precompiled configs.
   */
  struct PrecompiledLayoutRow {
    const char* key;   ///< e.g. "cpu", "plugin.weather.temp", "playing"
    const char* label; ///< Optional label override, nullptr/empty = default
    const char* icon;  ///< Optional icon override, nullptr/empty = default
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
   */
  [[nodiscard]] constexpr fn Row(const char* key, const char* label = nullptr, const char* icon = nullptr) -> PrecompiledLayoutRow {
    return { .key = key, .label = label, .icon = icon };
  }

  /**
   * @brief Helper to build a layout group from a std::array of rows.
   */
  template <std::size_t n>
  [[nodiscard]] constexpr fn Group(const char* name, const std::array<PrecompiledLayoutRow, n>& rows) -> PrecompiledLayoutGroup {
    return { .name = name, .rows = std::span<const PrecompiledLayoutRow>(rows) };
  }
} // namespace draconis::config

#endif // DRAC_PRECOMPILED_CONFIG
