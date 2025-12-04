#pragma once

#if !DRAC_PRECOMPILED_CONFIG
  #include <cctype>     // std::tolower
  #include <filesystem> // std::filesystem::path

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

#include <Drac++/Utils/Types.hpp>

namespace draconis::config {
  enum class LogoProtocol : draconis::utils::types::u8 {
    Kitty,
    KittyDirect,
  };

  struct Logo {
    draconis::utils::types::Option<draconis::utils::types::String> imagePath;
    draconis::utils::types::Option<draconis::utils::types::String> protocol; // "kitty" or "kitty-direct"
    draconis::utils::types::Option<draconis::utils::types::u32>    width;
    draconis::utils::types::Option<draconis::utils::types::u32>    height;

    [[nodiscard]] fn getProtocol() const -> LogoProtocol {
      if (!protocol)
        return LogoProtocol::Kitty;
      draconis::utils::types::String protoLower = *protocol;
      std::ranges::transform(protoLower, protoLower.begin(), [](unsigned char chr) -> char { return static_cast<char>(std::tolower(chr)); });
      return protoLower == "kitty-direct" ? LogoProtocol::KittyDirect : LogoProtocol::Kitty;
    }
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
  };

  /**
   * @brief A single row in the UI layout, optionally overriding label/icon.
   */
  struct UILayoutRow {
    draconis::utils::types::String                                 key;                                                  ///< Identifier for the row (e.g., "cpu", "plugin.weather.temp").
    draconis::utils::types::Option<draconis::utils::types::String> label    = std::nullopt;                              ///< Optional label override.
    draconis::utils::types::Option<draconis::utils::types::String> icon     = std::nullopt;                              ///< Optional icon override.
    draconis::utils::logging::LogColor                             color    = draconis::utils::logging::LogColor::White; ///< Value foreground color.
    bool                                                           autoWrap = false;                                     ///< Enable automatic word wrapping based on available width.
  };

  /**
   * @brief A group of rows in the UI layout.
   */
  struct UILayoutGroup {
    draconis::utils::types::String           name;
    draconis::utils::types::Vec<UILayoutRow> rows;
  };

  /**
   * @brief UI-specific configuration.
   */
  struct UI {
    draconis::utils::types::Vec<UILayoutGroup> layout;
  };

#if DRAC_ENABLE_PLUGINS
  /**
   * @struct Plugins
   * @brief Holds configuration settings for plugins.
   */
  struct Plugins {
    draconis::utils::types::Vec<draconis::utils::types::String> autoLoad; ///< List of plugin names to auto-load during initialization.

    bool enabled = true; ///< Flag to enable or disable the plugin system.
  };
#endif // DRAC_ENABLE_PLUGINS

  /**
   * @struct Config
   * @brief Holds the application configuration settings.
   */
  struct Config {
    General general; ///< General configuration settings.
    Logo    logo;
    UI      ui; ///< UI layout configuration.
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
