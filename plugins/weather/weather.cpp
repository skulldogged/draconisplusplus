/**
 * @file weather.cpp
 * @brief Weather information provider plugin for Draconis++
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin provides weather information from multiple providers:
 * - OpenMeteo (no API key required, coordinates only)
 * - Met.no (no API key required, coordinates only)
 * - OpenWeatherMap (API key required, supports city names)
 *
 * Configuration is read from:
 * - Runtime mode: ~/.config/draconis++/plugins/weather.toml
 * - Precompiled mode: plugins/weather/config.hpp
 *
 * This is a single-file plugin that combines all functionality for static plugin support.
 */

#include <curl/curl.h>
#include <filesystem>
#include <format>
#include <glaze/glaze.hpp>
#include <matchit.hpp>
#include <unordered_map>
#include <utility>

#if !DRAC_PRECOMPILED_CONFIG
  #include <toml++/toml.hpp>
#endif

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

namespace fs = std::filesystem;

using namespace draconis::core::plugin;
using namespace draconis::utils::types;
using namespace draconis::utils::error;
using namespace draconis::utils::logging;
using enum DracErrorCode;

namespace weather {
  /**
   * @brief Specifies the weather service provider.
   */
  enum class Provider : u8 {
    OpenWeatherMap,
    OpenMeteo,
    MetNo,
  };

  /**
   * @brief Specifies the unit system for weather information.
   */
  enum class UnitSystem : u8 {
    Metric,
    Imperial,
  };

  /**
   * @brief Geographic coordinates
   */
  struct Coords {
    f64 lat;
    f64 lon;
  };

  /**
   * @brief Weather report data
   */
  struct WeatherData {
    Option<f64>    temperature;
    Option<String> description;
    Option<String> location;
    UnitSystem     units = UnitSystem::Metric;
  };

  /**
   * @brief Plugin configuration
   */
  struct WeatherConfig {
    bool           enabled  = false;
    Provider       provider = Provider::OpenMeteo;
    UnitSystem     units    = UnitSystem::Metric;
    Option<Coords> coords;
    Option<String> city;
    Option<String> apiKey;
  };
} // namespace weather

// Include precompiled config after weather types are defined
#if DRAC_PRECOMPILED_CONFIG
  #include "config.hpp"
#endif

// DTO namespaces for API responses
namespace weather::dto {
  namespace metno {
    struct Details {
      f64 airTemperature;
    };

    struct Next1hSummary {
      String symbolCode;
    };

    struct Next1h {
      Next1hSummary summary;
    };

    struct Instant {
      Details details;
    };

    struct Data {
      Instant        instant;
      Option<Next1h> next1Hours;
    };

    struct Timeseries {
      String time;
      Data   data;
    };

    struct Properties {
      Vec<Timeseries> timeseries;
    };

    struct Response {
      Properties properties;
    };
  } // namespace metno

  namespace openmeteo {
    struct Response {
      struct Current {
        f64    temperature;
        i32    weathercode;
        String time;
      } currentWeather;
    };
  } // namespace openmeteo

  namespace owm {
    struct OWMResponse {
      struct Main {
        f64 temp;
      };

      struct Weather {
        String description;
      };

      Main           main;
      Vec<Weather>   weather;
      String         name;
      i64            dt;
      Option<i32>    cod;
      Option<String> message;
    };
  } // namespace owm
} // namespace weather::dto

namespace glz {
  template <>
  struct meta<weather::WeatherData> {
    using T                     = weather::WeatherData;
    static constexpr auto value = object(
      "temperature",
      &T::temperature,
      "description",
      &T::description,
      "location",
      &T::location,
      "units",
      &T::units
    );
  };

  template <>
  struct meta<weather::Coords> {
    using T                     = weather::Coords;
    static constexpr auto value = object("lat", &T::lat, "lon", &T::lon);
  };

  template <>
  struct meta<weather::dto::metno::Details> {
    static constexpr auto value = object("air_temperature", &weather::dto::metno::Details::airTemperature);
  };

  template <>
  struct meta<weather::dto::metno::Next1hSummary> {
    static constexpr auto value = object("symbol_code", &weather::dto::metno::Next1hSummary::symbolCode);
  };

