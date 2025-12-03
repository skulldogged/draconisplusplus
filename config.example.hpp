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

  #if DRAC_ENABLE_PACKAGECOUNT
    #include <Drac++/Services/Packages.hpp>
  #endif

namespace draconis::config {
  /**
   * @brief The username to display.
   * @details Used for the greeting message.
   */
  constexpr const char* DRAC_USERNAME = "User";

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
} // namespace draconis::config

#endif // DRAC_PRECOMPILED_CONFIG
