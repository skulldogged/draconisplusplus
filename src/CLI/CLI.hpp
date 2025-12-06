/**
 * @file CLI.hpp
 * @brief CLI utility functions for draconis++
 *
 * This file contains utility functions for CLI features including:
 * - Benchmark timing and reporting
 * - Shell completion generation
 * - Various output format helpers (doctor, JSON, compact)
 */

#pragma once

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"

namespace draconis::cli {
  /**
   * @brief Structure to hold benchmark timing results for each data source
   */
  struct BenchmarkResult {
    utils::types::String name;
    utils::types::f64    durationMs;
    bool                 success;
  };

  /**
   * @brief Run benchmark and collect timing information for each data source
   * @param cache Cache manager reference
   * @param config Application configuration
   * @return Vector of benchmark results
   */
  auto RunBenchmark(
    utils::cache::CacheManager& cache,
    const config::Config&       config
  ) -> utils::types::Vec<BenchmarkResult>;

  /**
   * @brief Print benchmark report showing timing for each data source
   * @param results Vector of benchmark results
   */
  auto PrintBenchmarkReport(const utils::types::Vec<BenchmarkResult>& results) -> utils::types::Unit;

  /**
   * @brief Print doctor report showing failed readouts
   * @param data System information data
   */
  auto PrintDoctorReport(const core::system::SystemInfo& data) -> utils::types::Unit;

  /**
   * @brief Print system information in JSON format
   * @param data System information data
   * @param prettyJson Whether to pretty-print the JSON
   */
  auto PrintJsonOutput(
    const core::system::SystemInfo& data,
    bool                            prettyJson
  ) -> utils::types::Unit;

  /**
   * @brief Print system information in compact single-line format using a template string
   * @param templateStr Template string with placeholders like {key}
   * @param data System information data
   *
   * Placeholders use the format {key} where key matches any field from SystemInfo::toMap().
   * Common keys: date, host, os, kernel, cpu, gpu, ram, disk, uptime, shell, de, wm, packages, playing
   */
  auto PrintCompactOutput(
    const utils::types::String&     templateStr,
    const core::system::SystemInfo& data
  ) -> utils::types::Unit;

#if DRAC_ENABLE_PLUGINS
  /**
   * @brief Format output using a plugin
   * @param formatName Name of the format to use
   * @param data System information data
   */
  auto FormatOutputViaPlugin(
    const utils::types::String&     formatName,
    const core::system::SystemInfo& data
  ) -> utils::types::Unit;

  /**
   * @brief Handle --list-plugins command
   * @param pluginManager Reference to plugin manager
   * @return Exit code
   */
  auto HandleListPluginsCommand(const core::plugin::PluginManager& pluginManager) -> utils::types::i32;

  /**
   * @brief Handle --plugin-info command
   * @param pluginManager Reference to plugin manager
   * @param pluginName Name of plugin to show info for
   * @return Exit code
   */
  auto HandlePluginInfoCommand(const core::plugin::PluginManager& pluginManager, const utils::types::String& pluginName) -> utils::types::i32;
#endif

  /**
   * @brief Generate shell completion scripts
   * @param shell The shell to generate completions for (bash, zsh, fish, powershell)
   */
  auto GenerateCompletions(const utils::types::String& shell) -> utils::types::Unit;
} // namespace draconis::cli