  template <>
  struct meta<weather::dto::metno::Next1h> {
    static constexpr auto value = object("summary", &weather::dto::metno::Next1h::summary);
  };

  template <>
  struct meta<weather::dto::metno::Instant> {
    static constexpr auto value = object("details", &weather::dto::metno::Instant::details);
  };

  template <>
  struct meta<weather::dto::metno::Data> {
    static constexpr auto value = object(
      "instant",
      &weather::dto::metno::Data::instant,
      "next_1_hours",
      &weather::dto::metno::Data::next1Hours
    );
  };

  template <>
  struct meta<weather::dto::metno::Timeseries> {
    static constexpr auto value = object(
      "time",
      &weather::dto::metno::Timeseries::time,
      "data",
      &weather::dto::metno::Timeseries::data
    );
  };

  template <>
  struct meta<weather::dto::metno::Properties> {
    static constexpr auto value = object("timeseries", &weather::dto::metno::Properties::timeseries);
  };

  template <>
  struct meta<weather::dto::metno::Response> {
    static constexpr auto value = object("properties", &weather::dto::metno::Response::properties);
  };

  template <>
  struct meta<weather::dto::openmeteo::Response::Current> {
    static constexpr auto value = object(
      "temperature",
      &weather::dto::openmeteo::Response::Current::temperature,
      "weathercode",
      &weather::dto::openmeteo::Response::Current::weathercode,
      "time",
      &weather::dto::openmeteo::Response::Current::time
    );
  };

  template <>
  struct meta<weather::dto::openmeteo::Response> {
    static constexpr auto value = object("current_weather", &weather::dto::openmeteo::Response::currentWeather);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse::Main> {
    static constexpr auto value = object("temp", &weather::dto::owm::OWMResponse::Main::temp);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse::Weather> {
    static constexpr auto value = object("description", &weather::dto::owm::OWMResponse::Weather::description);
  };

  template <>
  struct meta<weather::dto::owm::OWMResponse> {
    static constexpr auto value = object(
      "main",
      &weather::dto::owm::OWMResponse::main,
      "weather",
      &weather::dto::owm::OWMResponse::weather,
      "name",
      &weather::dto::owm::OWMResponse::name,
      "dt",
      &weather::dto::owm::OWMResponse::dt,
      "cod",
      &weather::dto::owm::OWMResponse::cod,
      "message",
      &weather::dto::owm::OWMResponse::message
    );
  };
} // namespace glz

namespace weather::curl {
  struct EasyOptions {
    Option<String> url                = None;
    String*        writeBuffer        = nullptr;
    Option<i64>    timeoutSecs        = None;
    Option<i64>    connectTimeoutSecs = None;
    Option<String> userAgent          = None;
  };

  class Easy {
    CURL*             m_curl      = nullptr;
    Option<DracError> m_initError = None;

    static fn writeCallback(RawPointer contents, const usize size, const usize nmemb, String* str) -> usize {
      const usize totalSize = size * nmemb;
      str->append(static_cast<char*>(contents), totalSize);
      return totalSize;
    }

   public:
    Easy() : m_curl(curl_easy_init()) {
      if (!m_curl)
        m_initError = DracError(ApiUnavailable, "curl_easy_init() failed");
    }

