#pragma once

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"

namespace draconis::ui {
  namespace types   = ::draconis::utils::types;
  namespace logging = ::draconis::utils::logging;
  namespace config  = ::draconis::config;
  namespace system  = ::draconis::core::system;

  struct Theme {
    logging::LogColor icon;
    logging::LogColor label;
    logging::LogColor value;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    types::StringView calendar;
    types::StringView desktopEnvironment;
    types::StringView disk;
    types::StringView host;
    types::StringView kernel;
    types::StringView memory;
    types::StringView cpu;
    types::StringView gpu;
    types::StringView uptime;
#if DRAC_ENABLE_NOWPLAYING
    types::StringView music;
#endif
    types::StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    types::StringView package;
#endif
    types::StringView palette;
    types::StringView shell;
    types::StringView user;
    types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data.
   * @param noAscii Whether to disable ASCII art.
   * @return A string containing the formatted UI.
   */
  fn CreateUI(const config::Config& config, const system::SystemInfo& data, bool noAscii) -> types::String;
} // namespace draconis::ui
