/**
 * @file ArgumentParser.hpp
 * @brief Simple command-line argument parser for Drac++.
 *
 * This header provides a lightweight argument parser that follows the Drac++
 * coding conventions and type system. It supports basic argument parsing
 * including flags, optional arguments, and help text generation.
 *
 * Supports automatic struct binding via bindTo() for cleaner argument handling.
 */

#pragma once

#include <algorithm>
#include <concepts>                  // std::convertible_to
#include <cstdlib>                   // std::exit
#include <format>                    // std::format
#include <functional>                // std::function
#include <magic_enum/magic_enum.hpp> // magic_enum::enum_name, magic_enum::enum_cast
#include <sstream>                   // std::ostringstream
#include <unordered_set>             // std::unordered_set for fast choice look-ups
#include <utility>                   // std::forward

#include "Error.hpp"
#include "Logging.hpp"
#include "Types.hpp"

namespace draconis::utils::argparse {
  namespace error   = ::draconis::utils::error;
  namespace logging = ::draconis::utils::logging;
  namespace types   = ::draconis::utils::types;

  // Forward declaration for binding function
  class Argument;

  /**
   * @brief Type alias for argument values.
   */
  using ArgValue = std::variant<bool, types::i32, types::f64, types::String>;

  /**
   * @brief Type-erased binding function for struct member assignment.
   */
  using ArgBinding = std::function<void(const Argument&)>;

  /**
   * @brief Type alias for allowed choices for enum-style arguments.
   */
  using ArgChoices = types::Vec<types::String>;

  /**
   * @brief Generic traits class for enum string conversion using magic_enum.
   * @tparam EnumType The enum type
   */
  template <typename EnumType>
  struct EnumTraits {
    static constexpr bool has_string_conversion = magic_enum::is_scoped_enum_v<EnumType>;

    static fn getChoices() -> const ArgChoices& {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");

      static const ArgChoices CACHED_CHOICES = [] {
        ArgChoices vec;
        const auto enumValues = magic_enum::enum_values<EnumType>();
        vec.reserve(enumValues.size());
        for (const auto value : enumValues)
          vec.emplace_back(magic_enum::enum_name(value));
        return vec;
      }();

      return CACHED_CHOICES;
    }

    static fn stringToEnum(const types::String& str) -> EnumType {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");

      auto result = magic_enum::enum_cast<EnumType>(str);
      if (result.has_value())
        return result.value();

      const auto enumValues = magic_enum::enum_values<EnumType>();
      for (const auto value : enumValues) {
        types::StringView enumName = magic_enum::enum_name(value);
        if (std::ranges::equal(str, enumName, [](char charA, char charB) { return std::tolower(charA) == std::tolower(charB); }))
          return value;
      }

      return enumValues[0];
    }

    static fn enumToString(EnumType value) -> types::String {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");
      return types::String(magic_enum::enum_name(value));
    }
  };

  /**
   * @brief Represents a command-line argument with its metadata and value.
   */
  class Argument {
   public:
    /**
     * @brief Construct a new Argument.
     * @param names Vector of argument names (e.g., {"-v", "--verbose"})
     * @param help_text Help text for this argument
     * @param is_flag Whether this is a flag (boolean) argument
     */
    explicit Argument(types::Vec<types::String> names, types::String help_text = "", bool is_flag = false)
      : m_names(std::move(names)), m_helpText(std::move(help_text)), m_isFlag(is_flag) {
      if (m_isFlag)
        m_defaultValue = false;
    }

    /**
     * @brief Construct a new Argument with variadic names.
     * @tparam NameTs Variadic list of types convertible to String
     * @param help_text Help text for this argument
     * @param is_flag Whether this is a flag (boolean) argument
     * @param names One or more argument names
     */
    template <typename... NameTs>
      requires(sizeof...(NameTs) >= 1 && (std::convertible_to<NameTs, types::String> && ...))
    explicit Argument(types::String help_text, bool is_flag, NameTs&&... names)
      : m_names { types::String(std::forward<NameTs>(names))... }, m_helpText(std::move(help_text)), m_isFlag(is_flag) {
      if (m_isFlag)
        m_defaultValue = false;
    }

