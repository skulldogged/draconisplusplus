#include "Config.hpp"

#include <magic_enum/magic_enum.hpp>

#include <Drac++/Utils/Logging.hpp>

#if !DRAC_PRECOMPILED_CONFIG
  #include <filesystem> // std::filesystem::{path, operator/, exists, create_directories}
  #include <glaze/toml.hpp>
  #include <system_error> // std::error_code

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

// Intermediate structs for TOML parsing with glaze
// Note: glaze's TOML parser doesn't support std::optional directly,
// so we use empty strings/zero values as sentinels for "not provided"
namespace {
  struct TomlGeneral {
    String name;     // Empty = not provided
    String language; // Empty = not provided
  };

  struct TomlLogo {
    String path;       // Empty = not provided
    String protocol;   // Empty = not provided
    u32    width  = 0; // 0 = not provided
    u32    height = 0; // 0 = not provided
  };

  struct TomlPackages {
    Vec<String> enabled;
  };

  struct TomlPlugins {
    bool        enabled = true;
    Vec<String> autoLoad;
  };

  struct TomlLayoutRow {
    String key;              // Required identifier
    String label;            // Optional override, empty = not provided
    String icon;             // Optional override, empty = not provided
    String color;            // Optional value color name, empty = default
    bool   autoWrap = false; // Enable automatic word wrapping
  };

  struct TomlLayoutGroup {
    String             name;
    Vec<TomlLayoutRow> rows;
  };

  struct TomlUI {
    Vec<TomlLayoutGroup> layout;
  };

  struct TomlConfig {
    TomlGeneral  general;
    TomlLogo     logo;
    TomlPackages packages;
    TomlPlugins  plugins;
    TomlUI       ui;
  };
} // namespace

  // Explicit glz::meta specializations with field name mappings
  // The 'value' members are used by glaze's compile-time reflection, not directly referenced
  #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunused-const-variable"
  #endif

template <>
struct glz::meta<TomlGeneral> {
  using T                     = TomlGeneral;
  static constexpr auto value = object("name", &T::name, "language", &T::language);
};

template <>
struct glz::meta<TomlLogo> {
  using T                     = TomlLogo;
  static constexpr auto value = object("path", &T::path, "protocol", &T::protocol, "width", &T::width, "height", &T::height);
};

template <>
struct glz::meta<TomlPackages> {
  using T                     = TomlPackages;
  static constexpr auto value = object("enabled", &T::enabled);
};

template <>
struct glz::meta<TomlPlugins> {
  using T                     = TomlPlugins;
  static constexpr auto value = object("enabled", &T::enabled, "auto_load", &T::autoLoad);
};

template <>
struct glz::meta<TomlLayoutRow> {
  using T                     = TomlLayoutRow;
  static constexpr auto value = object("key", &T::key, "label", &T::label, "icon", &T::icon, "color", &T::color, "auto_wrap", &T::autoWrap);
};

template <>
struct glz::meta<TomlLayoutGroup> {
  using T                     = TomlLayoutGroup;
  static constexpr auto value = object("name", &T::name, "rows", &T::rows);
};

template <>
struct glz::meta<TomlUI> {
  using T                     = TomlUI;
  static constexpr auto value = object("layout", &T::layout);
};

template <>
struct glz::meta<TomlConfig> {
  using T                     = TomlConfig;
  static constexpr auto value = object(
    "general",
    &T::general,
    "logo",
    &T::logo,
    "packages",
    &T::packages,
    "plugins",
    &T::plugins,
    "ui",
    &T::ui
  );
};

  #ifdef __clang__
    #pragma clang diagnostic pop
  #endif

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

      // Build default config using glaze
      TomlConfig defaultCfg;
      defaultCfg.general.name = draconis::config::General::getDefaultName();

  #if DRAC_ENABLE_PLUGINS
      defaultCfg.plugins.enabled = true;
  #endif

      // Write config using glaze
      String     buffer;
      const auto writeError = glz::write_file_toml(defaultCfg, configPath.string(), buffer);

      if (writeError) {
        error_log("Failed to write default config: {}", glz::format_error(writeError, buffer));
        return false;
      }

      info_log("Created default config file at {}", configPath.string());
      return true;
    } catch (const fs::filesystem_error& fsErr) {
      error_log("Filesystem error during default config creation: {}", fsErr.what());
      return false;
    } catch (const Exception& exc) {
      error_log("Failed to create default config file: {}", exc.what());
      return false;
    } catch (...) {
      error_log("An unexpected error occurred during default config creation.");
      return false;
    }
  }
} // namespace

#endif // !DRAC_PRECOMPILED_CONFIG

namespace draconis::config {
#if DRAC_PRECOMPILED_CONFIG
  namespace {
    fn PopulatePrecompiledLayout(Config& cfg) -> void {
      cfg.ui.layout.clear();

      for (const auto& group : DRAC_UI_LAYOUT) {
        UILayoutGroup cfgGroup;
        cfgGroup.name = group.name ? group.name : "";

        for (const auto& row : group.rows) {
          if (row.key == nullptr || std::strlen(row.key) == 0)
            continue;

          UILayoutRow cfgRow;
          cfgRow.key = row.key;

          if (row.label != nullptr && std::strlen(row.label) > 0)
            cfgRow.label = row.label;
          if (row.icon != nullptr && std::strlen(row.icon) > 0)
            cfgRow.icon = row.icon;
          cfgRow.color    = row.color;
          cfgRow.autoWrap = row.autoWrap;

          cfgGroup.rows.push_back(std::move(cfgRow));
        }

        if (!cfgGroup.rows.empty())
          cfg.ui.layout.push_back(std::move(cfgGroup));
      }
    }
  } // namespace
#endif

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

