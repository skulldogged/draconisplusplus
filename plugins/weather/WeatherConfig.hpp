/**
 * @file WeatherConfig.hpp
 * @brief Typed configuration for the weather plugin (precompiled config mode)
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This header provides type-safe configuration for the weather plugin
 * when using precompiled config mode. The configuration uses:
 * - std::variant for location (either coordinates or city name)
 * - consteval validation to catch config errors at compile time
 *
 * Users define their config in config.hpp, and validation happens automatically
 * in the weather plugin during compilation.
 */

#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace weather::config {
  // Location types - type aliases for cleaner syntax
  using Coordinates = std::pair<double, double>; // {lat, lon}
  using CityName    = std::string_view;

  // Weather providers
  enum class Provider : unsigned char {
    OpenMeteo,      // Free, no API key, coordinates only
    MetNo,          // Free, no API key, coordinates only
    OpenWeatherMap, // Requires API key, supports city names
  };

  // Unit systems
  enum class Units : unsigned char {
    Metric,   // Celsius, m/s
    Imperial, // Fahrenheit, mph
  };

  // Weather plugin configuration with type-safe location
  struct Config {
    Provider                            provider = Provider::OpenMeteo;
    Units                               units    = Units::Metric;
    std::variant<Coordinates, CityName> location = Coordinates { 0.0, 0.0 };
    std::optional<std::string_view>     apiKey; // Only needed for OpenWeatherMap
  };

  // Factory functions to create configs without designated initializers
  // (avoids -Wmissing-designated-field-initializers warning)
  consteval auto MakeConfig(Provider provider, Units units, std::variant<Coordinates, CityName> location) -> Config {
    return { .provider = provider, .units = units, .location = location, .apiKey = std::nullopt };
  }

  consteval auto MakeConfig(Provider provider, Units units, std::variant<Coordinates, CityName> location, std::string_view apiKey) -> Config {
    return { .provider = provider, .units = units, .location = location, .apiKey = apiKey };
  }

  /**
   * @brief Compile-time validation for weather configuration
   * @param cfg The configuration to validate
   * @return true if valid, false otherwise
   *
   * Rules:
   * 1. City names (CityName) only work with OpenWeatherMap
   * 2. OpenWeatherMap requires an API key
   * 3. OpenMeteo and MetNo require coordinates
   */
  consteval auto Validate(const Config& cfg) -> bool {
    // Rule 1: City name is only valid for OpenWeatherMap
    if (std::holds_alternative<CityName>(cfg.location) && cfg.provider != Provider::OpenWeatherMap)
      return false;

    // Rule 2: OpenWeatherMap requires an API key
    if (cfg.provider == Provider::OpenWeatherMap && !cfg.apiKey.has_value())
      return false;

    return true;
  }

} // namespace weather::config
