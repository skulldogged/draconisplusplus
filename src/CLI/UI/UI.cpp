#include "UI.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#ifndef _WIN32
  #include <sys/ioctl.h> // TIOCGWINSZ
  #include <unistd.h>    // STDOUT_FILENO
#endif

#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include "AsciiArt.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::utils::localization;

namespace draconis::ui {
  using config::Config;
  using config::LogoProtocol;
  using config::UILayoutGroup;
  using config::UILayoutRow;

  using core::system::SystemInfo;

  constexpr Theme DEFAULT_THEME = {
    .icon  = LogColor::Cyan,
    .label = LogColor::Yellow,
    .value = LogColor::White,
  };

  [[maybe_unused]] static constexpr Icons NONE = {
    .calendar           = "",
    .desktopEnvironment = "",
    .disk               = "",
    .host               = "",
    .kernel             = "",
    .memory             = "",
    .cpu                = "",
    .gpu                = "",
    .uptime             = "",
    .os                 = "",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = "",
#endif
    .palette       = "",
    .shell         = "",
    .user          = "",
    .windowManager = "",
  };

  [[maybe_unused]] static constexpr Icons NERD = {
    .calendar           = "   ",
    .desktopEnvironment = " 󰇄  ",
    .disk               = " 󰋊  ",
    .host               = " 󰌢  ",
    .kernel             = "   ",
    .memory             = "   ",
#if DRAC_ARCH_64BIT
    .cpu = " 󰻠  ",
#else
    .cpu = " 󰻟  ",
#endif
    .gpu    = "   ",
    .uptime = "   ",
#ifdef __linux__
    .os = " 󰌽  ",
#elifdef __APPLE__
    .os = "   ",
#elifdef _WIN32
    .os = "   ",
#elifdef __FreeBSD__
    .os = "   ",
#else
    .os = "   ",
#endif
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 󰏖  ",
#endif
    .palette       = "   ",
    .shell         = "   ",
    .user          = "   ",
    .windowManager = "   ",
  };

  [[maybe_unused]] static constexpr Icons EMOJI = {
    .calendar           = " 📅 ",
    .desktopEnvironment = " 🖥️ ",
    .disk               = " 💾 ",
    .host               = " 💻 ",
    .kernel             = " 🫀 ",
    .memory             = " 🧠 ",
    .cpu                = " 💻 ",
    .gpu                = " 🎨 ",
    .uptime             = " ⏰ ",
    .os                 = " 🤖 ",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 📦 ",
#endif
    .palette       = " 🎨 ",
    .shell         = " 💲 ",
    .user          = " 👤 ",
    .windowManager = " 🪟 ",
  };

  constexpr inline Icons ICON_TYPE = NERD;

  struct RowInfo {
    String   icon;
    String   label;
    String   value;
    LogColor color    = LogColor::White;
    bool     autoWrap = false;
  };

  struct UIGroup {
    Vec<RowInfo>  rows;
    Vec<usize>    iconWidths;
    Vec<usize>    labelWidths;
    Vec<usize>    valueWidths;
    Vec<String>   coloredIcons;
    Vec<String>   coloredLabels;
    Vec<String>   coloredValues;
    Vec<bool>     autoWraps;
    Vec<LogColor> valueColors;
    usize         maxLabelWidth = 0;
  };

  namespace {
    struct LogoRender {
      Vec<String> lines;    // ASCII art lines when using ascii logos
      String      sequence; // Escape sequence when using inline image logos
      usize       width    = 0;
      usize       height   = 0;
      bool        isInline = false;
    };

    constexpr Array<char, 65> BASE64_TABLE = {
      'A',
      'B',
      'C',
      'D',
      'E',
      'F',
      'G',
      'H',
      'I',
      'J',
      'K',
      'L',
      'M',
      'N',
      'O',
      'P',
      'Q',
      'R',
      'S',
      'T',
      'U',
      'V',
      'W',
      'X',
      'Y',
      'Z',
      'a',
      'b',
      'c',
      'd',
      'e',
      'f',
      'g',
      'h',
      'i',
      'j',
      'k',
      'l',
      'm',
      'n',
      'o',
      'p',
      'q',
      'r',
      's',
      't',
      'u',
      'v',
      'w',
      'x',
      'y',
      'z',
      '0',
      '1',
      '2',
      '3',
      '4',
      '5',
      '6',
      '7',
      '8',
      '9',
      '+',
      '/',
      '\0'
    };

    auto Base64Encode(Span<const u8> data) -> String {
      String out;
      out.reserve(((data.size() + 2) / 3) * 4);

      usize idx = 0;

      while (idx + 2 < data.size()) {
        const u32 triple = (static_cast<u32>(data[idx]) << 16) |
          (static_cast<u32>(data[idx + 1]) << 8) |
          static_cast<u32>(data[idx + 2]);

        out.push_back(BASE64_TABLE.at((triple >> 18) & 0x3F));
        out.push_back(BASE64_TABLE.at((triple >> 12) & 0x3F));
        out.push_back(BASE64_TABLE.at((triple >> 6) & 0x3F));
        out.push_back(BASE64_TABLE.at(triple & 0x3F));

        idx += 3;
      }

      const usize remaining = data.size() - idx;

      if (remaining == 1) {
        const u32 triple = static_cast<u32>(data[idx]) << 16;
        out.push_back(BASE64_TABLE.at((triple >> 18) & 0x3F));
        out.push_back(BASE64_TABLE.at((triple >> 12) & 0x3F));
        out.push_back('=');
        out.push_back('=');
      } else if (remaining == 2) {
        const u32 triple = (static_cast<u32>(data[idx]) << 16) |
          (static_cast<u32>(data[idx + 1]) << 8);
        out.push_back(BASE64_TABLE.at((triple >> 18) & 0x3F));
        out.push_back(BASE64_TABLE.at((triple >> 12) & 0x3F));
        out.push_back(BASE64_TABLE.at((triple >> 6) & 0x3F));
        out.push_back('=');
      }

      return out;
    }

    auto Base64Encode(const String& str) -> String {
      const auto bytes = std::as_bytes(Span<const char>(str.data(), str.size()));
      return Base64Encode(Span<const u8>(reinterpret_cast<const u8*>(bytes.data()), bytes.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    auto ReadFileBytes(const String& path) -> Option<Vec<u8>> {
      std::ifstream file(path, std::ios::binary);

      if (!file)
        return None;

      file.seekg(0, std::ios::end);
      const std::streampos size = file.tellg();

      if (size <= 0)
        return None;

      Vec<u8> buffer(static_cast<usize>(size));

      file.seekg(0, std::ios::beg);
      file.read(reinterpret_cast<char*>(buffer.data()), size); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

      if (!file)
        return None;

      return buffer;
    }

    struct ImageSize {
      usize width  = 0;
      usize height = 0;
    };

    // Query terminal cell pixel dimensions (stdout). Returns None if unavailable.
    auto GetCellMetricsPx() -> Option<Pair<double, double>> {
#ifndef _WIN32
      winsize ws {};
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0 && ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_ypixel > 0) {
          const double cellW = static_cast<double>(ws.ws_xpixel) / static_cast<double>(ws.ws_col);
          const double cellH = static_cast<double>(ws.ws_ypixel) / static_cast<double>(ws.ws_row);
          return Pair<double, double> { cellW, cellH };
        }
      }
#endif
      return None;
    }

    // Detect if the terminal supports inline image rendering (Kitty/iTerm2 protocols).
    auto SupportsInlineImages(LogoProtocol protocol) -> bool {
#ifdef _WIN32
      // Windows terminals generally don't support inline images via these protocols
      return false;
#else
      // Must be a TTY to render images
      if (isatty(STDOUT_FILENO) == 0)
        return false;

      // Helper to get environment variable
      const auto getEnv = [](const char* name) -> Option<String> {
        if (const char* val = std::getenv(name))
          return String(val);
        return None;
      };

      // WezTerm supports both Kitty and iTerm2 protocols
      if (const Option<String> termProgram = getEnv("TERM_PROGRAM"))
        if (*termProgram == "WezTerm")
          return true;

      // Check for Kitty terminal
      if (protocol == LogoProtocol::Kitty || protocol == LogoProtocol::KittyDirect) {
        // KITTY_WINDOW_ID is set by Kitty terminal
        if (getEnv("KITTY_WINDOW_ID"))
          return true;

        // Check TERM for kitty
        if (const Option<String> term = getEnv("TERM"))
          if (term->find("kitty") != String::npos)
            return true;

        return false;
      }

      // Check for iTerm2
      if (protocol == LogoProtocol::Iterm2) {
        // TERM_PROGRAM is set by iTerm2
        if (const Option<String> termProgram = getEnv("TERM_PROGRAM"))
          if (*termProgram == "iTerm.app")
            return true;

        // LC_TERMINAL is another indicator
        if (const Option<String> lcTerminal = getEnv("LC_TERMINAL"))
          if (*lcTerminal == "iTerm2")
            return true;

        return false;
      }

      return false;
#endif
    }

    // Best-effort probe for PNG and JPEG dimensions to estimate cell footprint.
    auto ProbeImageSize(const String& path) -> Option<ImageSize> {
      std::ifstream file(path, std::ios::binary);
      if (!file)
        return None;

      // Read enough bytes for signatures/header parsing
      Array<u8, 64> header {};
      file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())); // NOLINT
      const auto readCount = static_cast<usize>(file.gcount());
      if (readCount < 24)
        return None;

      // PNG signature
      const Array<u8, 8> pngSig { 137, 80, 78, 71, 13, 10, 26, 10 };
      if (std::equal(pngSig.begin(), pngSig.end(), header.begin())) {
        // IHDR starts at byte 16
        const auto be32 = [](Span<const u8> data, usize offset) -> usize {
          return (static_cast<usize>(data[offset]) << 24) |
            (static_cast<usize>(data[offset + 1]) << 16) |
            (static_cast<usize>(data[offset + 2]) << 8) |
            static_cast<usize>(data[offset + 3]);
        };

        return ImageSize {
          .width  = be32(header, 16),
          .height = be32(header, 20),
        };
      }

      // JPEG SOF parsing
      if (header[0] == 0xFF && header[1] == 0xD8) {
        file.clear();
        file.seekg(2, std::ios::beg);

        while (file) {
          int markerPrefix = file.get();
          if (markerPrefix != 0xFF)
            break;
          int marker = file.get();
          // Skip padding bytes
          while (marker == 0xFF)
            marker = file.get();
          if (marker == 0xD9 || marker == EOF)
            break;

          // Read segment length
          Array<u8, 2> lenBytes {};
          file.read(reinterpret_cast<char*>(lenBytes.data()), 2); // NOLINT
          if (!file)
            break;
          const usize segLen = (static_cast<usize>(lenBytes[0]) << 8) | static_cast<usize>(lenBytes[1]);
          if (segLen < 2)
            break;

          // SOF0/1/2/3/5/6/7/9/A/B/C/D/E/F markers carry dimensions
          if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) || (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
            Array<u8, 5> sof {};
            file.read(reinterpret_cast<char*>(sof.data()), 5); // NOLINT
            if (!file)
              break;
            const usize height = (static_cast<usize>(sof[1]) << 8) | static_cast<usize>(sof[2]);
            const usize width  = (static_cast<usize>(sof[3]) << 8) | static_cast<usize>(sof[4]);
            if (height > 0 && width > 0)
              return ImageSize { .width = width, .height = height };
            break;
          }

          file.seekg(static_cast<std::streamoff>(segLen) - 2, std::ios::cur);
        }
      }

      return None;
    }

    auto BuildInlineSequence(const config::Logo& logoCfg, usize widthCells, usize heightCells, usize widthPx, usize heightPx) -> Option<String> {
      if (!logoCfg.imagePath)
        return None;

      const LogoProtocol protocol = logoCfg.getProtocol();
      String             sequence;

      if (protocol == LogoProtocol::KittyDirect) {
        const String payload = Base64Encode(*logoCfg.imagePath);

        sequence = "\033_Ga=T,f=100,t=f";

        if (widthCells > 0)
          sequence += std::format(",c={}", widthCells);
        else if (widthPx > 0)
          sequence += std::format(",s={}", widthPx);

        if (heightCells > 0)
          sequence += std::format(",r={}", heightCells);
        else if (heightPx > 0)
          sequence += std::format(",v={}", heightPx);

        sequence += ",C=1"; // keep cursor position stable
        sequence += ";";
        sequence += payload;
        sequence += "\033\\";

        return sequence;
      }

      const Option<Vec<u8>> bytes = ReadFileBytes(*logoCfg.imagePath);

      if (!bytes || bytes->empty())
        return None;

      const auto formatDimension = [](usize cells, usize pixels) -> Option<String> {
        if (cells > 0)
          return std::make_optional<String>(std::to_string(cells));
        if (pixels > 0)
          return std::make_optional<String>(std::format("{}px", pixels));
        return None;
      };

      if (protocol == LogoProtocol::Iterm2) {
        const String payload = Base64Encode(Span<const u8>(*bytes));

        sequence = "\033]1337;File=inline=1";
        sequence += std::format(";size={}", bytes->size());

        const std::filesystem::path filePath(*logoCfg.imagePath);
        const String                fileName = filePath.filename().string();
        if (!fileName.empty())
          sequence += std::format(";name={}", Base64Encode(fileName));

        if (const Option<String> widthArg = formatDimension(widthCells, widthPx))
          sequence += std::format(";width={}", *widthArg);
        if (const Option<String> heightArg = formatDimension(heightCells, heightPx))
          sequence += std::format(";height={}", *heightArg);

        sequence += ";preserveAspectRatio=1";
        sequence += ":";
        sequence += payload;
        sequence += "\a";

        return sequence;
      }

      const String payload = Base64Encode(Span<const u8>(*bytes));

      sequence = "\033_Ga=T,f=100";

      if (widthCells > 0)
        sequence += std::format(",c={}", widthCells);
      else if (widthPx > 0)
        sequence += std::format(",s={}", widthPx);

      if (heightCells > 0)
        sequence += std::format(",r={}", heightCells);
      else if (heightPx > 0)
        sequence += std::format(",v={}", heightPx);

      sequence += ",C=1"; // keep cursor position stable
      sequence += ";";
      sequence += payload;
      sequence += "\033\\";

      return sequence;
    }

    auto BuildInlineLogo(const config::Logo& logoCfg, [[maybe_unused]] usize suggestedHeight) -> Option<LogoRender> {
      if (!logoCfg.imagePath)
        return None;

      const LogoProtocol protocol = logoCfg.getProtocol();
      if (protocol != LogoProtocol::Kitty && protocol != LogoProtocol::KittyDirect && protocol != LogoProtocol::Iterm2)
        return None;

      // Check if terminal supports the requested image protocol; fall back to ASCII if not
      if (!SupportsInlineImages(protocol))
        return None;

      usize logoWidthPx  = logoCfg.width.value_or(0);
      usize logoHeightPx = logoCfg.height.value_or(0); // leave height unset unless explicitly provided

      // If only one dimension (or none) is provided, try to derive the other from the image aspect ratio.
      if (const Option<ImageSize> imgSize = ProbeImageSize(*logoCfg.imagePath)) {
        const double aspect = imgSize->height == 0 ? 1.0 : static_cast<double>(imgSize->width) / static_cast<double>(imgSize->height);

        if (logoWidthPx == 0 && logoHeightPx > 0)
          logoWidthPx = std::max<usize>(1, static_cast<usize>(std::llround(aspect * static_cast<double>(logoHeightPx))));
        else if (logoHeightPx == 0 && logoWidthPx > 0)
          logoHeightPx = std::max<usize>(1, static_cast<usize>(std::llround(static_cast<double>(logoWidthPx) / aspect)));
        else if (logoWidthPx == 0 && logoHeightPx == 0) {
          logoWidthPx  = imgSize->width;
          logoHeightPx = imgSize->height;
        }
      }

      // Only send explicit sizing when the user provided at least one dimension.
      const bool  hasExplicitSize = logoCfg.width.has_value() || logoCfg.height.has_value();
      const usize sendWidthPx     = hasExplicitSize ? logoWidthPx : 0;
      const usize sendHeightPx    = hasExplicitSize ? logoHeightPx : 0;

      // Determine how many terminal columns/rows the image will occupy to shift the text,
      // and prefer to send cell sizing when possible (kitty scales to fit).
      usize shiftWidthCells = 0,
            logoHeightCells = 0,
            sendWidthCells  = 0,
            sendHeightCells = 0;

      const usize renderWidthPx  = logoWidthPx,
                  renderHeightPx = logoHeightPx;

      if (const Option<Pair<double, double>> cellMetrics = GetCellMetricsPx()) {
        const double cellW = cellMetrics->first;
        const double cellH = cellMetrics->second;
        if (cellW > 0.0 && renderWidthPx > 0)
          shiftWidthCells = std::max<usize>(1, static_cast<usize>(std::llround(static_cast<double>(renderWidthPx) / cellW)));
        if (cellH > 0.0 && renderHeightPx > 0)
          logoHeightCells = std::max<usize>(1, static_cast<usize>(std::llround(static_cast<double>(renderHeightPx) / cellH)));

        if (hasExplicitSize) {
          if (cellW > 0.0 && sendWidthPx > 0)
            sendWidthCells = std::max<usize>(1, static_cast<usize>(std::llround(static_cast<double>(sendWidthPx) / cellW)));
          if (cellH > 0.0 && sendHeightPx > 0)
            sendHeightCells = std::max<usize>(1, static_cast<usize>(std::llround(static_cast<double>(sendHeightPx) / cellH)));
        }
      }

      // Fallback to a conservative estimate when the terminal doesn't report pixel metrics,
      // so explicit pixel sizing still has an effect on display size.
      if (shiftWidthCells == 0 && renderWidthPx > 0)
        shiftWidthCells = std::max<usize>(1, renderWidthPx / 10);
      if (logoHeightCells == 0 && renderHeightPx > 0)
        logoHeightCells = std::max<usize>(1, renderHeightPx / 10);

      if (hasExplicitSize) {
        if (sendWidthCells == 0 && sendWidthPx > 0)
          sendWidthCells = std::max<usize>(1, sendWidthPx / 10);
        if (sendHeightCells == 0 && sendHeightPx > 0)
          sendHeightCells = std::max<usize>(1, sendHeightPx / 10);
      }

      if (shiftWidthCells == 0)
        shiftWidthCells = 24; // minimal gap to avoid overlap when sizing is unknown

      const Option<String> sequence = BuildInlineSequence(logoCfg, sendWidthCells, sendHeightCells, sendWidthPx, sendHeightPx);

      if (!sequence)
        return None;

      LogoRender render;
      // width/height reported back are used for shifting/padding the text box; ensure
      // height reflects any sizing we applied (cells or derived pixels).

      render.width    = shiftWidthCells;
      render.height   = logoHeightCells > 0
          ? logoHeightCells
          : (
            sendHeightCells > 0
                ? sendHeightCells
                : (
                  renderHeightPx > 0
                      ? std::max<usize>(1, renderHeightPx / 10)
                      : 0
                )
          );
      render.isInline = true;
      render.sequence = *sequence;

      return render;
    }

#ifdef __linux__
    // clang-format off
    constexpr Array<Pair<StringView, StringView>, 13> distro_icons {{
      {      "arch", "   " },
      {     "nixos", "   " },
      {     "popos", "   " },
      {     "zorin", "   " },
      {    "debian", "   " },
      {    "fedora", "   " },
      {    "gentoo", "   " },
      {    "ubuntu", "   " },
      {    "alpine", "   " },
      {   "manjaro", "   " },
      { "linuxmint", "   " },
      { "voidlinux", "   " },
    }};
    // clang-format on

    constexpr auto GetDistroIcon(StringView distro) -> Option<StringView> {
      for (const auto& [distroName, distroIcon] : distro_icons)
        if (distro.contains(distroName))
          return distroIcon;

      return None;
    }