    explicit Easy(const EasyOptions& options) : m_curl(curl_easy_init()) {
      if (!m_curl) {
        m_initError = DracError(ApiUnavailable, "curl_easy_init() failed");
        return;
      }

      if (options.url)
        if (Result<> res = setUrl(*options.url); !res) {
          m_initError = res.error();
          return;
        }

      if (options.writeBuffer)
        if (Result<> res = setWriteFunction(options.writeBuffer); !res) {
          m_initError = res.error();
          return;
        }

      if (options.timeoutSecs)
        if (Result<> res = setTimeout(*options.timeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.connectTimeoutSecs)
        if (Result<> res = setConnectTimeout(*options.connectTimeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.userAgent)
        if (Result<> res = setUserAgent(*options.userAgent); !res) {
          m_initError = res.error();
          return;
        }
    }

    ~Easy() {
      if (m_curl)
        curl_easy_cleanup(m_curl);
    }

    Easy(const Easy&)                = delete;
    fn operator=(const Easy&)->Easy& = delete;

    Easy(Easy&& other) noexcept
      : m_curl(std::exchange(other.m_curl, nullptr)), m_initError(std::move(other.m_initError)) {}

    fn operator=(Easy&& other) noexcept -> Easy& {
      if (this != &other) {
        if (m_curl)
          curl_easy_cleanup(m_curl);
        m_curl      = std::exchange(other.m_curl, nullptr);
        m_initError = std::move(other.m_initError);
      }
      return *this;
    }

    [[nodiscard]] explicit operator bool() const {
      return m_curl != nullptr && !m_initError;
    }

    [[nodiscard]] fn getInitializationError() const -> const Option<DracError>& {
      return m_initError;
    }

    [[nodiscard]] fn get() const -> CURL* {
      return m_curl;
    }

    template <typename T>
    fn setOpt(const CURLoption option, T value) -> Result<> {
      if (!m_curl)
        ERR(InternalError, "CURL handle is not initialized");
      if (m_initError)
        ERR(InternalError, "CURL handle initialization previously failed");
      if (const CURLcode res = curl_easy_setopt(m_curl, option, value); res != CURLE_OK)
        ERR_FMT(PlatformSpecific, "curl_easy_setopt failed: {}", curl_easy_strerror(res));
      return {};
    }

    fn perform() -> Result<> {
      if (!m_curl)
        ERR(InternalError, "CURL handle is not initialized");
      if (m_initError)
        ERR_FMT(InternalError, "CURL init failed: {}", m_initError->message);
      if (const CURLcode res = curl_easy_perform(m_curl); res != CURLE_OK)
        ERR_FMT(ApiUnavailable, "curl_easy_perform failed: {}", curl_easy_strerror(res));
      return {};
    }

    static fn escape(const String& url) -> Result<String> {
      char* escapedUrl = curl_easy_escape(nullptr, url.c_str(), static_cast<i32>(url.length()));
      if (!escapedUrl)
        ERR(OutOfMemory, "curl_easy_escape failed");
      String result(escapedUrl);
      curl_free(escapedUrl);
      return result;
    }

    fn setUrl(const String& url) -> Result<> {
      return setOpt(CURLOPT_URL, url.c_str());
    }

    fn setWriteFunction(String* buffer) -> Result<> {
      if (!buffer)
        ERR(InvalidArgument, "Write buffer cannot be null");
      if (Result<> res = setOpt(CURLOPT_WRITEFUNCTION, writeCallback); !res)
        return res;
      return setOpt(CURLOPT_WRITEDATA, buffer);
    }

    fn setTimeout(const i64 timeout) -> Result<> {
      return setOpt(CURLOPT_TIMEOUT, timeout);
    }
    fn setConnectTimeout(const i64 timeout) -> Result<> {
      return setOpt(CURLOPT_CONNECTTIMEOUT, timeout);
    }
    fn setUserAgent(const String& userAgent) -> Result<> {
      return setOpt(CURLOPT_USERAGENT, userAgent.c_str());
    }
  };
} // namespace weather::curl

// ============================================================================
// Weather Providers
// ============================================================================

namespace weather::providers {
  /**
   * @brief Interface for weather providers
   */
  class IWeatherProvider {
   public:
    IWeatherProvider()                                       = default;
    virtual ~IWeatherProvider()                              = default;
    IWeatherProvider(const IWeatherProvider&)                = delete;
    fn operator=(const IWeatherProvider&)->IWeatherProvider& = delete;
    IWeatherProvider(IWeatherProvider&&)                     = default;
    fn operator=(IWeatherProvider&&)->IWeatherProvider&      = default;

    virtual fn fetch() -> Result<WeatherData> = 0;
  };

