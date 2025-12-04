/**
 * @file Bridge.mm
 * @brief macOS-specific implementations for retrieving system information.
 *
 * This file contains functions that interact with private and public macOS frameworks
 * (MediaRemote and Metal) to fetch details about the currently playing media and the system's GPU.
 * This implementation is conditionally compiled and should only be included on Apple platforms.
 */

#ifdef __APPLE__

  #include "Bridge.hpp"

  #include <Metal/Metal.h>       // For MTLDevice to identify the GPU.

  #include <Drac++/Utils/Error.hpp>

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace draconis::core::system::macOS {
  fn GetGPUModel() -> Result<String> {
    @autoreleasepool {
      // Get the default Metal device, which typically corresponds to the active, primary GPU.
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();

      if (!device)
        return Err(DracError(ApiUnavailable, "Failed to get default Metal device. No Metal-compatible GPU found."));

      NSString* gpuName = device.name;
      if (!gpuName)
        return Err(DracError(NotFound, "Failed to get GPU name from Metal device."));

      return [gpuName UTF8String];
    }
  }

  fn GetOSVersion() -> Result<OSInfo> {
    @autoreleasepool {
      using matchit::match, matchit::is, matchit::_;

      // NSProcessInfo is the easiest/fastest way to get the OS version.
      NSProcessInfo*           processInfo = [NSProcessInfo processInfo];
      NSOperatingSystemVersion version     = [processInfo operatingSystemVersion];

      return OSInfo(
        "macOS",
        std::format(
          "{}.{} {}",
          version.majorVersion,
          version.minorVersion,
          match(version.majorVersion)(
            is | 11 = "Big Sur",
            is | 12 = "Monterey",
            is | 13 = "Ventura",
            is | 14 = "Sonoma",
            is | 15 = "Sequoia",
            is | 26 = "Tahoe",
            is | _  = "Unknown"
          )
        ),
        "macos"
      );
    }
  }
} // namespace draconis::core::system::macOS

#endif
