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

    using enum draconis::utils::error::DracErrorCode;

#if DRAC_ENABLE_PLUGINS
    using draconis::core::plugin::IInfoProviderPlugin;
#endif

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

    // Plugin data - flatten with plugin_<pluginId>_<fieldName> format for compact templates
#if DRAC_ENABLE_PLUGINS
    for (const auto& [pluginId, fields] : pluginData)
      for (const auto& [fieldName, value] : fields)
        data[std::format("plugin_{}_{}", pluginId, fieldName)] = value;
#endif

    return data;
  }

#if DRAC_ENABLE_PLUGINS
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
        if (auto result = pluginManager.loadPlugin(pluginName, cache); !result)
          debug_log("Failed to load plugin '{}': {}", pluginName, result.error().message);
        else
          debug_log("Successfully loaded plugin: {}", pluginName);
      } else {
        debug_log("Plugin '{}' is already loaded", pluginName);
      }
    }

    // Get all info provider plugins (high-performance lookup)
    Vec<IInfoProviderPlugin*> infoProviderPlugins = pluginManager.getInfoProviderPlugins();

    debug_log("Found {} info provider plugins", infoProviderPlugins.size());

    if (infoProviderPlugins.empty())
      return;

    // Collect data from each plugin efficiently
    // Create a PluginCache using the persistent cache directory for plugins
    PluginCache pluginCacheInstance(utils::cache::CacheManager::getPersistentCacheDir() / "plugins");

    for (IInfoProviderPlugin* plugin : infoProviderPlugins) {
      if (!plugin || !plugin->isReady()) {
        debug_log("Skipping plugin - null or not ready");
        continue;
      }

      if (!plugin->isEnabled()) {
        debug_log("Skipping plugin - disabled in config");
        continue;
      }

      const auto& metadata = plugin->getMetadata();
      const auto  pluginId = plugin->getProviderId();
      debug_log("Collecting data from plugin: {} (id: {})", metadata.name, pluginId);

      PluginDisplayInfo displayInfo {
        .icon  = plugin->getDisplayIcon(),
        .label = plugin->getDisplayLabel()
      };

      try {
        // Collect plugin data with error handling
        if (auto result = plugin->collectData(pluginCacheInstance); result) {
          // Get fields from plugin
          auto fields = plugin->getFields();
          debug_log("Plugin '{}' collected {} fields", metadata.name, fields.size());

          // Create entry for this plugin and move data efficiently
          auto& pluginFields = pluginData[pluginId];
          for (auto&& [key, value] : fields) {
            debug_log("Adding plugin field: {}[{}] = {}", pluginId, key, value);
            pluginFields.emplace(key, std::move(value));
          }

          if (auto displayValue = plugin->getDisplayValue(); displayValue)
            displayInfo.value = *displayValue;
        } else {
          debug_log("Plugin '{}' failed to collect data: {}", metadata.name, result.error().message);
        }
      } catch (const std::exception& e) {
        debug_log("Exception in plugin '{}': {}", metadata.name, e.what());
      } catch (...) {
        debug_log("Unknown exception in plugin '{}'", metadata.name);
      }

      pluginDisplay[pluginId] = std::move(displayInfo);
    }

    debug_log("Total plugins with data: {}", pluginData.size());
  }
#endif
} // namespace draconis::core::system