  namespace {
    fn GetMetnoSymbolDescriptions() -> const std::unordered_map<StringView, StringView>& {
      static const std::unordered_map<StringView, StringView> MAP = {
        {             "clearsky",               "clear sky" },
        {                 "fair",                    "fair" },
        {         "partlycloudy",           "partly cloudy" },
        {               "cloudy",                  "cloudy" },
        {                  "fog",                     "fog" },
        {            "lightrain",              "light rain" },
        {     "lightrainshowers",      "light rain showers" },
        {  "lightrainandthunder",  "light rain and thunder" },
        {                 "rain",                    "rain" },
        {          "rainshowers",            "rain showers" },
        {       "rainandthunder",        "rain and thunder" },
        {            "heavyrain",              "heavy rain" },
        {     "heavyrainshowers",      "heavy rain showers" },
        {  "heavyrainandthunder",  "heavy rain and thunder" },
        {           "lightsleet",             "light sleet" },
        {    "lightsleetshowers",     "light sleet showers" },
        { "lightsleetandthunder", "light sleet and thunder" },
        {                "sleet",                   "sleet" },
        {         "sleetshowers",           "sleet showers" },
        {      "sleetandthunder",       "sleet and thunder" },
        {           "heavysleet",             "heavy sleet" },
        {    "heavysleetshowers",     "heavy sleet showers" },
        { "heavysleetandthunder", "heavy sleet and thunder" },
        {            "lightsnow",              "light snow" },
        {     "lightsnowshowers",      "light snow showers" },
        {  "lightsnowandthunder",  "light snow and thunder" },
        {                 "snow",                    "snow" },
        {          "snowshowers",            "snow showers" },
        {       "snowandthunder",        "snow and thunder" },
        {            "heavysnow",              "heavy snow" },
        {     "heavysnowshowers",      "heavy snow showers" },
        {  "heavysnowandthunder",  "heavy snow and thunder" },
      };
      return MAP;
    }

    fn StripTimeOfDayFromSymbol(StringView symbol) -> String {
      static constexpr Array<StringView, 3> SUFFIXES = { "_day", "_night", "_polartwilight" };
      for (const StringView& suffix : SUFFIXES)
        if (symbol.size() > suffix.size() && symbol.ends_with(suffix))
          return String(symbol.substr(0, symbol.size() - suffix.size()));
      return String(symbol);
    }

    class MetNoProvider : public IWeatherProvider {
      f64        m_lat;
      f64        m_lon;
      UnitSystem m_units;

     public:
      MetNoProvider(f64 lat, f64 lon, UnitSystem units)
        : m_lat(lat), m_lon(lon), m_units(units) {}

      fn fetch() -> Result<WeatherData> override {
        String responseBuffer;

        curl::Easy curlHandle({
          .url                = std::format("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat={:.4f}&lon={:.4f}", m_lat, m_lon),
          .writeBuffer        = &responseBuffer,
          .timeoutSecs        = 10L,
          .connectTimeoutSecs = 5L,
          .userAgent          = String("draconisplusplus-weather-plugin/1.0"),
        });

        if (!curlHandle) {
          if (const auto& initError = curlHandle.getInitializationError())
            ERR_FROM(*initError);
          ERR(ApiUnavailable, "Failed to initialize cURL");
        }

        TRY_VOID(curlHandle.perform());

        dto::metno::Response apiResp {};
        if (auto errc = glz::read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != glz::error_code::none)
          ERR_FMT(ParseError, "Failed to parse Met.no response: {}", glz::format_error(errc, responseBuffer.data()));

        if (apiResp.properties.timeseries.empty())
          ERR(ParseError, "No timeseries data in met.no response");

        const auto& [time, data] = apiResp.properties.timeseries.front();

        f64 temp = data.instant.details.airTemperature;
        if (m_units == UnitSystem::Imperial)
          temp = (temp * 9.0 / 5.0) + 32.0;

        String description;
        if (data.next1Hours) {
          String strippedSymbol = StripTimeOfDayFromSymbol(data.next1Hours->summary.symbolCode);
          if (auto iter = GetMetnoSymbolDescriptions().find(strippedSymbol); iter != GetMetnoSymbolDescriptions().end())
            description = String(iter->second);
          else
            description = strippedSymbol;
        }

        return WeatherData {
          .temperature = temp,
          .description = description.empty() ? None : Some(description),
          .location    = None,
          .units       = m_units,
        };
      }
    };
  } // namespace

  namespace {
    fn GetOpenmeteoWeatherDescription(i32 code) -> String {
      if (code == 0)
        return "clear sky";
      if (code == 1)
        return "mainly clear";
      if (code == 2)
        return "partly cloudy";
      if (code == 3)
        return "overcast";
      if (code == 45 || code == 48)
        return "fog";
      if (code >= 51 && code <= 55)
        return "drizzle";
      if (code == 56 || code == 57)
        return "freezing drizzle";
      if (code >= 61 && code <= 65)
        return "rain";
      if (code == 66 || code == 67)
        return "freezing rain";
      if (code >= 71 && code <= 75)
        return "snow fall";
      if (code == 77)
        return "snow grains";
      if (code >= 80 && code <= 82)
        return "rain showers";
      if (code == 85 || code == 86)
        return "snow showers";
      if (code == 95)
        return "thunderstorm";
      if (code >= 96 && code <= 99)
        return "thunderstorm with hail";
      return "unknown";
    }

    class OpenMeteoProvider : public IWeatherProvider {
      f64        m_lat;
      f64        m_lon;
      UnitSystem m_units;

     public:
      OpenMeteoProvider(f64 lat, f64 lon, UnitSystem units)
        : m_lat(lat), m_lon(lon), m_units(units) {}

      fn fetch() -> Result<WeatherData> override {
        String url = std::format(
          "https://api.open-meteo.com/v1/forecast?latitude={:.4f}&longitude={:.4f}&current_weather=true&temperature_unit={}",
          m_lat,
          m_lon,
          m_units == UnitSystem::Imperial ? "fahrenheit" : "celsius"
        );

        String responseBuffer;

        curl::Easy curlHandle({
          .url                = url,
          .writeBuffer        = &responseBuffer,
          .timeoutSecs        = 10L,
          .connectTimeoutSecs = 5L,
        });

        if (!curlHandle) {
          if (const auto& initError = curlHandle.getInitializationError())
            ERR_FROM(*initError);
          ERR(ApiUnavailable, "Failed to initialize cURL");
        }

        TRY_VOID(curlHandle.perform());

        dto::openmeteo::Response apiResp {};
        if (auto errc = glz::read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer.data()); errc.ec != glz::error_code::none)
          ERR_FMT(ParseError, "Failed to parse OpenMeteo response: {}", glz::format_error(errc, responseBuffer.data()));

        return WeatherData {
          .temperature = apiResp.currentWeather.temperature,
          .description = Some(GetOpenmeteoWeatherDescription(apiResp.currentWeather.weathercode)),
          .location    = None,
          .units       = m_units,
        };
      }
    };
  } // namespace

  namespace {
    fn MakeOWMApiRequest(const String& url) -> Result<WeatherData> {
      String responseBuffer;

      curl::Easy curlHandle({
        .url                = url,
        .writeBuffer        = &responseBuffer,
        .timeoutSecs        = 10L,
        .connectTimeoutSecs = 5L,
      });

      if (!curlHandle) {
        if (const auto& initError = curlHandle.getInitializationError())
          ERR_FROM(*initError);
        ERR(ApiUnavailable, "Failed to initialize cURL");
      }

      TRY_VOID(curlHandle.perform());

      dto::owm::OWMResponse owmResponse;
      if (auto errc = glz::read<glz::opts { .error_on_unknown_keys = false }>(owmResponse, responseBuffer); errc.ec != glz::error_code::none)
        ERR_FMT(ParseError, "Failed to parse OpenWeatherMap response: {}", glz::format_error(errc, responseBuffer.data()));

      if (owmResponse.cod && *owmResponse.cod != 200) {
        using matchit::match, matchit::is, matchit::or_, matchit::_;

        String apiErrorMessage = "OpenWeatherMap API error";
        if (owmResponse.message && !owmResponse.message->empty())
          apiErrorMessage += std::format(" ({}): {}", *owmResponse.cod, *owmResponse.message);
        else
          apiErrorMessage += std::format(" (Code: {})", *owmResponse.cod);

        ERR(
          match(*owmResponse.cod)(is | 401 = PermissionDenied, is | 404 = NotFound, is | or_(429, _) = ApiUnavailable),
          std::move(apiErrorMessage)
        );
      }

      return WeatherData {
        .temperature = owmResponse.main.temp,
        .description = !owmResponse.weather.empty() ? Some(owmResponse.weather[0].description) : None,
        .location    = owmResponse.name.empty() ? None : Some(owmResponse.name),
        .units       = UnitSystem::Metric, // Will be set by caller
      };
    }

    class OpenWeatherMapProvider : public IWeatherProvider {
      Option<Coords> m_coords;
      Option<String> m_city;
      String         m_apiKey;
      UnitSystem     m_units;

