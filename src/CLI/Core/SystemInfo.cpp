#include "SystemInfo.hpp"

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using draconis::config::Config;
    using namespace draconis::utils::types;

    using draconis::utils::error::DracError;
    using enum draconis::utils::error::DracErrorCode;

    fn GetDate() -> Result<String> {
      using std::chrono::system_clock;

      const system_clock::time_point nowTp = system_clock::now();
      const std::time_t              nowTt = system_clock::to_time_t(nowTp);

      std::tm nowTm;

#ifdef _WIN32
      if (localtime_s(&nowTm, &nowTt) == 0) {
#else
      if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
        i32 day = nowTm.tm_mday;

        String monthBuffer(32, '\0');

        if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
          using matchit::match, matchit::is, matchit::_, matchit::in;

          monthBuffer.resize(monthLen);

          PCStr suffix = match(day)(
            is | in(11, 13)    = "th",
            is | (_ % 10 == 1) = "st",
            is | (_ % 10 == 2) = "nd",
            is | (_ % 10 == 3) = "rd",
            is | _             = "th"
          );

          return std::format("{} {}{}", monthBuffer, day, suffix);
        }

        ERR(ParseError, "Failed to format date");
      }

      ERR(ParseError, "Failed to get local time");
    }
  } // namespace

  SystemInfo::SystemInfo(utils::cache::CacheManager& cache, const Config& config) {
    debug_log("SystemInfo: Starting construction");

    // I'm not sure if AMD uses trademark symbols in their CPU models, but I know
    // Intel does. Might as well replace them with their unicode counterparts.
    auto replaceTrademarkSymbols = [](Result<String> str) -> Result<String> {
      String value = TRY(str);

      usize pos = 0;

      while ((pos = value.find("(TM)")) != String::npos)
        value.replace(pos, 4, "™");

      while ((pos = value.find("(R)")) != String::npos)
        value.replace(pos, 3, "®");

      return value;
    };

    debug_log("SystemInfo: Getting desktop environment");
    this->desktopEnv = GetDesktopEnvironment(cache);
    debug_log("SystemInfo: Getting window manager");
    this->windowMgr = GetWindowManager(cache);
    debug_log("SystemInfo: Getting operating system");
    this->operatingSystem = GetOperatingSystem(cache);
    debug_log("SystemInfo: Getting kernel version");
    this->kernelVersion = GetKernelVersion(cache);
    debug_log("SystemInfo: Getting host");
    this->host = GetHost(cache);
    debug_log("SystemInfo: Getting CPU model");
    this->cpuModel = replaceTrademarkSymbols(GetCPUModel(cache));
    debug_log("SystemInfo: Getting CPU cores");
    this->cpuCores = GetCPUCores(cache);
    debug_log("SystemInfo: Getting GPU model");
    this->gpuModel = GetGPUModel(cache);
    debug_log("SystemInfo: Getting shell");
    this->shell = GetShell(cache);
    debug_log("SystemInfo: Getting memory info");
    this->memInfo = GetMemInfo(cache);
    debug_log("SystemInfo: Getting disk usage");
    this->diskUsage = GetDiskUsage(cache);
    debug_log("SystemInfo: Getting uptime");
    this->uptime = GetUptime();
    debug_log("SystemInfo: Getting date");
    this->date = GetDate();

    if constexpr (DRAC_ENABLE_PACKAGECOUNT) {
      debug_log("SystemInfo: Getting package count");
      this->packageCount = draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers);
    }

    if constexpr (DRAC_ENABLE_NOWPLAYING) {
      debug_log("SystemInfo: Getting now playing");
      this->nowPlaying = config.nowPlaying.enabled ? GetNowPlaying() : Err(DracError(ApiUnavailable, "Now Playing API disabled"));
    }

#if DRAC_ENABLE_PLUGINS
    debug_log("SystemInfo: Collecting plugin data");
    // Collect plugin data efficiently (only if plugins are enabled and initialized)
    collectPluginData(cache);
#endif
    debug_log("SystemInfo: Construction complete");
  }

  fn SystemInfo::toMap() const -> Map<String, String> {
    Map<String, String> data;

    // Basic system info
    if (date)
      data["date"] = *date;
    if (host)
      data["host"] = *host;
    if (kernelVersion)
      data["kernel"] = *kernelVersion;
    if (shell)
      data["shell"] = *shell;

    // CPU info
    if (cpuModel)
      data["cpu"] = *cpuModel;
    if (cpuCores) {
      data["cpu_cores_physical"] = std::to_string(cpuCores->physical);
      data["cpu_cores_logical"]  = std::to_string(cpuCores->logical);
    }

    // GPU info
    if (gpuModel)
      data["gpu"] = *gpuModel;

    // Desktop environment
    if (desktopEnv)
      data["de"] = *desktopEnv;
    if (windowMgr)
      data["wm"] = *windowMgr;

    // Operating system info
    if (operatingSystem) {
      data["os"]         = std::format("{} {}", operatingSystem->name, operatingSystem->version);
      data["os_name"]    = operatingSystem->name;
      data["os_version"] = operatingSystem->version;
      if (!operatingSystem->id.empty())
        data["os_id"] = operatingSystem->id;
    }

    // Memory info
    if (memInfo) {
      data["ram"]                = std::format("{}/{}", BytesToGiB(memInfo->usedBytes), BytesToGiB(memInfo->totalBytes));
      data["memory_used_bytes"]  = std::to_string(memInfo->usedBytes);
      data["memory_total_bytes"] = std::to_string(memInfo->totalBytes);
    }

    // Disk info
    if (diskUsage) {
      data["disk"]             = std::format("{}/{}", BytesToGiB(diskUsage->usedBytes), BytesToGiB(diskUsage->totalBytes));
      data["disk_used_bytes"]  = std::to_string(diskUsage->usedBytes);
      data["disk_total_bytes"] = std::to_string(diskUsage->totalBytes);
    }

    // Uptime
    if (uptime) {
      data["uptime"]         = std::format("{}", SecondsToFormattedDuration { *uptime });
      data["uptime_seconds"] = std::to_string(uptime->count());
    }

    // Package count
#if DRAC_ENABLE_PACKAGECOUNT
    if (packageCount && *packageCount > 0)
      data["packages"] = std::to_string(*packageCount);
#endif

    // Now playing
#if DRAC_ENABLE_NOWPLAYING
    if (nowPlaying) {
      data["playing"]        = std::format("{} - {}", nowPlaying->artist.value_or("Unknown Artist"), nowPlaying->title.value_or("Unknown Title"));
      data["playing_artist"] = nowPlaying->artist.value_or("Unknown Artist");
      data["playing_title"]  = nowPlaying->title.value_or("Unknown Title");
    }
#endif

    // Plugin data
#if DRAC_ENABLE_PLUGINS
    for (const auto& [key, value] : pluginData)
      data[std::format("plugin_{}", key)] = value;
#endif

    return data;
  }

