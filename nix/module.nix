{self}: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.draconisplusplus;

  tomlFormat = pkgs.formats.toml {};

  defaultPackage = self.packages.${pkgs.system}.default;

  stdenvHost = pkgs.stdenv;
  isLinux = stdenvHost.isLinux or false;
  isDarwin = stdenvHost.isDarwin or false;

  managerEnumMap = {
    cargo = "Cargo";
    nix = "Nix";
    apk = "Apk";
    dpkg = "Dpkg";
    moss = "Moss";
    pacman = "Pacman";
    rpm = "Rpm";
    xbps = "Xbps";
    homebrew = "Homebrew";
    macports = "Macports";
    winget = "Winget";
    chocolatey = "Chocolatey";
    scoop = "Scoop";
    pkgng = "PkgNg";
    pkgsrc = "PkgSrc";
    haikupkg = "HaikuPkg";
  };

  selectedManagers = map (pkg: "services::packages::Manager::${managerEnumMap.${pkg}}") cfg.packageManagers;

  packageManagerValue =
    if selectedManagers == []
    then "services::packages::Manager::None"
    else builtins.concatStringsSep " | " selectedManagers;

  logoAttrs =
    filterAttrs (_: v: v != null) {
      path = cfg.logo.path;
      protocol = cfg.logo.protocol;
      width = cfg.logo.width;
      height = cfg.logo.height;
    };

  cfgDelimiter = "DRACCFG";

  pluginConfigEntries =
    lib.mapAttrsToList (
      name: val:
        let
          content = builtins.readFile (tomlFormat.generate "${name}.toml" val);
        in ''
          PrecompiledPluginConfig{ .name = "${name}", .config = R"${cfgDelimiter}(${content})${cfgDelimiter}" },
        ''
    )
    cfg.pluginConfigs;

  pluginConfigArray =
    if pluginConfigEntries == []
    then ''
      inline constexpr bool DRAC_HAS_PLUGIN_CONFIGS = false;
      inline constexpr std::array<PrecompiledPluginConfig, 0> DRAC_PLUGIN_CONFIGS = {};
    ''
    else ''
      inline constexpr bool DRAC_HAS_PLUGIN_CONFIGS = true;
      inline constexpr std::array<PrecompiledPluginConfig, ${toString (builtins.length pluginConfigEntries)}> DRAC_PLUGIN_CONFIGS = {
        ${builtins.concatStringsSep "\n        " pluginConfigEntries}
      };
    '';

  configHpp =
    pkgs.writeText "config.hpp"
    # cpp
    ''
      #pragma once

      #if DRAC_PRECOMPILED_CONFIG

        #include <array>
        #include <Drac++/Config/PrecompiledLayout.hpp>
        #include <Drac++/Config/PrecompiledPlugins.hpp>

        #if DRAC_ENABLE_PACKAGECOUNT
          #include <Drac++/Services/Packages.hpp>
        #endif

      namespace draconis::config {
        constexpr const char* DRAC_USERNAME = "${cfg.username}";

        #if DRAC_ENABLE_PACKAGECOUNT
        constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = ${packageManagerValue};
        #endif

        inline constexpr std::array<PrecompiledLayoutRow, 1> DRAC_UI_INTRO_ROWS = {
          Row("date"),
        };

        inline constexpr std::array<PrecompiledLayoutRow, 3> DRAC_UI_SYSTEM_ROWS = {
          Row("host"),
          Row("os"),
          Row("kernel"),
        };

        inline constexpr std::array<PrecompiledLayoutRow, 5> DRAC_UI_HARDWARE_ROWS = {
          Row("cpu"),
          Row("gpu"),
          Row("ram"),
          Row("disk"),
          Row("uptime"),
        };

        inline constexpr std::array<PrecompiledLayoutRow, 2> DRAC_UI_SOFTWARE_ROWS = {
          Row("shell"),
          Row("packages"),
        };

        inline constexpr std::array<PrecompiledLayoutRow, 3> DRAC_UI_SESSION_ROWS = {
          Row("de"),
          Row("wm"),
          Row("playing"),
        };

        inline constexpr std::array<PrecompiledLayoutGroup, 5> DRAC_UI_LAYOUT = {
          Group("intro", DRAC_UI_INTRO_ROWS),
          Group("system", DRAC_UI_SYSTEM_ROWS),
          Group("hardware", DRAC_UI_HARDWARE_ROWS),
          Group("software", DRAC_UI_SOFTWARE_ROWS),
          Group("session", DRAC_UI_SESSION_ROWS),
        };

        ${pluginConfigArray}
      }

      #endif
    '';

  draconisWithOverrides = cfg.package.overrideAttrs (oldAttrs: {
    postPatch =
      (oldAttrs.postPatch or "")
      + lib.optionalString (cfg.configFormat == "hpp") ''
        cp ${configHpp} ./config.hpp
      '';

    mesonFlags =
      (oldAttrs.mesonFlags or [])
      ++ [
        "-Dprecompiled_config=${if cfg.configFormat == "hpp" then "true" else "false"}"
        "-Dcaching=${if cfg.enableCaching then "enabled" else "disabled"}"
        "-Dpackagecount=${if cfg.enablePackageCount then "enabled" else "disabled"}"
        "-Dplugins=${if cfg.enablePlugins then "enabled" else "disabled"}"
        "-Dpugixml=${if cfg.usePugixml then "enabled" else "disabled"}"
      ]
      ++ lib.optional (cfg.staticPlugins != []) "-Dstatic_plugins=${lib.concatStringsSep "," cfg.staticPlugins}";
  });

  draconisPkg = draconisWithOverrides;
