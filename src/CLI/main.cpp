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
#include <ranges>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "CLI.hpp"
#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::utils::localization;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;
using namespace draconis::cli;

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
    compactFormat,
    language,
    logoPath,
    logoProtocol,
    logoWidth,
    logoHeight,
    listPlugins,
    pluginInfo,
    benchmarkMode,
    showConfigPath,
    generateCompletions
  ] = Tuple(false, false, false, false, false, false, String(""), String(""), String(""), String(""), String(""), u32(0), u32(0), false, String(""), false, false, String(""));
  // clang-format on

  {
    using draconis::utils::argparse::ArgumentParser;

    // Build enhanced version string with build date and git hash
#if defined(DRAC_BUILD_DATE) && defined(DRAC_GIT_HASH)
    String versionString = std::format("draconis++ {} ({}) [{}]", DRAC_VERSION, DRAC_BUILD_DATE, DRAC_GIT_HASH);
#elif defined(DRAC_BUILD_DATE)
    String versionString = std::format("draconis++ {} ({})", DRAC_VERSION, DRAC_BUILD_DATE);
#else
    String versionString = std::format("draconis++ {}", DRAC_VERSION);
#endif

    ArgumentParser parser(versionString);

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
      .help("Output system information in the specified format (e.g., 'markdown', 'json', 'yaml').")
      .defaultValue(String(""));

    parser
      .addArguments("--compact")
      .help(
        "Output a single line using a template string (e.g., '{host} | {cpu} | {ram}'). "
        "Available placeholders: {date}, {host}, {os}, {kernel}, {cpu}, {gpu}, {ram}, {disk}, "
        "{uptime}, {shell}, {de}, {wm}, {packages}, {weather}, {playing}."
      )
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

    parser
      .addArguments("--benchmark")
      .help("Print timing information for each data source.")
      .flag();

    parser
      .addArguments("--config-path")
      .help("Display the active configuration file location.")
      .flag();

    parser
      .addArguments("--generate-completions")
      .help("Generate shell completion script. Supported shells: bash, zsh, fish, powershell.")
      .defaultValue(String(""));

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
    compactFormat  = parser.get<String>("--compact");
    language       = parser.get<String>("--lang");
    logoPath       = parser.get<String>("--logo-path");
    logoProtocol   = parser.get<String>("--logo-protocol");
    logoWidth      = static_cast<u32>(std::max<i32>(0, parser.get<i32>("--logo-width")));
    logoHeight     = static_cast<u32>(std::max<i32>(0, parser.get<i32>("--logo-height")));

#if DRAC_ENABLE_PLUGINS
    listPlugins = parser.get<bool>("--list-plugins");
    pluginInfo  = parser.get<String>("--plugin-info");
#endif

    benchmarkMode       = parser.get<bool>("--benchmark");
    showConfigPath      = parser.get<bool>("--config-path");
    generateCompletions = parser.get<String>("--generate-completions");

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

  // Handle --generate-completions (early exit, no config needed)
  if (!generateCompletions.empty()) {
    GenerateCompletions(generateCompletions);
    return EXIT_SUCCESS;
  }

  // Handle --config-path (early exit)
  if (showConfigPath) {
#if DRAC_PRECOMPILED_CONFIG
    Println("Using precompiled configuration (no external config file).");
#else
    Println("{}", Config::getConfigPath().string());
#endif
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
      std::ranges::transform(
        protoLower,
        protoLower.begin(),
        [](unsigned char chr) -> char {
          return static_cast<char>(std::tolower(chr));
        }
      );

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
      return HandleListPluginsCommand(pluginManager);

    if (!pluginInfo.empty())
      return HandlePluginInfoCommand(pluginManager, pluginInfo);
#endif

    // Handle benchmark mode (runs timing for each data source)
    if (benchmarkMode) {
      Vec<BenchmarkResult> results = RunBenchmark(cache, config);
      PrintBenchmarkReport(results);
      return EXIT_SUCCESS;
    }

    debug_log("About to construct SystemInfo...");
    SystemInfo data(cache, config);
    debug_log("SystemInfo constructed successfully");

#if DRAC_ENABLE_WEATHER
    using enum draconis::utils::error::DracErrorCode;

    Result<Report> weatherReport;

    if (config.weather.enabled && config.weather.service == nullptr)
      weatherReport = Err({ Other, "Weather service not configured. Check your [weather] section: ensure 'location', 'provider', and 'api_key' (for OpenWeatherMap) are set correctly." });
    else if (config.weather.enabled)
      weatherReport = config.weather.service->getWeatherInfo();
    else
      weatherReport = Err({ ApiUnavailable, "Weather is disabled. Set 'enabled = true' in [weather] section to enable." });
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
    } else if (!compactFormat.empty())
      PrintCompactOutput(
        compactFormat,
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data
      );
    else if (jsonOutput)
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
