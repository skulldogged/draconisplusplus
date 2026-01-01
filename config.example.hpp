/**
 * @file config.example.hpp
 * @brief Example configuration file for the application.
 *
 * @details This file serves as a template for `config.hpp`.
 * Users should copy this file to `config.hpp` and customize the
 * settings according to their preferences.
 *
 * To enable these precompiled settings, `DRAC_PRECOMPILED_CONFIG` must be defined
 * in your build system or `meson.options`.
 *
 * @note When DRAC_PRECOMPILED_CONFIG is enabled together with DRAC_ENABLE_PLUGINS,
 * plugins specified via `-Dstatic_plugins=...` meson option will be statically
 * compiled into the final binary, making it fully portable without needing
 * separate plugin files.
 */
#pragma once

#if DRAC_PRECOMPILED_CONFIG

  #include <array>

  #include <Drac++/Config/PrecompiledLayout.hpp>

  #if DRAC_ENABLE_PACKAGECOUNT
    #include <Drac++/Services/Packages.hpp>
  #endif

namespace draconis::config {
  /**
   * @brief The username to display.
   * @details Used for the greeting message.
   */
  constexpr const char* DRAC_USERNAME = "User";

  /**
   * @brief Logo configuration for inline image rendering.
   * @details Supports Kitty, Kitty-Direct, and iTerm2 protocols.
   * Set path to nullptr to disable inline logo and use ASCII art instead.
   *
   * Example with custom logo:
   * @code{.cpp}
   * inline constexpr PrecompiledLogo DRAC_LOGO = {
   *   .path     = "/path/to/logo.png",
   *   .protocol = "kitty",  // or "kitty-direct", "iterm2"
   *   .width    = 200,      // optional, in pixels
   *   .height   = 0,        // optional, 0 = auto from aspect ratio
   * };
   * @endcode
   */
  inline constexpr PrecompiledLogo DRAC_LOGO = {};

  #if DRAC_ENABLE_PACKAGECOUNT
  /**
   * @brief Configures which package managers' counts are displayed.
   *
   * This is a bitmask field. Combine multiple `Manager` enum values
   * using the bitwise OR operator (`|`).
   * The available `Manager` enum values are defined in `Util/ConfigData.hpp`
   * and may vary based on the operating system.
   *
   * @see Manager
   * @see HasPackageManager
   * @see Util/ConfigData.hpp
   *
   * To enable Cargo, Pacman, and Nix package managers:
   * @code{.cpp}
   * constexpr Manager DRAC_ENABLED_PACKAGE_MANAGERS = Manager::Cargo | Manager::Pacman | Manager::Nix;
   * @endcode
   *
   * To enable only Cargo:
   * @code{.cpp}
   * constexpr Manager DRAC_ENABLED_PACKAGE_MANAGERS = Manager::Cargo;
   * @endcode
   */
  constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = services::packages::Manager::Cargo;
  #endif

  /**
   * @brief UI Layout Configuration
   *
   * The UI is organized into groups, each containing rows that display system information.
   * Each row is created using the `Row()` helper function with the following signature:
   *
   * @code{.cpp}
   * Row(key, label = nullptr, icon = nullptr)
   * @endcode
   *
   * @param key    The data key identifying what information to display.
   *               Built-in keys include: "date", "host", "os", "kernel", "cpu", "gpu",
   *               "ram", "disk", "uptime", "shell", "packages", "de", "wm", "playing".
   *               Plugin keys use the format "plugin.<name>" (e.g., "plugin.weather").
   * @param label  Optional custom label. If nullptr or empty, uses the default label.
   * @param icon   Optional custom icon. If nullptr or empty, uses the default icon.
   *
   * Groups are created using the `Group()` helper:
   * @code{.cpp}
   * Group(name, rows_array)
   * @endcode
   *
   * @note Row arrays must be declared as separate `inline constexpr` variables because
   *       `PrecompiledLayoutGroup` uses `std::span` internally, which requires the
   *       underlying data to have static storage duration. Inline temporaries would
   *       result in dangling pointers.
   *
   * @note The array size in the template parameter (e.g., `std::array<..., 3>`) must
   *       match the actual number of elements in the initializer list.
   *
   * Example with custom label and icon:
   * @code{.cpp}
   * inline constexpr std::array<PrecompiledLayoutRow, 2> DRAC_UI_INTRO_ROWS = {
   *   Row("date"),
   *   Row("plugin.weather", "Weather", " 󰖐  "),  // Custom label and icon
   * };
   * @endcode
   */

