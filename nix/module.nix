{self}: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg =
    config.programs.draconisplusplus
    or config.myconfig.programs.draconisplusplus
    or {};

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

  defaultLayout = [
    {
      name = "intro";
      rows = [{key = "date";}];
    }
    {
      name = "system";
      rows = [
        {key = "host";}
        {key = "os";}
        {key = "kernel";}
      ];
    }
    {
      name = "hardware";
      rows = [
        {key = "cpu";}
        {key = "gpu";}
        {key = "ram";}
        {key = "disk";}
        {key = "uptime";}
      ];
    }
    {
      name = "software";
      rows = [
        {key = "shell";}
        {key = "packages";}
      ];
    }
    {
      name = "session";
      rows = [
        {key = "de";}
        {key = "wm";}
        {key = "playing";}
      ];
    }
  ];

  sanitizeLayoutName = name: lib.toUpper (lib.strings.replaceStrings [" " "-" "."] ["_" "_" "_"] name);

  escapeCppString =
    s:
    builtins.replaceStrings
    ["\\" "\""]
    ["\\\\"
      "\\\""]
    s;

  layoutRowToHpp =
    row:
    let
      autoWrapVal = if (row.autoWrap or false) then "true" else "false";
      labelVal =
        let label = row.label or null;
        in if label == null then "nullptr" else "\"${escapeCppString label}\"";
      iconVal =
        let icon = row.icon or null;
        in if icon == null then "nullptr" else "\"${escapeCppString icon}\"";
      colorVal =
        let color = row.color or null;
        in if color == null then "LogColor::White" else "LogColor::${color}";
      keyVal = "\"${escapeCppString row.key}\"";
    in "Row(${keyVal}, ${autoWrapVal}, ${colorVal}, ${labelVal}, ${iconVal})";

  layoutGroupHppEntries =
    map (
      group:
        let
          rows      = group.rows or [];
          arrayName = "DRAC_UI_${sanitizeLayoutName group.name}_ROWS";
        in {
          inherit arrayName;
          name = group.name;
          rowsCode = ''
            inline constexpr std::array<PrecompiledLayoutRow, ${toString (builtins.length rows)}> ${arrayName} = {
              ${builtins.concatStringsSep ",\n              " (map (row: layoutRowToHpp row) rows)}
            };
          '';
        }
    )
    cfg.layout;

  layoutGroupsHppCode = builtins.concatStringsSep "\n\n        " (map (g: g.rowsCode) layoutGroupHppEntries);

  layoutArrayHppCode = ''
        inline constexpr std::array<PrecompiledLayoutGroup, ${toString (builtins.length layoutGroupHppEntries)}> DRAC_UI_LAYOUT = {
          ${builtins.concatStringsSep "\n          " (map (g: ''Group("${g.name}", ${g.arrayName}),'') layoutGroupHppEntries)}
        };
  '';

  layoutToml =
    map (
      group: {
        name = group.name;
        rows =
          map (
            row:
              filterAttrs (_: v: v != null) {
                key = row.key;
                label = row.label or null;
                icon = row.icon or null;
                color = row.color or null;
                auto_wrap = row.autoWrap or false;
              }
          )
          (group.rows or []);
      }
    )
    cfg.layout;

  # Helper to convert weather provider string to enum
  weatherProviderToEnum = provider:
    if provider == "metno" then "MetNo"
    else if provider == "openweathermap" then "OpenWeatherMap"
    else "OpenMeteo";

  # Helper to convert units string to enum
  weatherUnitsToEnum = units:
    if units == "imperial" then "Imperial"
    else "Metric";

  # Get weather plugin config or empty
  weatherPluginConfig = cfg.pluginConfigs.weather or {};
  hasWeatherConfig = weatherPluginConfig != {};

  # Generate location variant
  weatherLocation = 
    let
      coords = weatherPluginConfig.coords or null;
      city = weatherPluginConfig.location or null;
    in
      if city != null then
        ''CityName { "${city}" }''
      else if coords != null then
        ''Coordinates { ${toString coords.lat}, ${toString coords.lon} }''
      else
        ''Coordinates { 0.0, 0.0 }'';

  # Generate weather config struct (inside draconis::config namespace)
  weatherConfigCode =
    if hasWeatherConfig then ''
      #include "plugins/weather/WeatherConfig.hpp"
      inline constexpr auto WEATHER_CONFIG = weather::config::MakeConfig(
        weather::config::Provider::${weatherProviderToEnum (weatherPluginConfig.provider or "openmeteo")},
        weather::config::Units::${weatherUnitsToEnum (weatherPluginConfig.units or "metric")},
        weather::config::${weatherLocation}${lib.optionalString (weatherPluginConfig.api_key or "" != "") ",\n        \"${weatherPluginConfig.api_key}\""}
      );
    ''
    else ''
      // No weather plugin configured - using defaults
      #include "plugins/weather/WeatherConfig.hpp"
      inline constexpr auto WEATHER_CONFIG = weather::config::MakeConfig(
        weather::config::Provider::OpenMeteo,
        weather::config::Units::Metric,
        weather::config::Coordinates { 0.0, 0.0 }
      );
    '';
  # Check if weather plugin is in static plugins
  hasWeatherPlugin = builtins.elem "weather" cfg.staticPlugins;

  configHpp =
    pkgs.writeText "config.hpp"
    # cpp
    ''
      #pragma once

      #if DRAC_PRECOMPILED_CONFIG

        #include <array>
        #include <Drac++/Config/PrecompiledLayout.hpp>

        #if DRAC_ENABLE_PACKAGECOUNT
          #include <Drac++/Services/Packages.hpp>
        #endif

      namespace draconis::config {
        constexpr const char* DRAC_USERNAME = "${cfg.username}";

        #if DRAC_ENABLE_PACKAGECOUNT
        constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = ${packageManagerValue};
        #endif

        ${layoutGroupsHppCode}

        ${layoutArrayHppCode}

        ${lib.optionalString hasWeatherPlugin weatherConfigCode}
      }

      #endif
    '';

  packageWithPlugins =
    if cfg.pluginsSrc == null
    then cfg.package
    else if lib.hasAttr "override" cfg.package
    then cfg.package.override {pluginsSrc = cfg.pluginsSrc;}
    else cfg.package;

  draconisWithOverrides = packageWithPlugins.overrideAttrs (oldAttrs: {
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

    pluginsSrc = mkOption {
      type = types.nullOr types.path;
      default = null;
      description = "Path to the draconis++ plugins repository for static plugin builds.";
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
      description = "Logo configuration (used for kitty/kitty-direct/iTerm2 inline images).";
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
            type = types.enum ["kitty" "kitty-direct" "iterm2"];
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

    layout = mkOption {
      type = types.listOf (types.submodule {
        options = {
          name = mkOption {
            type = types.str;
            description = "Display name for the layout group (not rendered, for readability).";
          };

          rows = mkOption {
            type = types.listOf (types.submodule {
              options = {
                key = mkOption {
                  type = types.str;
                  description = "Data key to render (e.g., \"cpu\", \"plugin.weather\").";
                };

                label = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional label override.";
                };

                icon = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional icon override.";
                };

                color = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional value color (matches LogColor enum, e.g., \"Magenta\").";
                };

                autoWrap = mkOption {
                  type = types.bool;
                  default = false;
                  description = "Enable automatic word wrapping for this row.";
                };
              };
            });
            default = [];
            description = "Rows to render within this group.";
          };
        };
      });
      default = defaultLayout;
      description = "UI layout groups and rows for the draconis++ dashboard.";
      example = literalExpression ''
        [
          {
            name = "intro";
            rows = [
              { key = "date"; }
              { key = "plugin.weather"; color = "Magenta"; autoWrap = true; }
            ];
          }
          {
            name = "nowplaying";
            rows = [
              { key = "plugin.now_playing"; color = "Magenta"; autoWrap = true; }
            ];
          }
        ]
      '';
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
              ui = {layout = layoutToml;};
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

  # Compatibility: allow consumers that nest modules under `myconfig` to
  # reference the same options.
  options.myconfig.programs.draconisplusplus =
    mkAliasDefinitions options.programs.draconisplusplus;
}