    /**
     * @brief Set the help text for this argument.
     * @param help_text The help text
     * @return Reference to this argument for method chaining
     */
    fn help(types::String help_text) -> Argument& {
      m_helpText = std::move(help_text);
      return *this;
    }

    /**
     * @brief Set the default value for this argument.
     * @tparam T Type of the default value
     * @param value The default value
     * @return Reference to this argument for method chaining
     */
    template <typename T>
    fn defaultValue(T value) -> Argument& {
      m_defaultValue = std::move(value);
      return *this;
    }

    /**
     * @brief Set the default value for this argument as an enum.
     * @tparam EnumType The enum type
     * @param value The default enum value
     * @return Reference to this argument for method chaining
     */
    template <typename EnumType>
      requires std::is_enum_v<EnumType> && EnumTraits<EnumType>::has_string_conversion
                                         fn defaultValue(EnumType value) -> Argument& {
      types::String strValue = EnumTraits<EnumType>::enumToString(value);

      m_defaultValue = strValue;

      /* Setting choices via helper ensures lowercase set is populated. */
      return this->choices(EnumTraits<EnumType>::getChoices());
    }

    /**
     * @brief Configure this argument as a flag.
     * @return Reference to this argument for method chaining
     */
    fn flag() -> Argument& {
      m_isFlag       = true;
      m_defaultValue = false;
      return *this;
    }

    /**
     * @brief Set allowed choices for enum-style arguments.
     * @param choices Vector of allowed string values
     * @return Reference to this argument for method chaining
     */
    fn choices(const ArgChoices& choices) -> Argument& {
      m_choices = choices;

      std::unordered_set<types::String> lowered;
      lowered.reserve(choices.size());

      for (const types::String& choice : choices) {
        types::String lower = choice;
        std::ranges::transform(
          lower,
          lower.begin(),
          [](types::u8 chr) -> types::CStr { return static_cast<types::CStr>(std::tolower(chr)); }
        );
        lowered.emplace(std::move(lower));
      }

      m_lowerChoices = std::move(lowered);

      return *this;
    }

    /**
     * @brief Get the value of this argument.
     * @tparam T Type to get the value as
     * @return The argument value, or default value if not provided
     */
    template <typename T>
    fn get() const -> T {
      if (m_isUsed && m_value.has_value())
        return std::get<T>(m_value.value());

      if (m_defaultValue.has_value())
        return std::get<T>(m_defaultValue.value());

      return T {};
    }

    /**
     * @brief Get the value of this argument as an enum type.
     * @tparam EnumType The enum type to convert to
     * @return The argument value converted to the enum type
     */
    template <typename EnumType>
      requires std::is_enum_v<EnumType> && EnumTraits<EnumType>::has_string_conversion
                                         fn getEnum() const -> EnumType {
      const auto strValue = get<types::String>();

      return EnumTraits<EnumType>::stringToEnum(strValue);
    }

    /**
     * @brief Check if this argument was used in the command line.
     * @return true if the argument was used, false otherwise
     */
    [[nodiscard]] fn isUsed() const -> bool {
      return m_isUsed;
    }

    /**
     * @brief Get the primary name of this argument.
     * @return The first name in the names list
     */
    [[nodiscard]] fn getPrimaryName() const -> const types::String& {
      return m_names.front();
    }

    /**
     * @brief Get all names for this argument.
     * @return Vector of all argument names
     */
    [[nodiscard]] fn getNames() const -> const types::Vec<types::String>& {
      return m_names;
    }

    /**
     * @brief Get the help text for this argument.
     * @return The help text
     */
    [[nodiscard]] fn getHelpText() const -> const types::String& {
      return m_helpText;
    }

    /**
     * @brief Check if this argument is a flag.
// ...
     * @return true if this is a flag argument, false otherwise
     */
    [[nodiscard]] fn isFlag() const -> bool {
      return m_isFlag;
    }

    /**
     * @brief Check if this argument has choices (enum-style).
     * @return true if this argument has choices, false otherwise
     */
    [[nodiscard]] fn hasChoices() const -> bool {
      return m_choices.has_value();
    }

