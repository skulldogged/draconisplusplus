#pragma once

#if !DRAC_PRECOMPILED_CONFIG
  #include <cctype>                    // std::tolower
  #include <filesystem>                // std::filesystem::path
  #include <memory>                    // std::{make_unique, unique_ptr}
  #include <ranges>                    // std::ranges::transform
  #include <toml++/impl/node.hpp>      // toml::node
  #include <toml++/impl/node_view.hpp> // toml::node_view
  #include <toml++/impl/table.hpp>     // toml::table

  #include <Drac++/Utils/Logging.hpp>
#endif

#if DRAC_ENABLE_PACKAGECOUNT
  #include <Drac++/Services/Packages.hpp>
#endif

#ifdef _WIN32
  #include <windows.h> // GetUserNameA, DWORD
#else
  #include <pwd.h>    // getpwuid, passwd
  #include <unistd.h> // getuid

  #include <Drac++/Utils/Env.hpp>
#endif

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

namespace draconis::config {
  enum class LogoProtocol : draconis::utils::types::u8 {
    Kitty,
    KittyDirect,
  };

  struct Logo {
    draconis::utils::types::Option<draconis::utils::types::String> imagePath;
    LogoProtocol                                                   protocol = LogoProtocol::Kitty;
    draconis::utils::types::Option<draconis::utils::types::u32>    width;
    draconis::utils::types::Option<draconis::utils::types::u32>    height;

#if !DRAC_PRECOMPILED_CONFIG
    static fn fromToml(const toml::table& tbl) -> Logo {
      using draconis::utils::types::String;
      using draconis::utils::types::u32;

      Logo logo;

      if (const toml::node_view<const toml::node> pathNode = tbl["path"])
        if (auto pathVal = pathNode.value<String>())
          logo.imagePath = *pathVal;

      if (const toml::node_view<const toml::node> protocolNode = tbl["protocol"])
        if (auto protoVal = protocolNode.value<String>()) {
          String protoLower = *protoVal;

          std::ranges::transform(protoLower, protoLower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

          logo.protocol = protoLower == "kitty-direct" ? LogoProtocol::KittyDirect : LogoProtocol::Kitty;
        }

      if (const auto widthNode = tbl["width"])
        if (auto widthVal = widthNode.value<u32>())
          logo.width = *widthVal;

      if (const auto heightNode = tbl["height"])
        if (auto heightVal = heightNode.value<u32>())
          logo.height = *heightVal;

      return logo;
    }
#endif
  };

  /**
   * @struct General
   * @brief Holds general configuration settings.
   */
  struct General {
    mutable draconis::utils::types::Option<draconis::utils::types::String> name;     ///< Display name; resolved lazily via getDefaultName() when needed.
    draconis::utils::types::Option<draconis::utils::types::String>         language; ///< Language code for localization (e.g., "en", "es", "fr")

    /**
     * @brief Retrieves the default name for the user.
     * @return The default name for the user, either from the system or a fallback.
     *
     * Retrieves the default name for the user based on the operating system.
     * On Windows, it uses GetUserNameA to get the username.
     * On POSIX systems, it first tries to get the username using getpwuid,
     * then checks the USER and LOGNAME environment variables.
     */
    static fn getDefaultName() -> draconis::utils::types::String {
#ifdef _WIN32
      using draconis::utils::types::Array;

      Array<char, 256> username {};

      unsigned long size = username.size();

      return GetUserNameA(username.data(), &size) ? username.data() : "User";
#else
      using draconis::utils::env::GetEnv;
      using draconis::utils::types::PCStr, draconis::utils::types::String, draconis::utils::types::Result;

      info_log("Getting default name from system");

      const passwd*        pwd        = getpwuid(getuid());
      PCStr                pwdName    = pwd ? pwd->pw_name : nullptr;
      const Result<String> envUser    = GetEnv("USER");
      const Result<String> envLogname = GetEnv("LOGNAME");

      return pwdName ? pwdName
        : envUser    ? *envUser
        : envLogname ? *envLogname
                     : "User";
#endif // _WIN32
    }