    PopulatePrecompiledLayout(cfg);
    ;

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

      // Parse TOML using glaze with lenient parsing (ignore unknown keys like [weather])
      TomlConfig tomlCfg;
      String     buffer;

      // Read file into buffer
      glz::context ctx {};
      ctx.current_file = configPath.string();
      if (const auto fileError = glz::file_to_buffer(buffer, ctx.current_file); bool(fileError)) {
        error_log("Failed to read config file: {}", configPath.string());
        return {};
      }

      // Parse with error_on_unknown_keys = false to allow plugin sections like [weather]
      const auto readError = glz::read<glz::opts { .format = glz::TOML, .error_on_unknown_keys = false }>(tomlCfg, buffer, ctx);

      if (readError) {
        error_log("Failed to parse config file: {}", glz::format_error(readError, buffer));
        return {};
      }

      debug_log("Config loaded from {}", configPath.string());

      // Convert TomlConfig to Config
      // Note: Empty strings and zero values indicate "not provided" in TOML
      Config cfg;

      // General settings - convert empty string to std::nullopt, non-empty to value
      cfg.general.name     = tomlCfg.general.name.empty() ? std::nullopt : Option<String>(tomlCfg.general.name);
      cfg.general.language = tomlCfg.general.language.empty() ? std::nullopt : Option<String>(tomlCfg.general.language);

      if (!cfg.general.name)
        cfg.general.name = General::getDefaultName();

      // Logo settings - convert empty/zero to std::nullopt
      cfg.logo.imagePath = tomlCfg.logo.path.empty() ? std::nullopt : Option<String>(tomlCfg.logo.path);
      cfg.logo.protocol  = tomlCfg.logo.protocol.empty() ? std::nullopt : Option<String>(tomlCfg.logo.protocol);
      cfg.logo.width     = tomlCfg.logo.width == 0 ? std::nullopt : Option<u32>(tomlCfg.logo.width);
      cfg.logo.height    = tomlCfg.logo.height == 0 ? std::nullopt : Option<u32>(tomlCfg.logo.height);

      // Package manager settings
      if constexpr (DRAC_ENABLE_PACKAGECOUNT) {
        using enum draconis::services::packages::Manager;

        cfg.enabledPackageManagers = None;

        for (const String& val : tomlCfg.packages.enabled) {
          if (val == "cargo")
            cfg.enabledPackageManagers |= Cargo;
  #if defined(__linux__) || defined(__APPLE__)
          else if (val == "nix")
            cfg.enabledPackageManagers |= Nix;
  #endif
  #ifdef __linux__
          else if (val == "apk")
            cfg.enabledPackageManagers |= Apk;
          else if (val == "dpkg")
            cfg.enabledPackageManagers |= Dpkg;
          else if (val == "moss")
            cfg.enabledPackageManagers |= Moss;
          else if (val == "pacman")
            cfg.enabledPackageManagers |= Pacman;
          else if (val == "rpm")
            cfg.enabledPackageManagers |= Rpm;
          else if (val == "xbps")
            cfg.enabledPackageManagers |= Xbps;
  #endif
  #ifdef __APPLE__
          else if (val == "homebrew")
            cfg.enabledPackageManagers |= Homebrew;
          else if (val == "macports")
            cfg.enabledPackageManagers |= Macports;
  #endif
  #ifdef _WIN32
          else if (val == "winget")
            cfg.enabledPackageManagers |= Winget;
          else if (val == "chocolatey")
            cfg.enabledPackageManagers |= Chocolatey;
          else if (val == "scoop")
            cfg.enabledPackageManagers |= Scoop;
  #endif
  #if defined(__FreeBSD__) || defined(__DragonFly__)
          else if (val == "pkgng")
            cfg.enabledPackageManagers |= PkgNg;
  #endif
  #ifdef __NetBSD__
          else if (val == "pkgsrc")
            cfg.enabledPackageManagers |= PkgSrc;
  #endif
  #ifdef __HAIKU__
          else if (val == "haikupkg")
            cfg.enabledPackageManagers |= HaikuPkg;
  #endif
          else
            warn_log("Unknown package manager in config: {}", val);
        }
      }

      // Plugin settings
      if constexpr (DRAC_ENABLE_PLUGINS) {
        cfg.plugins.enabled  = tomlCfg.plugins.enabled;
        cfg.plugins.autoLoad = tomlCfg.plugins.autoLoad;
      }

      // UI layout settings
      cfg.ui.layout.clear();
      for (const TomlLayoutGroup& group : tomlCfg.ui.layout) {
        UILayoutGroup cfgGroup;
        cfgGroup.name = group.name;

        for (const TomlLayoutRow& row : group.rows) {
          if (row.key.empty())
            continue;

          UILayoutRow cfgRow;
          cfgRow.key = row.key;
          if (!row.label.empty())
            cfgRow.label = row.label;
          if (!row.icon.empty())
            cfgRow.icon = row.icon;
          if (!row.color.empty())
            if (auto parsed = magic_enum::enum_cast<LogColor>(row.color, magic_enum::case_insensitive))
              cfgRow.color = *parsed;

          cfgRow.autoWrap = row.autoWrap;
          cfgGroup.rows.push_back(std::move(cfgRow));
        }

        cfg.ui.layout.push_back(std::move(cfgGroup));
      }

      return cfg;
    } catch (const Exception& exc) {
      debug_log("Config loading failed: {}, using defaults", exc.what());
      return {};
    } catch (...) {
      error_log("An unexpected error occurred during config loading. Using in-memory defaults.");
      return {};
    }
#endif // DRAC_PRECOMPILED_CONFIG
  }
} // namespace draconis::config