    /**
     * @brief Get the allowed choices for this argument.
     * @return Vector of allowed choices, or empty vector if none set
     */
    [[nodiscard]] fn getChoices() const -> ArgChoices {
      return m_choices.value_or(ArgChoices {});
    }

    /**
     * @brief Check if this argument has a default value.
     * @return true if a default value is set, false otherwise
     */
    [[nodiscard]] fn hasDefault() const -> bool {
      return m_defaultValue.has_value();
    }

    /**
     * @brief Get the default value as a lowercase string (for help text).
     *        Returns an empty string if no default value is set.
     */
    [[nodiscard]] fn getDefaultAsString() const -> types::String {
      if (!m_defaultValue.has_value())
        return {};

      const ArgValue& value = m_defaultValue.value();

      types::String result;
      std::visit(
        [&](const auto& value) {
          using V = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<V, bool>)
            result = value ? "true" : "false";
          else if constexpr (std::is_same_v<V, types::String>)
            result = value;
          else
            result = std::format("{}", value);
        },
        value
      );

      std::ranges::transform(result, result.begin(), [](char chr) { return std::tolower(chr); });

      return result;
    }

    /**
     * @brief Set the value for this argument.
     * @param value The value to set
     * @return Result indicating success or failure
     */
    fn setValue(ArgValue value) -> types::Result<> {
      if (hasChoices() && std::holds_alternative<types::String>(value)) {
        const types::String& strValue = std::get<types::String>(value);

        /* Lower-case once for lookup */
        types::String lowerValue = strValue;
        std::ranges::transform(
          lowerValue,
          lowerValue.begin(),
          [](types::u8 chr) { return static_cast<types::CStr>(std::tolower(chr)); }
        );

        bool isValid = m_lowerChoices && m_lowerChoices->contains(lowerValue);

        if (!isValid) {
          const ArgChoices& choices = m_choices.value();

          std::ostringstream choicesStream;
          for (types::usize i = 0; i < choices.size(); ++i) {
            if (i > 0)
              choicesStream << ", ";
            types::String lower = choices[i];
            std::ranges::transform(
              lower,
              lower.begin(),
              [](types::u8 chr) { return static_cast<types::CStr>(std::tolower(chr)); }
            );
            choicesStream << lower;
          }

          ERR_FMT(
            error::DracErrorCode::InvalidArgument,
            "Invalid value '{}' for argument '{}'. Allowed values: {}",
            strValue,
            getPrimaryName(),
            choicesStream.str()
          );
        }
      }

      if (std::holds_alternative<types::String>(value) && m_defaultValue.has_value()) {
        const types::String& raw = std::get<types::String>(value);

        if (std::holds_alternative<types::i32>(*m_defaultValue)) {
          try {
            value = static_cast<types::i32>(std::stoi(raw));
          } catch (...) {
            ERR_FMT(error::DracErrorCode::InvalidArgument, "Failed to parse '{}' as integer for argument '{}'", raw, getPrimaryName());
          }
        } else if (std::holds_alternative<types::f64>(*m_defaultValue)) {
          try {
            value = static_cast<types::f64>(std::stod(raw));
          } catch (...) {
            ERR_FMT(error::DracErrorCode::InvalidArgument, "Failed to parse '{}' as number for argument '{}'", raw, getPrimaryName());
          }
        }
      }

      m_value  = std::move(value);
      m_isUsed = true;
      return {};
    }

    /**
     * @brief Mark this argument as used.
     */
    fn markUsed() -> types::Unit {
      m_isUsed = true;

      if (m_isFlag)
        m_value = true;
    }

    /**
     * @brief Bind this argument to a struct member (bool, i32, f64, or String).
     * @tparam T The member type (must be bool, i32, f64, or String)
     * @param member Pointer to the struct member
     * @return Reference to this argument for method chaining
     *
     * @example
     *   struct Options { bool verbose; String output; };
     *   Options opts;
     *   parser.addArguments("-v", "--verbose").flag().bindTo(opts.verbose);
     *   parser.addArguments("-o", "--output").bindTo(opts.output);
     */
    template <typename T>
      requires std::same_as<T, bool> || std::same_as<T, types::i32> || std::same_as<T, types::f64> || std::same_as<T, types::String>
                                                                                                    fn bindTo(T& member) -> Argument& {
      m_binding = [&member](const Argument& arg) {
        member = arg.get<T>();
      };
      return *this;
    }

    /**
     * @brief Bind this argument to a struct member with custom transformation.
     * @tparam T The member type
     * @tparam Func Transformation function type
     * @param member Pointer to the struct member
     * @param transform Function to transform the argument value
     * @return Reference to this argument for method chaining
     *
     * @example
     *   struct Options { u32 width; };
     *   Options opts;
     *   parser.addArguments("--width").defaultValue(i32(0)).bindTo(opts.width,
     *     [](const Argument& arg) { return static_cast<u32>(std::max(0, arg.get<i32>())); });
     */
    template <typename T, typename Func>
      requires std::invocable<Func, const Argument&> && std::convertible_to<std::invoke_result_t<Func, const Argument&>, T>
                                                      fn bindTo(T& member, Func&& transform) -> Argument& {
      m_binding = [&member, transform = std::forward<Func>(transform)](const Argument& arg) {
        member = transform(arg);
      };
      return *this;
    }

    /**
     * @brief Bind this argument to an enum struct member.
     * @tparam EnumType The enum type
     * @param member Reference to the struct member
     * @return Reference to this argument for method chaining
     */
    template <typename EnumType>
      requires std::is_enum_v<EnumType> && EnumTraits<EnumType>::has_string_conversion
                                         fn bindToEnum(EnumType& member) -> Argument& {
      m_binding = [&member](const Argument& arg) {
        member = arg.getEnum<EnumType>();
      };
      return *this;
    }

    /**
     * @brief Check if this argument has a binding.
     * @return true if a binding is set, false otherwise
     */
    [[nodiscard]] fn hasBinding() const -> bool {
      return m_binding != nullptr;
    }

    /**
     * @brief Apply the binding if one exists.
     */
    fn applyBinding() const -> types::Unit {
      if (m_binding)
        m_binding(*this);
    }

   private:
    types::Vec<types::String>                        m_names;        ///< Argument names (e.g., {"-v", "--verbose"})
    types::String                                    m_helpText;     ///< Help text for this argument
    types::Option<ArgValue>                          m_value;        ///< The actual value provided
    types::Option<ArgValue>                          m_defaultValue; ///< Default value if none provided
    types::Option<ArgChoices>                        m_choices;      ///< Allowed choices for enum-style arguments
    types::Option<std::unordered_set<types::String>> m_lowerChoices; ///< Lower-cased set for fast validation
    ArgBinding                                       m_binding;      ///< Optional binding to a struct member
    bool                                             m_isFlag {};    ///< Whether this is a flag argument
    bool                                             m_isUsed {};    ///< Whether this argument was used
  };

  /**
   * @brief Main argument parser class.
   */
  class ArgumentParser {
   public:
    /**
     * @brief Construct a new ArgumentParser.
     * @param programName Name of the program
     * @param version Version string of the program
     */
    explicit ArgumentParser(types::String programName, types::String version)
      : m_programName(std::move(programName)), m_version(std::move(version)) {
      addArguments("-h", "--help")
        .help("Show this help message and exit")
        .flag()
        .defaultValue(false);

      addArguments("-v", "--version")
        .help("Show version information and exit")
        .flag()
        .defaultValue(false);
    }

    /**
     * @brief Construct a new ArgumentParser.
     * @param version Version string of the program
     *
     * @details Program name is set to argv[0] at runtime.
     */
    explicit ArgumentParser(types::String version)
      : m_version(std::move(version)) {
      addArguments("-h", "--help")
        .help("Show this help message and exit")
        .flag()
        .defaultValue(false);

      addArguments("-v", "--version")
        .help("Show version information and exit")
        .flag()
        .defaultValue(false);
    }

    /**
     * @brief Add a new argument (or multiple aliases) to the parser.
     *
     * This variadic overload allows callers to pass one or more names directly, e.g.
     *   parser.addArgument("-f", "--file");
     * without the need to manually construct a `Vec<String>` or invoke `addArguments`.
     *
     * @tparam NameTs Variadic list of types convertible to `String`
     * @param names   One or more argument names / aliases
     * @return Reference to the newly created argument
     */
    template <typename... NameTs>
      requires(sizeof...(NameTs) >= 1 && (std::convertible_to<NameTs, types::String> && ...))
    fn addArguments(NameTs&&... names) -> Argument& {
      m_arguments.emplace_back(std::make_unique<Argument>(types::String {}, false, std::forward<NameTs>(names)...));
      Argument& arg = *m_arguments.back();

      for (const types::String& name : arg.getNames())
        m_argumentMap[name] = &arg;

      return arg;
    }

    /**
     * @brief Parse command-line arguments.
     * @param args Span of argument strings
     * @return Result indicating success or failure
     */
    fn parseArgs(types::Span<const char* const> args) -> types::Result<> {
      if (args.empty())
        return {};

      if (m_programName.empty())
        m_programName = args[0];

      for (types::usize i = 1; i < args.size(); ++i) {
        types::StringView arg = args[i];

        if (arg == "-h" || arg == "--help") {
          printHelp();
          std::exit(0);
        }

        if (arg == "-v" || arg == "--version") {
          logging::Println(m_version);
          std::exit(0);
        }

        auto iter = m_argumentMap.find(types::String(arg));
        if (iter == m_argumentMap.end())
          ERR_FMT(error::DracErrorCode::InvalidArgument, "Unknown argument: {}", arg);

        Argument* argument = iter->second;

        if (argument->isFlag()) {
          argument->markUsed();
        } else {
          if (i + 1 >= args.size())
            ERR_FMT(error::DracErrorCode::InvalidArgument, "Argument {} requires a value", arg);

          types::String value = args[++i];
          if (types::Result<> result = argument->setValue(value); !result)
            return result;
        }
      }

      return {};
    }

    /**
     * @brief Parse command-line arguments from a vector.
     * @param args Vector of argument strings
     * @return Result indicating success or failure
     */
    fn parseArgs(const types::Vec<types::String>& args) -> types::Result<> {
      if (args.empty())
        return {};

      if (m_programName.empty())
        m_programName = args[0];

      for (types::usize i = 1; i < args.size(); ++i) {
        const types::String& arg = args[i];

        if (arg == "-h" || arg == "--help") {
          printHelp();
          std::exit(0);
        }

        if (arg == "-v" || arg == "--version") {
          logging::Println(m_version);
          std::exit(0);
        }

        auto iter = m_argumentMap.find(arg);
        if (iter == m_argumentMap.end())
          ERR_FMT(error::DracErrorCode::InvalidArgument, "Unknown argument: {}", arg);

        Argument* argument = iter->second;

        if (argument->isFlag()) {
          argument->markUsed();
        } else {
          if (i + 1 >= args.size())
            ERR_FMT(error::DracErrorCode::InvalidArgument, "Argument {} requires a value", arg);

          types::String value = args[++i];
          if (types::Result<> result = argument->setValue(value); !result)
            return result;
        }
      }

      return {};
    }

    /**
     * @brief Get the value of an argument.
     * @tparam T Type to get the value as
     * @param name Argument name
     * @return The argument value, or default value if not provided
     */
    template <typename T = types::String>
    fn get(types::StringView name) const -> T {
      auto iter = m_argumentMap.find(types::String(name));

      if (iter != m_argumentMap.end())
        return iter->second->get<T>();

      return T {};
    }

    /**
     * @brief Get the value of an argument as an enum type.
     * @tparam EnumType The enum type to convert to
     * @param name Argument name
     * @return The argument value converted to the enum type
     */
    template <typename EnumType>
    fn getEnum(types::StringView name) const -> EnumType {
      auto iter = m_argumentMap.find(types::String(name));

      if (iter != m_argumentMap.end())
        return iter->second->getEnum<EnumType>();

      static_assert(EnumTraits<EnumType>::has_string_conversion, "Enum type not supported. Add a specialization to EnumTraits.");

      return EnumTraits<EnumType>::stringToEnum("");
    }

    /**
     * @brief Check if an argument was used.
     * @param name Argument name
     * @return true if the argument was used, false otherwise
     */
    [[nodiscard]] fn isUsed(types::StringView name) const -> bool {
      auto iter = m_argumentMap.find(types::String(name));
      if (iter != m_argumentMap.end())
        return iter->second->isUsed();

      return false;
    }

    /**
     * @brief Print help message.
     */
    fn printHelp() const -> types::Unit {
      std::ostringstream usageStream;
      usageStream << "Usage: " << m_programName;

      for (const auto& arg : m_arguments)
        if (arg->getPrimaryName().starts_with('-')) {
          usageStream << " [" << arg->getPrimaryName();

          if (!arg->isFlag())
            usageStream << " VALUE";

          usageStream << "]";
        }

      logging::Println(usageStream.str());
      logging::Println();

      if (!m_arguments.empty()) {
        logging::Println("Arguments:");
        for (const auto& arg : m_arguments) {
          std::ostringstream namesStream;
          for (types::usize i = 0; i < arg->getNames().size(); ++i) {
            if (i > 0)
              namesStream << ", ";

            namesStream << arg->getNames()[i];
          }

          std::ostringstream argLineStream;
          argLineStream << "  " << namesStream.str();
          if (!arg->isFlag())
            argLineStream << " VALUE";

          logging::Println(argLineStream.str());

          if (!arg->getHelpText().empty())
            logging::Println("    " + arg->getHelpText());

          if (arg->hasChoices()) {
            std::ostringstream choicesStream;
            choicesStream << "    Available values: ";
            const ArgChoices& choices = arg->getChoices();

            for (types::usize i = 0; i < choices.size(); ++i) {
              if (i > 0)
                choicesStream << ", ";

              types::String lower = choices[i];

              std::ranges::transform(lower, lower.begin(), [](char character) { return std::tolower(character); });

              choicesStream << lower;
            }

            logging::Println(choicesStream.str());
          }

          if (arg->hasChoices() && arg->hasDefault())
            logging::Println(std::format("    Default: {}", arg->getDefaultAsString()));

          logging::Println();
        }
      }
    }

    /**
     * @brief Apply all registered bindings to populate bound struct members.
     *
     * Call this after parseArgs() to transfer parsed values to bound struct members.
     * Each argument with a binding will have its value (or default) assigned to the
     * bound member.
     *
     * @example
     *   struct Options { bool verbose; String output; };
     *   Options opts;
     *   ArgumentParser parser("myapp");
     *   parser.addArguments("-v").flag().bindTo(opts.verbose);
     *   parser.addArguments("-o").defaultValue(String("")).bindTo(opts.output);
     *   parser.parseArgs(args);
     *   parser.applyBindings(); // opts.verbose and opts.output are now populated
     */
    fn applyBindings() const -> types::Unit {
      for (const auto& arg : m_arguments)
        arg->applyBinding();
    }

    /**
     * @brief Parse arguments and apply bindings in one call.
     * @param args Span of argument strings
     * @return Result indicating success or failure
     *
     * Convenience method that combines parseArgs() and applyBindings().
     */
    fn parseInto(types::Span<const char* const> args) -> types::Result<> {
      if (auto result = parseArgs(args); !result)
        return result;
      applyBindings();
      return {};
    }

    /**
     * @brief Parse arguments from vector and apply bindings in one call.
     * @param args Vector of argument strings
     * @return Result indicating success or failure
     */
    fn parseInto(const types::Vec<types::String>& args) -> types::Result<> {
      if (auto result = parseArgs(args); !result)
        return result;
      applyBindings();
      return {};
    }

   private:
    types::String                              m_programName; ///< Program name
    types::String                              m_version;     ///< Program version
    types::Vec<types::UniquePointer<Argument>> m_arguments;   ///< List of all arguments
    types::Map<types::String, Argument*>       m_argumentMap; ///< Map of argument names to arguments
  };
} // namespace draconis::utils::argparse
