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
    IPlugin* (*createFunc)();
    void (*destroyFunc)(IPlugin*);
  };

  /**
   * @brief Get the mutable registry of static plugins (for registration)
   * @return Reference to the map of plugin name -> entry
   */
  auto GetStaticPluginRegistry() -> utils::types::UnorderedMap<utils::types::String, StaticPluginEntry>&;

  /**
   * @brief Register a static plugin (called automatically by DRAC_PLUGIN macro)
   * @param name The plugin name
   * @param entry The plugin entry to register
   * @return true (used to enable static initialization)
   */
  inline auto RegisterStaticPlugin(const char* name, StaticPluginEntry entry) -> bool {
    GetStaticPluginRegistry().emplace(name, entry);
    return true;
  }

  /**
   * @brief Check if a plugin is available as a static plugin
   * @param name The plugin name to check
   * @return true if the plugin is statically compiled, false otherwise
   */
  auto IsStaticPlugin(const utils::types::String& name) -> bool;

  /**
   * @brief Create an instance of a static plugin
   * @param name The plugin name
   * @return Pointer to the created plugin instance, or nullptr if not found
   */
  auto CreateStaticPlugin(const utils::types::String& name) -> IPlugin*;

  /**
   * @brief Destroy an instance of a static plugin
   * @param name The plugin name
   * @param plugin The plugin instance to destroy
   */
  auto DestroyStaticPlugin(const utils::types::String& name, IPlugin* plugin) -> void;

} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG
