#define ASIO_HAS_CO_AWAIT      1
#define ASIO_HAS_STD_COROUTINE 1

#include <asio/error.hpp>            // asio::error::operation_aborted
#include <csignal>                   // SIGINT, SIGTERM, SIG_ERR, std::signal
#include <cstdlib>                   // EXIT_FAILURE, EXIT_SUCCESS
#include <fstream>                   // std::ifstream
#include <magic_enum/magic_enum.hpp> // magic_enum::enum_name

#ifdef DELETE
  #undef DELETE
#endif

#ifdef fn
  #undef fn
#endif

#include <glaze/core/context.hpp>    // glz::error_ctx
#include <glaze/core/meta.hpp>       // glz::{meta, detail::Object}
#include <glaze/net/http_server.hpp> // glz::http_server
#include <matchit.hpp>               // matchit::impl::Overload
#include <utility>                   // std::move

#ifndef fn
  #define fn auto
#endif

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace {
  constexpr i16   port        = 3722;
  constexpr PCStr indexFile   = "examples/glaze_http/web/index.mustache";
  constexpr PCStr stylingFile = "examples/glaze_http/web/style.css";

  fn readFile(const std::filesystem::path& path) -> Result<String> {
    if (!std::filesystem::exists(path))
      ERR_FMT(NotFound, "File not found: {}", path.string());

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
      ERR_FMT(IoError, "Failed to open file: {}", path.string());

    const usize size = std::filesystem::file_size(path);

    String result(size, '\0');

    file.read(result.data(), static_cast<std::streamsize>(size));

    return result;
  }
} // namespace

struct SystemProperty {
  String name;
  String value;
  String error;
  bool   hasError = false;

  SystemProperty(String name, String value)
    : name(std::move(name)), value(std::move(value)) {}

  SystemProperty(String name, const DracError& err)
    : name(std::move(name)), error(std::format("{} ({})", err.message, magic_enum::enum_name(err.code))), hasError(true) {}
};

struct SystemInfo {
  Vec<SystemProperty> properties;
  String              version = DRAC_VERSION;
};

namespace glz {
  template <>
  struct meta<SystemProperty> {
    using T = SystemProperty;

    // clang-format off
    static constexpr glz::detail::Object value = glz::object(
      "name",     &T::name,
      "value",    &T::value,
      "error",    &T::error,
      "hasError", &T::hasError
    );
    // clang-format on
  };

  template <>
  struct meta<SystemInfo> {
    using T = SystemInfo;

    static constexpr glz::detail::Object value = glz::object("properties", &T::properties, "version", &T::version);
  };
} // namespace glz

fn main() -> i32 {
  glz::http_server server;

  server.on_error([](const std::error_code errc, const std::source_location& loc) {
    if (errc != asio::error::operation_aborted)
      error_log("Server error at {}:{} -> {}", loc.file_name(), loc.line(), errc.message());
  });

  server.get("/style.css", [](const glz::request& req, glz::response& res) {
    info_log("Handling request for style.css from {}", req.remote_ip);

    Result<String> result = readFile(stylingFile);

    if (result)
      res.header("Content-Type", "text/css; charset=utf-8")
        .header("Cache-Control", "no-cache, no-store, must-revalidate")
        .header("Pragma", "no-cache")
        .header("Expires", "0")
        .body(*result);
    else {
      error_log("Failed to serve style.css: {}", result.error().message);
      res.status(500).body("Internal Server Error: Could not load stylesheet.");
    }
  });

  server.get("/", [](const glz::request& req, glz::response& res) {
    info_log("Handling request from {}", req.remote_ip);

    SystemInfo sysInfo;

    draconis::utils::cache::CacheManager cacheManager;

    {
      using namespace draconis::core::system;
      using matchit::impl::Overload;
      using enum draconis::utils::error::DracErrorCode;

      fn addProperty = Overload {
        [&](const String& name, const Result<String>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, *result);
          else if (result.error().code != NotSupported)
            sysInfo.properties.emplace_back(name, result.error());
        },
        [&](const String& name, const Result<OSInfo>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} {}", result->name, result->version));
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
        [&](const String& name, const Result<ResourceUsage>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} / {}", BytesToGiB(result->usedBytes), BytesToGiB(result->totalBytes)));
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
      };

      addProperty("OS", GetOperatingSystem(cacheManager));
      addProperty("Kernel Version", GetKernelVersion(cacheManager));
      addProperty("Host", GetHost(cacheManager));
      addProperty("Shell", GetShell(cacheManager));
      addProperty("Desktop Environment", GetDesktopEnvironment(cacheManager));
      addProperty("Window Manager", GetWindowManager(cacheManager));
      addProperty("CPU Model", GetCPUModel(cacheManager));
      addProperty("GPU Model", GetGPUModel(cacheManager));
      addProperty("Memory", GetMemInfo(cacheManager));
      addProperty("Disk Usage", GetDiskUsage(cacheManager));
    }

    Result<String> htmlTemplate = readFile(indexFile);

    if (!htmlTemplate) {
      error_log("Failed to read HTML template: {}", htmlTemplate.error().message);
      res.status(500).body("Internal Server Error: Template file not found.");
      return;
    }

    if (Result<String, glz::error_ctx> result = glz::stencil(*htmlTemplate, sysInfo)) {
      res.header("Content-Type", "text/html; charset=utf-8")
        .header("Cache-Control", "no-cache, no-store, must-revalidate")
        .header("Pragma", "no-cache")
        .header("Expires", "0")
        .body(*result);
    } else {
      error_log("Failed to render stencil template:\n{}", glz::format_error(result.error(), *htmlTemplate));
      res.status(500).body("Internal Server Error: Template rendering failed.");
    }
  });

  server.bind(port);
  server.start();

  info_log("Server started at http://localhost:{}. Press Ctrl+C to exit.", port);

  {
    using namespace asio;

    io_context signalContext;

    signal_set signals(signalContext, SIGINT, SIGTERM);

    signals.async_wait([&](const error_code& error, i32 signal_number) {
      if (!error) {
        info_log("\nShutdown signal ({}) received. Stopping server...", signal_number);
        server.stop();
        signalContext.stop();
      }
    });

    signalContext.run();
  }

  info_log("Server stopped. Exiting.");
  return EXIT_SUCCESS;
}
