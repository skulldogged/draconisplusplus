#pragma once

/**
 * @file config.hpp
 * @brief Precompiled weather plugin configuration
 *
 * @details When DRAC_PRECOMPILED_CONFIG is enabled, the weather plugin
 * reads configuration from this file instead of weather.toml.
 *
 * To configure the weather plugin:
 * 1. Set WEATHER_ENABLED to true
 * 2. Choose your WEATHER_PROVIDER (OpenMeteo, MetNo, or OpenWeatherMap)
 * 3. Set your coordinates (WEATHER_LAT, WEATHER_LON) or city (WEATHER_CITY)
 * 4. For OpenWeatherMap, set WEATHER_API_KEY
 * 5. Choose your units (Metric or Imperial)
 *
 * Example configuration for New York with OpenMeteo:
 *   constexpr bool WEATHER_ENABLED = true;
 *   #define WEATHER_PROVIDER weather::Provider::OpenMeteo
 *   constexpr double WEATHER_LAT = 40.7128;
 *   constexpr double WEATHER_LON = -74.0060;
 *   constexpr bool WEATHER_USE_COORDS = true;
 */

#include <string_view>

// Enable or disable weather functionality
constexpr bool WEATHER_ENABLED = true;

// Weather provider: OpenMeteo (free, no API key), MetNo (free, no API key), OpenWeatherMap (requires API key)
// Options: weather::Provider::OpenMeteo, weather::Provider::MetNo, weather::Provider::OpenWeatherMap
#define WEATHER_PROVIDER weather::Provider::OpenMeteo

// Unit system: Metric (Celsius, m/s) or Imperial (Fahrenheit, mph)
// Options: weather::UnitSystem::Metric, weather::UnitSystem::Imperial
#define WEATHER_UNITS weather::UnitSystem::Imperial

// Location configuration - use EITHER coordinates OR city name
// For coordinates (required for OpenMeteo and MetNo):
constexpr double WEATHER_LAT        = 40.7128;  // Latitude (e.g., 40.7128 for New York)
constexpr double WEATHER_LON        = -74.0060; // Longitude (e.g., -74.0060 for New York)
constexpr bool   WEATHER_USE_COORDS = true;     // Set to true to use coordinates

// For city name (OpenWeatherMap only):
constexpr std::string_view WEATHER_CITY;             // City name (e.g., "London,UK" or "New York,US")
constexpr bool             WEATHER_USE_CITY = false; // Set to true to use city name

// API key (required for OpenWeatherMap, ignored for others)
constexpr std::string_view WEATHER_API_KEY;
