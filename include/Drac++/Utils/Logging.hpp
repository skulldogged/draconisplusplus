#pragma once

#include <chrono>     // std::chrono::{days, floor, seconds, system_clock}
#include <ctime>      // localtime_r/s, strftime, time_t, tm
#include <filesystem> // std::filesystem::path
#include <format>     // std::format
#include <utility>    // std::forward

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <fcntl.h>
  #include <io.h>
  #include <windows.h>
#endif

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout, std::cerr
#endif

#ifndef NDEBUG
  #include <source_location> // std::source_location
#endif

#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::logging {
  namespace types = ::draconis::utils::types;

  inline auto GetLogMutex() -> types::Mutex& {
    static types::Mutex LogMutexInstance;
    return LogMutexInstance;
  }

  /**
   * @brief Helper to write to console handling Windows specifics
   * @param text The text to write
   * @param useStderr Whether to write to stderr instead of stdout
   */
  inline auto WriteToConsole(const types::StringView text, bool useStderr = false) -> void {
#ifdef _WIN32
    HANDLE hOutput = GetStdHandle(useStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (hOutput != INVALID_HANDLE_VALUE) {
      // Check if output is redirected (not a console)
      // GetConsoleMode fails for redirected handles (files, pipes)
      DWORD consoleMode = 0;
      if (GetConsoleMode(hOutput, &consoleMode)) {
        // Output is a console - use WriteConsoleA for proper Unicode/encoding support
        WriteConsoleA(hOutput, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
      } else {
        // Output is redirected (file/pipe) - use WriteFile for proper redirection support
        WriteFile(hOutput, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
      }
      return;
    }
#endif

#ifdef __cpp_lib_print
    if (useStderr)
      std::print(stderr, "{}", text);
    else
      std::print("{}", text);
#else
    if (useStderr)
      std::cerr << text;
    else
      std::cout << text;
#endif
  }

  enum class LogColor : types::u8 {
    Black         = 0,
    Red           = 1,
    Green         = 2,
    Yellow        = 3,
    Blue          = 4,
    Magenta       = 5,
    Cyan          = 6,
    White         = 7,
    Gray          = 8,
    BrightRed     = 9,
    BrightGreen   = 10,
    BrightYellow  = 11,
    BrightBlue    = 12,
    BrightMagenta = 13,
    BrightCyan    = 14,
    BrightWhite   = 15,
  };

  struct LogLevelConst {
    // clang-format off
    static constexpr types::Array<types::StringView, 16> COLOR_CODE_LITERALS = {
      "\033[38;5;0m",  "\033[38;5;1m",  "\033[38;5;2m",  "\033[38;5;3m",
      "\033[38;5;4m",  "\033[38;5;5m",  "\033[38;5;6m",  "\033[38;5;7m",
      "\033[38;5;8m",  "\033[38;5;9m",  "\033[38;5;10m", "\033[38;5;11m",
      "\033[38;5;12m", "\033[38;5;13m", "\033[38;5;14m", "\033[38;5;15m",
    };
    // clang-format on

    static constexpr const char* RESET_CODE   = "\033[0m";
    static constexpr const char* BOLD_START   = "\033[1m";
    static constexpr const char* BOLD_END     = "\033[22m";
    static constexpr const char* ITALIC_START = "\033[3m";
    static constexpr const char* ITALIC_END   = "\033[23m";

    // Pre-formatted level strings with ANSI codes (bold + color + text + reset)
    // These are string literals with static storage duration - never destroyed
    // Format: BOLD_START + COLOR + TEXT + RESET_CODE
    static constexpr types::StringView DEBUG_STYLED = "\033[1m\033[38;5;6mDEBUG\033[0m"; // Cyan
    static constexpr types::StringView INFO_STYLED  = "\033[1m\033[38;5;2mINFO \033[0m"; // Green
    static constexpr types::StringView WARN_STYLED  = "\033[1m\033[38;5;3mWARN \033[0m"; // Yellow
    static constexpr types::StringView ERROR_STYLED = "\033[1m\033[38;5;1mERROR\033[0m"; // Red

    static constexpr types::PCStr TIMESTAMP_FORMAT = "%X";
    static constexpr types::PCStr LOG_FORMAT       = "{} {} {}";

#ifndef NDEBUG
    static constexpr types::PCStr DEBUG_INFO_FORMAT = "{}{}{}\n";
    static constexpr types::PCStr FILE_LINE_FORMAT  = "{}:{}";
    static constexpr types::PCStr DEBUG_LINE_PREFIX = "           ╰──── ";
#endif
  };

  /**
   * @enum LogLevel
   * @brief Represents different log levels.
   */
  enum class LogLevel : types::u8 {
    Debug,
    Info,
    Warn,
    Error,
  };

  /**
   * @brief Gets a reference to the shared log level pointer storage.
   * @details Using a function with static local avoids global variable warnings
   *          while maintaining the same semantics for cross-DLL log level sharing.
   */
  inline auto GetLogLevelPtrStorage() -> LogLevel*& {
    static LogLevel* Ptr = nullptr;
    return Ptr;
  }

  /**
   * @brief Gets a reference to the local log level storage.
   * @details Used as fallback when no shared pointer is set.
   */
  inline auto GetLocalLogLevel() -> LogLevel& {
    static LogLevel Level = LogLevel::Info;
    return Level;
  }

  /**
   * @brief Sets the log level pointer for plugin support.
   * @details Called by the plugin manager to share the main executable's log level with plugins.
   * @param ptr Pointer to the main executable's log level storage.
   */
  inline auto SetLogLevelPtr(LogLevel* ptr) -> void {
    GetLogLevelPtrStorage() = ptr;
  }

  /**
   * @brief Gets a pointer to the log level storage owned by this module.
   * @details Used by the main executable to get its log level address to share with plugins.
   * @return Pointer to the local log level storage.
   */
  inline auto GetLogLevelPtr() -> LogLevel* {
    return &GetLocalLogLevel();
  }

  /**
   * @brief Gets the current runtime log level.
   * @return Reference to the current log level.
   */
  inline auto GetRuntimeLogLevel() -> LogLevel& {
    if (LogLevel* ptr = GetLogLevelPtrStorage())
      return *ptr;

    return GetLocalLogLevel();
  }

  /**
   * @brief Sets the runtime log level.
   * @param level The new log level to set.
   */
  inline auto SetRuntimeLogLevel(const LogLevel level) {
    if (LogLevel* ptr = GetLogLevelPtrStorage())
      *ptr = level;
    else
      GetLocalLogLevel() = level;
  }

  /**
   * @struct Style
   * @brief Options for text styling with ANSI codes.
   */
  struct Style {
    LogColor color  = LogColor::White; ///< Optional color to apply
    bool     bold   = false;           ///< Whether to make text bold
    bool     italic = false;           ///< Whether to make text italic
  };

  /**
   * @brief Applies ANSI styling to text based on the provided style options.
   * @param text The text to style
   * @param style The style options (color, bold, italic)
   * @return Styled string with ANSI codes
   */
  inline auto Stylize(const types::StringView text, const Style& style) -> types::String {
    const bool hasStyle = style.bold || style.italic || style.color != LogColor::White;

    if (!hasStyle)
      return types::String(text);

    types::String result;
    result.reserve(text.size() + 24); // Pre-allocate for ANSI codes

    if (style.bold)
      result += LogLevelConst::BOLD_START;
    if (style.italic)
      result += LogLevelConst::ITALIC_START;
    if (style.color != LogColor::White)
      result += LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<types::usize>(style.color));

    result += text;
    result += LogLevelConst::RESET_CODE;

    return result;
  }

  /**
   * @brief Returns the pre-formatted and styled log level strings.
   * @note Uses constexpr StringViews pointing to string literals, which have
   *       static storage duration and are never destroyed - safe for use
   *       during static destruction.
   */
  constexpr auto GetLevelInfo() -> const types::Array<types::StringView, 4>& {
    static constexpr types::Array<types::StringView, 4> LEVEL_INFO_INSTANCE = {
      LogLevelConst::DEBUG_STYLED,
      LogLevelConst::INFO_STYLED,
      LogLevelConst::WARN_STYLED,
      LogLevelConst::ERROR_STYLED,
    };

    return LEVEL_INFO_INSTANCE;
  }

  /**
   * @brief Returns whether a log level should use stderr
   * @param level The log level
   * @return true if the level should use stderr, false for stdout
   */
  constexpr auto ShouldUseStderr(const LogLevel level) -> bool {
    return level == LogLevel::Warn || level == LogLevel::Error;
  }

  /**
   * @brief Helper function to print formatted text with automatic std::print/std::cout selection
   * @tparam Args Parameter pack for format arguments
   * @param level The log level to determine output stream
   * @param fmt The format string
   * @param args The arguments for the format string
   */
  template <typename... Args>
  inline auto Print(const LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...), ShouldUseStderr(level));
  }

  /**
   * @brief Helper function to print pre-formatted text with automatic std::print/std::cout selection
   * @param level The log level to determine output stream
   * @param text The pre-formatted text to print
   */
  inline auto Print(const LogLevel level, const types::StringView text) {
    WriteToConsole(text, ShouldUseStderr(level));
  }

  /**
   * @brief Helper function to print formatted text with newline with automatic std::print/std::cout selection
   * @tparam Args Parameter pack for format arguments
   * @param level The log level to determine output stream
   * @param fmt The format string
   * @param args The arguments for the format string
   */
  template <typename... Args>
  inline auto Println(const LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...) + '\n', ShouldUseStderr(level));
  }

  /**
   * @brief Helper function to print text with newline with automatic std::print/std::cout selection
   * @param level The log level to determine output stream
   * @param text The text to print
   */
  inline auto Println(const LogLevel level, const types::StringView text) {
    // We need to construct a string with newline because WriteToConsole doesn't add it
    // and we want a single atomic write if possible (though WriteToConsole isn't atomic across calls)
    types::String textWithNewline(text);
    textWithNewline += '\n';
    WriteToConsole(textWithNewline, ShouldUseStderr(level));
  }

  /**
   * @brief Helper function to print just a newline with automatic std::print/std::cout selection
   * @param level The log level to determine output stream
   */
  inline auto Println(const LogLevel level) {
    WriteToConsole("\n", ShouldUseStderr(level));
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // User-Facing Output Functions (no log level, stdout only)
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Print a formatted message to stdout (user-facing output, not logging)
   * @tparam Args Format argument types
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  inline auto Print(std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...));
  }

  /**
   * @brief Print a string to stdout (user-facing output, not logging)
   * @param text The text to print
   */
  inline auto Print(const types::StringView text) {
    WriteToConsole(text);
  }

  /**
   * @brief Print a formatted message with newline to stdout (user-facing output, not logging)
   * @tparam Args Format argument types
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  inline auto Println(std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...) + '\n');
  }

  /**
   * @brief Print a string with newline to stdout (user-facing output, not logging)
   * @param text The text to print
   */
  inline auto Println(const types::StringView text) {
    types::String textWithNewline(text);
    textWithNewline += '\n';
    WriteToConsole(textWithNewline);
  }

  /**
   * @brief Print just a newline to stdout (user-facing output, not logging)
   */
  inline auto Println() {
    WriteToConsole("\n");
  }

  /**
   * @brief Returns a HH:MM:SS timestamp string for the provided epoch time.
   *        The value is cached per-thread and only recomputed when the seconds
   *        value changes, greatly reducing the cost when many log calls land
   *        in the same second.
   * @param tt The epoch time (seconds since epoch).
   * @return StringView pointing to a thread-local null-terminated buffer.
   */
  inline auto GetCachedTimestamp(const std::time_t timeT) -> types::StringView {
    thread_local auto                  LastTt   = static_cast<std::time_t>(-1);
    thread_local types::Array<char, 9> TsBuffer = { '\0' };

    if (timeT != LastTt) {
      std::tm localTm {};

      if (
#ifdef _WIN32
        localtime_s(&localTm, &timeT) == 0
#else
        localtime_r(&timeT, &localTm) != nullptr
#endif
      ) {
        if (std::strftime(TsBuffer.data(), TsBuffer.size(), LogLevelConst::TIMESTAMP_FORMAT, &localTm) == 0)
          std::copy_n("??:??:??", 9, TsBuffer.data());
      } else
        std::copy_n("??:??:??", 9, TsBuffer.data()); // fallback

      LastTt = timeT;
    }

    return { TsBuffer.data(), 8 };
  }

  /**
   * @brief Logs a message with the specified log level, source location, and format string.
   * @tparam Args Parameter pack for format arguments.
   * @param level The log level (DEBUG, INFO, WARN, ERROR).
   * @param loc The source location of the log message (only in Debug builds).
   * @param fmt The format string.
   * @param args The arguments for the format string.
   */
  template <typename... Args>
  auto LogImpl(
    const LogLevel level,
#ifndef NDEBUG
    const std::source_location& loc,
#endif
    std::format_string<Args...> fmt,
    Args&&... args
  ) {
    using namespace std::chrono;
    using std::filesystem::path;

    if (level < GetRuntimeLogLevel())
      return;

    const auto        nowTp = system_clock::now();
    const std::time_t nowTt = system_clock::to_time_t(nowTp);

    const types::StringView timestamp = GetCachedTimestamp(nowTt);

    const types::String message          = std::format(fmt, std::forward<Args>(args)...);
    const types::String coloredTimestamp = Stylize(std::format("[{}]", timestamp), { .color = LogColor::White });

#ifndef NDEBUG
    const types::String fileLine      = std::format(LogLevelConst::FILE_LINE_FORMAT, path(loc.file_name()).lexically_normal().string(), loc.line());
    const types::String fullDebugLine = std::format("{}{}", LogLevelConst::DEBUG_LINE_PREFIX, fileLine);
#endif

    {
      const types::LockGuard lock(GetLogMutex());

      Println(
        level,
        LogLevelConst::LOG_FORMAT,
        coloredTimestamp,
        GetLevelInfo().at(static_cast<types::usize>(level)),
        message
      );

#ifndef NDEBUG
      Print(level, Stylize(fullDebugLine, { .color = LogColor::White, .italic = true }));
      Println(level, LogLevelConst::RESET_CODE);
#else
      Print(level, LogLevelConst::RESET_CODE);
#endif
    }
  }

  template <typename ErrorType>
  auto LogError(const LogLevel level, const ErrorType& error_obj) {
    using DecayedErrorType = std::decay_t<ErrorType>;

#ifndef NDEBUG
    std::source_location logLocation;
#endif

    types::String errorMessagePart;

    if constexpr (std::is_same_v<DecayedErrorType, error::DracError>) {
#ifndef NDEBUG
      logLocation = error_obj.location;
#endif
      errorMessagePart = error_obj.message;
    } else {
#ifndef NDEBUG
      logLocation = std::source_location::current();
#endif
      if constexpr (std::is_base_of_v<std::exception, DecayedErrorType>)
        errorMessagePart = error_obj.what();
      else if constexpr (requires { error_obj.message; })
        errorMessagePart = error_obj.message;
      else
        errorMessagePart = "Unknown error type logged";
    }

#ifndef NDEBUG
    LogImpl(level, logLocation, "{}", errorMessagePart);
#else
    LogImpl(level, "{}", errorMessagePart);
#endif
  }

#define debug_at(error_obj) ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Debug, error_obj)
#define info_at(error_obj)  ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Info, error_obj)
#define warn_at(error_obj)  ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Warn, error_obj)
#define error_at(error_obj) ::draconis::utils::logging::LogError(::draconis::utils::logging::LogLevel::Error, error_obj)

#ifdef NDEBUG
  #define debug_log(fmt, ...) ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Debug, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...)  ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Info, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...)  ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Warn, fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Error, fmt __VA_OPT__(, ) __VA_ARGS__)
#else
  #define debug_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define info_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Info, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define warn_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Warn, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
  #define error_log(fmt, ...) \
    ::draconis::utils::logging::LogImpl(::draconis::utils::logging::LogLevel::Error, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)
#endif
} // namespace draconis::utils::logging