     public:
      OpenWeatherMapProvider(const Option<Coords>& coords, const Option<String>& city, String apiKey, UnitSystem units)
        : m_coords(coords), m_city(city), m_apiKey(std::move(apiKey)), m_units(units) {}

      fn fetch() -> Result<WeatherData> override {
        String unitsParam = m_units == UnitSystem::Imperial ? "imperial" : "metric";

        if (m_city) {
          String escapedCity = TRY(curl::Easy::escape(*m_city));
          String apiUrl      = std::format(
            "https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}",
            escapedCity,
            m_apiKey,
            unitsParam
          );
          auto result  = TRY(MakeOWMApiRequest(apiUrl));
          result.units = m_units;
          return result;
        }

        if (m_coords) {
          String apiUrl = std::format(
            "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
            m_coords->lat,
            m_coords->lon,
            m_apiKey,
            unitsParam
          );
          auto result  = TRY(MakeOWMApiRequest(apiUrl));
          result.units = m_units;
          return result;
        }

        ERR(InvalidArgument, "No location (city or coordinates) provided for OpenWeatherMap");
      }
    };
  } // namespace

  namespace {
    fn CreateMetNoProvider(f64 lat, f64 lon, UnitSystem units) -> UniquePointer<IWeatherProvider> {
      return std::make_unique<MetNoProvider>(lat, lon, units);
    }

    fn CreateOpenMeteoProvider(f64 lat, f64 lon, UnitSystem units) -> UniquePointer<IWeatherProvider> {
      return std::make_unique<OpenMeteoProvider>(lat, lon, units);
    }

    fn CreateOpenWeatherMapProvider(
      const Option<Coords>& coords,
      const Option<String>& city,
      const String&         apiKey,
      UnitSystem            units
    ) -> UniquePointer<IWeatherProvider> {
      return std::make_unique<OpenWeatherMapProvider>(coords, city, apiKey, units);
    }
  } // namespace
} // namespace weather::providers

namespace {
  class WeatherPlugin : public IInfoProviderPlugin {
   private:
    PluginMetadata                                      m_metadata;
    weather::WeatherConfig                              m_config;
    weather::WeatherData                                m_data;
    Option<String>                                      m_lastError;
    UniquePointer<weather::providers::IWeatherProvider> m_provider;
    bool                                                m_ready = false;

#if DRAC_PRECOMPILED_CONFIG
    // Load configuration from compile-time constants (config.hpp)
    static fn loadConfig(const fs::path& /*configDir*/) -> Result<weather::WeatherConfig> {
      weather::WeatherConfig cfg;
      cfg.enabled  = WEATHER_ENABLED;
      cfg.provider = WEATHER_PROVIDER;
      cfg.units    = WEATHER_UNITS;

      if constexpr (WEATHER_USE_COORDS)
        cfg.coords = weather::Coords {
          .lat = WEATHER_LAT,
          .lon = WEATHER_LON,
        };

      if constexpr (WEATHER_USE_CITY && !WEATHER_CITY.empty())
        cfg.city = String(WEATHER_CITY);

      if constexpr (!WEATHER_API_KEY.empty())
        cfg.apiKey = String(WEATHER_API_KEY);

      return cfg;
    }
#else
    // Load configuration from TOML file at runtime
    static fn loadConfig(const fs::path& configDir) -> Result<weather::WeatherConfig> {
      fs::path configPath = configDir / "weather.toml";

      if (!fs::exists(configPath)) {
        // Create default config
        createDefaultConfig(configPath);
        return weather::WeatherConfig {};
      }

      try {
        toml::table tbl = toml::parse_file(configPath.string());

        weather::WeatherConfig cfg;
        cfg.enabled = tbl["enabled"].value_or(false);

        if (!cfg.enabled)
          return cfg;

        // Parse provider
        String providerStr = tbl["provider"].value_or("openmeteo");
        if (providerStr == "openmeteo")
          cfg.provider = weather::Provider::OpenMeteo;
        else if (providerStr == "metno")
          cfg.provider = weather::Provider::MetNo;
        else if (providerStr == "openweathermap")
          cfg.provider = weather::Provider::OpenWeatherMap;
        else {
          warn_log("Unknown weather provider '{}', defaulting to openmeteo", providerStr);
          cfg.provider = weather::Provider::OpenMeteo;
        }

        // Parse units
        String unitsStr = tbl["units"].value_or("metric");
        cfg.units       = (unitsStr == "imperial") ? weather::UnitSystem::Imperial : weather::UnitSystem::Metric;

        // Parse location
        if (auto locationNode = tbl["location"]) {
          if (locationNode.is_string()) {
            cfg.city = *locationNode.value<String>();
          } else if (locationNode.is_table()) {
            auto* locTable = locationNode.as_table();
            if (locTable->contains("lat") && locTable->contains("lon")) {
              cfg.coords = weather::Coords {
                .lat = *(*locTable)["lat"].value<double>(),
                .lon = *(*locTable)["lon"].value<double>(),
              };
            }
          }
        }

        // Parse API key
        if (auto apiKey = tbl["api_key"].value<String>())
          cfg.apiKey = *apiKey;

        return cfg;
      } catch (const toml::parse_error& err) {
        ERR_FMT(ParseError, "Failed to parse weather config: {}", err.what());
      }
    }
#endif // DRAC_PRECOMPILED_CONFIG

#if !DRAC_PRECOMPILED_CONFIG
    static fn createDefaultConfig(const fs::path& configPath) -> void {
      std::error_code errc;
      fs::create_directories(configPath.parent_path(), errc);

      std::ofstream file(configPath);
      if (!file)
        return;

      file << R"(# Weather Plugin Configuration
# Enable or disable the weather plugin
enabled = false

# Weather provider: "openmeteo", "metno", or "openweathermap"
# - openmeteo: Free, no API key required, coordinates only
# - metno: Free, no API key required, coordinates only
# - openweathermap: Requires API key, supports city names
provider = "openmeteo"

# Temperature units: "metric" (Celsius) or "imperial" (Fahrenheit)
units = "metric"

# Location - either coordinates or city name
# For coordinates (required for openmeteo and metno):
# [location]
# lat = 40.7128
# lon = -74.0060

# For city name (openweathermap only):
# location = "New York, NY"

# API key (required for openweathermap)
# Get a free key at: https://openweathermap.org/api
# api_key = "your_api_key_here"
)";
    }
#endif // !DRAC_PRECOMPILED_CONFIG

    fn createProvider() -> Result<Unit> {
      if (!m_config.enabled) {
        m_provider = nullptr;
        return {};
      }

      switch (m_config.provider) {
        case weather::Provider::OpenMeteo:
          if (!m_config.coords)
            ERR(InvalidArgument, "OpenMeteo requires coordinates. Set [location] with lat and lon in weather.toml");
          m_provider = weather::providers::CreateOpenMeteoProvider(
            m_config.coords->lat, m_config.coords->lon, m_config.units
          );
          break;

        case weather::Provider::MetNo:
          if (!m_config.coords)
            ERR(InvalidArgument, "Met.no requires coordinates. Set [location] with lat and lon in weather.toml");
          m_provider = weather::providers::CreateMetNoProvider(
            m_config.coords->lat, m_config.coords->lon, m_config.units
          );
          break;

        case weather::Provider::OpenWeatherMap:
          if (!m_config.apiKey)
            ERR(InvalidArgument, "OpenWeatherMap requires an API key. Set api_key in weather.toml");
          if (!m_config.coords && !m_config.city)
            ERR(InvalidArgument, "OpenWeatherMap requires a location. Set location in weather.toml");
          m_provider = weather::providers::CreateOpenWeatherMapProvider(
            m_config.coords, m_config.city, *m_config.apiKey, m_config.units
          );
          break;
      }

      return {};
    }

   public:
    WeatherPlugin() {
      m_metadata = {
        .name         = "Weather",
        .version      = "1.0.0",
        .author       = "Draconis++ Team",
        .description  = "Provides weather information from OpenMeteo, Met.no, or OpenWeatherMap",
        .type         = PluginType::InfoProvider,
        .dependencies = { .requiresNetwork = true, .requiresCaching = true },
      };
    }

    [[nodiscard]] fn getMetadata() const -> const PluginMetadata& override {
      return m_metadata;
    }

