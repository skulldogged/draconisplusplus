/**
 * @file now_playing.cpp
 * @brief Now Playing plugin - Cross-platform implementation
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details This plugin provides currently playing media information using
 * platform-specific APIs:
 * - Windows: NPSM (Now Playing Session Manager) COM API
 * - Linux/BSD: MPRIS over DBus
 * - macOS: MediaRemote private framework
 */

#include <format>
#include <glaze/glaze.hpp>
#include <utility>

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::core::plugin;
using namespace draconis::utils::types;
using namespace draconis::utils::error;
using namespace draconis::utils::logging;
using enum DracErrorCode;

namespace now_playing {
  /**
   * @brief Media information data structure
   */
  struct MediaData {
    Option<String> title;
    Option<String> artist;
    Option<String> album;
    Option<String> playerName;
  };

  /**
   * @brief Plugin configuration
   */
  struct NowPlayingConfig {
    bool enabled = true;
  };
} // namespace now_playing

// Glaze metadata for serialization
namespace glz {
  template <>
  struct meta<now_playing::MediaData> {
    using T                     = now_playing::MediaData;
    static constexpr auto value = object(
      "title",
      &T::title,
      "artist",
      &T::artist,
      "album",
      &T::album,
      "playerName",
      &T::playerName
    );
  };
} // namespace glz

#ifdef _WIN32

  #include <propsys.h>
  #include <windows.h>
  #include <wrl/client.h>

namespace now_playing::npsm {
  // {BCBB9860-C012-4AD7-A938-6E337AE6ABA5}
  static const GUID CLSID_NowPlayingSessionManager = {
    0xBCBB9860,
    0xC012,
    0x4AD7,
    { 0xA9, 0x38, 0x6E, 0x33, 0x7A, 0xE6, 0xAB, 0xA5 }
  };

  // INowPlayingSessionManager - {3b6a7908-ce07-4ba9-878c-6e4a15db5e5b} (19041+)
  // NOLINTBEGIN(cppcoreguidelines-virtual-class-destructor, readability-identifier-naming)
  MIDL_INTERFACE("3b6a7908-ce07-4ba9-878c-6e4a15db5e5b")
  INowPlayingSessionManager : public IUnknown {
   public:
    virtual auto STDMETHODCALLTYPE get_Count(ULONG64 * pCount)->HRESULT               = 0;
    virtual auto STDMETHODCALLTYPE get_CurrentSession(IUnknown * *ppSession)->HRESULT = 0;
  };

  // INowPlayingSession - {431268cf-7477-4285-950b-6f892a944712} (14393+)
  MIDL_INTERFACE("431268cf-7477-4285-950b-6f892a944712")
  INowPlayingSession : public IUnknown {
   public:
    virtual auto STDMETHODCALLTYPE get_SessionType(int* pType) -> HRESULT                            = 0;
    virtual auto STDMETHODCALLTYPE get_SourceAppId(LPWSTR * pszSrcAppId)->HRESULT                    = 0;
    virtual auto STDMETHODCALLTYPE get_SourceDeviceId(LPWSTR * pszSourceDeviceId)->HRESULT           = 0;
    virtual auto STDMETHODCALLTYPE get_RenderDeviceId(LPWSTR * pszRenderId)->HRESULT                 = 0;
    virtual auto STDMETHODCALLTYPE get_HWND(HWND * pHwnd)->HRESULT                                   = 0;
    virtual auto STDMETHODCALLTYPE get_PID(DWORD * pdwPID)->HRESULT                                  = 0;
    virtual auto STDMETHODCALLTYPE get_Info(IUnknown * *ppInfo)->HRESULT                             = 0;
    virtual auto STDMETHODCALLTYPE get_Connection(IUnknown * *ppUnknown)->HRESULT                    = 0;
    virtual auto STDMETHODCALLTYPE ActivateMediaPlaybackDataSource(IUnknown * *ppMediaCtrl)->HRESULT = 0;
  };

  // IMediaPlaybackDataSource - {0F4521BE-A0B8-4116-B3B1-BFECEBAEEBE6} (10586-19041)
  MIDL_INTERFACE("0F4521BE-A0B8-4116-B3B1-BFECEBAEEBE6")
  IMediaPlaybackDataSource : public IUnknown {
   public:
    virtual auto STDMETHODCALLTYPE GetMediaPlaybackInfo(void* pPlaybackInfo) -> HRESULT       = 0;
    virtual auto STDMETHODCALLTYPE SendMediaPlaybackCommand(int command) -> HRESULT           = 0;
    virtual auto STDMETHODCALLTYPE GetMediaObjectInfo(IPropertyStore * *ppPropStore)->HRESULT = 0;
  };

  // IMediaPlaybackDataSource2 - {c4f66b80-df04-4f79-afc2-bee3fc7c46e3} (20279+ / Windows 11)
  MIDL_INTERFACE("c4f66b80-df04-4f79-afc2-bee3fc7c46e3")
  IMediaPlaybackDataSource2 : public IUnknown {
   public:
    virtual auto STDMETHODCALLTYPE GetMediaPlaybackInfo(void* pPlaybackInfo) -> HRESULT       = 0;
    virtual auto STDMETHODCALLTYPE SendMediaPlaybackCommand(int command) -> HRESULT           = 0;
    virtual auto STDMETHODCALLTYPE GetMediaObjectInfo(IPropertyStore * *ppPropStore)->HRESULT = 0;
  };
  // NOLINTEND(cppcoreguidelines-virtual-class-destructor, readability-identifier-naming)

  // Property keys for media metadata
  // PKEY_Title = {F29F85E0-4FF9-1068-AB91-08002B27B3D9}, 2
  static const PROPERTYKEY PKEY_Title = {
    { 0xF29F85E0, 0x4FF9, 0x1068, { 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9 } },
    2
  };

  // PKEY_Music_Artist = {56A3372E-CE9C-11D2-9F0E-006097C686F6}, 2
  static const PROPERTYKEY PKEY_Music_Artist = {
    { 0x56A3372E, 0xCE9C, 0x11D2, { 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6 } },
    2
  };

  // PKEY_Music_AlbumTitle = {56A3372E-CE9C-11D2-9F0E-006097C686F6}, 4
  static const PROPERTYKEY PKEY_Music_AlbumTitle = {
    { 0x56A3372E, 0xCE9C, 0x11D2, { 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6 } },
    4
  };

  namespace {
    /**
     * @brief Converts a wide string (UTF-16) to a UTF-8 encoded string.
     */
    auto ConvertWStringToUTF8(const WString& wstr) -> Result<String> {
      if (wstr.empty())
        return String {};

      const i32 sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<i32>(wstr.length()), nullptr, 0, nullptr, nullptr);

      if (sizeNeeded == 0)
        ERR_FMT(InternalError, "Failed to get buffer size for UTF-8 conversion. Error code: {}", GetLastError());

      String result(static_cast<usize>(sizeNeeded), '\0');

      const i32 bytesConverted =
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<i32>(wstr.length()), result.data(), sizeNeeded, nullptr, nullptr);

      if (bytesConverted == 0)
        ERR_FMT(InternalError, "Failed to convert wide string to UTF-8. Error code: {}", GetLastError());

