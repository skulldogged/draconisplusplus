/**
 * @file StaticPlugins.cpp
 * @brief Static plugin registry implementation for precompiled configuration mode
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This file provides the static plugin registry infrastructure.
 * Plugins self-register at static initialization time using the DRAC_PLUGIN macro.
 * No hardcoded plugin data is needed here - the registry is populated automatically
 * when plugin object files are linked into the binary.
 */

#include <Drac++/Core/StaticPlugins.hpp>

#if DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG

  #include <algorithm>

namespace draconis::core::plugin {
  fn GetStaticPluginRegistry() -> utils::types::Vec<StaticPluginEntry>& {
    static utils::types::Vec<StaticPluginEntry> Registry;
    return Registry;
  }

  fn GetStaticPlugins() -> const utils::types::Vec<StaticPluginEntry>& {
    return GetStaticPluginRegistry();
  }

  fn IsStaticPlugin(const utils::types::String& name) -> bool {
    const auto& plugins = GetStaticPlugins();

    return std::ranges::any_of(
      plugins,
      [&name](const auto& entry) -> auto {
        return name == entry.name;
      }
    );
  }

  fn CreateStaticPlugin(const utils::types::String& name) -> IPlugin* {
    const auto& plugins = GetStaticPlugins();

    for (const auto& entry : plugins)
      if (name == entry.name)
        return entry.createFunc();

    return nullptr;
  }

  fn DestroyStaticPlugin(const utils::types::String& name, IPlugin* plugin) -> void {
    if (!plugin)
      return;

    const auto& plugins = GetStaticPlugins();

    for (const auto& entry : plugins)
      if (name == entry.name) {
        entry.destroyFunc(plugin);
        return;
      }
  }
} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG
