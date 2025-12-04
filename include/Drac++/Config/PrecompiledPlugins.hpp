#pragma once

#include <string_view>

namespace draconis::config {
  struct PrecompiledPluginConfig {
    std::string_view name;   // Plugin name (e.g., "weather")
    std::string_view config; // TOML configuration content
  };
} // namespace draconis::config
