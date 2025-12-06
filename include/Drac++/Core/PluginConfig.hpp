/**
 * @file PluginConfig.hpp
 * @brief Plugin system configuration types
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This header provides configuration types for the plugin system.
 * It is part of the library interface and can be used by external programs
 * to configure plugin loading without depending on the CLI.
 */

#pragma once

#include "../Utils/Types.hpp"

namespace draconis::core::plugin {
  /**
   * @struct PluginConfig
   * @brief Configuration settings for the plugin system
   */
  struct PluginConfig {
    bool                                                        enabled = true; ///< Whether the plugin system is enabled
    draconis::utils::types::Vec<draconis::utils::types::String> autoLoad;       ///< List of plugin names to auto-load
  };
} // namespace draconis::core::plugin
