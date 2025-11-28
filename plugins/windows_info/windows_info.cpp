/**
 * @file windows_info.cpp
 * @brief Windows system information plugin for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin demonstrates:
 * - Windows-specific system information gathering
 * - Registry access for system details
 * - Environment variable reading
 * - Windows API usage within plugins
 *
 * This file supports both dynamic (shared library) and static compilation.
 * When compiled as a static plugin (DRAC_STATIC_PLUGIN_BUILD defined),
 * it exports factory functions in a namespace instead of extern "C".
 */

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>

#ifdef _WIN32
  #include <cstring>
  #include <shlobj.h>
  #include <string>
  #include <windows.h>
#endif

using namespace draconis::core::plugin;
using namespace draconis::utils::types;
using namespace draconis::utils::error;
using enum DracErrorCode;

namespace {
  class WindowsInfoPlugin : public ISystemInfoPlugin {
   private:
    PluginMetadata    m_metadata;
    std::atomic<bool> m_ready = false;

   public:
    WindowsInfoPlugin() {
      m_metadata = {
        .name         = "Windows Info",
        .version      = "1.0.0",
        .author       = "Draconis++ Team",
        .description  = "Provides Windows-specific system information",
        .type         = PluginType::SystemInfo,
        .dependencies = { .requiresFilesystem = true } // May access registry/filesystem
      };
    }

    fn getMetadata() const -> const PluginMetadata& override {
      return m_metadata;
    }

    fn initialize(IPluginCache& cache) -> Result<Unit> override {
#ifdef _WIN32
      // Cache static system information during initialization
      if (Result<String> buildNumber = getWindowsBuildNumber(); buildNumber)
        cache.set("windows_build", *buildNumber, 3600); // Cache for 1 hour

      if (Result<String> arch = getSystemArchitecture(); arch)
        cache.set("system_architecture", *arch, 3600); // Cache for 1 hour

      m_ready = true;
      return {};
#else
      m_ready = false;
      ERR(NotSupported, "Windows Info plugin only supported on Windows.");
#endif
    }

    fn shutdown() -> Unit override {
      m_ready = false;
    }

    fn isReady() const -> bool override {
      return m_ready;
    }

    fn collectInfo(IPluginCache& cache) -> Result<Map<String, String>> override {
      if (!m_ready) {
        ERR(NotSupported, "WindowsInfoPlugin is not ready.");
      }

      Map<String, String> info;

#ifdef _WIN32
      // Get Windows build number from cache first
      if (auto cached = cache.get("windows_build"); cached) {
        info["windowsBuild"] = *cached;
      } else if (auto buildNumber = getWindowsBuildNumber(); buildNumber) {
        info["windowsBuild"] = *buildNumber;
        cache.set("windows_build", *buildNumber, 3600); // Cache for 1 hour
      }

      // Get system architecture from cache first
      if (auto cached = cache.get("system_architecture"); cached) {
        info["systemArchitecture"] = *cached;
      } else if (auto arch = getSystemArchitecture(); arch) {
        info["systemArchitecture"] = *arch;
        cache.set("system_architecture", *arch, 3600); // Cache for 1 hour
      }

      // Get computer name
      if (auto computerName = getComputerName(); computerName)
        info["computerName"] = *computerName;

      // Get Windows directory
      if (auto winDir = getWindowsDirectory(); winDir)
        info["windowsDirectory"] = *winDir;

      // Get system directory
      if (auto sysDir = getSystemDirectory(); sysDir)
        info["systemDirectory"] = *sysDir;

      // Get temp directory
      if (auto tempDir = getTempDirectory(); tempDir)
        info["tempDirectory"] = *tempDir;

      // Get number of processors
      info["processorCount"] = std::to_string(getProcessorCount());

      // Get system uptime in a different way than the main app
      if (auto uptime = getSystemUptimeMs(); uptime) {
        // Convert to hours for a different perspective
        double uptimeHours  = static_cast<double>(*uptime) / (1000.0 * 60.0 * 60.0);
        info["uptimeHours"] = std::format("{:.2f}", uptimeHours);
      }
#endif

      return info;
    }

    fn getFieldNames() const -> Vec<String> override {
      return {
        "windowsBuild",
        "systemArchitecture",
        "computerName",
        "windowsDirectory",
        "systemDirectory",
        "tempDirectory",
        "processorCount",
        "uptimeHours"
      };
    }

   private:
#ifdef _WIN32
    static fn getWindowsBuildNumber() -> Result<String> {
      HKEY hKey = nullptr;

      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        ERR(IoError, "Failed to open registry key");

      Array<unsigned char, 256> buffer     = {};
      DWORD                     bufferSize = buffer.size();
      DWORD                     type       = 0;

      if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, &type, buffer.data(), &bufferSize) == ERROR_SUCCESS) {
        RegCloseKey(hKey);

        Array<char, 256> charBuffer = {};
        std::memcpy(charBuffer.data(), buffer.data(), bufferSize);
        return String(charBuffer.data(), bufferSize);
      }

      RegCloseKey(hKey);

      ERR(NotFound, "Build number not found in registry");
    }

    static fn getSystemArchitecture() -> Result<String> {
      SYSTEM_INFO sysInfo;
      GetSystemInfo(&sysInfo);

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
      switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
          return "x64";
        case PROCESSOR_ARCHITECTURE_ARM:
          return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:
          return "ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL:
          return "x86";
        default:
          return "Unknown";
      }
    }

    static fn getComputerName() -> Result<String> {
      Array<char, MAX_COMPUTERNAME_LENGTH + 1> buffer = {};
      DWORD                                    size   = buffer.size();

      if (GetComputerNameA(buffer.data(), &size))
        return buffer.data();

      ERR(IoError, "Failed to get computer name");
    }

    static fn getWindowsDirectory() -> Result<String> {
      Array<char, MAX_PATH> buffer = {};

      if (GetWindowsDirectoryA(buffer.data(), MAX_PATH) > 0)
        return buffer.data();

      ERR(IoError, "Failed to get Windows directory");
    }

    static fn getSystemDirectory() -> Result<String> {
      Array<char, MAX_PATH> buffer = {};

      if (GetSystemDirectoryA(buffer.data(), MAX_PATH) > 0)
        return buffer.data();

      ERR(IoError, "Failed to get system directory");
    }

    static fn getTempDirectory() -> Result<String> {
      Array<char, MAX_PATH> buffer = {};

      if (GetTempPathA(MAX_PATH, buffer.data()) > 0)
        return buffer.data();

      ERR(IoError, "Failed to get temp directory");
    }

    static fn getProcessorCount() -> u32 {
      SYSTEM_INFO sysInfo;
      GetSystemInfo(&sysInfo);
      return static_cast<u32>(sysInfo.dwNumberOfProcessors);
    }

    static fn getSystemUptimeMs() -> Result<u64> {
      return static_cast<u64>(GetTickCount64());
    }
#endif
  };
} // anonymous namespace

DRAC_PLUGIN(WindowsInfoPlugin)
