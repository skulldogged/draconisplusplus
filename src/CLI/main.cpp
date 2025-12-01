#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#if DRAC_ENABLE_WEATHER
  #include <Drac++/Services/Weather.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <glaze/glaze.hpp>
#include <magic_enum/magic_enum.hpp>
#include <ranges>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::utils::localization;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;

namespace {
  fn PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data
  ) -> Unit {
    using draconis::utils::error::DracError;

    Array<Option<Pair<String, DracError>>, 10 + DRAC_ENABLE_PACKAGECOUNT + DRAC_ENABLE_NOWPLAYING + DRAC_ENABLE_WEATHER>
      failures {};

    usize failureCount = 0;

#define DRAC_CHECK(expr, label) \
  if (!(expr))                  \
  failures.at(failureCount++) = { label, (expr).error() }

    DRAC_CHECK(data.date, "Date");
    DRAC_CHECK(data.host, "Host");
    DRAC_CHECK(data.kernelVersion, "KernelVersion");
    DRAC_CHECK(data.operatingSystem, "OperatingSystem");
    DRAC_CHECK(data.memInfo, "MemoryInfo");
    DRAC_CHECK(data.desktopEnv, "DesktopEnvironment");
    DRAC_CHECK(data.windowMgr, "WindowManager");
    DRAC_CHECK(data.diskUsage, "DiskUsage");
    DRAC_CHECK(data.shell, "Shell");
    DRAC_CHECK(data.uptime, "Uptime");

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      DRAC_CHECK(data.packageCount, "PackageCount");

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      DRAC_CHECK(data.nowPlaying, "NowPlaying");

    if constexpr (DRAC_ENABLE_WEATHER)
      DRAC_CHECK(weather, "Weather");

#undef DRAC_CHECK

    if (failureCount == 0)
      Println("All readouts were successful!");
    else {
      Println(
        "Out of {} readouts, {} failed.\n",
        failures.size(),
        failureCount
      );

      for (const Option<Pair<String, DracError>>& failure : failures)
        if (failure)
          Println(
            R"(Readout "{}" failed: {} ({}))",
            failure->first,
            failure->second.message,
            magic_enum::enum_name(failure->second.code)
          );
    }
  }

  fn PrintJsonOutput(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data,
    bool              prettyJson
  ) -> Unit {
    using draconis::core::system::JsonInfo;

    JsonInfo output;

#define DRAC_SET_OPTIONAL(field) \
  if (data.field)                \
  output.field = *data.field

    DRAC_SET_OPTIONAL(date);
    DRAC_SET_OPTIONAL(host);
    DRAC_SET_OPTIONAL(kernelVersion);
    DRAC_SET_OPTIONAL(operatingSystem);
    DRAC_SET_OPTIONAL(memInfo);
    DRAC_SET_OPTIONAL(desktopEnv);
    DRAC_SET_OPTIONAL(windowMgr);
    DRAC_SET_OPTIONAL(diskUsage);
    DRAC_SET_OPTIONAL(shell);
    DRAC_SET_OPTIONAL(cpuModel);
    DRAC_SET_OPTIONAL(cpuCores);
    DRAC_SET_OPTIONAL(gpuModel);

    if (data.uptime)
      output.uptimeSeconds = data.uptime->count();

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      DRAC_SET_OPTIONAL(packageCount);

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      DRAC_SET_OPTIONAL(nowPlaying);

    if constexpr (DRAC_ENABLE_WEATHER)
      if (weather)
        output.weather = *weather;

#if DRAC_ENABLE_PLUGINS
    output.pluginFields = data.pluginData;
#endif

#undef DRAC_SET_OPTIONAL

    String jsonStr;

    glz::error_ctx errorContext =
      prettyJson
      ? glz::write<glz::opts { .prettify = true }>(output, jsonStr)
      : glz::write_json(output, jsonStr);

    if (errorContext)
      Print("Failed to write JSON output: {}", glz::format_error(errorContext, jsonStr));
    else
      Print(jsonStr);
  }

