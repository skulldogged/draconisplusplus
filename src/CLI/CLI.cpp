/**
 * @file CLI.cpp
 * @brief CLI utility functions implementation
 */

#include "CLI.hpp"

#include <algorithm>
#include <chrono>
#include <glaze/glaze.hpp>
#include <magic_enum/magic_enum.hpp>

#include <Drac++/Services/Packages.hpp>

#include <Drac++/Utils/DataTypes.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>

  #include <Drac++/Utils/CacheManager.hpp>

#endif

namespace draconis::cli {
  using namespace utils::types;
  using namespace utils::logging;
  using namespace core::system;
  using namespace config;

  fn RunBenchmark(
    utils::cache::CacheManager& cache,
    const Config&               config
  ) -> Vec<BenchmarkResult> {
    using std::chrono::high_resolution_clock, std::chrono::duration;

    Vec<BenchmarkResult> results;
    results.reserve(20);

    // Time each data source individually
    auto timeOperation = [&results](const String& name, auto&& func) -> auto {
      auto start  = high_resolution_clock::now();
      auto result = func();
      auto end    = high_resolution_clock::now();
      auto dur    = duration<f64, std::milli>(end - start).count();
      results.push_back({ .name = name, .durationMs = dur, .success = static_cast<bool>(result) });
    };

    timeOperation("Desktop Environment", [&]() -> Result<String> { return GetDesktopEnvironment(cache); });
    timeOperation("Window Manager", [&]() -> Result<String> { return GetWindowManager(cache); });
    timeOperation("Operating System", [&]() -> Result<OSInfo> { return GetOperatingSystem(cache); });
    timeOperation("Kernel Version", [&]() -> Result<String> { return GetKernelVersion(cache); });
    timeOperation("Host", [&]() -> Result<String> { return GetHost(cache); });
    timeOperation("CPU Model", [&]() -> Result<String> { return GetCPUModel(cache); });
    timeOperation("CPU Cores", [&]() -> Result<CPUCores> { return GetCPUCores(cache); });
    timeOperation("GPU Model", [&]() -> Result<String> { return GetGPUModel(cache); });
    timeOperation("Shell", [&]() -> Result<String> { return GetShell(cache); });
    timeOperation("Memory Info", [&]() -> Result<ResourceUsage> { return GetMemInfo(cache); });
    timeOperation("Disk Usage", [&]() -> Result<ResourceUsage> { return GetDiskUsage(cache); });
    timeOperation("Uptime", [&]() -> Result<std::chrono::seconds> { return GetUptime(); });

#if DRAC_ENABLE_PACKAGECOUNT
    timeOperation("Package Count", [&]() -> Result<u64> {
      return draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers);
    });
#else
    (void)config; // Suppress unused warning
#endif

#if DRAC_ENABLE_PLUGINS
    // Benchmark info provider plugins
    auto& pluginManager = draconis::core::plugin::GetPluginManager();
    if (pluginManager.isInitialized()) {
      // First, load all discovered plugins
      for (const auto& pluginName : pluginManager.listDiscoveredPlugins())
        if (!pluginManager.isPluginLoaded(pluginName)) {
          Result<Unit> loadResult = pluginManager.loadPlugin(pluginName, cache);

          if (!loadResult)
            Print("Warning: failed to load plugin '{}'\n", pluginName);
        }

      // Create a PluginCache for benchmarking using the persistent cache directory
      PluginCache pluginCache(utils::cache::CacheManager::getPersistentCacheDir() / "plugins");

      // Then benchmark each info provider plugin
      for (auto* plugin : pluginManager.getInfoProviderPlugins())
        if (plugin && plugin->isReady() && plugin->isEnabled())
          timeOperation(std::format("Plugin: {}", plugin->getMetadata().name), [&]() -> Result<String> {
            if (auto result = plugin->collectData(pluginCache); !result)
              return std::unexpected(result.error());
            return plugin->getDisplayValue();
          });
    }
#endif

    return results;
  }

  fn PrintBenchmarkReport(const Vec<BenchmarkResult>& results) -> Unit {
    f64 totalTime  = 0.0;
    f64 coreTime   = 0.0;
    f64 pluginTime = 0.0;

    // Separate core system results from plugin results
    Vec<BenchmarkResult> coreResults;
    Vec<BenchmarkResult> pluginResults;

    for (const auto& result : results) {
      if (result.name.starts_with("Plugin: "))
        pluginResults.push_back(result);
      else
        coreResults.push_back(result);
    }

    // Sort each group by duration (slowest first)
    auto sortByDuration = [](const BenchmarkResult& resA, const BenchmarkResult& resB) -> bool {
      return resA.durationMs > resB.durationMs;
    };
    std::ranges::sort(coreResults, sortByDuration);
    std::ranges::sort(pluginResults, sortByDuration);

    // Find longest name for alignment (check both groups)
    usize maxNameLen = 0;
    for (const auto& result : coreResults)
      maxNameLen = std::max(maxNameLen, result.name.size());
    for (const auto& result : pluginResults)
      maxNameLen = std::max(maxNameLen, result.name.size());

    // Helper to print a single result
    auto printResult = [&maxNameLen](const BenchmarkResult& result) {
      String status  = result.success ? "✓" : "✗";
      String padding = String(maxNameLen - result.name.size(), ' ');
      Println("  {} {}{} {:>8.2f} ms", status, result.name, padding, result.durationMs);
    };

    Println("Benchmark Results:");
    Println("==================");

    // Print core system results
    if (!coreResults.empty()) {
      Println();
      Println("Core System Data:");
      Println("-----------------");
      for (const auto& result : coreResults) {
        printResult(result);
        coreTime += result.durationMs;
      }
      Println();
      Println("  Subtotal: {:>8.2f} ms ({} sources)", coreTime, coreResults.size());
    }

    // Print plugin results
    if (!pluginResults.empty()) {
      Println();
      Println("Plugin Data:");
      Println("------------");
      for (const auto& result : pluginResults) {
        printResult(result);
        pluginTime += result.durationMs;
      }
      Println();
      Println("  Subtotal: {:>8.2f} ms ({} plugins)", pluginTime, pluginResults.size());
    }

    totalTime = coreTime + pluginTime;
    Println();
    Println("  Total: {:>8.2f} ms ({} data sources)", totalTime, results.size());
  }

  fn PrintDoctorReport(
    const SystemInfo& data
  ) -> Unit {
    using draconis::utils::error::DracError;

    constexpr usize                                          coreReadoutCount = 10 + DRAC_ENABLE_PACKAGECOUNT;
    Array<Option<Pair<String, DracError>>, coreReadoutCount> coreFailures {};

    usize coreFailureCount = 0;

#define DRAC_CHECK(expr, label) \
  if (!(expr))                  \
  coreFailures.at(coreFailureCount++) = { label, (expr).error() }

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

#undef DRAC_CHECK

    // Report core system readouts
    Println("Doctor Report:");
    Println("==============");
    Println();
    Println("Core System Readouts:");
    Println("---------------------");

    if (coreFailureCount == 0)
      Println("  ✓ All {} core readouts were successful!", coreReadoutCount);
    else {
      Println(
        "  Out of {} core readouts, {} failed.\n",
        coreReadoutCount,
        coreFailureCount
      );

      for (const Option<Pair<String, DracError>>& failure : coreFailures)
        if (failure)
          Println(
            R"(  ✗ "{}" failed: {} ({}))",
            failure->first,
            failure->second.message,
            magic_enum::enum_name(failure->second.code)
          );
    }

#if DRAC_ENABLE_PLUGINS
    // Report plugin readouts
    const auto& pluginDisplay = data.pluginDisplay;

    if (!pluginDisplay.empty()) {
      Vec<String> pluginFailures;
      Vec<String> pluginSuccesses;

      for (const auto& [pluginId, displayInfo] : pluginDisplay) {
        if (displayInfo.value.has_value())
          pluginSuccesses.push_back(displayInfo.label);
        else
          pluginFailures.push_back(displayInfo.label);
      }

      Println();
      Println("Plugin Readouts:");
      Println("----------------");

      if (pluginFailures.empty())
        Println("  ✓ All {} plugin readouts were successful!", pluginDisplay.size());
      else {
        Println(
          "  Out of {} plugin readouts, {} failed.\n",
          pluginDisplay.size(),
          pluginFailures.size()
        );

        for (const auto& label : pluginFailures)
          Println(R"(  ✗ Plugin "{}" failed to provide data)", label);
      }

      if (!pluginSuccesses.empty()) {
        Println();
        Println("  Successful plugins:");
        for (const auto& label : pluginSuccesses)
          Println("    ✓ {}", label);
      }
    }
#endif

    // Summary
    Println();
    usize totalReadouts = coreReadoutCount;
    usize totalFailures = coreFailureCount;

#if DRAC_ENABLE_PLUGINS
    totalReadouts += data.pluginDisplay.size();
    for (const auto& [pluginId, displayInfo] : data.pluginDisplay)
      if (!displayInfo.value.has_value())
        totalFailures++;
#endif

    if (totalFailures == 0)
      Println("Summary: All {} readouts passed! ✓", totalReadouts);
    else
      Println("Summary: {}/{} readouts passed ({} failed)", totalReadouts - totalFailures, totalReadouts, totalFailures);
  }

  fn PrintJsonOutput(
    const SystemInfo& data,
    bool              prettyJson
  ) -> Unit {
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

  fn PrintCompactOutput(
    const String&     templateStr,
    const SystemInfo& data
  ) -> Unit {
    // Get all system info as a map
    Map<String, String> infoMap = data.toMap();

    // Generic placeholder substitution: replace all {key} with values from the map
    String output = templateStr;
    for (const auto& [key, value] : infoMap) {
      String placeholder = std::format("{{{}}}", key);
      usize  pos         = 0;
      while ((pos = output.find(placeholder, pos)) != String::npos)
        output.replace(pos, placeholder.length(), value);
    }

    // Remove any remaining unmatched placeholders (keys that weren't in the map)
    usize pos = 0;
    while ((pos = output.find('{', pos)) != String::npos) {
      usize endPos = output.find('}', pos);
      if (endPos != String::npos)
        output.replace(pos, endPos - pos + 1, "");
      else
        break;
    }

    Println("{}", output);
  }

#if DRAC_ENABLE_PLUGINS
  fn FormatOutputViaPlugin(
    const String&     formatName,
    const SystemInfo& data
  ) -> Unit {
    using draconis::core::plugin::GetPluginManager;

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

    // Get system info as a map (single source of truth for core data)
    Map<String, String> outputData = data.toMap();

    // Get plugin data directly (already organized by plugin ID)
    const auto& pluginData = data.pluginData;

    // Format output using plugin - format name determines the output mode
    auto result = formatPlugin->formatOutput(formatName, outputData, pluginData);
    if (!result) {
      Print("Failed to format '{}' output: {}\n", formatName, result.error().message);
      return;
    }

    Print(*result);
  }

  fn HandleListPluginsCommand(const draconis::core::plugin::PluginManager& pluginManager) -> i32 {
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

  fn HandlePluginInfoCommand(const draconis::core::plugin::PluginManager& pluginManager, const String& pluginName) -> i32 {
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

    // Show fields for InfoProvider plugins
    if (metadata.type == draconis::core::plugin::PluginType::InfoProvider) {
      if (const auto* infoProviderPlugin = dynamic_cast<const draconis::core::plugin::IInfoProviderPlugin*>(*plugin)) {
        const auto fields = infoProviderPlugin->getFields();
        if (!fields.empty()) {
          Print("\nProvided Fields:\n");
          for (const auto& [key, description] : fields)
            Print("  • {} - {}\n", key, description);
        }
      }
    }

    return EXIT_SUCCESS;
  }
#endif

  fn GenerateCompletions(const String& shell) -> Unit {
    // Shell completion definitions for draconis++ flags/options
    if (shell == "bash") {
      Print(R"bash(
_draconis++_completions() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local opts="-V --verbose -d --doctor -l --log-level --clear-cache --lang --ignore-cache --no-ascii --json --pretty --format --compact --logo-path --logo-protocol --logo-width --logo-height --version --help --benchmark --config-path --generate-completions --list-plugins --plugin-info"

    if [[ "$cur" == -* ]]; then
        COMPREPLY=($(compgen -W "$opts" -- "$cur"))
    fi

    case "${COMP_WORDS[COMP_CWORD-1]}" in
        --log-level|-l)
            COMPREPLY=($(compgen -W "trace debug info warn error" -- "$cur"))
            ;;
        --logo-protocol)
            COMPREPLY=($(compgen -W "kitty kitty-direct iterm2" -- "$cur"))
            ;;
        --generate-completions)
            COMPREPLY=($(compgen -W "bash zsh fish powershell" -- "$cur"))
            ;;
        --lang)
            COMPREPLY=($(compgen -W "en es fr de" -- "$cur"))
            ;;
    esac
}
complete -F _draconis++_completions draconis++
)bash");
    } else if (shell == "zsh") {
      Print(R"zsh(
#compdef draconis++

_draconis++() {
    local -a opts
    opts=(
        '-V[Enable verbose logging]'
        '--verbose[Enable verbose logging]'
        '-d[Reports any failed readouts]'
        '--doctor[Reports any failed readouts]'
        '-l[Set minimum log level]:level:(trace debug info warn error)'
        '--log-level[Set minimum log level]:level:(trace debug info warn error)'
        '--clear-cache[Clears the cache]'
        '--lang[Set language]:language:(en es fr de)'
        '--ignore-cache[Ignore cache for this run]'
        '--no-ascii[Disable ASCII art]'
        '--json[Output in JSON format]'
        '--pretty[Pretty-print JSON]'
        '--format[Output format]'
        '--compact[Single-line output with template]'
        '--logo-path[Path to logo image]:file:_files'
        '--logo-protocol[Logo protocol]:protocol:(kitty kitty-direct iterm2)'
        '--logo-width[Logo width in pixels]'
        '--logo-height[Logo height in pixels]'
        '--version[Show version info]'
        '--help[Show help message]'
        '--benchmark[Show timing for each data source]'
        '--config-path[Display config file location]'
        '--generate-completions[Generate shell completions]:shell:(bash zsh fish powershell)'
        '--list-plugins[List all available plugins]'
        '--plugin-info[Show detailed plugin information]'
    )
    _describe 'draconis++' opts
}

_draconis++ "$@"
)zsh");
    } else if (shell == "fish") {
      Print(R"fish(
# Fish completions for draconis++
complete -c draconis++ -s V -l verbose -d 'Enable verbose logging'
complete -c draconis++ -s d -l doctor -d 'Reports any failed readouts'
complete -c draconis++ -s l -l log-level -x -a 'trace debug info warn error' -d 'Set minimum log level'
complete -c draconis++ -l clear-cache -d 'Clears the cache'
complete -c draconis++ -l lang -x -a 'en es fr de' -d 'Set language'
complete -c draconis++ -l ignore-cache -d 'Ignore cache for this run'
complete -c draconis++ -l no-ascii -d 'Disable ASCII art'
complete -c draconis++ -l json -d 'Output in JSON format'
complete -c draconis++ -l pretty -d 'Pretty-print JSON'
complete -c draconis++ -l format -x -d 'Output format'
complete -c draconis++ -l compact -d 'Single-line output with template'
complete -c draconis++ -l logo-path -r -d 'Path to logo image'
complete -c draconis++ -l logo-protocol -x -a 'kitty kitty-direct iterm2' -d 'Logo protocol'
complete -c draconis++ -l logo-width -d 'Logo width in pixels'
complete -c draconis++ -l logo-height -d 'Logo height in pixels'
complete -c draconis++ -l version -d 'Show version info'
complete -c draconis++ -l help -d 'Show help message'
complete -c draconis++ -l benchmark -d 'Show timing for each data source'
complete -c draconis++ -l config-path -d 'Display config file location'
complete -c draconis++ -l generate-completions -x -a 'bash zsh fish powershell' -d 'Generate shell completions'
complete -c draconis++ -l list-plugins -d 'List all available plugins'
complete -c draconis++ -l plugin-info -d 'Show detailed plugin information'
)fish");
    } else if (shell == "powershell" || shell == "pwsh") {
      Print(R"pwsh(
# PowerShell completions for draconis++
Register-ArgumentCompleter -CommandName draconis++ -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $options = @(
        @{ Name = '-V'; Tooltip = 'Enable verbose logging' }
        @{ Name = '--verbose'; Tooltip = 'Enable verbose logging' }
        @{ Name = '-d'; Tooltip = 'Reports any failed readouts' }
        @{ Name = '--doctor'; Tooltip = 'Reports any failed readouts' }
        @{ Name = '-l'; Tooltip = 'Set minimum log level' }
        @{ Name = '--log-level'; Tooltip = 'Set minimum log level' }
        @{ Name = '--clear-cache'; Tooltip = 'Clears the cache' }
        @{ Name = '--lang'; Tooltip = 'Set language' }
        @{ Name = '--ignore-cache'; Tooltip = 'Ignore cache for this run' }
        @{ Name = '--no-ascii'; Tooltip = 'Disable ASCII art' }
        @{ Name = '--json'; Tooltip = 'Output in JSON format' }
        @{ Name = '--pretty'; Tooltip = 'Pretty-print JSON' }
        @{ Name = '--format'; Tooltip = 'Output format' }
        @{ Name = '--compact'; Tooltip = 'Single-line output with template' }
        @{ Name = '--logo-path'; Tooltip = 'Path to logo image' }
        @{ Name = '--logo-protocol'; Tooltip = 'Logo protocol' }
        @{ Name = '--logo-width'; Tooltip = 'Logo width in pixels' }
        @{ Name = '--logo-height'; Tooltip = 'Logo height in pixels' }
        @{ Name = '--version'; Tooltip = 'Show version info' }
        @{ Name = '--help'; Tooltip = 'Show help message' }
        @{ Name = '--benchmark'; Tooltip = 'Show timing for each data source' }
        @{ Name = '--config-path'; Tooltip = 'Display config file location' }
        @{ Name = '--generate-completions'; Tooltip = 'Generate shell completions' }
        @{ Name = '--list-plugins'; Tooltip = 'List all available plugins' }
        @{ Name = '--plugin-info'; Tooltip = 'Show detailed plugin information' }
    )

    $options | Where-Object { $_.Name -like "$wordToComplete*" } | ForEach-Object {
        [System.Management.Automation.CompletionResult]::new($_.Name, $_.Name, 'ParameterValue', $_.Tooltip)
    }
}
)pwsh");
    } else {
      Println("Unknown shell: {}. Supported shells: bash, zsh, fish, powershell", shell);
    }
  }
} // namespace draconis::cli
