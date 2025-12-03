#include "UI.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

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
#if DRAC_ENABLE_NOWPLAYING
    .music = "",
#endif
    .os = "",
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
#if DRAC_ENABLE_NOWPLAYING
    .music = "   ",
#endif
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
#if DRAC_ENABLE_NOWPLAYING
    .music = " 🎵 ",
#endif
    .os = " 🤖 ",
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
    String icon;
    String label;
    String value;
  };

  struct UIGroup {
    Vec<RowInfo> rows;
    Vec<usize>   iconWidths;
    Vec<usize>   labelWidths;
    Vec<usize>   valueWidths;
    Vec<String>  coloredIcons;
    Vec<String>  coloredLabels;
    Vec<String>  coloredValues;
    usize        maxLabelWidth = 0;
  };

  namespace {
    struct LogoRender {
      Vec<String> lines;    // ASCII art lines when using ascii logos
      String      sequence; // Kitty escape sequence when using kitty logos
      usize       width   = 0;
      usize       height  = 0;
      bool        isKitty = false;
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

    fn Base64Encode(Span<const u8> data) -> String {
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

    fn Base64Encode(const String& str) -> String {
      const auto bytes = std::as_bytes(Span<const char>(str.data(), str.size()));
      return Base64Encode(Span<const u8>(reinterpret_cast<const u8*>(bytes.data()), bytes.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    fn ReadFileBytes(const String& path) -> Option<Vec<u8>> {
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

    fn BuildKittySequence(const config::Logo& logoCfg, usize widthCells, usize heightCells) -> Option<String> {
      if (!logoCfg.imagePath)
        return None;

      String sequence;

      if (logoCfg.getProtocol() == LogoProtocol::KittyDirect) {
        const String payload = Base64Encode(*logoCfg.imagePath);

        sequence = "\033_Ga=T,f=100,t=f";

        if (widthCells > 0)
          sequence += std::format(",c={}", widthCells);

        if (heightCells > 0)
          sequence += std::format(",r={}", heightCells);

        sequence += ";";
        sequence += payload;
        sequence += "\033\\";

        return sequence;
      }

      const Option<Vec<u8>> bytes = ReadFileBytes(*logoCfg.imagePath);

      if (!bytes)
        return None;

      const String payload = Base64Encode(Span<const u8>(*bytes));

      sequence = "\033_Ga=T,f=100";

      if (widthCells > 0)
        sequence += std::format(",s={}", widthCells);

      if (heightCells > 0)
        sequence += std::format(",v={}", heightCells);

      sequence += ";";
      sequence += payload;
      sequence += "\033\\";

      return sequence;
    }

    fn BuildKittyLogo(const config::Logo& logoCfg, usize suggestedHeight) -> Option<LogoRender> {
      if (!logoCfg.imagePath)
        return None;

      const usize logoWidth  = std::max<usize>(1, logoCfg.width.value_or(24));
      usize       logoHeight = logoCfg.height.value_or(suggestedHeight);

      if (logoHeight == 0)
        logoHeight = suggestedHeight == 0 ? 12 : suggestedHeight;

      const Option<String> sequence = BuildKittySequence(logoCfg, logoWidth, logoHeight);

      if (!sequence)
        return None;

      LogoRender render;
      render.width    = logoWidth;
      render.height   = logoHeight;
      render.isKitty  = true;
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

    constexpr fn GetDistroIcon(StringView distro) -> Option<StringView> {
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

    constexpr fn IsWideCharacter(char32_t codepoint) -> bool {
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

    constexpr fn DecodeUTF8(const StringView& str, usize& pos) -> char32_t {
      if (pos >= str.length())
        return 0;

      const fn getByte = [&](usize index) -> u8 {
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

    constexpr fn GetVisualWidth(const StringView& str) -> usize {
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

    constexpr fn CreateDistributedColorCircles(usize availableWidth) -> String {
      if (COLOR_CIRCLES.empty() || availableWidth == 0)
        return "";

      const usize circleWidth = GetVisualWidth(COLOR_CIRCLES.at(0));
      const usize numCircles  = COLOR_CIRCLES.size();

      const usize minSpacingPerGap  = 1;
      const usize totalMinSpacing   = (numCircles - 1) * minSpacingPerGap;
      const usize totalCirclesWidth = numCircles * circleWidth;
      const usize requiredWidth     = totalCirclesWidth + totalMinSpacing;
      const usize effectiveWidth    = std::max(availableWidth, requiredWidth);

      if (numCircles == 1) {
        const usize padding = effectiveWidth / 2;
        return String(padding, ' ') + String(COLOR_CIRCLES.at(0));
      }

      const usize totalSpacing   = effectiveWidth - totalCirclesWidth;
      const usize spacingBetween = totalSpacing / (numCircles - 1);

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

    constexpr fn ProcessGroup(UIGroup& group) -> usize {
      if (group.rows.empty())
        return 0;

      group.iconWidths.reserve(group.rows.size());
      group.labelWidths.reserve(group.rows.size());
      group.valueWidths.reserve(group.rows.size());
      group.coloredIcons.reserve(group.rows.size());
      group.coloredLabels.reserve(group.rows.size());
      group.coloredValues.reserve(group.rows.size());

      usize groupMaxWidth = 0;

      for (const RowInfo& row : group.rows) {
        const usize labelWidth = GetVisualWidth(row.label);
        group.maxLabelWidth    = std::max(group.maxLabelWidth, labelWidth);

        const usize iconW  = GetVisualWidth(row.icon);
        const usize valueW = GetVisualWidth(row.value);

        group.iconWidths.push_back(iconW);
        group.labelWidths.push_back(labelWidth);
        group.valueWidths.push_back(valueW);

        String coloredIcon  = Stylize(row.icon, { .color = DEFAULT_THEME.icon });
        String coloredLabel = Stylize(row.label, { .color = DEFAULT_THEME.label });
        String coloredValue = Stylize(row.value, { .color = DEFAULT_THEME.value });

        // Debug: check if colored strings have different visual widths
        usize coloredIconW  = GetVisualWidth(coloredIcon);
        usize coloredLabelW = GetVisualWidth(coloredLabel);
        usize coloredValueW = GetVisualWidth(coloredValue);
        if (coloredIconW != iconW || coloredLabelW != labelWidth || coloredValueW != valueW)
          debug_log(
            "Width mismatch! Icon: {} vs {}, Label: {} vs {}, Value: {} vs {}",
            // clang-format off
            iconW, coloredIconW,
            labelWidth, coloredLabelW,
            valueW, coloredValueW
            // clang-format on
          );

        group.coloredIcons.push_back(coloredIcon);
        group.coloredLabels.push_back(coloredLabel);
        group.coloredValues.push_back(coloredValue);

        groupMaxWidth = std::max(groupMaxWidth, iconW + valueW); // label handled after loop
      }

      groupMaxWidth += group.maxLabelWidth + 1;

      return groupMaxWidth;
    }

    constexpr fn RenderGroup(String& out, const UIGroup& group, const usize maxContentWidth, const String& hBorder, bool& hasRenderedContent) {
      if (group.rows.empty())
        return;

      if (hasRenderedContent) {
        out += "├";
        out += hBorder;
        out += "┤\n";
      }

      for (usize i = 0; i < group.rows.size(); ++i) {
        const usize leftWidth  = group.iconWidths[i] + group.maxLabelWidth;
        const usize rightWidth = group.valueWidths[i];
        const usize padding    = (maxContentWidth >= leftWidth + rightWidth)
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

      hasRenderedContent = true;
    }

    constexpr fn WordWrap(const StringView& text, const usize wrapWidth) -> Vec<String> {
      Vec<String> lines;

      if (wrapWidth == 0) {
        lines.emplace_back(text);
        return lines;
      }

      std::stringstream textStream((String(text)));
      String            word;
      String            currentLine;

      while (textStream >> word) {
        if (!currentLine.empty() && GetVisualWidth(currentLine) + GetVisualWidth(word) + 1 > wrapWidth) {
          lines.emplace_back(currentLine);
          currentLine.clear();
        }

        if (!currentLine.empty())
          currentLine += " ";

        currentLine += word;
      }

      if (!currentLine.empty())
        lines.emplace_back(currentLine);

      return lines;
    }

    constexpr fn ToLowerCopy(String str) -> String {
      std::ranges::transform(
        str,
        str.begin(),
        [](unsigned char chr) -> char {
          return static_cast<char>(std::tolower(chr));
        }
      );
      return str;
    }

    constexpr fn ParsePluginKey(const String& key) -> Option<Pair<String, Option<String>>> {
      const StringView prefixDot        = "plugin.";
      const StringView prefixUnderscore = "plugin_";

      if (!key.starts_with(prefixDot) && !key.starts_with(prefixUnderscore))
        return None;

      const usize prefixLen = key.starts_with(prefixDot) ? prefixDot.size() : prefixUnderscore.size();
      const char  separator = key.starts_with(prefixDot) ? '.' : '_';

      if (key.size() <= prefixLen)
        return None;

      const String remainder = key.substr(prefixLen);
      const usize  sepPos    = remainder.find(separator);

      if (sepPos == String::npos)
        return Pair<String, Option<String>> { remainder, None };

      String pluginId = remainder.substr(0, sepPos);
      String field    = remainder.substr(sepPos + 1);

      return Pair<String, Option<String>> { std::move(pluginId), Option<String>(std::move(field)) };
    }

    fn BuildDefaultLayout(const SystemInfo& data) -> Vec<UILayoutGroup> {
      Vec<UILayoutGroup> layout;

      UILayoutGroup introGroup;
      introGroup.name = "intro";
      introGroup.rows.push_back(UILayoutRow { .key = "date" });

#if DRAC_ENABLE_PLUGINS
      for (const auto& [pluginId, displayInfo] : data.pluginDisplay) {
        (void)displayInfo;
        introGroup.rows.push_back(UILayoutRow { .key = std::format("plugin.{}", pluginId) });
      }
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

    fn BuildRowFromLayout(
      const UILayoutRow&      layoutRow,
      const Icons&            iconType,
      const SystemInfo&       data,
      Option<StringView>      distroIcon,
      bool&                   nowPlayingConsumed
    ) -> Option<RowInfo> {
      const String keyLower = ToLowerCopy(layoutRow.key);

      if (const auto pluginKey = ParsePluginKey(layoutRow.key)) {
#if DRAC_ENABLE_PLUGINS
        const String&             pluginId  = pluginKey->first;
        const Option<String>&     fieldName = pluginKey->second;
        Option<String> value     = None;
        String         icon      = String(iconType.palette);
        String         label     = pluginId;
        const auto     displayIt = data.pluginDisplay.find(pluginId);
        const bool     hasDisplayInfo = displayIt != data.pluginDisplay.end();

        if (fieldName) {
          if (const auto pluginDataIt = data.pluginData.find(pluginId); pluginDataIt != data.pluginData.end()) {
            if (const auto valueIt = pluginDataIt->second.find(*fieldName); valueIt != pluginDataIt->second.end())
              value = valueIt->second;
          }

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
          .icon  = std::move(icon),
          .label = std::move(label),
          .value = *std::move(value)
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

      RowInfo row;

      if (keyLower == "date") {
        if (!data.date)
          return None;
        row.icon  = iconType.calendar;
        row.label = _("date");
        row.value = *data.date;
      } else if (keyLower == "host") {
        if (!data.host || data.host->empty())
          return None;
        row.icon  = iconType.host;
        row.label = _("host");
        row.value = *data.host;
      } else if (keyLower == "os") {
        if (!data.operatingSystem)
          return None;
        row.icon = distroIcon ? String(*distroIcon) : String(iconType.os);
        row.label = _("os");
        row.value = std::format("{} {}", data.operatingSystem->name, data.operatingSystem->version);
      } else if (keyLower == "kernel") {
        if (!data.kernelVersion)
          return None;
        row.icon  = iconType.kernel;
        row.label = _("kernel");
        row.value = *data.kernelVersion;
      } else if (keyLower == "ram") {
        if (!data.memInfo)
          return None;
        row.icon  = iconType.memory;
        row.label = _("ram");
        row.value = std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes));
      } else if (keyLower == "disk") {
        if (!data.diskUsage)
          return None;
        row.icon  = iconType.disk;
        row.label = _("disk");
        row.value = std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes));
      } else if (keyLower == "cpu") {
        if (!data.cpuModel)
          return None;
        row.icon  = iconType.cpu;
        row.label = _("cpu");
        row.value = *data.cpuModel;
      } else if (keyLower == "gpu") {
        if (!data.gpuModel)
          return None;
        row.icon  = iconType.gpu;
        row.label = _("gpu");
        row.value = *data.gpuModel;
      } else if (keyLower == "uptime") {
        if (!data.uptime)
          return None;
        row.icon  = iconType.uptime;
        row.label = _("uptime");
        row.value = std::format("{}", SecondsToFormattedDuration { *data.uptime });
      } else if (keyLower == "shell") {
        if (!data.shell)
          return None;
        row.icon  = iconType.shell;
        row.label = _("shell");
        row.value = *data.shell;
      }
#if DRAC_ENABLE_PACKAGECOUNT
      else if (keyLower == "packages" || keyLower == "package") {
        if (!data.packageCount || *data.packageCount == 0)
          return None;
        row.icon  = iconType.package;
        row.label = _("packages");
        row.value = std::format("{}", *data.packageCount);
      }
#endif
      else if (keyLower == "de") {
        if (!data.desktopEnv)
          return None;
        if (data.windowMgr && *data.desktopEnv == *data.windowMgr)
          return None;
        row.icon  = iconType.desktopEnvironment;
        row.label = _("de");
        row.value = *data.desktopEnv;
      } else if (keyLower == "wm") {
        if (!data.windowMgr)
          return None;
        row.icon  = iconType.windowManager;
        row.label = _("wm");
        row.value = *data.windowMgr;
      }
#if DRAC_ENABLE_NOWPLAYING
      else if (keyLower == "playing" || keyLower == "nowplaying") {
        if (!data.nowPlaying)
          return None;
        row.icon  = iconType.music;
        row.label = _("playing");
        row.value = std::format(
          "{} - {}",
          data.nowPlaying->artist.value_or("Unknown Artist"),
          data.nowPlaying->title.value_or("Unknown Title")
        );
        nowPlayingConsumed = true;
      }
#endif
      else
        return None;

      if (layoutRow.icon)
        row.icon = *layoutRow.icon;
      if (layoutRow.label)
        row.label = *layoutRow.label;

      return row;
    }

  } // namespace

  fn CreateUI(const Config& config, const SystemInfo& data, bool noAscii) -> String {
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

    bool nowPlayingConsumed = false;

    for (const auto& groupCfg : *layoutGroups) {
      UIGroup group;
      group.rows.reserve(groupCfg.rows.size());

      for (const auto& rowCfg : groupCfg.rows) {
        if (auto row = BuildRowFromLayout(rowCfg, iconType, data, distroIcon, nowPlayingConsumed))
          group.rows.push_back(std::move(*row));
      }

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

#if DRAC_ENABLE_NOWPLAYING
    bool   nowPlayingActive = false;
    String npText;

    if (!nowPlayingConsumed && config.nowPlaying.enabled && data.nowPlaying) {
      npText           = std::format("{} - {}", data.nowPlaying->artist.value_or("Unknown Artist"), data.nowPlaying->title.value_or("Unknown Title"));
      nowPlayingActive = true;
    }
#endif

    String out;

    usize estimatedLines = 4;

    for (const UIGroup& grp : groups)
      estimatedLines += grp.rows.empty() ? 0 : (grp.rows.size() + 1);

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      if (nowPlayingActive)
        ++estimatedLines;

    out.reserve(estimatedLines * (maxContentWidth + 4));

    const usize innerWidth = maxContentWidth + 1;

    String hBorder;
    hBorder.reserve(innerWidth * 3);
    for (usize i = 0; i < innerWidth; ++i) hBorder += "─";

    const fn createLine = [&](const String& left, const String& right = "") {
      const usize leftWidth  = GetVisualWidth(left);
      const usize rightWidth = GetVisualWidth(right);
      const usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

      out += "│";
      out += left;
      out.append(padding, ' ');
      out += right;
      out += " │\n";
    };

    const fn createLeftAlignedLine = [&](const String& content) { createLine(content, ""); };

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

    if constexpr (DRAC_ENABLE_NOWPLAYING)
      if (nowPlayingActive) {
        if (hasRenderedContent) {
          out += "├";
          out += hBorder;
          out += "┤\n";
        }

        const String leftPart      = Stylize(iconType.music, { .color = DEFAULT_THEME.icon }) + Stylize(_("playing"), { .color = DEFAULT_THEME.label });
        const usize  leftPartWidth = GetVisualWidth(leftPart);

        const usize availableWidth = maxContentWidth - leftPartWidth;

        const Vec<String> wrappedLines = WordWrap(npText, availableWidth);

        if (!wrappedLines.empty()) {
          createLine(leftPart, Stylize(wrappedLines[0], { .color = LogColor::Magenta }));

          const String indent(leftPartWidth, ' ');

          for (usize i = 1; i < wrappedLines.size(); ++i) {
            String rightPart      = Stylize(wrappedLines[i], { .color = LogColor::Magenta });
            usize  rightPartWidth = GetVisualWidth(rightPart);

            usize padding = (maxContentWidth > leftPartWidth + rightPartWidth)
              ? maxContentWidth - leftPartWidth - rightPartWidth
              : 0;

            String lineContent = indent;
            lineContent.append(padding, ' ');
            lineContent.append(rightPart);
            createLine(lineContent);
          }
        }
      }

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
    String      kittySequence;
    bool        isKittyLogo = false;

    if (!noAscii) {
      if (const Option<LogoRender> kittyLogo = BuildKittyLogo(config.logo, boxLines.size())) {
        logoLines     = kittyLogo->lines;
        maxLogoW      = kittyLogo->width;
        logoHeightOpt = kittyLogo->height;
        isKittyLogo   = kittyLogo->isKitty;
        kittySequence = kittyLogo->sequence;
      }

      if (!isKittyLogo) {
        if (logoLines.empty()) {
          const Vec<StringView> asciiLines = ascii::GetAsciiArt(data.operatingSystem->id);

          for (const auto& aLine : asciiLines) {
            logoLines.emplace_back(aLine);
            maxLogoW = std::max(maxLogoW, GetVisualWidth(aLine));
          }
        }
      }
    }

    if (!isKittyLogo && logoLines.empty())
      return out;

    const usize logoHeight = isKittyLogo ? (logoHeightOpt ? logoHeightOpt : boxLines.size()) : logoLines.size();
    String      emptyLogo(maxLogoW, ' ');

    // Kitty: emit the image to stdout once, then print the box shifted right by logo width.
    if (isKittyLogo) {
      if (!kittySequence.empty())
        std::cout << "\033[s" << kittySequence << "\033[u" << std::flush;

      String      newOut;
      const usize shift = maxLogoW + 2; // logo width plus gap

      for (const auto& boxLine : boxLines) {
        newOut += "\r";
        newOut += std::format("\033[{}C", shift);
        newOut += boxLine;
        newOut += "\n";
      }

      return newOut;
    }

    // ASCII logo: center relative to box height
    const usize totalHeight = std::max(logoHeight, boxLines.size());
    const usize logoPadTop  = (totalHeight > logoHeight) ? (totalHeight - logoHeight) / 2 : 0;
    const usize boxPadTop   = (totalHeight > boxLines.size()) ? (totalHeight - boxLines.size()) / 2 : 0;

    String newOut;

    for (usize i = 0; i < totalHeight; ++i) {
      String outputLine;

      if (i < logoPadTop || i >= logoPadTop + logoHeight) {
        outputLine += emptyLogo;
      } else {
        const auto& logoLine      = logoLines[i - logoPadTop];
        const usize logoLineWidth = GetVisualWidth(logoLine);
        const usize logoPadding   = maxLogoW > logoLineWidth ? maxLogoW - logoLineWidth : 0;

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