#if DRAC_ENABLE_PLUGINS
  // Proper cache wrapper that uses the full CacheManager infrastructure with TTL support
  class CacheWrapper : public IPluginCache {
   public:
    explicit CacheWrapper(utils::cache::CacheManager& manager) : m_manager(manager) {}

    fn get(const String& key) -> Option<String> override {
      // Use a fetcher that always fails - we only want cached data, not to fetch fresh data
      fn fetcher = []() -> utils::types::Result<String> {
        return utils::types::Err(utils::error::DracError(utils::error::DracErrorCode::Other, "Cache miss - no fetcher provided"));
      };

      // Try to get from cache. If it's a cache miss, the fetcher will fail and we'll return nullopt
      Result<String> result = m_manager.getOrSet<String>(key, fetcher);

      if (result)
        return *result;

      return std::nullopt;
    }

    fn set(const String& key, const String& value, utils::types::u32 ttlSeconds) -> void override {
      // First invalidate any existing cache entry to ensure we store fresh data
      m_manager.invalidate(key);

      // Create a fetcher that returns the provided value
      fn fetcher = [&value]() -> utils::types::Result<String> {
        return value;
      };

      // Set cache policy with the specified TTL
      using namespace std::chrono;
      utils::cache::CachePolicy policy {
        .location = utils::cache::CacheLocation::Persistent, // Use persistent cache for plugins
        .ttl      = seconds(ttlSeconds)
      };

      // Store the value with TTL - ignore the return value since we just want to cache it
      [[maybe_unused]] auto cacheResult = m_manager.getOrSet<String>(key, policy, fetcher);
    }

   private:
    utils::cache::CacheManager& m_manager; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  fn SystemInfo::collectPluginData(utils::cache::CacheManager& cache) -> Unit {
    using draconis::core::plugin::GetPluginManager;

    auto& pluginManager = GetPluginManager();

    // Early exit if plugin system not initialized (zero-cost when disabled)
    if (!pluginManager.isInitialized())
      return;

    // Load discovered plugins automatically
    Vec<String> discoveredPlugins = pluginManager.listDiscoveredPlugins();
    debug_log("Attempting to load {} discovered plugins", discoveredPlugins.size());

    for (const auto& pluginName : discoveredPlugins) {
      if (!pluginManager.isPluginLoaded(pluginName)) {
        debug_log("Loading plugin: {}", pluginName);
        if (auto result = pluginManager.loadPlugin(pluginName, cache); !result) {
          debug_log("Failed to load plugin '{}': {}", pluginName, result.error().message);
        } else {
          debug_log("Successfully loaded plugin: {}", pluginName);
        }
      } else {
        debug_log("Plugin '{}' is already loaded", pluginName);
      }
    }

    // Get all system info plugins (high-performance lookup)
    Vec<ISystemInfoPlugin*> systemInfoPlugins = pluginManager.getSystemInfoPlugins();

    debug_log("Found {} system info plugins", systemInfoPlugins.size());

    if (systemInfoPlugins.empty()) {
      return;
    }

    // Collect data from each plugin efficiently
    for (ISystemInfoPlugin* plugin : systemInfoPlugins) {
      if (!plugin || !plugin->isReady()) {
        debug_log("Skipping plugin - null or not ready");
        continue;
      }

      const auto& metadata = plugin->getMetadata();
      debug_log("Collecting data from plugin: {}", metadata.name);

      try {
        // Collect plugin data with error handling
        CacheWrapper cacheWrapper(cache);
        if (auto result = plugin->collectInfo(cacheWrapper); result) {
          debug_log("Plugin '{}' collected {} fields", metadata.name, result->size());

          // Move data efficiently to avoid copying
          for (auto&& [key, value] : *result) {
            debug_log("Adding plugin field: {} = {}", key, value);
            pluginData.emplace(key, std::move(value));
          }

          // Plugin contributed successfully
        } else {
          debug_log("Plugin '{}' failed to collect data: {}", metadata.name, result.error().message);
        }
      } catch (const std::exception& e) {
        debug_log("Exception in plugin '{}': {}", metadata.name, e.what());
      } catch (...) {
        debug_log("Unknown exception in plugin '{}'", metadata.name);
      }
    }

    debug_log("Total plugin fields collected: {}", pluginData.size());
  }
#endif
} // namespace draconis::core::system