      return result;
    }

    /**
     * @brief Fetch now playing information via Windows NPSM API
     */
    auto FetchNowPlaying() -> Result<MediaData> {
      Microsoft::WRL::ComPtr<INowPlayingSessionManager> sessionManager;

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
      HRESULT result = CoCreateInstance(
        CLSID_NowPlayingSessionManager,
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&sessionManager)
      );
  #pragma clang diagnostic pop

      if (FAILED(result))
        ERR_FMT(ApiUnavailable, "Failed to create NowPlayingSessionManager (HRESULT: 0x{:08X})", static_cast<u32>(result));

      // Check session count for debugging
      ULONG64 sessionCount = 0;
      if (SUCCEEDED(sessionManager->get_Count(&sessionCount)))
        debug_log("Now Playing: Session count = {}", sessionCount);

      Microsoft::WRL::ComPtr<IUnknown> sessionUnknown;
      result = sessionManager->get_CurrentSession(&sessionUnknown);
      if (FAILED(result) || !sessionUnknown)
        ERR_FMT(NotFound, "No media session found (HRESULT: 0x{:08X}, sessionCount={})", static_cast<u32>(result), sessionCount);

      Microsoft::WRL::ComPtr<INowPlayingSession> session;
      result = sessionUnknown.As(&session);
      if (FAILED(result))
        ERR_FMT(ApiUnavailable, "Failed to get INowPlayingSession interface (HRESULT: 0x{:08X})", static_cast<u32>(result));

      Microsoft::WRL::ComPtr<IUnknown> dataSourceUnknown;
      result = session->ActivateMediaPlaybackDataSource(&dataSourceUnknown);
      if (FAILED(result) || !dataSourceUnknown)
        ERR_FMT(ApiUnavailable, "Failed to activate MediaPlaybackDataSource (HRESULT: 0x{:08X})", static_cast<u32>(result));

      // Try Windows 11 interface first (IMediaPlaybackDataSource2), then fall back to older interface
      Microsoft::WRL::ComPtr<IPropertyStore>            propStore;
      Microsoft::WRL::ComPtr<IMediaPlaybackDataSource2> dataSource2;
      result = dataSourceUnknown.As(&dataSource2);
      if (SUCCEEDED(result)) {
        result = dataSource2->GetMediaObjectInfo(&propStore);
      } else {
        // Fall back to older interface for Windows 10
        Microsoft::WRL::ComPtr<IMediaPlaybackDataSource> dataSource;
        result = dataSourceUnknown.As(&dataSource);
        if (FAILED(result))
          ERR_FMT(ApiUnavailable, "Failed to get IMediaPlaybackDataSource interface (HRESULT: 0x{:08X})", static_cast<u32>(result));
        result = dataSource->GetMediaObjectInfo(&propStore);
      }

      if (FAILED(result) || !propStore)
        ERR(ApiUnavailable, "Failed to get media object info");

      MediaData data;

      PROPVARIANT pVar;
      PropVariantInit(&pVar);

      // Get title
      // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
      if (SUCCEEDED(propStore->GetValue(PKEY_Title, &pVar)) && pVar.vt == VT_LPWSTR && pVar.pwszVal)
        if (Result<String> titleStr = ConvertWStringToUTF8(pVar.pwszVal); titleStr && !titleStr->empty())
          data.title = std::move(*titleStr);

      PropVariantClear(&pVar);

      // Get artist
      if (SUCCEEDED(propStore->GetValue(PKEY_Music_Artist, &pVar)) && pVar.vt == VT_LPWSTR && pVar.pwszVal)
        if (Result<String> artistStr = ConvertWStringToUTF8(pVar.pwszVal); artistStr && !artistStr->empty())
          data.artist = std::move(*artistStr);

      PropVariantClear(&pVar);

      // Get album
      if (SUCCEEDED(propStore->GetValue(PKEY_Music_AlbumTitle, &pVar)) && pVar.vt == VT_LPWSTR && pVar.pwszVal)
        if (Result<String> albumStr = ConvertWStringToUTF8(pVar.pwszVal); albumStr && !albumStr->empty())
          data.album = std::move(*albumStr);
      // NOLINTEND(cppcoreguidelines-pro-type-union-access)

      PropVariantClear(&pVar);

      return data;
    }
  } // anonymous namespace
} // namespace now_playing::npsm

#endif // _WIN32

#ifdef __APPLE__

  #import <Foundation/Foundation.h>
  #import <dispatch/dispatch.h>

namespace now_playing::macos {
  // Forward-declare the function pointer type for the private MediaRemote API.
  using MRMediaRemoteGetNowPlayingInfoFn =
    void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

  /**
   * @brief Fetch now playing information via macOS MediaRemote framework
   */
  auto fetchNowPlaying() -> Result<MediaData> {
    @autoreleasepool {
      // Since MediaRemote.framework is private, we cannot link against it directly.
      // Instead, it must be loaded at runtime using CFURL and CFBundle.
      CFURLRef urlRef = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
        kCFURLPOSIXPathStyle,
        false
      );

      if (!urlRef)
        ERR(NotFound, "Failed to create CFURL for MediaRemote.framework");

      // Create a bundle from the URL
      CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);
      CFRelease(urlRef);

