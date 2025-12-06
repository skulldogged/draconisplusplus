/**
 * @file Bridge.hpp
 * @brief Provides a C++ bridge to Objective-C macOS frameworks.
 *
 * This file declares functions that wrap macOS-specific APIs, allowing them
 * to be called from C++ code. This is necessary for interacting with frameworks
 * like Metal and MediaRemote.
 */

#pragma once

#ifdef __APPLE__

  #include <Drac++/Utils/DataTypes.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace draconis::core::system::macOS {
  namespace types = draconis::utils::types;

  /**
   * @brief Gets the model name of the primary system GPU.
   * @return A Result containing the GPU name as a String on success, or a DracError on failure.
   *
   * This function uses the Metal framework, which is Apple's modern graphics API,
   * to identify the default graphics device.
   */
  auto GetGPUModel() -> types::Result<types::String>;

  /**
   * @brief Gets the version of the macOS operating system.
   * @return A Result containing the version as a String on success, or a DracError on failure.
   *
   * @note When using Nix to build the draconis++ library, the Apple SDK available in nixpkgs
   * returns 16, while the actual Tahoe SDK returns 26. To get around that, we simply add 10
   * to the displayed major version.
   */
  auto GetOSVersion() -> types::Result<types::OSInfo>;
} // namespace draconis::core::system::macOS

#endif