#endif // __linux__

    constexpr Array<StringView, 16> COLOR_CIRCLES {
      "\033[38;5;0m◯\033[0m",
      "\033[38;5;1m◯\033[0m",
      "\033[38;5;2m◯\033[0m",
      "\033[38;5;3m◯\033[0m",
      "\033[38;5;4m◯\033[0m",
      "\033[38;5;5m◯\033[0m",
      "\033[38;5;6m◯\033[0m",
      "\033[38;5;7m◯\033[0m",
      "\033[38;5;8m◯\033[0m",
      "\033[38;5;9m◯\033[0m",
      "\033[38;5;10m◯\033[0m",
      "\033[38;5;11m◯\033[0m",
      "\033[38;5;12m◯\033[0m",
      "\033[38;5;13m◯\033[0m",
      "\033[38;5;14m◯\033[0m",
      "\033[38;5;15m◯\033[0m"
    };

    constexpr auto IsWideCharacter(char32_t codepoint) -> bool {
      return (codepoint >= 0x1100 && codepoint <= 0x115F) || // Hangul Jamo
        (codepoint >= 0x2329 && codepoint <= 0x232A) ||      // Angle brackets
        (codepoint >= 0x2E80 && codepoint <= 0x2EFF) ||      // CJK Radicals Supplement
        (codepoint >= 0x2F00 && codepoint <= 0x2FDF) ||      // Kangxi Radicals
        (codepoint >= 0x2FF0 && codepoint <= 0x2FFF) ||      // Ideographic Description Characters
        (codepoint >= 0x3000 && codepoint <= 0x303E) ||      // CJK Symbols and Punctuation
        (codepoint >= 0x3041 && codepoint <= 0x3096) ||      // Hiragana
        (codepoint >= 0x3099 && codepoint <= 0x30FF) ||      // Katakana
        (codepoint >= 0x3105 && codepoint <= 0x312F) ||      // Bopomofo
        (codepoint >= 0x3131 && codepoint <= 0x318E) ||      // Hangul Compatibility Jamo
        (codepoint >= 0x3190 && codepoint <= 0x31BF) ||      // Kanbun
        (codepoint >= 0x31C0 && codepoint <= 0x31EF) ||      // CJK Strokes
        (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||      // Katakana Phonetic Extensions
        (codepoint >= 0x3200 && codepoint <= 0x32FF) ||      // Enclosed CJK Letters and Months
        (codepoint >= 0x3300 && codepoint <= 0x33FF) ||      // CJK Compatibility
        (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||      // CJK Unified Ideographs Extension A
        (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||      // CJK Unified Ideographs
        (codepoint >= 0xA000 && codepoint <= 0xA48F) ||      // Yi Syllables
        (codepoint >= 0xA490 && codepoint <= 0xA4CF) ||      // Yi Radicals
        (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||      // Hangul Syllables
        (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||      // CJK Compatibility Ideographs
        (codepoint >= 0xFE10 && codepoint <= 0xFE19) ||      // Vertical Forms
        (codepoint >= 0xFE30 && codepoint <= 0xFE6F) ||      // CJK Compatibility Forms
        (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||      // Fullwidth Forms
        (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||      // Fullwidth Forms
        (codepoint >= 0x20000 && codepoint <= 0x2FFFD) ||    // CJK Unified Ideographs Extension B, C, D, E
        (codepoint >= 0x30000 && codepoint <= 0x3FFFD);      // CJK Unified Ideographs Extension F
    }

    constexpr auto DecodeUTF8(const StringView& str, usize& pos) -> char32_t {
      if (pos >= str.length())
        return 0;

      const auto getByte = [&](usize index) -> u8 {
        return static_cast<u8>(str[index]);
      };

      const u8 first = getByte(pos++);

      if ((first & 0x80) == 0) // ASCII (0xxxxxxx)
        return first;

      if ((first & 0xE0) == 0xC0) {
        // 2-byte sequence (110xxxxx 10xxxxxx)
        if (pos >= str.length())
          return 0;

        const u8 second = getByte(pos++);

        return ((first & 0x1F) << 6) | (second & 0x3F);
      }

      if ((first & 0xF0) == 0xE0) {
        // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
        if (pos + 1 >= str.length())
          return 0;

        const u8 second = getByte(pos++);
        const u8 third  = getByte(pos++);

        return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
      }

      if ((first & 0xF8) == 0xF0) {
        // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        if (pos + 2 >= str.length())
          return 0;

        const u8 second = getByte(pos++);
        const u8 third  = getByte(pos++);
        const u8 fourth = getByte(pos++);

        return ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
      }

      return 0; // Invalid UTF-8
    }

    constexpr auto GetVisualWidth(const StringView& str) -> usize {
      usize width    = 0;
      bool  inEscape = false;
      usize pos      = 0;

      while (pos < str.length()) {
        const char current = str[pos];

        if (inEscape) {
          if (current == 'm' || current == '\\' || current == '\a')
            inEscape = false;

          pos++;
        } else if (current == '\033') {
          inEscape = true;
          pos++;
        } else {
          const char32_t codepoint = DecodeUTF8(str, pos);
          if (codepoint != 0)
            width += IsWideCharacter(codepoint) ? 2 : 1;
        }
      }

      return width;
    }

    /**
     * @brief Word-wrap text to a specified visual width with balanced line lengths
     * @param text The text to wrap
     * @param wrapWidth Maximum visual width per line (0 = no wrap)
     * @return Vector of wrapped lines
     */
    auto WordWrap(const StringView& text, const usize wrapWidth) -> Vec<String> {
      Vec<String> lines;

      if (wrapWidth == 0) {
        lines.emplace_back(text);
        return lines;
      }

      // First, split into words with their widths
      Vec<String>       words;
      Vec<usize>        wordWidths;
      std::stringstream textStream((String(text)));
      String            word;
      while (textStream >> word) {
        wordWidths.push_back(GetVisualWidth(word));
        words.push_back(std::move(word));
      }

      if (words.empty())
        return lines;

      // Calculate prefix sums for efficient width calculation
      // prefixWidth[i] = total width of words 0..i-1 including spaces between them
      Vec<usize> prefixWidth(words.size() + 1, 0);
      for (usize idx = 0; idx < words.size(); ++idx)
        prefixWidth[idx + 1] = prefixWidth[idx] + wordWidths[idx] + (idx > 0 ? 1 : 0);

      // Helper to get width of words[start..end) with spaces
      auto getLineWidth = [&](usize start, usize end) -> usize {
        if (start >= end)
          return 0;
        // Width is: sum of word widths + (end-start-1) spaces
        usize width = 0;
        for (usize idx = start; idx < end; ++idx)
          width += wordWidths[idx];
        if (end > start)
          width += end - start - 1; // spaces between words
        return width;
      };

      // Do greedy wrap first to determine minimum number of lines needed
      Vec<usize> greedyBreaks; // indices where lines start
      greedyBreaks.push_back(0);
      usize currentWidth = 0;
      for (usize idx = 0; idx < words.size(); ++idx) {
        const usize addedWidth = wordWidths[idx] + (currentWidth > 0 ? 1 : 0);
        if (currentWidth > 0 && currentWidth + addedWidth > wrapWidth) {
          greedyBreaks.push_back(idx);
          currentWidth = wordWidths[idx];
        } else {
          currentWidth += addedWidth;
        }
      }

      const usize numLines = greedyBreaks.size();

      // If only one line, return as-is
      if (numLines == 1) {
        String line;
        for (usize idx = 0; idx < words.size(); ++idx) {
          if (idx > 0)
            line += " ";
          line += words[idx];
        }
        lines.push_back(line);
        return lines;
      }

      // For balanced wrapping, find optimal break points
      // Use dynamic programming to find breaks that minimize max line length difference
      // For simplicity with 2 lines, just find the break that makes lines most equal
      if (numLines == 2) {
        usize bestBreak = 1;
        usize bestDiff  = std::numeric_limits<usize>::max();

        for (usize breakPoint = 1; breakPoint < words.size(); ++breakPoint) {
          const usize firstWidth  = getLineWidth(0, breakPoint);
          const usize secondWidth = getLineWidth(breakPoint, words.size());

          // Both lines must fit within wrapWidth
          if (firstWidth > wrapWidth || secondWidth > wrapWidth)
            continue;

          const usize diff = firstWidth > secondWidth ? firstWidth - secondWidth : secondWidth - firstWidth;
          if (diff < bestDiff) {
            bestDiff  = diff;
            bestBreak = breakPoint;
          }
        }

        // Build lines from best break
        String line1, line2;
        for (usize idx = 0; idx < bestBreak; ++idx) {
          if (idx > 0)
            line1 += " ";
          line1 += words[idx];
        }
        for (usize idx = bestBreak; idx < words.size(); ++idx) {
          if (idx > bestBreak)
            line2 += " ";
          line2 += words[idx];
        }
        lines.push_back(line1);
        lines.push_back(line2);
        return lines;
      }

      // For 3+ lines, use a generalized approach: aim for equal distribution
      const usize totalWidth  = getLineWidth(0, words.size());
      const usize targetWidth = (totalWidth + numLines - 1) / numLines;

      lines.clear();
      String currentLine;
      currentWidth    = 0;
      usize linesLeft = numLines;

      for (usize idx = 0; idx < words.size(); ++idx) {
        const usize widthIfAdded        = currentWidth + wordWidths[idx] + (currentWidth > 0 ? 1 : 0);
        const usize remainingWidth      = getLineWidth(idx, words.size());
        const usize avgRemainingPerLine = linesLeft > 0 ? (remainingWidth + linesLeft - 1) / linesLeft : 0;

        // Break if: exceeds max, or current line is at target and remaining fits well in remaining lines
        const bool shouldBreak = !currentLine.empty() &&
          (widthIfAdded > wrapWidth ||
           (currentWidth >= targetWidth && linesLeft > 1 && remainingWidth >= avgRemainingPerLine));

        if (shouldBreak) {
          lines.push_back(currentLine);
          currentLine.clear();
          currentWidth = 0;
          linesLeft--;
        }

        if (!currentLine.empty()) {
          currentLine += " ";
          currentWidth += 1;
        }
        currentLine += words[idx];
        currentWidth += wordWidths[idx];
      }

      if (!currentLine.empty())
        lines.push_back(currentLine);

      return lines;
    }

    constexpr auto CreateDistributedColorCircles(usize availableWidth) -> String {
      if (COLOR_CIRCLES.empty() || availableWidth == 0)
        return "";

      const usize
        circleWidth       = GetVisualWidth(COLOR_CIRCLES.at(0)),
        numCircles        = COLOR_CIRCLES.size(),
        minSpacingPerGap  = 1,
        totalMinSpacing   = (numCircles - 1) * minSpacingPerGap,
        totalCirclesWidth = numCircles * circleWidth,
        requiredWidth     = totalCirclesWidth + totalMinSpacing,
        effectiveWidth    = std::max(availableWidth, requiredWidth);

      if (numCircles == 1) {
        const usize padding = effectiveWidth / 2;
        return String(padding, ' ') + String(COLOR_CIRCLES.at(0));
      }

      const usize
        totalSpacing   = effectiveWidth - totalCirclesWidth,
        spacingBetween = totalSpacing / (numCircles - 1);

      String result;
      result.reserve(effectiveWidth);

      for (usize i = 0; i < numCircles; ++i) {
        if (i > 0)
          result.append(spacingBetween, ' ');

        const auto& circle = COLOR_CIRCLES.at(i);
        result.append(circle.data(), circle.size());
      }

      return result;
    }

    constexpr auto ProcessGroup(UIGroup& group) -> usize {
      if (group.rows.empty())
        return 0;

      group.iconWidths.reserve(group.rows.size());
      group.labelWidths.reserve(group.rows.size());
      group.valueWidths.reserve(group.rows.size());
      group.coloredIcons.reserve(group.rows.size());
      group.coloredLabels.reserve(group.rows.size());
      group.coloredValues.reserve(group.rows.size());
      group.autoWraps.reserve(group.rows.size());
      group.valueColors.reserve(group.rows.size());

      usize groupMaxWidth = 0;

      for (const RowInfo& row : group.rows) {
        const usize labelWidth = GetVisualWidth(row.label);
        group.maxLabelWidth    = std::max(group.maxLabelWidth, labelWidth);

        const usize iconW  = GetVisualWidth(row.icon);
        const usize valueW = GetVisualWidth(row.value);

        group.iconWidths.push_back(iconW);
        group.labelWidths.push_back(labelWidth);
        group.valueWidths.push_back(valueW);
        group.autoWraps.push_back(row.autoWrap);
        group.valueColors.push_back(row.color);

        String coloredIcon  = Stylize(row.icon, { .color = DEFAULT_THEME.icon });
        String coloredLabel = Stylize(row.label, { .color = DEFAULT_THEME.label });
        String coloredValue = Stylize(row.value, { .color = row.color });

        // Debug: check if colored strings have different visual widths
        usize coloredIconW  = GetVisualWidth(coloredIcon);
        usize coloredLabelW = GetVisualWidth(coloredLabel);
        usize coloredValueW = GetVisualWidth(coloredValue);
        if (coloredIconW != iconW || coloredLabelW != labelWidth || coloredValueW != valueW)
          debug_log(
            "Width mismatch! Icon: {} vs {}, Label: {} vs {}, Value: {} vs {}",
            // clang-format off
            iconW,      coloredIconW,
            labelWidth, coloredLabelW,
            valueW,     coloredValueW
            // clang-format on
          );

        group.coloredIcons.push_back(coloredIcon);
        group.coloredLabels.push_back(coloredLabel);
        group.coloredValues.push_back(coloredValue);

        // Don't include value width for autoWrap rows - they will wrap to fit available width
        if (!row.autoWrap)
          groupMaxWidth = std::max(groupMaxWidth, iconW + valueW);
      }

      groupMaxWidth += group.maxLabelWidth + 1;

      return groupMaxWidth;
    }

    constexpr auto RenderGroup(String& out, const UIGroup& group, const usize maxContentWidth, const String& hBorder, bool& hasRenderedContent) {
      if (group.rows.empty())
        return;

      if (hasRenderedContent) {
        out += "├";
        out += hBorder;
        out += "┤\n";
      }

      for (usize i = 0; i < group.rows.size(); ++i) {
        const usize    leftWidth  = group.iconWidths[i] + group.maxLabelWidth;
        const LogColor valueColor = group.valueColors[i];

        // Handle word wrapping if enabled for this row
        if (group.autoWraps[i]) {
          // Leave at least 1 space between label and value
          const usize       availableWidth = maxContentWidth - leftWidth - 1;
          const Vec<String> wrappedLines   = WordWrap(group.rows[i].value, availableWidth);

          if (!wrappedLines.empty()) {
            // First line: icon + label + first wrapped segment
            const String coloredFirstLine = Stylize(wrappedLines[0], { .color = valueColor });

            const usize
              firstLineWidth = GetVisualWidth(wrappedLines[0]),
              firstPadding   = (maxContentWidth >= leftWidth + firstLineWidth + 1)
                ? maxContentWidth - (leftWidth + firstLineWidth)
                : 1;

            out += "│";
            out += group.coloredIcons[i];
            out += group.coloredLabels[i];
            out.append(group.maxLabelWidth - group.labelWidths[i], ' ');
            out.append(firstPadding, ' ');
            out += coloredFirstLine;
            out += " │\n";

            // Subsequent lines: indent + wrapped segment (right-aligned)
            for (usize j = 1; j < wrappedLines.size(); ++j) {
              const String coloredLine = Stylize(wrappedLines[j], { .color = valueColor });

              const usize
                lineWidth   = GetVisualWidth(wrappedLines[j]),
                linePadding = (maxContentWidth > lineWidth)
                ? maxContentWidth - lineWidth
                : 0;

              out += "│";
              out.append(linePadding, ' ');
              out += coloredLine;
              out += " │\n";
            }
          }
        } else {
          // Normal rendering without word wrap
          const usize
            rightWidth = group.valueWidths[i],
            padding    = (maxContentWidth >= leftWidth + rightWidth)
               ? maxContentWidth - (leftWidth + rightWidth)
               : 0;

          out += "│";
          out += group.coloredIcons[i];
          out += group.coloredLabels[i];
          out.append(group.maxLabelWidth - group.labelWidths[i], ' ');
          out.append(padding, ' ');
          out += group.coloredValues[i];
          out += " │\n";
        }
      }

      hasRenderedContent = true;
    }

    constexpr auto ToLowerCopy(String str) -> String {
      std::ranges::transform(
        str,
        str.begin(),
        [](unsigned char chr) -> char { return static_cast<char>(std::tolower(chr)); }
      );
      return str;
    }

    constexpr auto ParsePluginKey(const String& key) -> Option<Pair<String, Option<String>>> {
      const StringView prefix = "plugin.";

      if (!key.starts_with(prefix))
        return None;

      const usize prefixLen = prefix.size();

      if (key.size() <= prefixLen)
        return None;

      const String remainder = key.substr(prefixLen);
      const usize  sepPos    = remainder.find('.');

      if (sepPos == String::npos)
        return Pair<String, Option<String>> { remainder, None };

      String pluginId = remainder.substr(0, sepPos),
             field    = remainder.substr(sepPos + 1);

      return Pair<String, Option<String>> { std::move(pluginId), Option<String>(std::move(field)) };
    }

    auto BuildDefaultLayout(const SystemInfo& data) -> Vec<UILayoutGroup> {
      Vec<UILayoutGroup> layout;

      UILayoutGroup introGroup;
      introGroup.name = "intro";
      introGroup.rows.push_back(UILayoutRow { .key = "date" });

#if DRAC_ENABLE_PLUGINS
      for (const auto& [pluginId, _] : data.pluginDisplay)
        introGroup.rows.push_back(UILayoutRow { .key = std::format("plugin.{}", pluginId) });
#endif

      layout.push_back(std::move(introGroup));

      UILayoutGroup systemGroup;
      systemGroup.name = "system";
      systemGroup.rows.push_back(UILayoutRow { .key = "host" });
      systemGroup.rows.push_back(UILayoutRow { .key = "os" });
      systemGroup.rows.push_back(UILayoutRow { .key = "kernel" });
      layout.push_back(std::move(systemGroup));

      UILayoutGroup hardwareGroup;
      hardwareGroup.name = "hardware";
      hardwareGroup.rows.push_back(UILayoutRow { .key = "ram" });
      hardwareGroup.rows.push_back(UILayoutRow { .key = "disk" });
      hardwareGroup.rows.push_back(UILayoutRow { .key = "cpu" });
      hardwareGroup.rows.push_back(UILayoutRow { .key = "gpu" });
      hardwareGroup.rows.push_back(UILayoutRow { .key = "uptime" });
      layout.push_back(std::move(hardwareGroup));

      UILayoutGroup softwareGroup;
      softwareGroup.name = "software";
      softwareGroup.rows.push_back(UILayoutRow { .key = "shell" });
#if DRAC_ENABLE_PACKAGECOUNT
      softwareGroup.rows.push_back(UILayoutRow { .key = "packages" });
#endif
      layout.push_back(std::move(softwareGroup));

      UILayoutGroup envGroup;
      envGroup.name = "environment";
      envGroup.rows.push_back(UILayoutRow { .key = "de" });
      envGroup.rows.push_back(UILayoutRow { .key = "wm" });
      layout.push_back(std::move(envGroup));

      return layout;
    }

    auto BuildRowFromLayout(
      const UILayoutRow& layoutRow,
      const Icons&       iconType,
      const SystemInfo&  data,
      Option<StringView> distroIcon
    ) -> Option<RowInfo> {
      const String keyLower = ToLowerCopy(layoutRow.key);

      if (const auto pluginKey = ParsePluginKey(layoutRow.key)) {
#if DRAC_ENABLE_PLUGINS
        const String&         pluginId       = pluginKey->first;
        const Option<String>& fieldName      = pluginKey->second;
        Option<String>        value          = None;
        String                icon           = String(iconType.palette);
        String                label          = pluginId;
        const auto            displayIt      = data.pluginDisplay.find(pluginId);
        const bool            hasDisplayInfo = displayIt != data.pluginDisplay.end();

        if (fieldName) {
          if (const auto pluginDataIt = data.pluginData.find(pluginId); pluginDataIt != data.pluginData.end())
            if (const auto valueIt = pluginDataIt->second.find(*fieldName); valueIt != pluginDataIt->second.end())
              value = valueIt->second;

          if (!value)
            return None;

          if (hasDisplayInfo) {
            if (!displayIt->second.icon.empty())
              icon = displayIt->second.icon;
            if (!displayIt->second.label.empty())
              label = std::format("{} {}", displayIt->second.label, *fieldName);
            else
              label = *fieldName;
          } else
            label = *fieldName;
        } else {
          if (!hasDisplayInfo)
            return None;

          icon  = displayIt->second.icon.empty() ? iconType.palette : displayIt->second.icon;
          label = displayIt->second.label.empty() ? pluginId : displayIt->second.label;

          if (displayIt->second.value)
            value = *displayIt->second.value;
        }

        if (!value)
          return None;

        RowInfo row {
          .icon     = std::move(icon),
          .label    = std::move(label),
          .value    = *std::move(value),
          .color    = layoutRow.color,
          .autoWrap = layoutRow.autoWrap
        };

        if (layoutRow.icon)
          row.icon = *layoutRow.icon;
        if (layoutRow.label)
          row.label = *layoutRow.label;

        return row;
#else
        (void)pluginKey;
        return None;
#endif
      }

      // clang-format off
      using RowBuilder = std::function<Option<RowInfo>(const Icons&, const SystemInfo&, Option<StringView>)>;
      static const std::unordered_map<String, RowBuilder> K_ROW_BUILDERS = {
        { "date", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.date) return None;
          return RowInfo { .icon = String(icons.calendar), .label = String(_("date")), .value = *info.date };
        }},
        { "host", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.host || info.host->empty()) return None;
          return RowInfo { .icon = String(icons.host), .label = String(_("host")), .value = *info.host };
        }},
        { "os", [](const Icons& icons, const SystemInfo& info, Option<StringView> distro) -> Option<RowInfo> {
          if (!info.operatingSystem) return None;
          return RowInfo {
            .icon  = distro ? String(*distro) : String(icons.os),
            .label = String(_("os")),
            .value = std::format("{} {}", info.operatingSystem->name, info.operatingSystem->version)
          };
        }},
        { "kernel", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.kernelVersion) return None;
          return RowInfo { .icon = String(icons.kernel), .label = String(_("kernel")), .value = *info.kernelVersion };
        }},
        { "ram", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.memInfo) return None;
          return RowInfo {
            .icon  = String(icons.memory),
            .label = String(_("ram")),
            .value = std::format("{}/{}", BytesToGiB(info.memInfo->usedBytes), BytesToGiB(info.memInfo->totalBytes))
          };
        }},
        { "disk", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.diskUsage) return None;
          return RowInfo {
            .icon  = String(icons.disk),
            .label = String(_("disk")),
            .value = std::format("{}/{}", BytesToGiB(info.diskUsage->usedBytes), BytesToGiB(info.diskUsage->totalBytes))
          };
        }},
        { "cpu", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.cpuModel) return None;
          return RowInfo { .icon = String(icons.cpu), .label = String(_("cpu")), .value = *info.cpuModel };
        }},
        { "gpu", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.gpuModel) return None;
          return RowInfo { .icon = String(icons.gpu), .label = String(_("gpu")), .value = *info.gpuModel };
        }},
        { "uptime", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.uptime) return None;
          return RowInfo {
            .icon  = String(icons.uptime),
            .label = String(_("uptime")),
            .value = std::format("{}", SecondsToFormattedDuration { *info.uptime })
          };
        }},
        { "shell", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.shell) return None;
          return RowInfo { .icon = String(icons.shell), .label = String(_("shell")), .value = *info.shell };
        }},
#if DRAC_ENABLE_PACKAGECOUNT
        { "packages", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.packageCount || *info.packageCount == 0) return None;
          return RowInfo { .icon = String(icons.package), .label = String(_("packages")), .value = std::format("{}", *info.packageCount) };
        }},
        { "package", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.packageCount || *info.packageCount == 0) return None;
          return RowInfo { .icon = String(icons.package), .label = String(_("packages")), .value = std::format("{}", *info.packageCount) };
        }},
#endif
        { "de", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.desktopEnv) return None;
          if (info.windowMgr && *info.desktopEnv == *info.windowMgr) return None;
          return RowInfo { .icon = String(icons.desktopEnvironment), .label = String(_("de")), .value = *info.desktopEnv };
        }},
        { "wm", [](const Icons& icons, const SystemInfo& info, Option<StringView>) -> Option<RowInfo> {
          if (!info.windowMgr) return None;
          return RowInfo { .icon = String(icons.windowManager), .label = String(_("wm")), .value = *info.windowMgr };
        }},
      };
      // clang-format on

      const auto builderIt = K_ROW_BUILDERS.find(keyLower);
      if (builderIt == K_ROW_BUILDERS.end())
        return None;

      Option<RowInfo> rowOpt = builderIt->second(iconType, data, distroIcon);
      if (!rowOpt)
        return None;

      RowInfo row = std::move(*rowOpt);

      if (layoutRow.icon)
        row.icon = *layoutRow.icon;
      if (layoutRow.label)
        row.label = *layoutRow.label;
      row.color    = layoutRow.color;
      row.autoWrap = layoutRow.autoWrap;

      return row;
    }

  } // namespace

  auto CreateUI(const Config& config, const SystemInfo& data, bool noAscii) -> String {
    const String& name     = config.general.getName();
    const Icons&  iconType = ICON_TYPE;

    Option<StringView> distroIcon = None;
#ifdef __linux__
    if (data.operatingSystem)
      distroIcon = GetDistroIcon(data.operatingSystem->id);
#endif

    const Vec<UILayoutGroup>* layoutGroups = &config.ui.layout;
    Vec<UILayoutGroup>        defaultLayout;

    if (layoutGroups->empty()) {
      defaultLayout = BuildDefaultLayout(data);
      layoutGroups  = &defaultLayout;
    }

    Vec<UIGroup> groups;
    groups.reserve(layoutGroups->size());

    for (const auto& groupCfg : *layoutGroups) {
      UIGroup group;
      group.rows.reserve(groupCfg.rows.size());

      for (const auto& rowCfg : groupCfg.rows)
        if (auto row = BuildRowFromLayout(rowCfg, iconType, data, distroIcon))
          group.rows.push_back(std::move(*row));

      groups.push_back(std::move(group));
    }

    usize maxContentWidth = 0;

    for (UIGroup& group : groups) {
      if (group.rows.empty())
        continue;

      maxContentWidth = std::max(maxContentWidth, ProcessGroup(group));
    }

    String greetingLine = std::format("{}{}", iconType.user, _format_f("hello", name));
    maxContentWidth     = std::max(maxContentWidth, GetVisualWidth(greetingLine));

    // Calculate width needed for color circles (including minimum spacing)
    const usize circleWidth       = GetVisualWidth(COLOR_CIRCLES[0]);
    const usize totalCirclesWidth = COLOR_CIRCLES.size() * circleWidth;
    const usize minSpacingPerGap  = 1;
    const usize totalMinSpacing   = (COLOR_CIRCLES.size() - 1) * minSpacingPerGap;
    const usize colorCirclesWidth = GetVisualWidth(iconType.palette) + totalCirclesWidth + totalMinSpacing;
    maxContentWidth               = std::max(maxContentWidth, colorCirclesWidth);

    String out;

    usize estimatedLines = 4;

    for (const UIGroup& grp : groups)
      estimatedLines += grp.rows.empty() ? 0 : (grp.rows.size() + 1);

    out.reserve(estimatedLines * (maxContentWidth + 4));

    const usize innerWidth = maxContentWidth + 1;

    String hBorder;
    hBorder.reserve(innerWidth * 3);
    for (usize i = 0; i < innerWidth; ++i) hBorder += "─";

    const auto createLine = [&](const String& left, const String& right = "") -> void {
      const usize leftWidth  = GetVisualWidth(left);
      const usize rightWidth = GetVisualWidth(right);
      const usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

      out += "│";
      out += left;
      out.append(padding, ' ');
      out += right;
      out += " │\n";
    };

    const auto createLeftAlignedLine =
      [&](const String& content) -> void { createLine(content, ""); };

    // Top border and greeting
    out += "╭";
    out += hBorder;
    out += "╮\n";

    createLeftAlignedLine(Stylize(greetingLine, { .color = DEFAULT_THEME.icon }));

    // Palette line
    out += "├";
    out += hBorder;
    out += "┤\n";

    const String paletteIcon    = Stylize(iconType.palette, { .color = DEFAULT_THEME.icon });
    const usize  availableWidth = maxContentWidth - GetVisualWidth(paletteIcon);
    createLeftAlignedLine(paletteIcon + CreateDistributedColorCircles(availableWidth));

    bool hasRenderedContent = true;

    for (const UIGroup& group : groups)
      RenderGroup(out, group, maxContentWidth, hBorder, hasRenderedContent);

    out += "╰";
    out += hBorder;
    out += "╯\n";

    Vec<String>       boxLines;
    std::stringstream stream(out);
    String            line;

    while (std::getline(stream, line, '\n'))
      boxLines.push_back(line);

    if (!boxLines.empty() && boxLines.back().empty())
      boxLines.pop_back();

    usize  boxWidth = GetVisualWidth(boxLines[0]);
    String emptyBox = "│" + String(boxWidth - 2, ' ') + "│";

    Vec<String> logoLines;
    usize       maxLogoW      = 0;
    usize       logoHeightOpt = 0;
    String      inlineSequence;
    bool        isInlineLogo = false;

    if (!noAscii) {
      if (const Option<LogoRender> inlineLogo = BuildInlineLogo(config.logo, boxLines.size())) {
        logoLines      = inlineLogo->lines;
        maxLogoW       = inlineLogo->width;
        logoHeightOpt  = inlineLogo->height;
        isInlineLogo   = inlineLogo->isInline;
        inlineSequence = inlineLogo->sequence;
      }

      if (!isInlineLogo && logoLines.empty()) {
        const Vec<StringView> asciiLines = ascii::GetAsciiArt(data.operatingSystem->id);

        for (const auto& aLine : asciiLines) {
          logoLines.emplace_back(aLine);
          maxLogoW = std::max(maxLogoW, GetVisualWidth(aLine));
        }
      }
    }

    if (!isInlineLogo && logoLines.empty())
      return out;

    const usize logoHeight = isInlineLogo ? (logoHeightOpt ? logoHeightOpt : boxLines.size()) : logoLines.size();
    String      emptyLogo(maxLogoW, ' ');

    // Inline logo: emit the image to stdout once, then print the box shifted right by logo width.
    if (isInlineLogo) {
      const usize shift       = maxLogoW + 2 /* logo width plus gap */,
                  totalHeight = std::max(logoHeight, boxLines.size()),
                  logoPadTop  = (totalHeight > logoHeight) ? (totalHeight - logoHeight) / 2 : 0,
                  boxPadTop   = (totalHeight > boxLines.size()) ? (totalHeight - boxLines.size()) / 2 : 0;

      if (!inlineSequence.empty()) {
        std::cout << "\033[s"; // save cursor
        if (logoPadTop > 0)
          std::cout << std::format("\033[{}B", logoPadTop); // move down to vertically center logo
        std::cout << inlineSequence;
        if (logoPadTop > 0)
          std::cout << std::format("\033[{}A", logoPadTop); // move back up
        std::cout << "\033[u" << std::flush;                // restore cursor
      }

      String newOut;

      for (usize i = 0; i < totalHeight; ++i) {
        const bool    isBoxLine = i >= boxPadTop && i < boxPadTop + boxLines.size();
        const String& line      = isBoxLine ? boxLines[i - boxPadTop] : emptyBox;

        newOut += "\r";
        newOut += std::format("\033[{}C", shift);
        newOut += line;
        newOut += "\n";
      }

      return newOut;
    }

    // ASCII logo: center relative to box height
    const usize totalHeight = std::max(logoHeight, boxLines.size()),
                logoPadTop  = (totalHeight > logoHeight) ? (totalHeight - logoHeight) / 2 : 0,
                boxPadTop   = (totalHeight > boxLines.size()) ? (totalHeight - boxLines.size()) / 2 : 0;

    String newOut;

    for (usize i = 0; i < totalHeight; ++i) {
      String outputLine;

      if (i < logoPadTop || i >= logoPadTop + logoHeight) {
        outputLine += emptyLogo;
      } else {
        const String& logoLine = logoLines[i - logoPadTop];

        const usize logoLineWidth = GetVisualWidth(logoLine);
        const usize logoPadding =
          maxLogoW > logoLineWidth
          ? maxLogoW - logoLineWidth
          : 0;

        outputLine.append(logoLine.data(), logoLine.size());
        outputLine.append(logoPadding, ' ');
        outputLine += "\033[0m";
      }

      outputLine += "  ";

      if (i < boxPadTop || i >= boxPadTop + boxLines.size())
        outputLine += emptyBox;
      else
        outputLine += boxLines[i - boxPadTop];

      newOut += outputLine + "\n";
    }

    return newOut;
  }
} // namespace draconis::ui