    fn getName() const -> const draconis::utils::types::String& {
      if (!name)
        name = getDefaultName();
      return *name;
    }

#if !DRAC_PRECOMPILED_CONFIG
    /**
     * @brief Parses a TOML table to create a General instance.
     * @param tbl The TOML table to parse, containing [general].
     * @return A General instance with the parsed values, or defaults otherwise.
     */
    static fn fromToml(const toml::table& tbl) -> General {
      using draconis::utils::types::String;

      General gen;

      if (const toml::node_view<const toml::node> nameNode = tbl["name"])
        if (auto nameVal = nameNode.value<String>())
          gen.name = *nameVal;

      if (const toml::node_view<const toml::node> langNode = tbl["language"])
        if (auto langVal = langNode.value<String>())
          gen.language = *langVal;

      return gen;
    }
#endif // DRAC_PRECOMPILED_CONFIG
  };

#if DRAC_ENABLE_NOWPLAYING
  /**
   * @struct NowPlaying
   * @brief Holds configuration settings for the Now Playing feature.
   */
  struct NowPlaying {
    bool enabled = true; ///< Flag to enable or disable the Now Playing feature.

  #if !DRAC_PRECOMPILED_CONFIG
    /**
     * @brief Parses a TOML table to create a NowPlaying instance.
     * @param tbl The TOML table to parse, containing [now_playing].
     * @return A NowPlaying instance with the parsed values, or defaults otherwise.
     */
    static fn fromToml(const toml::table& tbl) -> NowPlaying {
      return { .enabled = tbl["enabled"].value_or(true) };
    }
  #endif
  };
#endif // DRAC_ENABLE_NOWPLAYING

#if DRAC_ENABLE_PLUGINS
  /**
   * @struct Plugins
   * @brief Holds configuration settings for plugins.
   */
  struct Plugins {
    draconis::utils::types::Vec<draconis::utils::types::String> autoLoad; ///< List of plugin names to auto-load during initialization.

    bool enabled = true; ///< Flag to enable or disable the plugin system.

  #if !DRAC_PRECOMPILED_CONFIG
    /**
     * @brief Parses a TOML table to create a Plugins instance.
     * @param tbl The TOML table to parse, containing [plugins].
     * @return A Plugins instance with the parsed values, or defaults otherwise.
     */
    static fn fromToml(const toml::table& tbl) -> Plugins {
      using draconis::utils::types::String, draconis::utils::types::Vec;

      Plugins plugins;

      plugins.enabled = tbl["enabled"].value_or(true);

      if (const toml::node_view<const toml::node> autoLoadNode = tbl["auto_load"]) {
        if (auto arr = autoLoadNode.as_array()) {
          for (const auto& elem : *arr) {
            if (auto valOpt = elem.value<String>()) {
              plugins.autoLoad.emplace_back(*valOpt);
            }
          }
        }
      }

      return plugins;
    }
  #endif // DRAC_PRECOMPILED_CONFIG
  };
#endif // DRAC_ENABLE_PLUGINS

  /**
   * @struct Config
   * @brief Holds the application configuration settings.
   */
  struct Config {
    General general; ///< General configuration settings.
    Logo    logo;
#if DRAC_ENABLE_NOWPLAYING
    NowPlaying nowPlaying; ///< Now Playing configuration settings.
#endif
#if DRAC_ENABLE_PACKAGECOUNT
    draconis::services::packages::Manager enabledPackageManagers; ///< Enabled package managers.
#endif
#if DRAC_ENABLE_PLUGINS
    Plugins plugins; ///< Plugin configuration settings.
#endif

    /**
     * @brief Default constructor for Config.
     */
    Config() = default;

#if !DRAC_PRECOMPILED_CONFIG
    /**
     * @brief Constructs a Config instance from a TOML table.
     * @param tbl The TOML table to parse, containing [general], [weather], and [now_playing].
     */
    explicit Config(const toml::table& tbl);
#endif

    /**
     * @brief Retrieves the path to the configuration file.
     * @return The path to the configuration file.
     *
     * This function constructs the path to the configuration file based on
     * the operating system and user directory. It returns a std::filesystem::path
     * object representing the configuration file path.
     */
    static fn getInstance() -> Config;

#if !DRAC_PRECOMPILED_CONFIG
    /**
     * @brief Gets the path to the configuration file without loading it.
     * @return The path to the configuration file.
     */
    static fn getConfigPath() -> std::filesystem::path;
#endif
  };
} // namespace draconis::config
