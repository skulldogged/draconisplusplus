#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <cstring>              // std::strlen
#include <dbus/dbus-protocol.h> // DBUS_TYPE_*
#include <dbus/dbus-shared.h>   // DBUS_BUS_SESSION
#include <fstream>              // ifstream
#include <sys/socket.h>         // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/sysctl.h>         // sysctlbyname
#include <sys/un.h>             // LOCAL_PEERCRED

#ifndef __NetBSD__
  #include <kenv.h>      // kenv
  #include <sys/ucred.h> // xucred
#endif

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Env.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "OS/Unix.hpp"

#include "Wrappers/DBus.hpp"
#include "Wrappers/Wayland.hpp"
#include "Wrappers/XCB.hpp"
// clang-format on

using namespace draconis::utils::types;
using draconis::utils::cache::CacheManager;
using draconis::utils::env::GetEnv;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace {
  #ifdef __FreeBSD__
  fn GetPathByPid(pid_t pid) -> Result<String> {
    Array<char, PATH_MAX> exePathBuf {};
    usize                 size = exePathBuf.size();
    Array<i32, 4>         mib {};

    mib.at(0) = CTL_KERN;
    mib.at(1) = KERN_PROC_ARGS;
    mib.at(2) = pid;
    mib.at(3) = KERN_PROC_PATHNAME;

    if (sysctl(mib.data(), 4, exePathBuf.data(), &size, nullptr, 0) == -1)
      return Err(DracError(std::format("sysctl KERN_PROC_PATHNAME failed for pid {}", pid)));

    if (size == 0 || exePathBuf[0] == '\0')
      return Err(
        DracError(DracErrorCode::NotFound, std::format("sysctl KERN_PROC_PATHNAME returned empty path for pid {}", pid))
      );

    exePathBuf.at(std::min(size, exePathBuf.size() - 1)) = '\0';

    return String(exePathBuf.data());
  }
  #endif

  #if DRAC_ENABLE_X11
  fn GetX11WindowManager() -> Result<String> {
    using namespace xcb;
    using namespace matchit;
    using enum ConnError;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = ConnectionHasError(conn.get()))
        return Err(
          DracError(
            DracErrorCode::ApiUnavailable,
            match(err)(
              is | Generic         = "Stream/Socket/Pipe Error",
              is | ExtNotSupported = "Extension Not Supported",
              is | MemInsufficient = "Insufficient Memory",
              is | ReqLenExceed    = "Request Length Exceeded",
              is | ParseErr        = "Display String Parse Error",
              is | InvalidScreen   = "Invalid Screen",
              is | FdPassingFailed = "FD Passing Failed",
              is | _               = std::format("Unknown Error Code ({})", err)
            )
          )
        );

    fn internAtom = [&conn](const StringView name) -> Result<atom_t> {
      const ReplyGuard<intern_atom_reply_t> reply(InternAtomReply(conn.get(), InternAtom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr));

      if (!reply)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name)));

      return reply->atom;
    };

    const Result<atom_t> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<atom_t> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<atom_t> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<get_property_reply_t> wmWindowReply(GetPropertyReply(
      conn.get(),
      GetProperty(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        GetPropertyValueLength(wmWindowReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const window_t wmRootWindow = *static_cast<window_t*>(GetPropertyValue(wmWindowReply.get()));

    const ReplyGuard<get_property_reply_t> wmNameReply(GetPropertyReply(
      conn.get(), GetProperty(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || GetPropertyValueLength(wmNameReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(GetPropertyValue(wmNameReply.get()));
    const usize length   = GetPropertyValueLength(wmNameReply.get());

    return String(nameData, length);
  }
  #else
  fn GetX11WindowManager() -> Result<String> {
    return Err(DracError(DracErrorCode::NotSupported, "XCB (X11) support not available"));
  }
  #endif

  fn GetWaylandCompositor() -> Result<String> {
  #ifndef __FreeBSD__
    return "Wayland Compositor";
  #else
    const wl::DisplayGuard display;

    if (!display)
      return Err(DracError(DracErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    pid_t peerPid = -1; // Initialize PID

    struct xucred cred;

    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, LOCAL_PEERCRED, &cred, &len) == -1)
      return Err(DracError("Failed to get socket credentials (LOCAL_PEERCRED)"));

    peerPid = cred.cr_pid;

    if (peerPid <= 0)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to obtain a valid peer PID"));

    String exeRealPath = TRY(GetPathByPid(peerPid));

    StringView compositorNameView;

    if (const usize lastSlash = exeRealPath.rfind('/'); lastSlash != String::npos)
      compositorNameView = StringView(exeRealPath).substr(lastSlash + 1);
    else
      compositorNameView = exeRealPath;

    if (compositorNameView.empty() || compositorNameView == "." || compositorNameView == "/")
      return Err(DracError(DracErrorCode::NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(DracError(DracErrorCode::NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  #endif
  }
} // namespace

namespace draconis::core::system {

  fn GetOperatingSystem(CacheManager& cache) -> Result<OSInfo> {
    return cache.getOrSet<OSInfo>("bsd_os_info", []() -> Result<OSInfo> {
      constexpr PCStr path = "/etc/os-release";
      String          name;
      String          version;
      String          id;

      std::ifstream file(path);

      if (file) {
        String line;

        while (std::getline(file, line)) {
          auto extractValue = [](const String& str, StringView prefix) -> Option<String> {
            if (!StringView(str).starts_with(prefix))
              return None;

            String value = str.substr(prefix.size());

            if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
                (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
              value = value.substr(1, value.length() - 2);

            return value;
          };

          if (auto val = extractValue(line, "NAME="))
            name = *val;
          else if (auto val = extractValue(line, "VERSION="))
            version = *val;
          else if (auto val = extractValue(line, "ID="))
            id = *val;
        }

        if (!name.empty())
          return OSInfo(std::move(name), std::move(version), std::move(id));
      }

      // Fallback to uname
      Result<String> sysName = os::unix_shared::GetSystemName();
      if (!sysName)
        return Err(sysName.error());

      return OSInfo(*sysName, "", "");
    });
  }

  fn GetMemInfo() -> Result<u64> {
    u64   mem  = 0;
    usize size = sizeof(mem);

  #ifdef __NetBSD__
    sysctlbyname("hw.physmem64", &mem, &size, nullptr, 0);
  #else
    sysctlbyname("hw.physmem", &mem, &size, nullptr, 0);
  #endif

    return mem;
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    using namespace dbus;

    Connection connection = TRY(Connection::busGet(DBUS_BUS_SESSION));

    Option<String> activePlayer = None;

    {
      Message listNamesMsg = TRY(
        Message::newMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames")
      );

      Message listNamesReply = TRY(connection.sendWithReplyAndBlock(listNamesMsg, 100));

      MessageIter iter = listNamesReply.iterInit();
      if (!iter.isValid() || iter.getArgType() != DBUS_TYPE_ARRAY)
        return Err(DracError(DracErrorCode::ParseError, "Invalid DBus ListNames reply format: Expected array"));

      MessageIter subIter = iter.recurse();
      if (!subIter.isValid())
        return Err(
          DracError(DracErrorCode::ParseError, "Invalid DBus ListNames reply format: Could not recurse into array")
        );

      while (subIter.getArgType() != DBUS_TYPE_INVALID) {
        if (Option<String> name = subIter.getString())
          if (name->starts_with("org.mpris.MediaPlayer2.")) {
            activePlayer = std::move(*name);
            break;
          }
        if (!subIter.next())
          break;
      }
    }

    if (!activePlayer)
      return Err(DracError(DracErrorCode::NotFound, "No active MPRIS players found"));

    Message msg = TRY(Message::newMethodCall(activePlayer->c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get"));

    if (!msg.appendArgs("org.mpris.MediaPlayer2.Player", "Metadata"))
      return Err(DracError(DracErrorCode::InternalError, "Failed to append arguments to Properties.Get message"));

    Message reply = TRY(connection.sendWithReplyAndBlock(msg, 100));

    Option<String> title  = None;
    Option<String> artist = None;

    MessageIter propIter = reply.iterInit();
    if (!propIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Properties.Get reply has no arguments or invalid iterator"));

    if (propIter.getArgType() != DBUS_TYPE_VARIANT)
      return Err(DracError(DracErrorCode::ParseError, "Properties.Get reply argument is not a variant"));

    MessageIter variantIter = propIter.recurse();
    if (!variantIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Could not recurse into variant"));

    if (variantIter.getArgType() != DBUS_TYPE_ARRAY || variantIter.getElementType() != DBUS_TYPE_DICT_ENTRY)
      return Err(DracError(DracErrorCode::ParseError, "Metadata variant content is not a dictionary array (a{sv})"));

    MessageIter dictIter = variantIter.recurse();
    if (!dictIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Could not recurse into metadata dictionary array"));

    while (dictIter.getArgType() == DBUS_TYPE_DICT_ENTRY) {
      MessageIter entryIter = dictIter.recurse();
      if (!entryIter.isValid()) {
        if (!dictIter.next())
          break;
        continue;
      }

      Option<String> key = entryIter.getString();
      if (!key) {
        if (!dictIter.next())
          break;
        continue;
      }

      if (!entryIter.next() || entryIter.getArgType() != DBUS_TYPE_VARIANT) {
        if (!dictIter.next())
          break;
        continue;
      }

      MessageIter valueVariantIter = entryIter.recurse();
      if (!valueVariantIter.isValid()) {
        if (!dictIter.next())
          break;
        continue;
      }

      if (*key == "xesam:title") {
        title = valueVariantIter.getString();
      } else if (*key == "xesam:artist") {
        if (valueVariantIter.getArgType() == DBUS_TYPE_ARRAY && valueVariantIter.getElementType() == DBUS_TYPE_STRING) {
          if (MessageIter artistArrayIter = valueVariantIter.recurse(); artistArrayIter.isValid())
            artist = artistArrayIter.getString();
        }
      }

      if (!dictIter.next())
        break;
    }

    return MediaInfo(std::move(title), std::move(artist));
  }

  fn GetWindowManager(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("bsd_wm", []() -> Result<String> {
      if (!GetEnv("DISPLAY") && !GetEnv("WAYLAND_DISPLAY") && !GetEnv("XDG_SESSION_TYPE"))
        return Err(DracError(DracErrorCode::NotFound, "Could not find a graphical session"));

      if (Result<String> waylandResult = GetWaylandCompositor())
        return String(*waylandResult);

      if (Result<String> x11Result = GetX11WindowManager())
        return String(*x11Result);

      return Err(DracError(DracErrorCode::NotFound, "Could not detect window manager (Wayland/X11) or both failed"));
    });
  }

  fn GetDesktopEnvironment(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("bsd_desktop_environment", []() -> Result<String> {
      if (!GetEnv("DISPLAY") && !GetEnv("WAYLAND_DISPLAY") && !GetEnv("XDG_SESSION_TYPE"))
        return Err(DracError(DracErrorCode::NotFound, "Could not find a graphical session"));

      return GetEnv("XDG_CURRENT_DESKTOP")
        .transform([](String xdgDesktop) -> String {
          if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
            xdgDesktop.resize(colon);

          return xdgDesktop;
        })
        .or_else([](const DracError&) -> Result<String> { return GetEnv("DESKTOP_SESSION"); })
        .transform([](String desktopSession) -> String {
          if (const usize colon = desktopSession.find(':'); colon != String::npos)
            desktopSession.resize(colon);

          return desktopSession;
        })
        .transform([](String desktop) -> String { return String(desktop); });
    });
  }

  fn GetShell(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("bsd_shell", []() -> Result<String> {
      if (const Result<String> shellPath = GetEnv("SHELL")) {
        // clang-format off
            constexpr Array<Pair<StringView, StringView>, 5> shellMap {{
                { "bash",    "Bash" },
                {  "zsh",     "Zsh" },
                { "fish",    "Fish" },
                {   "nu", "Nushell" },
                {   "sh",      "SH" }, // sh last because other shells contain "sh"
            }};
        // clang-format on

        for (const auto& [exe, name] : shellMap)
          if (shellPath->contains(exe))
            return String(name);

        return String(*shellPath); // fallback to the raw shell path
      }

      return Err(DracError(DracErrorCode::NotFound, "Could not find SHELL environment variable"));
    });
  }

  fn GetHost(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("bsd_host", []() -> Result<String> {
      Array<char, 256> buffer {};
      usize            size = buffer.size();

  #if defined(__FreeBSD__) || defined(__DragonFly__)
      i32 result = kenv(KENV_GET, "smbios.system.product", buffer.data(), buffer.size() - 1); // Ensure space for null

      if (result == -1) {
        if (sysctlbyname("hw.model", buffer.data(), &size, nullptr, 0) == -1)
          return Err(DracError("kenv smbios.system.product failed and sysctl hw.model also failed"));

        buffer.at(std::min(size, buffer.size() - 1)) = '\0';
        return String(buffer.data());
      }

      if (result > 0)
        buffer.at(result) = '\0';
      else
        buffer.at(0) = '\0';

  #elifdef __NetBSD__
        if (sysctlbyname("machdep.dmi.system-product", buffer.data(), &size, nullptr, 0) == -1)
            return Err(DracError(std::format("sysctlbyname failed for")));

        buffer[std::min(size, buffer.size() - 1)] = '\0';
  #endif
      if (buffer[0] == '\0')
        return Err(DracError(DracErrorCode::NotFound, "Failed to get host product information (empty result)"));

      return String(buffer.data());
    });
  }

  fn GetKernelVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("bsd_kernel_version", []() -> Result<String> {
      return os::unix_shared::GetKernelRelease();
    });
  }

  fn GetDiskUsage(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    return os::unix_shared::GetRootDiskUsage();
  }
} // namespace draconis::core::system

#endif // __FreeBSD__ || __DragonFly__ || __NetBSD__