  /**
   * @brief Introductory information rows (date, weather, etc.)
   */
  inline constexpr std::array<PrecompiledLayoutRow, 1> DRAC_UI_INTRO_ROWS = {
    Row("date"),
  };

  /**
   * @brief System information rows (host, OS, kernel)
   */
  inline constexpr std::array<PrecompiledLayoutRow, 3> DRAC_UI_SYSTEM_ROWS = {
    Row("host"),
    Row("os"),
    Row("kernel"),
  };

  /**
   * @brief Hardware information rows (CPU, GPU, RAM, disk, uptime)
   */
  inline constexpr std::array<PrecompiledLayoutRow, 5> DRAC_UI_HARDWARE_ROWS = {
    Row("cpu"),
    Row("gpu"),
    Row("ram"),
    Row("disk"),
    Row("uptime"),
  };

  /**
   * @brief Software information rows (shell, package counts)
   */
  inline constexpr std::array<PrecompiledLayoutRow, 2> DRAC_UI_SOFTWARE_ROWS = {
    Row("shell"),
    Row("packages"),
  };

  /**
   * @brief Session information rows (desktop environment, window manager, now playing)
   */
  inline constexpr std::array<PrecompiledLayoutRow, 3> DRAC_UI_SESSION_ROWS = {
    Row("de"),
    Row("wm"),
    Row("playing"),
  };

  /**
   * @brief The complete UI layout combining all groups.
   * @details Groups are displayed in the order they appear in this array.
   */
  inline constexpr std::array<PrecompiledLayoutGroup, 5> DRAC_UI_LAYOUT = {
    Group("intro", DRAC_UI_INTRO_ROWS),
    Group("system", DRAC_UI_SYSTEM_ROWS),
    Group("hardware", DRAC_UI_HARDWARE_ROWS),
    Group("software", DRAC_UI_SOFTWARE_ROWS),
    Group("session", DRAC_UI_SESSION_ROWS),
  };

  //============================================================================
  // Plugin Configurations
  //
  // Only define configs for plugins you're actually using!
  // If you don't include a plugin in -Dstatic_plugins, you don't need its config.
  //============================================================================

  //==============================================================================
  // Weather Plugin Config - only needed if using -Dstatic_plugins=weather
  //==============================================================================
  #include "plugins/weather/WeatherConfig.hpp"

  /**
   * @brief Weather Plugin Configuration
   *
   * Location can be either:
   * - Coordinates{lat, lon} for OpenMeteo/MetNo/OpenWeatherMap
   * - CityName{"city, country"} for OpenWeatherMap only (requires API key)
   */
  inline constexpr auto WEATHER_CONFIG = weather::config::MakeConfig(
    weather::config::Provider::OpenMeteo,
    weather::config::Units::Metric,
    weather::config::Coordinates { 40.7128, -74.0060 } // New York
  );

  // Example: Using city name with OpenWeatherMap (requires API key)
  // inline constexpr auto WEATHER_CONFIG = weather::config::MakeConfig(
  //   weather::config::Provider::OpenWeatherMap,
  //   weather::config::Units::Imperial,
  //   weather::config::CityName { "New York, US" },
  //   "your_api_key_here"
  // );

} // namespace draconis::config

#endif // DRAC_PRECOMPILED_CONFIG
