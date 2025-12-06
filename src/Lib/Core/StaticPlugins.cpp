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

namespace draconis::core::plugin {
  using namespace utils::types;

  auto GetStaticPluginRegistry() -> UnorderedMap<String, StaticPluginEntry>& {
    static UnorderedMap<String, StaticPluginEntry> Registry;
    return Registry;
  }

  auto IsStaticPlugin(const String& name) -> bool {
    return GetStaticPluginRegistry().contains(name);
  }

  auto CreateStaticPlugin(const String& name) -> IPlugin* {
    const auto& registry = GetStaticPluginRegistry();
    if (auto iter = registry.find(name); iter != registry.end())
      return iter->second.createFunc();
    return nullptr;
  }

  auto DestroyStaticPlugin(const String& name, IPlugin* plugin) -> void {
    if (!plugin)
      return;

    const auto& registry = GetStaticPluginRegistry();
    if (auto iter = registry.find(name); iter != registry.end())
      iter->second.destroyFunc(plugin);
  }
} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS && DRAC_PRECOMPILED_CONFIG