      if (!bundleRef)
        ERR(ApiUnavailable, "Failed to create bundle for MediaRemote.framework");

      // Ensure CFRelease is called even if an error occurs.
      SharedPointer<std::remove_pointer_t<CFBundleRef>> managedBundle(bundleRef, [](CFBundleRef bundle) {
        if (bundle)
          CFRelease(bundle);
      });

      // Get a pointer to the MRMediaRemoteGetNowPlayingInfo function from the bundle.
      auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFn>(
        CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
      );

      if (!mrMediaRemoteGetNowPlayingInfo)
        ERR(ApiUnavailable, "Failed to get MRMediaRemoteGetNowPlayingInfo function pointer");

      // A semaphore is used to make this asynchronous call behave synchronously.
      __block Result<MediaData>  result;
      const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

      mrMediaRemoteGetNowPlayingInfo(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(NSDictionary* information) {
          if (!information) {
            result = Err(DracError(NotFound, "No media is currently playing"));
          } else {
            MediaData data;

            // Extract the title, artist, and album from the dictionary
            const NSString* const titleNS  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];
            const NSString* const artistNS = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];
            const NSString* const albumNS  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoAlbum"];

            if (titleNS)
              data.title = String([titleNS UTF8String]);

            if (artistNS)
              data.artist = String([artistNS UTF8String]);

            if (albumNS)
              data.album = String([albumNS UTF8String]);

            result = data;
          }

          dispatch_semaphore_signal(semaphore);
        }
      );

      // Block until the callback signals completion
      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
      return result;
    }
  }
} // namespace now_playing::macos

#endif // __APPLE__

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

  #include <cstring>
  #include <dbus/dbus.h>

namespace now_playing::dbus {
  /**
   * @brief RAII wrapper for DBusError
   */
  class Error {
    DBusError m_err {};
    bool      m_isInitialized = false;

   public:
    Error() : m_isInitialized(true) {
      dbus_error_init(&m_err);
    }

    ~Error() {
      if (m_isInitialized)
        dbus_error_free(&m_err);
    }

    Error(const Error&)                    = delete;
    auto operator=(const Error&) -> Error& = delete;

    Error(Error&& other) noexcept
      : m_err(other.m_err), m_isInitialized(other.m_isInitialized) {
      other.m_isInitialized = false;
      dbus_error_init(&other.m_err);
    }

    auto operator=(Error&& other) noexcept -> Error& {
      if (this != &other) {
        if (m_isInitialized)
          dbus_error_free(&m_err);
        m_err                 = other.m_err;
        m_isInitialized       = other.m_isInitialized;
        other.m_isInitialized = false;
        dbus_error_init(&other.m_err);
      }
      return *this;
    }

    [[nodiscard]] auto isSet() const -> bool {
      return m_isInitialized && dbus_error_is_set(&m_err);
    }

    [[nodiscard]] auto message() const -> const char* {
      return isSet() ? m_err.message : "";
    }

    [[nodiscard]] auto get() -> DBusError* {
      return &m_err;
    }
  };

  /**
   * @brief RAII wrapper for DBusMessageIter
   */
  class MessageIter {
    DBusMessageIter m_iter {};
    bool            m_isValid = false;

    friend class Message;

    explicit MessageIter(const DBusMessageIter& iter, const bool isValid)
      : m_iter(iter), m_isValid(isValid) {}

    auto getBasic(RawPointer value) {
      if (m_isValid)
        dbus_message_iter_get_basic(&m_iter, value);
    }

   public:
    MessageIter(const MessageIter&)                    = delete;
    auto operator=(const MessageIter&) -> MessageIter& = delete;
    MessageIter(MessageIter&&)                         = delete;
    auto operator=(MessageIter&&) -> MessageIter&      = delete;
    ~MessageIter()                                     = default;