#if DRAC_ENABLE_PLUGINS
  /**
   * @brief Convert SystemInfo data to generic Map format for plugins
   * @param data System information data
   * @param weather Weather data (if available)
   * @return Generic data map for plugin consumption
   */
  fn ConvertSystemInfoToMap(
    const SystemInfo& data
  #if DRAC_ENABLE_WEATHER
    ,
    const Result<Report>& weather
  #endif
  ) -> Map<String, String> {
    Map<String, String> outputData;

    // Basic system info
    if (data.date)
      outputData["date"] = *data.date;
    if (data.host)
      outputData["host"] = *data.host;
    if (data.kernelVersion)
      outputData["kernel_version"] = *data.kernelVersion;
    if (data.shell)
      outputData["shell"] = *data.shell;
    if (data.cpuModel)
      outputData["cpu_model"] = *data.cpuModel;
    if (data.cpuCores) {
      outputData["cpu_cores_physical"] = std::to_string(data.cpuCores->physical);
      outputData["cpu_cores_logical"]  = std::to_string(data.cpuCores->logical);
    }
    if (data.gpuModel)
      outputData["gpu_model"] = *data.gpuModel;
    if (data.desktopEnv)
      outputData["desktop_environment"] = *data.desktopEnv;
    if (data.windowMgr)
      outputData["window_manager"] = *data.windowMgr;

    // Operating system info
    if (data.operatingSystem) {
      outputData["operating_system"] = std::format("{} {}", data.operatingSystem->name, data.operatingSystem->version);
      outputData["os_name"]          = data.operatingSystem->name;
      outputData["os_version"]       = data.operatingSystem->version;
      if (!data.operatingSystem->id.empty())
        outputData["os_id"] = data.operatingSystem->id;
    }

    // Memory and disk info
    if (data.memInfo) {
      outputData["memory_info"]        = std::format("{}/{} GB", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes));
      outputData["memory_used_bytes"]  = std::to_string(data.memInfo->usedBytes);
      outputData["memory_total_bytes"] = std::to_string(data.memInfo->totalBytes);
    }

    if (data.diskUsage) {
      outputData["disk_usage"]       = std::format("{}/{} GB", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes));
      outputData["disk_used_bytes"]  = std::to_string(data.diskUsage->usedBytes);
      outputData["disk_total_bytes"] = std::to_string(data.diskUsage->totalBytes);
    }

    // Uptime
    if (data.uptime) {
      outputData["uptime"]         = std::format("{}", SecondsToFormattedDuration { *data.uptime });
      outputData["uptime_seconds"] = std::to_string(data.uptime->count());
    }

    // Package count
  #if DRAC_ENABLE_PACKAGECOUNT
    if (data.packageCount && *data.packageCount > 0)
      outputData["package_count"] = std::to_string(*data.packageCount);
  #endif

    // Now playing
  #if DRAC_ENABLE_NOWPLAYING
    if (data.nowPlaying) {
      outputData["now_playing_artist"] = data.nowPlaying->artist.value_or("Unknown Artist");
      outputData["now_playing_title"]  = data.nowPlaying->title.value_or("Unknown Title");
    }
  #endif

    // Weather
  #if DRAC_ENABLE_WEATHER
    if (weather) {
      const auto& [temperature, townName, description] = *weather;
      outputData["weather_temperature"]                = std::to_string(temperature);
      if (townName)
        outputData["weather_town"] = *townName;
      outputData["weather_description"] = description;
    }
  #endif

    // Plugin data
    for (const auto& [key, value] : data.pluginData)
      outputData[std::format("plugin_{}", key)] = value;

    return outputData;
  }

  fn FormatOutputViaPlugin(
    const String& formatName,
  #if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
  #endif
    const SystemInfo& data
  ) -> Unit {
    using draconis::core::plugin::GetPluginManager;
    using draconis::utils::types::Map, draconis::utils::types::String;

    auto& pluginManager = GetPluginManager();
    if (!pluginManager.isInitialized()) {
      Print("Plugin system not initialized.\n");
      return;
    }

    // Get all loaded output format plugins directly
    auto outputPlugins = pluginManager.getOutputFormatPlugins();

    // Look for a plugin that provides the requested format
    draconis::core::plugin::IOutputFormatPlugin* formatPlugin = nullptr;

    for (auto* plugin : outputPlugins) {
      for (const auto& name : plugin->getFormatNames()) {
        if (name == formatName) {
          formatPlugin = plugin;
          break;
        }
      }
      if (formatPlugin)
        break;
    }

    if (!formatPlugin) {
      Print("No plugin found that provides '{}' output format.\n", formatName);
      return;
    }

    // Convert SystemInfo data to generic Map format using dedicated function
    Map<String, String> outputData = ConvertSystemInfoToMap(
      data
  #if DRAC_ENABLE_WEATHER
      ,
      weather
  #endif
    );

    // Format output using plugin - format name determines the output mode
    auto result = formatPlugin->formatOutput(formatName, outputData);
    if (!result) {
      Print("Failed to format '{}' output: {}\n", formatName, result.error().message);
      return;
    }

    Print(*result);
  }

#endif

#if DRAC_ENABLE_PLUGINS
  /**
   * @brief Handle --list-plugins command with high performance
   * @param pluginManager Reference to plugin manager
   * @return Exit code
   */
  fn handleListPluginsCommand(const draconis::core::plugin::PluginManager& pluginManager) -> i32 {
    using draconis::core::plugin::PluginType;

    if (!pluginManager.isInitialized()) {
      Print("Plugin system not initialized.\n");
      return EXIT_FAILURE;
    }

    auto loadedPlugins     = pluginManager.listLoadedPlugins();
    auto discoveredPlugins = pluginManager.listDiscoveredPlugins();

    Print("Plugin System Status: {} loaded, {} discovered\n\n", loadedPlugins.size(), discoveredPlugins.size());

    if (!loadedPlugins.empty()) {
      Print("Loaded Plugins:\n");
      Print("==============\n");

      for (const auto& metadata : loadedPlugins) {
        Print("  • {} v{} ({})\n", metadata.name, metadata.version, metadata.author);
        Print("    Description: {}\n", metadata.description);
        Print("    Type: {}\n", magic_enum::enum_name(metadata.type));
        Print("\n");
      }
    }

    if (!discoveredPlugins.empty()) {
      Print("Discovered Plugins:\n");
      Print("==================\n");

      for (const auto& pluginName : discoveredPlugins) {
        bool isLoaded = std::ranges::any_of(
          loadedPlugins,
          [&pluginName](const draconis::core::plugin::PluginMetadata& meta) -> bool {
            return meta.name == pluginName;
          }
        );

        Print("  • {} {}\n", pluginName, isLoaded ? "(loaded)" : "(available)");
      }

      Print("\n");
    }

    if (loadedPlugins.empty() && discoveredPlugins.empty()) {
      Print("No plugins found. Checked directories:\n");
      for (const auto& path : pluginManager.getSearchPaths())
        Print("  - {}\n", path.string());
    }

    return EXIT_SUCCESS;
  }

  /**
   * @brief Handle --plugin-info command
   * @param pluginManager Reference to plugin manager
   * @param pluginName Name of plugin to show info for
   * @return Exit code
   */
  fn handlePluginInfoCommand(const draconis::core::plugin::PluginManager& pluginManager, const String& pluginName) -> i32 {
    if (!pluginManager.isInitialized()) {
      Print("Plugin system not initialized.\n");
      return EXIT_FAILURE;
    }

    auto plugin = pluginManager.getPlugin(pluginName);
    if (!plugin) {
      Print("Plugin '{}' not found.\n", pluginName);
      Print("Use --list-plugins to see available plugins.\n");
      return EXIT_FAILURE;
    }

    const auto& metadata = (*plugin)->getMetadata();

    Print("Plugin Information: {}\n", metadata.name);
    Print("========================\n");
    Print("Name: {}\n", metadata.name);
    Print("Version: {}\n", metadata.version);
    Print("Author: {}\n", metadata.author);
    Print("Description: {}\n", metadata.description);
    Print("Type: {}\n", magic_enum::enum_name(metadata.type));
    Print("Status: {}\n", (*plugin)->isReady() ? "Ready" : "Not Ready");

    // Show dependencies
    const auto& deps = metadata.dependencies;
    if (deps.requiresNetwork || deps.requiresFilesystem || deps.requiresAdmin || deps.requiresCaching) {
      Print("\nDependencies:\n");
      if (deps.requiresNetwork)
        Print("  • Network access\n");
      if (deps.requiresFilesystem)
        Print("  • Filesystem access\n");
      if (deps.requiresAdmin)
        Print("  • Administrator privileges\n");
      if (deps.requiresCaching)
        Print("  • Caching system\n");
    }

    // Show fields for SystemInfo plugins
    if (metadata.type == draconis::core::plugin::PluginType::SystemInfo) {
      if (const auto* sysInfoPlugin = dynamic_cast<const draconis::core::plugin::ISystemInfoPlugin*>(*plugin)) {
        const auto& fieldNames = sysInfoPlugin->getFieldNames();
        if (!fieldNames.empty()) {
          Print("\nProvided Fields:\n");
          for (const auto& fieldName : fieldNames)
            Print("  • {}\n", fieldName);
        }
      }
    }

    return EXIT_SUCCESS;
  }

#endif
} // namespace

fn main(const i32 argc, CStr* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  // clang-format off
  auto [
    doctorMode,
    clearCache,
    ignoreCacheRun,
    noAscii,
    jsonOutput,
    prettyJson,
    outputFormat,
    language,
    logoPath,
    logoProtocol,
    logoWidth,
    logoHeight,
    listPlugins,
    pluginInfo
  ] = Tuple(false, false, false, false, false, false, String(""), String(""), String(""), String(""), u32(0), u32(0), false, String(""));
  // clang-format on

  {
    using draconis::utils::argparse::ArgumentParser;

    ArgumentParser parser(DRAC_VERSION);

    parser
      .addArguments("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .addArguments("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag();

    parser
      .addArguments("-l", "--log-level")
      .help("Set the minimum log level.")
      .defaultValue(LogLevel::Info);

    parser
      .addArguments("--clear-cache")
      .help("Clears the cache. This will remove all cached data, including in-memory and on-disk copies.")
      .flag();

    parser
      .addArguments("--lang")
      .help("Set the language for localization (e.g., 'en', 'es', 'fr', 'de').")
      .defaultValue(String(""));

    parser
      .addArguments("--ignore-cache")
      .help("Ignore cache for this run (fetch fresh data without reading/writing on-disk cache).")
      .flag();

    parser
      .addArguments("--no-ascii")
      .help("Disable ASCII art display.")
      .flag();

    parser
      .addArguments("--json")
      .help("Output system information in JSON format. Overrides --no-ascii.")
      .flag();

    parser
      .addArguments("--pretty")
      .help("Pretty-print JSON output. Only valid when --json is used.")
      .flag();

    parser
      .addArguments("--format")
      .help("Output system information in the specified format (e.g., 'markdown', 'json').")
      .defaultValue(String(""));

    parser
      .addArguments("--logo-path")
      .help("Path to an image to render in the logo area (kitty / kitty-direct only).")
      .defaultValue(String(""));

    parser
      .addArguments("--logo-protocol")
      .help("Logo image protocol: 'kitty' or 'kitty-direct'.")
      .defaultValue(String(""));

    parser
      .addArguments("--logo-width")
      .help("Logo image width in terminal cells.")
      .defaultValue(i32(0));

    parser
      .addArguments("--logo-height")
      .help("Logo image height in terminal cells.")
      .defaultValue(i32(0));

#if DRAC_ENABLE_PLUGINS
    parser
      .addArguments("--list-plugins")
      .help("List all available and loaded plugins.")
      .flag();

    parser
      .addArguments("--plugin-info")
      .help("Show detailed information about a specific plugin.")
      .defaultValue(String(""));
#endif

    if (Result<> result = parser.parseArgs({ argv, static_cast<usize>(argc) }); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode     = parser.get<bool>("-d") || parser.get<bool>("--doctor");
    clearCache     = parser.get<bool>("--clear-cache");
    ignoreCacheRun = parser.get<bool>("--ignore-cache");
    noAscii        = parser.get<bool>("--no-ascii");
    jsonOutput     = parser.get<bool>("--json");
    prettyJson     = parser.get<bool>("--pretty");
    outputFormat   = parser.get<String>("--format");
    language       = parser.get<String>("--lang");
    logoPath       = parser.get<String>("--logo-path");
    logoProtocol   = parser.get<String>("--logo-protocol");
    logoWidth      = static_cast<u32>(std::max<i32>(0, parser.get<i32>("--logo-width")));
    logoHeight     = static_cast<u32>(std::max<i32>(0, parser.get<i32>("--logo-height")));

#if DRAC_ENABLE_PLUGINS
    listPlugins = parser.get<bool>("--list-plugins");
    pluginInfo  = parser.get<String>("--plugin-info");
#endif

    SetRuntimeLogLevel(
      parser.get<bool>("-V") || parser.get<bool>("--verbose")
        ? LogLevel::Debug
        : parser.getEnum<LogLevel>("--log-level")
    );
  }

  using draconis::utils::cache::CacheManager, draconis::utils::cache::CachePolicy;

  CacheManager cache;

  if (ignoreCacheRun)
    CacheManager::ignoreCache = true;

  cache.setGlobalPolicy(CachePolicy::tempDirectory());

  if (clearCache) {
    const u8 removedCount = cache.invalidateAll(true);

    if (removedCount > 0)
      Println("Removed {} files.", removedCount);
    else
      Println("No cache files were found to clear.");

    return EXIT_SUCCESS;
  }

  {
    Config config = Config::getInstance();

    // Initialize translation manager with language from command line or config
    if (language.empty() && config.general.language)
      language = *config.general.language;

    // Initialize translation manager (this will auto-detect system language)
    TranslationManager& translationManager = GetTranslationManager();

    if (!language.empty())
      translationManager.setLanguage(language);

    if (!logoPath.empty())
      config.logo.imagePath = logoPath;

    if (!logoProtocol.empty()) {
      String protoLower = logoProtocol;
      std::ranges::transform(protoLower, protoLower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

      config.logo.protocol = protoLower == "kitty-direct"
        ? config::LogoProtocol::KittyDirect
        : config::LogoProtocol::Kitty;
    }

    if (logoWidth > 0)
      config.logo.width = logoWidth;

    if (logoHeight > 0)
      config.logo.height = logoHeight;

    debug_log("Current language: {}", translationManager.getCurrentLanguage());
    debug_log("Selected language: {}", language.empty() ? "auto" : language);

#if DRAC_ENABLE_PLUGINS
    // Initialize plugin system early for maximum performance
    auto& pluginManager = draconis::core::plugin::GetPluginManager();

    if (auto initResult = pluginManager.initialize(&config); !initResult)
      warn_log("Plugin system initialization failed: {}", initResult.error().message);
    else
      debug_log("Plugin system initialized successfully");

    // Handle plugin-specific commands with early exit for performance
    if (listPlugins)
      return handleListPluginsCommand(pluginManager);

    if (!pluginInfo.empty())
      return handlePluginInfoCommand(pluginManager, pluginInfo);
#endif

    debug_log("About to construct SystemInfo...");
    SystemInfo data(cache, config);
    debug_log("SystemInfo constructed successfully");

#if DRAC_ENABLE_WEATHER
    using enum draconis::utils::error::DracErrorCode;

    Result<Report> weatherReport;

    if (config.weather.enabled && config.weather.service == nullptr)
      weatherReport = Err({ Other, "Weather service is not configured" });
    else if (config.weather.enabled)
      weatherReport = config.weather.service->getWeatherInfo();
    else
      weatherReport = Err({ ApiUnavailable, "Weather is disabled" });
#endif

    if (doctorMode) {
      PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data
      );

      return EXIT_SUCCESS;
    }

    if (!outputFormat.empty()) {
#if DRAC_ENABLE_PLUGINS
      FormatOutputViaPlugin(
        outputFormat,
  #if DRAC_ENABLE_WEATHER
        weatherReport,
  #endif
        data
      );
#else
      Print("Plugin output formats require plugin support to be enabled.\n");
#endif
    } else if (jsonOutput)
      PrintJsonOutput(
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data,
        prettyJson
      );
    else
      Print(CreateUI(
        config,
        data,
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        noAscii
      ));
  }

  return EXIT_SUCCESS;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
