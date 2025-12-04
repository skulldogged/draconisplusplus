#ifdef __HAIKU__

// clang-format off
#include <AppFileInfo.h>               // For BAppFileInfo and version_info
#include <cerrno>                      // errno
#include <Errors.h>                    // B_OK, strerror, status_t
#include <File.h>                      // For BFile
#include <OS.h>                        // get_system_info, system_info
#include <climits>                     // HOST_NAME_MAX
#include <cstring>                     // std::strlen, strerror
#include <os/package/PackageDefs.h>    // BPackageKit::BPackageInfoSet
#include <os/package/PackageInfoSet.h> // BPackageKit::BPackageInfo
#include <os/package/PackageRoster.h>  // BPackageKit::BPackageRoster
#include <utility>                     // std::move

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Env.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "OS/Unix.hpp"
// clang-format on

using namespace draconis::utils::types;
using draconis::utils::cache::CacheManager;
using draconis::utils::env::GetEnv;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace draconis::core::system {
  fn GetOperatingSystem(CacheManager& cache) -> Result<OSInfo> {
    return cache.getOrSet<OSInfo>("haiku_os_info", []() -> Result<OSInfo> {
      BFile    file;
      status_t status = file.SetTo("/boot/system/lib/libbe.so", B_READ_ONLY);

      if (status != B_OK)
        return Err(DracError(DracErrorCode::InternalError, "Error opening /boot/system/lib/libbe.so"));

      BAppFileInfo appInfo;
      status = appInfo.SetTo(&file);

      if (status != B_OK)
        return Err(DracError(DracErrorCode::InternalError, "Error initializing BAppFileInfo"));

      version_info versionInfo;
      status = appInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND);

      if (status != B_OK)
        return Err(DracError(DracErrorCode::InternalError, "Error reading version info attribute"));

      String versionShortString = versionInfo.short_info;

      if (versionShortString.empty())
        return Err(DracError(InternalError, "Version info short_info is empty"));

      return OSInfo("Haiku", String(versionShortString.c_str()), "haiku");
    });
  }

  fn GetMemInfo(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    system_info    sysinfo;
    const status_t status = get_system_info(&sysinfo);

    if (status != B_OK)
      return Err(DracError(InternalError, std::format("get_system_info failed: {}", strerror(status))));

    const u64 totalMem = static_cast<u64>(sysinfo.max_pages) * B_PAGE_SIZE;
    const u64 usedMem  = static_cast<u64>(sysinfo.used_pages) * B_PAGE_SIZE;

    return ResourceUsage(usedMem, totalMem);
  }

  fn GetWindowManager(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("haiku_wm", []() -> Result<String> {
      return "app_server";
    });
  }

  fn GetDesktopEnvironment(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("haiku_desktop_environment", []() -> Result<String> {
      return "Haiku Desktop Environment";
    });
  }

  fn GetShell(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("haiku_shell", []() -> Result<String> {
      if (const Result<String> shellPath = GetEnv("SHELL")) {
        // clang-format off
        constexpr Array<Pair<StringView, StringView>, 5> shellMap {{
          { "bash",    "Bash" },
          {  "zsh",     "Zsh" },
          { "fish",    "Fish" },
          {   "nu", "Nushell" },
          {   "sh",      "SH" }, // sh last because other shells contain "sh"
        }};
        // clang-format on

        for (const auto& [exe, name] : shellMap)
          if (shellPath->contains(exe))
            return String(name);

        return *shellPath; // fallback to the raw shell path
      }

      return Err(DracError(NotFound, "Could not find SHELL environment variable"));
    });
  }

  fn GetHost(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("haiku_host", []() -> Result<String> {
      Array<char, HOST_NAME_MAX + 1> hostnameBuffer {};

      if (gethostname(hostnameBuffer.data(), hostnameBuffer.size()) != 0)
        return Err(DracError(
          ApiUnavailable, std::format("gethostname() failed: {} (errno {})", strerror(errno), errno)
        ));

      hostnameBuffer.at(HOST_NAME_MAX) = '\0';

      return String(hostnameBuffer.data());
    });
  }

  fn GetKernelVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("haiku_kernel_version", []() -> Result<String> {
      system_info    sysinfo;
      const status_t status = get_system_info(&sysinfo);

      if (status != B_OK)
        return Err(DracError(InternalError, std::format("get_system_info failed: {}", strerror(status))));

      return std::to_string(sysinfo.kernel_version);
    });
  }

  fn GetDiskUsage(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    // Haiku uses /boot as the primary filesystem
    return os::unix_shared::GetDiskUsageAt("/boot");
  }
} // namespace draconis::core::system

#endif // __HAIKU__
