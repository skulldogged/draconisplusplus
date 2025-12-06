#pragma once

#include <matchit.hpp>     // matchit::{match, is, or_, _}
#include <source_location> // std::source_location

#ifdef _WIN32
  // ReSharper disable once CppUnusedIncludeDirective
  #include <guiddef.h>  // GUID
  #include <winerror.h> // error values
#endif

#include "Types.hpp"

namespace draconis::utils::error {
  /**
   * @enum DracErrorCode
   * @brief Error codes for general OS-level operations.
   */
  enum class DracErrorCode : types::u8 {
    ApiUnavailable,     ///< A required OS service/API is unavailable or failed unexpectedly at runtime.
    ConfigurationError, ///< Configuration or environment issue.
    CorruptedData,      ///< Data present but corrupt or inconsistent.
    InternalError,      ///< An error occurred within the application's OS abstraction code logic.
    InvalidArgument,    ///< An invalid argument was passed to a function or method.
    IoError,            ///< General I/O error (filesystem, pipes, etc.).
    NetworkError,       ///< A network-related error occurred (e.g., DNS resolution, connection failure).
    NotFound,           ///< A required resource (file, registry key, device, API endpoint) was not found.
    NotSupported,       ///< The requested operation is not supported on this platform, version, or configuration.
    Other,              ///< A generic or unclassified error originating from the OS or an external library.
    OutOfMemory,        ///< The system ran out of memory or resources to complete the operation.
    ParseError,         ///< Failed to parse data obtained from the OS (e.g., file content, API output).
    PermissionDenied,   ///< Insufficient permissions to perform the operation.
    PermissionRequired, ///< Operation requires elevated privileges.
    PlatformSpecific,   ///< An unmapped error specific to the underlying OS platform occurred (check message).
    ResourceExhausted,  ///< System resource limit reached (not memory).
    Timeout,            ///< An operation timed out (e.g., waiting for IPC reply).
    UnavailableFeature, ///< Feature not present on this hardware/OS.
  };

  /**
   * @struct DracError
   * @brief Holds structured information about an OS-level error.
   *
   * Used as the error type in Result for many os:: functions.
   */
  struct DracError {
    types::String        message;  ///< A descriptive error message, potentially including platform details.
    std::source_location location; ///< The source location where the error occurred (file, line, function).
    DracErrorCode        code;     ///< The general category of the error.

    DracError(const DracErrorCode errc, types::String msg, const std::source_location& loc = std::source_location::current())
      : message(std::move(msg)), location(loc), code(errc) {}
  };
} // namespace draconis::utils::error

#define ERR(errc, msg)          return ::draconis::utils::types::Err(::draconis::utils::error::DracError(errc, msg))
#define ERR_FROM(err)           return ::draconis::utils::types::Err(::draconis::utils::error::DracError(err))
#define ERR_FMT(errc, fmt, ...) return ::draconis::utils::types::Err(::draconis::utils::error::DracError(errc, std::format(fmt, __VA_ARGS__)))

/**
 * @brief Macro for Rust-style error propagation.
 *
 * Evaluates the given expression (which must return a Result<T, E>).
 * If the result contains an error, it immediately returns from the enclosing
 * function with that error wrapped in Err(). Otherwise, it extracts and yields
 * the success value.
 *
 * @param expr An expression returning a Result<T, E> where T is not void
 * @return The unwrapped success value of type T
 *
 * @note On GCC/Clang, this macro uses GNU statement expressions.
 *       On MSVC, the macro requires declaring the target variable separately.
 *
 * @example
 * @code
 * fn fetchData() -> Result<Data> {
 *   // Instead of:
 *   // auto urlResult = buildUrl();
 *   // if (!urlResult) return Err(urlResult.error());
 *   // String url = *urlResult;
 *
 *   // Simply write:
 *   String url = TRY(buildUrl());
 *
 *   // Chain multiple fallible operations:
 *   auto response = TRY(httpGet(url));
 *   auto parsed = TRY(parseJson(response));
 *   return parsed;
 * }
 * @endcode
 */
#ifdef _MSC_VER
  // Helper macro for concatenation
  #define DRAC_CONCAT_IMPL(a, b) a##b
  #define DRAC_CONCAT(a, b)      DRAC_CONCAT_IMPL(a, b)

  // Simple throw-based unwrap for MSVC - avoids complex template deduction
  #define TRY(expr)            \
    [&]() {                    \
      auto _tmp = (expr);      \
      if (!_tmp)               \
        throw _tmp.error();    \
      return *std::move(_tmp); \
    }()

  // Helper for MSVC: assigns result to variable or returns error (no exceptions)
  #define TRY_RESULT(var, expr)                                                               \
    auto DRAC_CONCAT(_drac_try_result_, __LINE__) = (expr);                                   \
    if (!DRAC_CONCAT(_drac_try_result_, __LINE__))                                            \
      return ::draconis::utils::types::Err(DRAC_CONCAT(_drac_try_result_, __LINE__).error()); \
    (var) = std::move(*DRAC_CONCAT(_drac_try_result_, __LINE__))
#else
  #define TRY(expr)                                                                             \
    _Pragma("clang diagnostic push")                                                            \
      _Pragma("clang diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"")({ \
        auto&& _drac_try_result = (expr);                                                       \
        if (!_drac_try_result)                                                                  \
          return ::draconis::utils::types::Err(_drac_try_result.error());                       \
        std::move(*_drac_try_result);                                                           \
      })                                                                                        \
        _Pragma("clang diagnostic pop")
#endif

/**
 * @brief Macro for Rust-style error propagation with Result<void> types.
 *
 * Evaluates the given expression (which must return a Result<void, E>).
 * If the result contains an error, it immediately returns from the enclosing
 * function with that error wrapped in Err(). Otherwise, execution continues.
 *
 * Use this variant when the Result has no success value (Result<void> or Result<>).
 *
 * @param expr An expression returning a Result<void, E>
 *
 * @note On GCC/Clang, this macro uses GNU statement expressions.
 *       On MSVC, this uses a simple do-while(0) construct.
 *
 * @example
 * @code
 * fn initializeSystem() -> Result<> {
 *   // Propagate errors from void-returning operations:
 *   TRY_VOID(validateConfig());
 *   TRY_VOID(initializeCache());
 *   TRY_VOID(connectToDatabase());
 *   return {};
 * }
 * @endcode
 */
#ifdef _MSC_VER
  #define TRY_VOID(expr)                                                \
    do {                                                                \
      auto&& _drac_try_result = (expr);                                 \
      if (!_drac_try_result)                                            \
        return ::draconis::utils::types::Err(_drac_try_result.error()); \
    } while (0)
#else
  #define TRY_VOID(expr)                                                                        \
    _Pragma("clang diagnostic push")                                                            \
      _Pragma("clang diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"")({ \
        auto&& _drac_try_result = (expr);                                                       \
        if (!_drac_try_result)                                                                  \
          return ::draconis::utils::types::Err(_drac_try_result.error());                       \
      })                                                                                        \
        _Pragma("clang diagnostic pop")
#endif
