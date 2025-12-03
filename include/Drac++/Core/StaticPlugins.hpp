/**
 * @file StaticPlugins.hpp
 * @brief Static plugin registry for precompiled configuration mode
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details When DRAC_PRECOMPILED_CONFIG is enabled and plugins are specified
 * in the config, they can be statically linked into the binary rather than
 * loaded dynamically at runtime. This provides:
 * - Fully portable single-binary deployment
 * - Faster startup (no dynamic library loading)
 * - Smaller distribution (no separate .dll/.so files needed)
 *
 * Plugins self-register using the DRAC_PLUGIN macro at static initialization.
 */

#pragma once

#if DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG

  #include "../Utils/Types.hpp"
  #include "Plugin.hpp"

namespace draconis::core::plugin {
  /**
   * @struct StaticPluginEntry
   * @brief Entry for a statically compiled plugin
   */
  struct StaticPluginEntry {
    const char* name;
    IPlugin* (*createFunc)();
    void (*destroyFunc)(IPlugin*);
  };

  /**
   * @brief Get the mutable registry of static plugins (for registration)
   * @return Reference to the vector of static plugin entries
   */
  fn GetStaticPluginRegistry() -> utils::types::Vec<StaticPluginEntry>&;

  /**
   * @brief Register a static plugin (called automatically by DRAC_PLUGIN macro)
   * @param entry The plugin entry to register
   * @return true (used to enable static initialization)
   */
  inline fn RegisterStaticPlugin(StaticPluginEntry entry) -> bool {
    GetStaticPluginRegistry().push_back(entry);
    return true;
  }

  /**
   * @brief Get the list of statically compiled plugins
   * @return Vector of static plugin entries
   */
  fn GetStaticPlugins() -> const utils::types::Vec<StaticPluginEntry>&;

  /**
   * @brief Check if a plugin is available as a static plugin
   * @param name The plugin name to check
   * @return true if the plugin is statically compiled, false otherwise
   */
  fn IsStaticPlugin(const utils::types::String& name) -> bool;

  /**
   * @brief Create an instance of a static plugin
   * @param name The plugin name
   * @return Pointer to the created plugin instance, or nullptr if not found
   */
  fn CreateStaticPlugin(const utils::types::String& name) -> IPlugin*;

  /**
   * @brief Destroy an instance of a static plugin
   * @param name The plugin name
   * @param plugin The plugin instance to destroy
   */
  fn DestroyStaticPlugin(const utils::types::String& name, IPlugin* plugin) -> void;

} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG
