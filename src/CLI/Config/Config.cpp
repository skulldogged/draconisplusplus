#include "Config.hpp"

#include <Drac++/Utils/Logging.hpp>

#if !DRAC_PRECOMPILED_CONFIG
  #include <filesystem>                // std::filesystem::{path, operator/, exists, create_directories}
  #include <fstream>                   // std::{ifstream, ofstream, operator<<}
  #include <system_error>              // std::error_code
  #include <toml++/impl/node_view.hpp> // toml::node_view
  #include <toml++/impl/parser.hpp>    // toml::{parse_file, parse_result}
  #include <toml++/impl/table.hpp>     // toml::table

  #include <Drac++/Services/Packages.hpp>

  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace fs = std::filesystem;
#else
  #if DRAC_ENABLE_PLUGINS
    #include <Drac++/Core/StaticPlugins.hpp>
  #endif

  #include "../config.hpp" // user-defined config
#endif

#if !DRAC_PRECOMPILED_CONFIG
using namespace draconis::utils::types;
using draconis::utils::env::GetEnv;

namespace draconis::config {
  fn Config::getConfigPath() -> fs::path {
    Vec<fs::path> possiblePaths;

  #ifdef _WIN32
    if (Result<String> result = GetEnv("LOCALAPPDATA"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (Result<String> result = GetEnv("USERPROFILE")) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / "AppData" / "Local" / "draconis++" / "config.toml");
    }

    if (Result<String> result = GetEnv("APPDATA"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");
  #else
    if (Result<String> result = GetEnv("XDG_CONFIG_HOME"))
      possiblePaths.emplace_back(fs::path(*result) / "draconis++" / "config.toml");

    if (Result<String> result = GetEnv("HOME")) {
      possiblePaths.emplace_back(fs::path(*result) / ".config" / "draconis++" / "config.toml");
      possiblePaths.emplace_back(fs::path(*result) / ".draconis++" / "config.toml");
    }
  #endif

    possiblePaths.emplace_back(fs::path(".") / "config.toml");

    for (const fs::path& path : possiblePaths)
      if (std::error_code errc; fs::exists(path, errc) && !errc)
        return path;

    if (!possiblePaths.empty()) {
      const fs::path defaultDir = possiblePaths[0].parent_path();

      if (std::error_code errc; !fs::exists(defaultDir, errc) || errc) {
        create_directories(defaultDir, errc);
      }

      return possiblePaths[0];
    }

    warn_log("Could not determine a preferred config path. Falling back to './config.toml'");
    return fs::path(".") / "config.toml";
  }
} // namespace draconis::config

namespace {

  fn CreateDefaultConfig(const fs::path& configPath) -> bool {
    try {
      std::error_code errc;
      create_directories(configPath.parent_path(), errc);

      if (errc) {
        error_log("Failed to create config directory: {}", errc.message());
        return false;
      }

      const String defaultName   = draconis::config::General::getDefaultName();
      String       configContent = std::format(R"toml(# Draconis++ Configuration File

# General settings
[general]
name = "{}" # Your display name
)toml",
                                         defaultName);

  #if DRAC_ENABLE_NOWPLAYING
      configContent += R"toml(
# Now Playing integration
[now_playing]
enabled = true # Set to true to enable media integration
)toml";
  #endif

  #if !DRAC_PRECOMPILED_CONFIG
      configContent += R"toml(
# Image logo (kitty / kitty-direct)
[logo]
# path = ""           # Path to an image file; when empty, ascii art is used
# protocol = "kitty"  # Options: "kitty" or "kitty-direct"
# width = 24          # Width in terminal cells
# height = 12         # Height in terminal cells
)toml";
  #endif

  #if DRAC_ENABLE_PACKAGECOUNT
      configContent += R"toml(
# Package counting settings
[packages]
enabled = [] # List of package managers to count, e.g. ["cargo", "nix", "pacman"]

# Possible values depend on your OS: cargo, nix, apk, dpkg, moss, pacman, rpm, xbps, homebrew, macports, winget, chocolatey, scoop, pkgng, pkgsrc, haikupkg
# If you don't want to count any package managers, leave the list empty.
)toml";
  #endif

  #if DRAC_ENABLE_PLUGINS
      configContent += R"toml(
# Plugin settings
[plugins]
enabled = true        # Set to false to disable the plugin system entirely
auto_load = []        # List of plugin names to automatically load on startup
# Example: auto_load = ["weather"]
)toml";
  #endif

      std::ofstream file(configPath);
      file << configContent;

      info_log("Created default config file at {}", configPath.string());
      return true;
    } catch (const fs::filesystem_error& fsErr) {
      error_log("Filesystem error during default config creation: {}", fsErr.what());
      return false;
    } catch (const Exception& e) {
      error_log("Failed to create default config file: {}", e.what());
      return false;
    } catch (...) {
      error_log("An unexpected error occurred during default config creation.");
      return false;
    }
  }
} // namespace

#endif // !DRAC_PRECOMPILED_CONFIG

