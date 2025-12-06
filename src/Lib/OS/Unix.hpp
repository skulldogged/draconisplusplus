/**
 * @file Unix.hpp
 * @brief Shared utilities for Unix-like operating systems (Linux, macOS, BSD, Haiku, SerenityOS).
 *
 * @details This header provides common implementations for system information retrieval
 * that are shared across POSIX-compliant platforms. Functions are implemented as
 * inline/constexpr to enable inlining and avoid ODR issues when included in multiple TUs.
 *
 * Key features:
 * - Disk usage via statvfs
 * - Kernel version via uname
 * - Network interface enumeration via getifaddrs
 * - Environment variable utilities
 */

#pragma once

#if !defined(_WIN32)

  #include <cerrno>
  #include <chrono>
  #include <cstring>
  #include <format>
  #include <sys/statvfs.h>
  #include <sys/utsname.h>
  #include <utility>

  #if defined(__linux__)
    #include <sys/sysinfo.h>
  #endif

  #include <Drac++/Utils/DataTypes.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Types.hpp>

  // Network-related headers (not available on all platforms)
  #if __has_include(<ifaddrs.h>)
    #define DRAC_HAS_IFADDRS 1
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <netinet/in.h>
  #else
    #define DRAC_HAS_IFADDRS 0
  #endif

namespace draconis::os::unix_shared {
  namespace types = ::draconis::utils::types;
  namespace error = ::draconis::utils::error;

  using enum error::DracErrorCode;

  // ─────────────────────────────────────────────────────────────────────────────
  // Disk Usage (statvfs)
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Gets disk usage statistics for a given path using statvfs.
   * @param path The filesystem path to query (e.g., "/", "/home", "/boot").
   * @return ResourceUsage containing used and total bytes, or an error.
   *
   * @note This is a common implementation used by Linux, macOS, BSD, Haiku, and SerenityOS.
   */
  [[nodiscard]] inline auto GetDiskUsageAt(const char* path) -> types::Result<types::ResourceUsage> {
    struct statvfs stat;

    if (statvfs(path, &stat) == -1)
      return types::Err(error::DracError(error::DracErrorCode::IoError, std::format("statvfs('{}') failed: {} (errno {})", path, std::strerror(errno), errno)));

    const types::u64 blockSize  = stat.f_frsize;
    const types::u64 totalBytes = stat.f_blocks * blockSize;
    const types::u64 freeBytes  = stat.f_bfree * blockSize;
    const types::u64 usedBytes  = totalBytes - freeBytes;

    return types::ResourceUsage(usedBytes, totalBytes);
  }

  /**
   * @brief Gets disk usage statistics for the root filesystem.
   * @return ResourceUsage containing used and total bytes, or an error.
   */
  [[nodiscard]] inline auto GetRootDiskUsage() -> types::Result<types::ResourceUsage> {
    return GetDiskUsageAt("/");
  }

  /**
   * @brief Gets detailed disk information for a given path.
   * @param path The filesystem path to query.
   * @param name Optional display name for the disk (defaults to path).
   * @return DiskInfo struct with filesystem details, or an error.
   */
  [[nodiscard]] inline auto GetDiskInfoAt(const char* path, const char* name = nullptr)
    -> types::Result<types::DiskInfo> {
    struct statvfs stat;

    if (statvfs(path, &stat) == -1)
      return types::Err(error::DracError(IoError, std::format("statvfs('{}') failed: {} (errno {})", path, std::strerror(errno), errno)));

    const types::u64 blockSize  = stat.f_frsize;
    const types::u64 totalBytes = stat.f_blocks * blockSize;
    const types::u64 freeBytes  = stat.f_bfree * blockSize;

    return types::DiskInfo {
      .name          = name != nullptr ? types::String(name) : types::String(path),
      .mountPoint    = types::String(path),
      .filesystem    = types::String {},            // Filesystem type requires platform-specific code
      .driveType     = types::String { "Unknown" }, // Drive type requires platform-specific code
      .totalBytes    = totalBytes,
      .usedBytes     = totalBytes - freeBytes,
      .isSystemDrive = (std::strcmp(path, "/") == 0),
    };
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Kernel Version (uname)
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Gets the kernel release version string via uname.
   * @return Kernel release string (e.g., "6.14.4", "24.1.0"), or an error.
   *
   * @note Returns uts.release, which is the kernel version on Linux/BSD,
   * and the Darwin kernel version on macOS.
   */
  [[nodiscard]] inline auto GetKernelRelease() -> types::Result<types::String> {
    struct utsname uts;

    if (uname(&uts) == -1)
      return types::Err(error::DracError(error::DracErrorCode::InternalError, std::format("uname() failed: {} (errno {})", std::strerror(errno), errno)));

    if (std::strlen(uts.release) == 0)
      return types::Err(error::DracError(error::DracErrorCode::ParseError, "uname() returned empty kernel release string"));

    return types::String(uts.release);
  }

  /**
   * @brief Gets the system name via uname.
   * @return System name (e.g., "Linux", "Darwin", "FreeBSD"), or an error.
   */
  [[nodiscard]] inline auto GetSystemName() -> types::Result<types::String> {
    struct utsname uts;

    if (uname(&uts) == -1)
      return types::Err(error::DracError(InternalError, std::format("uname() failed: {} (errno {})", std::strerror(errno), errno)));

    if (std::strlen(uts.sysname) == 0)
      return types::Err(error::DracError(error::DracErrorCode::ParseError, "uname() returned empty system name"));

    return types::String(uts.sysname);
  }

  /**
   * @brief Gets the machine hardware name via uname.
   * @return Machine name (e.g., "x86_64", "aarch64"), or an error.
   */
  [[nodiscard]] inline auto GetMachineName() -> types::Result<types::String> {
    struct utsname uts;

    if (uname(&uts) == -1)
      return types::Err(error::DracError(InternalError, std::format("uname() failed: {} (errno {})", std::strerror(errno), errno)));

    if (std::strlen(uts.machine) == 0)
      return types::Err(error::DracError(error::DracErrorCode::ParseError, "uname() returned empty machine name"));

    return types::String(uts.machine);
  }

  /**
   * @brief Gets full uname information as a struct.
   * @return Struct containing sysname, release, version, and machine.
   */
  struct UnameInfo {
    types::String sysname;  // Operating system name
    types::String nodename; // Network node hostname
    types::String release;  // Kernel release
    types::String version;  // Kernel version
    types::String machine;  // Hardware identifier
  };

  [[nodiscard]] inline auto GetUnameInfo() -> types::Result<UnameInfo> {
    struct utsname uts;

    if (uname(&uts) == -1)
      return types::Err(error::DracError(InternalError, std::format("uname() failed: {} (errno {})", std::strerror(errno), errno)));

    return UnameInfo {
      .sysname  = types::String(uts.sysname),
      .nodename = types::String(uts.nodename),
      .release  = types::String(uts.release),
      .version  = types::String(uts.version),
      .machine  = types::String(uts.machine),
    };
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Network Interfaces (getifaddrs) - Available on most Unix systems
  // ─────────────────────────────────────────────────────────────────────────────

  #if DRAC_HAS_IFADDRS

  /**
   * @brief RAII wrapper for ifaddrs list.
   *
   * Automatically calls freeifaddrs on destruction.
   */
  class IfAddrsGuard {
    ifaddrs* m_list = nullptr;

   public:
    IfAddrsGuard() = default;

    ~IfAddrsGuard() {
      if (m_list != nullptr)
        freeifaddrs(m_list);
    }

    // Non-copyable
    IfAddrsGuard(const IfAddrsGuard&)                    = delete;
    auto operator=(const IfAddrsGuard&) -> IfAddrsGuard& = delete;

    // Movable
    IfAddrsGuard(IfAddrsGuard&& other) noexcept
      : m_list(std::exchange(other.m_list, nullptr)) {}

    auto operator=(IfAddrsGuard&& other) noexcept -> IfAddrsGuard& {
      if (this != &other) {
        if (m_list != nullptr)
          freeifaddrs(m_list);
        m_list = std::exchange(other.m_list, nullptr);
      }
      return *this;
    }

    /**
     * @brief Initializes by calling getifaddrs.
     * @return true on success, false on failure (check errno).
     */
    [[nodiscard]] auto init() -> bool {
      return getifaddrs(&m_list) == 0;
    }

    [[nodiscard]] auto get() const noexcept -> ifaddrs* {
      return m_list;
    }
    [[nodiscard]] auto operator*() const noexcept -> ifaddrs& {
      return *m_list;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
      return m_list != nullptr;
    }
  };

  /**
   * @brief Converts an IPv4 sockaddr to a string.
   * @param addr The sockaddr_in pointer.
   * @param buf Buffer to write the string (must be at least INET_ADDRSTRLEN).
   * @param bufSize Size of the buffer.
   * @return Option containing the IP string, or None on failure.
   */
  [[nodiscard]] inline auto FormatIPv4(const sockaddr_in* addr, char* buf, types::usize bufSize)
    -> types::Option<types::String> {
    if (getnameinfo(
          reinterpret_cast<const sockaddr*>(addr), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          sizeof(sockaddr_in),
          buf,
          static_cast<socklen_t>(bufSize),
          nullptr,
          0,
          NI_NUMERICHOST
        ) == 0)
      return types::String(buf);

    return types::None;
  }

  /**
   * @brief Converts an IPv6 sockaddr to a string.
   * @param addr The sockaddr_in6 pointer.
   * @param buf Buffer to write the string (must be at least INET6_ADDRSTRLEN).
   * @param bufSize Size of the buffer.
   * @return Option containing the IP string, or None on failure.
   */
  [[nodiscard]] inline auto FormatIPv6(const sockaddr_in6* addr, char* buf, types::usize bufSize)
    -> types::Option<types::String> {
    if (getnameinfo(
          reinterpret_cast<const sockaddr*>(addr), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          sizeof(sockaddr_in6),
          buf,
          static_cast<socklen_t>(bufSize),
          nullptr,
          0,
          NI_NUMERICHOST
        ) == 0)
      return types::String(buf);

    return types::None;
  }

  /**
   * @brief Checks if an interface is up.
   * @param flags The ifa_flags from ifaddrs.
   * @return true if IFF_UP is set.
   */
  [[nodiscard]] constexpr auto IsInterfaceUp(types::u32 flags) noexcept -> bool {
    return (flags & IFF_UP) != 0;
  }

  /**
   * @brief Checks if an interface is a loopback.
   * @param flags The ifa_flags from ifaddrs.
   * @return true if IFF_LOOPBACK is set.
   */
  [[nodiscard]] constexpr auto IsLoopback(types::u32 flags) noexcept -> bool {
    return (flags & IFF_LOOPBACK) != 0;
  }

  #endif // DRAC_HAS_IFADDRS

  // ─────────────────────────────────────────────────────────────────────────────
  // Uptime Utilities
  // ─────────────────────────────────────────────────────────────────────────────

  #if defined(__linux__)
  /**
   * @brief Gets system uptime on Linux via sysinfo.
   * @return Uptime in seconds, or an error.
   *
   * @note Linux-specific. macOS and BSD use different mechanisms.
   */
  [[nodiscard]] inline auto GetUptimeLinux() -> types::Result<std::chrono::seconds> {
    struct sysinfo info;

    if (sysinfo(&info) == -1)
      return types::Err(error::DracError(InternalError, std::format("sysinfo() failed: {} (errno {})", std::strerror(errno), errno)));

    return std::chrono::seconds(info.uptime);
  }
  #endif

} // namespace draconis::os::unix_shared

#endif // !defined(_WIN32)