    [[nodiscard]] auto isValid() const -> bool {
      return m_isValid;
    }

    [[nodiscard]] auto getArgType() -> i32 {
      return m_isValid ? dbus_message_iter_get_arg_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    [[nodiscard]] auto getElementType() -> i32 {
      return m_isValid ? dbus_message_iter_get_element_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    auto next() -> bool {
      return m_isValid && dbus_message_iter_next(&m_iter);
    }

    [[nodiscard]] auto recurse() -> MessageIter {
      if (!m_isValid)
        return MessageIter({}, false);

      DBusMessageIter subIter;
      dbus_message_iter_recurse(&m_iter, &subIter);
      return MessageIter(subIter, true);
    }

    [[nodiscard]] auto getString() -> Option<String> {
      if (m_isValid && getArgType() == DBUS_TYPE_STRING) {
        const char* strPtr = nullptr;
        getBasic(static_cast<RawPointer>(&strPtr));
        if (strPtr && strlen(strPtr) > 0)
          return String(strPtr);
      }
      return None;
    }
  };

  /**
   * @brief RAII wrapper for DBusMessage
   */
  class Message {
    DBusMessage* m_msg = nullptr;

   public:
    explicit Message(DBusMessage* msg = nullptr) : m_msg(msg) {}

    ~Message() {
      if (m_msg)
        dbus_message_unref(m_msg);
    }

    Message(const Message&)                    = delete;
    auto operator=(const Message&) -> Message& = delete;

    Message(Message&& other) noexcept
      : m_msg(std::exchange(other.m_msg, nullptr)) {}

    auto operator=(Message&& other) noexcept -> Message& {
      if (this != &other) {
        if (m_msg)
          dbus_message_unref(m_msg);
        m_msg = std::exchange(other.m_msg, nullptr);
      }
      return *this;
    }

    [[nodiscard]] auto get() const -> DBusMessage* {
      return m_msg;
    }

    [[nodiscard]] auto iterInit() const -> MessageIter {
      if (!m_msg)
        return MessageIter({}, false);

      DBusMessageIter iter;
      const bool      isValid = dbus_message_iter_init(m_msg, &iter);
      return MessageIter(iter, isValid);
    }

    template <typename... Args>
    [[nodiscard]] auto appendArgs(Args&&... args) -> bool {
      if (!m_msg)
        return false;

      DBusMessageIter iter;
      dbus_message_iter_init_append(m_msg, &iter);

      bool success = true;
      ((success = success && appendArgInternal(iter, std::forward<Args>(args))), ...);
      return success;
    }

    static auto newMethodCall(const char* destination, const char* path, const char* interface, const char* method)
      -> Result<Message> {
      DBusMessage* rawMsg = dbus_message_new_method_call(destination, path, interface, method);
      if (!rawMsg)
        ERR(OutOfMemory, "dbus_message_new_method_call failed");
      return Message(rawMsg);
    }

   private:
    template <typename T>
    auto appendArgInternal(DBusMessageIter& iter, T&& arg) -> bool {
      using DecayedT = std::decay_t<T>;
      if constexpr (std::is_convertible_v<DecayedT, const char*>) {
        const char* valuePtr = static_cast<const char*>(std::forward<T>(arg));
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, static_cast<const RawPointer>(&valuePtr));
      } else {
        static_assert(!sizeof(T*), "Unsupported type passed to appendArgs");
        return false;
      }
    }
  };

  /**
   * @brief RAII wrapper for DBusConnection
   */
  class Connection {
    DBusConnection* m_conn = nullptr;

   public:
    explicit Connection(DBusConnection* conn = nullptr) : m_conn(conn) {}

    ~Connection() {
      if (m_conn)
        dbus_connection_unref(m_conn);
    }

    Connection(const Connection&)                    = delete;
    auto operator=(const Connection&) -> Connection& = delete;

    Connection(Connection&& other) noexcept
      : m_conn(std::exchange(other.m_conn, nullptr)) {}

    auto operator=(Connection&& other) noexcept -> Connection& {
      if (this != &other) {
        if (m_conn)
          dbus_connection_unref(m_conn);
        m_conn = std::exchange(other.m_conn, nullptr);
      }
      return *this;
    }

    [[nodiscard]] auto get() const -> DBusConnection* {
      return m_conn;
    }

    [[nodiscard]] auto sendWithReplyAndBlock(const Message& message, const i32 timeout_milliseconds = 1000) const
      -> Result<Message> {
      if (!m_conn || !message.get())
        ERR(InvalidArgument, "Invalid connection or message");

      Error        err;
      DBusMessage* rawReply = dbus_connection_send_with_reply_and_block(m_conn, message.get(), timeout_milliseconds, err.get());

      if (err.isSet())
        ERR_FMT(PlatformSpecific, "DBus error: {}", err.message());

      if (!rawReply)
        ERR(ApiUnavailable, "DBus returned null without error");

      return Message(rawReply);
    }

    static auto busGet(const DBusBusType bus_type) -> Result<Connection> {
      Error           err;
      DBusConnection* rawConn = dbus_bus_get(bus_type, err.get());

      if (err.isSet())
        ERR_FMT(ApiUnavailable, "DBus bus_get failed: {}", err.message());

      if (!rawConn)
        ERR(ApiUnavailable, "dbus_bus_get returned null without error");

      return Connection(rawConn);
    }
  };

  /**
   * @brief Extract player name from MPRIS bus name
   */
  auto extractPlayerName(const String& busName) -> String {
    constexpr StringView prefix = "org.mpris.MediaPlayer2.";
    if (busName.starts_with(prefix))
      return busName.substr(prefix.size());
    return busName;
  }

  /**
   * @brief Fetch now playing information via MPRIS/DBus
   */
  auto fetchNowPlaying() -> Result<MediaData> {
    Connection connection = TRY(Connection::busGet(DBUS_BUS_SESSION));

    Option<String> activePlayer = None;

    // Find active MPRIS player
    {
      Message listNamesMsg = TRY(
        Message::newMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames")
      );

      Message listNamesReply = TRY(connection.sendWithReplyAndBlock(listNamesMsg, 100));

      MessageIter iter = listNamesReply.iterInit();
      if (!iter.isValid() || iter.getArgType() != DBUS_TYPE_ARRAY)
        ERR(ParseError, "Invalid DBus ListNames reply format: Expected array");

      MessageIter subIter = iter.recurse();
      if (!subIter.isValid())
        ERR(ParseError, "Invalid DBus ListNames reply format: Could not recurse into array");

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
      ERR(NotFound, "No active MPRIS players found");

    // Get metadata from active player
    Message msg = TRY(Message::newMethodCall(activePlayer->c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get"));

    if (!msg.appendArgs("org.mpris.MediaPlayer2.Player", "Metadata"))
      ERR(InternalError, "Failed to append arguments to Properties.Get message");

    Message reply = TRY(connection.sendWithReplyAndBlock(msg, 100));

    MediaData data;
    data.playerName = extractPlayerName(*activePlayer);

    MessageIter propIter = reply.iterInit();
    if (!propIter.isValid())
      ERR(ParseError, "Properties.Get reply has no arguments");

    if (propIter.getArgType() != DBUS_TYPE_VARIANT)
      ERR(ParseError, "Properties.Get reply argument is not a variant");

    MessageIter variantIter = propIter.recurse();
    if (!variantIter.isValid())
      ERR(ParseError, "Could not recurse into variant");

    if (variantIter.getArgType() != DBUS_TYPE_ARRAY || variantIter.getElementType() != DBUS_TYPE_DICT_ENTRY)
      ERR(ParseError, "Metadata is not a dictionary array");

    MessageIter dictIter = variantIter.recurse();
    if (!dictIter.isValid())
      ERR(ParseError, "Could not recurse into metadata dictionary");

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
        data.title = valueVariantIter.getString();
      } else if (*key == "xesam:album") {
        data.album = valueVariantIter.getString();
      } else if (*key == "xesam:artist") {
        if (valueVariantIter.getArgType() == DBUS_TYPE_ARRAY &&
            valueVariantIter.getElementType() == DBUS_TYPE_STRING) {
          MessageIter artistArrayIter = valueVariantIter.recurse();
          if (artistArrayIter.isValid())
            data.artist = artistArrayIter.getString();
        }
      }

      if (!dictIter.next())
        break;
    }

    return data;
  }
} // namespace now_playing::dbus

#endif // Linux/BSD

namespace {
  class NowPlayingPlugin : public IInfoProviderPlugin {
   private:
    PluginMetadata                m_metadata;
    now_playing::NowPlayingConfig m_config;
    now_playing::MediaData        m_data;
    Option<String>                m_lastError;
    bool                          m_ready = false;