in {
  options.programs.draconisplusplus = {
    enable = mkEnableOption "draconis++";

    package = mkOption {
      type = types.package;
      default = defaultPackage;
      description = "The base draconis++ package.";
    };

    configFormat = mkOption {
      type = types.enum ["toml" "hpp"];
      default = "toml";
      description = "The configuration format to use.";
    };

    username = mkOption {
      type = types.str;
      default = config.home.username // "User";
      description = "Username to display in the application.";
    };

    language = mkOption {
      type = types.str;
      default = "en";
      description = "Language code for localization (e.g., \"en\", \"es\").";
    };

    logo = mkOption {
      description = "Logo configuration (used for kitty/kitty-direct).";
      default = {
        path = null;
        protocol = "kitty";
        width = null;
        height = null;
      };
      type = types.submodule {
        options = {
          path = mkOption {
            type = types.nullOr types.str;
            default = null;
            description = "Path to the logo image.";
          };

          protocol = mkOption {
            type = types.enum ["kitty" "kitty-direct"];
            default = "kitty";
            description = "Logo protocol to use.";
          };

          width = mkOption {
            type = types.nullOr types.int;
            default = null;
            description = "Optional logo width.";
          };

          height = mkOption {
            type = types.nullOr types.int;
            default = null;
            description = "Optional logo height.";
          };
        };
      };
    };

    enablePlugins = mkOption {
      type = types.bool;
      default = true;
      description = "Enable plugin system.";
    };

    pluginConfigs = mkOption {
      type = types.attrsOf types.attrs;
      default = {};
      description = ''
        Per-plugin configuration written under [plugins.<name>] in config.toml.
        Keys and values are passed through directly to the TOML generator.
      '';
      example = literalExpression ''
        {
          weather = {
            enabled = true;
            provider = "openmeteo";
            coords = {
              lat = 40.7128;
              lon = -74.0060;
            };
          };
        }
      '';
    };

    packageManagers = mkOption {
      type = types.listOf (types.enum (
        ["cargo"]
        ++ lib.optionals isLinux ["apk" "dpkg" "moss" "pacman" "rpm" "xbps" "nix"]
        ++ lib.optionals isDarwin ["homebrew" "macports" "nix"]
      ));
      default = [];
      description = "List of package managers to check for package counts.";
    };

    staticPlugins = mkOption {
      type = types.listOf (types.enum ["weather" "now_playing" "json_format" "markdown_format" "yaml_format"]);
      default = [];
      description = "Plugins to compile statically into the binary (precompiled config only).";
    };

    pluginAutoLoad = mkOption {
      type = types.listOf types.str;
      default = [];
      description = "Plugin names to auto-load at runtime.";
    };

    enablePackageCount = mkOption {
      type = types.bool;
      default = true;
      description = "Enable getting package count.";
    };

    enableCaching = mkOption {
      type = types.bool;
      default = true;
      description = "Enable caching functionality.";
    };

    usePugixml = mkOption {
      type = types.bool;
      default = false;
      description = "Use pugixml to parse XBPS package metadata. Required for package count functionality on Void Linux.";
    };
  };

  config = mkIf cfg.enable {
    home.packages = [draconisPkg];

    xdg.configFile =
      lib.optionalAttrs (cfg.configFormat == "toml") {
        "draconis++/config.toml" = {
          source = tomlFormat.generate "config.toml" (
            {
              general = {
                name     = cfg.username;
                language = cfg.language;
              };
              packages.enabled = cfg.packageManagers;
              plugins =
                {
                  enabled   = cfg.enablePlugins;
                  auto_load = cfg.pluginAutoLoad;
                }
                // cfg.pluginConfigs;
            }
            // lib.optionalAttrs (logoAttrs != {}) {logo = logoAttrs;}
          );
        };
      };

    assertions = [
      {
        assertion = !(cfg.usePugixml && !cfg.enablePackageCount);
        message = "usePugixml should only be enabled when enablePackageCount is also enabled.";
      }
      {
        assertion = !(cfg.pluginAutoLoad != [] && !cfg.enablePlugins);
        message = "Plugins must be enabled to auto-load plugins.";
      }
      {
        assertion = !(cfg.staticPlugins != [] && cfg.configFormat != "hpp");
        message = "Static plugins require the precompiled (hpp) configuration.";
      }
    ];
  };
}
