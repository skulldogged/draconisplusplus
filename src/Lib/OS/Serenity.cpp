#ifdef __serenity__

// clang-format off
#include <cerrno>                 // errno
#include <climits>                // HOST_NAME_MAX
#include <cstring>                // strerror
#include <format>                 // std::format
#include <fstream>                // std::ifstream
#include <glaze/core/common.hpp>  // glz::object
#include <glaze/core/context.hpp> // glz::{error_ctx, error_code}
#include <glaze/core/meta.hpp>    // glz::detail::Object
#include <glaze/core/read.hpp>    // glz::read
#include <glaze/core/reflect.hpp> // glz::format_error
#include <glaze/json/read.hpp>    // glz::read<glaze_opts>
#include <iterator>               // std::istreambuf_iterator
#include <pwd.h>                  // getpwuid, passwd
#include <string>                 // std::string (String)
#include <sys/types.h>            // uid_t
#include <unistd.h>               // getuid, gethostname
#include <unordered_set>          // std::unordered_set

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

namespace {
  using glz::opts, glz::detail::Object, glz::object;

  constexpr opts glaze_opts = { .error_on_unknown_keys = false };

  struct MemStatData {
    u64 physical_allocated = 0;
    u64 physical_available = 0;

    // NOLINTBEGIN(readability-identifier-naming)
    struct glaze {
      using T = MemStatData;
      static constexpr Object value =
        object("physical_allocated", &T::physical_allocated, "physical_available", &T::physical_available);
    };
    // NOLINTEND(readability-identifier-naming)
  };

  fn CountUniquePackages(const String& dbPath) -> Result<u64> {
    std::ifstream dbFile(dbPath);

    if (!dbFile.is_open())
      return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open file: {}", dbPath)));

    std::unordered_set<String> uniquePackages;
    String                     line;

    while (std::getline(dbFile, line))
      if (line.starts_with("manual ") || line.starts_with("auto "))
        uniquePackages.insert(line);

    return uniquePackages.size();
  }
} // namespace

namespace draconis::core::system {
  fn GetOperatingSystem(CacheManager& cache) -> Result<OSInfo> {
    return cache.getOrSet<OSInfo>("serenity_os_info", []() -> Result<OSInfo> {
      Result<os::unix_shared::UnameInfo> unameInfo = os::unix_shared::GetUnameInfo();

      if (!unameInfo)
        return Err(unameInfo.error());

      return OSInfo(unameInfo->sysname, unameInfo->release, "serenity");
    });
  }

  fn GetMemInfo(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    constexpr PCStr path = "/sys/kernel/memstat";
    std::ifstream   file(path);

    if (!file)
      return Err(DracError(NotFound, std::format("Could not open {}", path)));

    String buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (buffer.empty())
      return Err(DracError(IoError, std::format("File is empty: {}", path)));

    MemStatData data;

    glz::error_ctx error_context = glz::read<glaze_opts>(data, buffer);

    if (error_context)
      return Err(DracError(
        ParseError,
        std::format("Failed to parse JSON from {}: {}", path, glz::format_error(error_context, buffer))
      ));

    if (data.physical_allocated > std::numeric_limits<u64>::max() - data.physical_available)
      return Err(DracError(InternalError, "Memory size overflow during calculation"));

    const u64 totalMem = (data.physical_allocated + data.physical_available) * PAGE_SIZE;
    const u64 usedMem  = data.physical_allocated * PAGE_SIZE;

    return ResourceUsage(usedMem, totalMem);
  }

  fn GetWindowManager(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("serenity_wm", []() -> Result<String> {
      return "WindowManager";
    });
  }

  fn GetDesktopEnvironment(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("serenity_desktop_environment", []() -> Result<String> {
      return "SerenityOS Desktop";
    });
  }

  fn GetShell(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("serenity_shell", []() -> Result<String> {
      uid_t   userId = getuid();
      passwd* pw     = getpwuid(userId);

      if (pw == nullptr)
        return Err(DracError(NotFound, std::format("User ID {} not found in /etc/passwd", userId)));

      if (pw->pw_shell == nullptr || *(pw->pw_shell) == '\0')
        return Err(DracError(
          NotFound, std::format("User shell entry is empty in /etc/passwd for user ID {}", userId)
        ));

      String shell = pw->pw_shell;

      if (shell.starts_with("/bin/"))
        shell = shell.substr(5);

      return shell;
    });
  }

  fn GetHost(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("serenity_host", []() -> Result<String> {
      Array<char, HOST_NAME_MAX> hostname_buffer {};

      if (gethostname(hostname_buffer.data(), hostname_buffer.size()) != 0)
        return Err(DracError(ApiUnavailable, std::format("gethostname() failed: {} (errno {})", strerror(errno), errno)));

      return String(hostname_buffer.data());
    });
  }

  fn GetKernelVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("serenity_kernel_version", []() -> Result<String> {
      return os::unix_shared::GetKernelRelease();
    });
  }

  fn GetDiskUsage(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    return os::unix_shared::GetRootDiskUsage();
  }
} // namespace draconis::core::system

#endif // __serenity__