   public:
    NowPlayingPlugin() {
      m_metadata = {
        .name    = "Now Playing",
        .version = "1.0.0",
        .author  = "Draconis++ Team",
#ifdef _WIN32
        .description = "Provides currently playing media information via Windows NPSM",
#elif defined(__APPLE__)
        .description = "Provides currently playing media information via macOS MediaRemote",
#else
        .description = "Provides currently playing media information via MPRIS/DBus",
#endif
        .type         = PluginType::InfoProvider,
        .dependencies = { .requiresNetwork = false, .requiresCaching = true },
      };
    }

    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
      return m_metadata;
    }

    [[nodiscard]] auto getProviderId() const -> String override {
      return "now_playing";
    }

    auto initialize(const PluginContext& /*ctx*/, PluginCache& /*cache*/) -> Result<Unit> override {
      m_config.enabled = true;
      m_ready          = true;
      return {};
    }

    auto shutdown() -> Unit override {
      m_ready = false;
    }

    [[nodiscard]] auto isReady() const -> bool override {
      return m_ready;
    }

    [[nodiscard]] auto isEnabled() const -> bool override {
      return m_config.enabled;
    }

    auto collectData(PluginCache& /*cache*/) -> Result<Unit> override {
      if (!m_ready)
        ERR(NotSupported, "Now Playing plugin is not ready");

      if (!m_config.enabled) {
        m_lastError = "Now Playing plugin is disabled";
        return {};
      }

      m_lastError = None;

      // Fetch fresh data using platform-specific implementation (no caching - media changes too frequently)
#ifdef _WIN32
      auto result = now_playing::npsm::FetchNowPlaying();
#elif defined(__APPLE__)
      auto result = now_playing::macos::fetchNowPlaying();
#else
      auto result = now_playing::dbus::fetchNowPlaying();
#endif

      if (!result) {
        m_lastError = result.error().message;
        return std::unexpected(result.error());
      }

      m_data = *result;

      return {};
    }

    [[nodiscard]] auto toJson() const -> Result<String> override {
      String jsonStr;
      if (glz::error_ctx errc = glz::write<glz::opts { .skip_null_members = true, .prettify = true }>(m_data, jsonStr); errc)
        ERR_FMT(ParseError, "Failed to serialize now playing data: {}", glz::format_error(errc, jsonStr));
      return jsonStr;
    }

    [[nodiscard]] auto getFields() const -> Map<String, String> override {
      Map<String, String> fields;

      if (m_data.title)
        fields["title"] = *m_data.title;

      if (m_data.artist)
        fields["artist"] = *m_data.artist;

      if (m_data.album)
        fields["album"] = *m_data.album;

      if (m_data.playerName)
        fields["player"] = *m_data.playerName;

      return fields;
    }

    [[nodiscard]] auto getDisplayValue() const -> Result<String> override {
      if (!m_data.title)
        ERR(NotFound, "No media currently playing");

      if (m_data.artist)
        return std::format("{} - {}", *m_data.artist, *m_data.title);

      return *m_data.title;
    }

    [[nodiscard]] auto getDisplayIcon() const -> String override {
      return " 󰝚  "; // Nerd Font music icon
    }

    [[nodiscard]] auto getDisplayLabel() const -> String override {
      return "Playing";
    }

    [[nodiscard]] auto getLastError() const -> Option<String> override {
      return m_lastError;
    }
  };
} // namespace

DRAC_PLUGIN(NowPlayingPlugin)