    [[nodiscard]] fn getProviderId() const -> String override {
      return "weather";
    }

    fn initialize(const PluginContext& ctx, PluginCache& /*cache*/) -> Result<Unit> override {
      // Load configuration
      auto configResult = loadConfig(ctx.configDir);
      if (!configResult) {
        m_lastError = configResult.error().message;
        warn_log("Weather plugin config error: {}", *m_lastError);
        m_config.enabled = false;
      } else {
        m_config = *configResult;
      }

      // Create provider if enabled
      if (m_config.enabled) {
        if (auto providerResult = createProvider(); !providerResult) {
          m_lastError = providerResult.error().message;
          warn_log("Weather plugin provider error: {}", *m_lastError);
          m_config.enabled = false;
        }
      }

      m_ready = true;
      return {};
    }

    fn shutdown() -> Unit override {
      m_provider = nullptr;
      m_ready    = false;
    }

    [[nodiscard]] fn isReady() const -> bool override {
      return m_ready;
    }

    [[nodiscard]] fn isEnabled() const -> bool override {
      return m_config.enabled;
    }

    fn collectData(PluginCache& cache) -> Result<Unit> override {
      if (!m_ready)
        ERR(NotSupported, "Weather plugin is not ready");

      if (!m_config.enabled) {
        m_lastError = "Weather plugin is disabled in configuration";
        return {};
      }

      if (!m_provider) {
        m_lastError = "No weather provider configured";
        ERR(NotSupported, "No weather provider configured");
      }

      m_lastError = None;

      // Check cache first - directly cache WeatherData using BEVE (no JSON conversion needed)
      String cacheKey = "weather_data";
      if (auto cached = cache.get<weather::WeatherData>(cacheKey)) {
        debug_log("Weather: Found cached data for key '{}'", cacheKey);
        m_data = *cached;
        return {};
      }
      debug_log("Weather: No cached data found for key '{}'", cacheKey);

      // Fetch fresh data
      auto result = m_provider->fetch();
      if (!result) {
        m_lastError = result.error().message;
        return std::unexpected(result.error());
      }

      m_data = *result;

      // Cache the result directly as WeatherData (BEVE format, 10 minute TTL)
      cache.set(cacheKey, m_data, 600);
      debug_log("Weather: Cached data with key '{}'", cacheKey);

      return {};
    }

    [[nodiscard]] fn toJson() const -> Result<String> override {
      String jsonStr;
      if (glz::error_ctx errc = glz::write<glz::opts { .skip_null_members = true, .prettify = true }>(m_data, jsonStr); errc)
        ERR_FMT(ParseError, "Failed to serialize weather data: {}", glz::format_error(errc, jsonStr));
      return jsonStr;
    }

    [[nodiscard]] fn getFields() const -> Map<String, String> override {
      Map<String, String> fields;

      if (m_data.temperature) {
        String tempStr        = std::format("{:.1f}°{}", *m_data.temperature, m_data.units == weather::UnitSystem::Metric ? "C" : "F");
        fields["temp"]        = tempStr;
        fields["temperature"] = tempStr;
      }

      if (m_data.description)
        fields["desc"] = *m_data.description;

      if (m_data.location)
        fields["location"] = *m_data.location;

      // Combined field for compact display
      if (m_data.temperature && m_data.description)
        fields["summary"] = std::format("{:.1f}°{}, {}", *m_data.temperature, m_data.units == weather::UnitSystem::Metric ? "C" : "F", *m_data.description);

      return fields;
    }

    [[nodiscard]] fn getDisplayValue() const -> Result<String> override {
      if (!m_data.temperature)
        ERR(NotFound, "No weather data available");

      String result = std::format("{:.0f}°{}", *m_data.temperature, m_data.units == weather::UnitSystem::Metric ? "C" : "F");

      if (m_data.description)
        result += ", " + *m_data.description;

      return result;
    }

    [[nodiscard]] fn getDisplayIcon() const -> String override {
      return "   "; // Nerd Font weather icon
    }

    [[nodiscard]] fn getDisplayLabel() const -> String override {
      return "Weather";
    }

    [[nodiscard]] fn getLastError() const -> Option<String> override {
      return m_lastError;
    }
  };
} // namespace

DRAC_PLUGIN(WeatherPlugin)