namespace draconis::config {
  fn Config::getInstance() -> Config {
#if DRAC_PRECOMPILED_CONFIG
    using namespace draconis::config;

    Config cfg;
    cfg.general.name = DRAC_USERNAME;

    if constexpr (DRAC_ENABLE_PACKAGECOUNT)
      cfg.enabledPackageManagers = config::DRAC_ENABLED_PACKAGE_MANAGERS;

  #if DRAC_ENABLE_PLUGINS
    cfg.plugins.enabled = true;
    // Auto-load all statically compiled plugins
    for (const auto& entry : draconis::core::plugin::GetStaticPlugins())
      cfg.plugins.autoLoad.emplace_back(entry.name);
  #endif

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      cfg.nowPlaying.enabled = true;

    debug_log("Using precompiled configuration.");
    return cfg;
#else
    try {
      const fs::path configPath = Config::getConfigPath();

      std::error_code errc;

      const bool exists = fs::exists(configPath, errc);

      if (!exists) {
        info_log("Config file not found at {}, creating defaults.", configPath.string());

        if (!CreateDefaultConfig(configPath))
          return {};
      }

      const toml::table parsedConfig = toml::parse_file(configPath.string());

      debug_log("Config loaded from {}", configPath.string());

      return Config(parsedConfig);
    } catch (const Exception& e) {
      debug_log("Config loading failed: {}, using defaults", e.what());
      return {};
    } catch (...) {
      error_log("An unexpected error occurred during config loading. Using in-memory defaults.");
      return {};
    }
#endif // DRAC_PRECOMPILED_CONFIG
  }

#if !DRAC_PRECOMPILED_CONFIG
  Config::Config(const toml::table& tbl) {
    const toml::node_view genTbl = tbl["general"];
    this->general                = genTbl.is_table() ? General::fromToml(*genTbl.as_table()) : General {};

    if (!this->general.name)
      this->general.name = General::getDefaultName();

    if (const toml::node_view logoTbl = tbl["logo"]; logoTbl.is_table())
      this->logo = Logo::fromToml(*logoTbl.as_table());

    if constexpr (DRAC_ENABLE_NOWPLAYING) {
      const toml::node_view npTbl = tbl["now_playing"];
      this->nowPlaying            = npTbl.is_table() ? NowPlaying::fromToml(*npTbl.as_table()) : NowPlaying {};
    }

    if constexpr (DRAC_ENABLE_PACKAGECOUNT) {
      const toml::node_view pkgTbl = tbl["packages"];

      if (pkgTbl.is_table()) {
        const auto enabledNode = pkgTbl["enabled"];

        if (enabledNode.is_array()) {
          using enum draconis::services::packages::Manager;

          this->enabledPackageManagers = None;

          for (const auto& elem : *enabledNode.as_array()) {
            if (auto valOpt = elem.value<String>()) {
              String val = *valOpt;

              if (val == "cargo")
                this->enabledPackageManagers |= Cargo;
  #if defined(__linux__) || defined(__APPLE__)
              else if (val == "nix")
                this->enabledPackageManagers |= Nix;
  #endif
  #ifdef __linux__
              else if (val == "apk")
                this->enabledPackageManagers |= Apk;
              else if (val == "dpkg")
                this->enabledPackageManagers |= Dpkg;
              else if (val == "moss")
                this->enabledPackageManagers |= Moss;
              else if (val == "pacman")
                this->enabledPackageManagers |= Pacman;
              else if (val == "rpm")
                this->enabledPackageManagers |= Rpm;
              else if (val == "xbps")
                this->enabledPackageManagers |= Xbps;
  #endif
  #ifdef __APPLE__
              else if (val == "homebrew")
                this->enabledPackageManagers |= Homebrew;
              else if (val == "macports")
                this->enabledPackageManagers |= Macports;
  #endif
  #ifdef _WIN32
              else if (val == "winget")
                this->enabledPackageManagers |= Winget;
              else if (val == "chocolatey")
                this->enabledPackageManagers |= Chocolatey;
              else if (val == "scoop")
                this->enabledPackageManagers |= Scoop;
  #endif
  #if defined(__FreeBSD__) || defined(__DragonFly__)
              else if (val == "pkgng")
                this->enabledPackageManagers |= PkgNg;
  #endif
  #ifdef __NetBSD__
              else if (val == "pkgsrc")
                this->enabledPackageManagers |= PkgSrc;
  #endif
  #ifdef __HAIKU__
              else if (val == "haikupkg")
                this->enabledPackageManagers |= HaikuPkg;
  #endif
              else
                warn_log("Unknown package manager in config: {}", val);
            }
          }
        }
      }
    }

    if constexpr (DRAC_ENABLE_PLUGINS) {
      const toml::node_view pluginTbl = tbl["plugins"];
      this->plugins                   = pluginTbl.is_table() ? Plugins::fromToml(*pluginTbl.as_table()) : Plugins {};
    }
  }
#endif // !DRAC_PRECOMPILED_CONFIG
} // namespace draconis::config
