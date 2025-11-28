#pragma once

#if DRAC_ENABLE_WEATHER

  #include <unordered_map>

  #include "Drac++/Utils/Types.hpp"

namespace draconis::services::weather::utils {
  namespace types = draconis::utils::types;

  /**
   * @brief Strips time-of-day suffixes (_day, _night, _polartwilight) from a weather symbol code.
   * @param symbol The weather symbol code.
   * @return The symbol code without the time-of-day suffix.
   */
  fn StripTimeOfDayFromSymbol(types::StringView symbol) -> types::String;

  /**
   * @brief Parses an ISO8601 timestamp string (YYYY-MM-DDTHH:MM:SSZ) to a Unix epoch timestamp.
   * @param iso8601 The ISO8601 timestamp string (must be UTC, ending in 'Z').
   * @return A Result containing the epoch timestamp (usize) on success, or a DracError on failure.
   */
  fn ParseIso8601ToEpoch(types::StringView iso8601) -> types::Result<time_t>;

  /**
   * @brief Provides a mapping from MetNo weather symbol codes (after time-of-day stripping) to human-readable descriptions.
   * @return A const reference to an unordered_map of symbol codes to descriptions.
   */
  fn GetMetnoSymbolDescriptions() -> const std::unordered_map<types::StringView, types::StringView>&;

  /**
   * @brief Provides a human-readable description for an OpenMeteo weather code.
   * @param code The OpenMeteo weather code (integer).
   * @return A StringView containing the description. Returns "unknown" if the code is not recognized.
   */
  fn GetOpenmeteoWeatherDescription(types::i32 code) -> types::String;
} // namespace draconis::services::weather::utils

#endif // DRAC_ENABLE_WEATHER