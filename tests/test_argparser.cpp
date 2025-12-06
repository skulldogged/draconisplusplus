#include <boost/ut.hpp>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/Types.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::argparse;
  using namespace draconis::utils::types;

  "ArgumentParser flag"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    bool           verbose = false;
    parser.addArguments("-v", "--verbose").flag().bindTo(verbose);

    Vec<String> args   = { "testprog", "--verbose" };
    Result<>    result = parser.parseInto(args);

    expect(result.has_value());
    expect(verbose);
    expect(parser.isUsed("-v"));
  };

  "ArgumentParser value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    String         output;
    parser.addArguments("-o", "--output").bindTo(output);

    Vec<String> args   = { "testprog", "-o", "out.txt" };
    Result<>    result = parser.parseInto(args);

    expect(result.has_value());
    expect(output == String("out.txt"));
  };

  "ArgumentParser integer conversion"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    i32            count = 0;
    parser.addArguments("-c", "--count").defaultValue(i32(1)).bindTo(count);

    Vec<String> args   = { "testprog", "--count", "42" };
    Result<>    result = parser.parseInto(args);

    expect(result.has_value());
    expect(count == 42);
  };

  "ArgumentParser default value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    i32            count = 0;
    parser.addArguments("-c", "--count").defaultValue(i32(10)).bindTo(count);

    Vec<String> args   = { "testprog" };
    Result<>    result = parser.parseInto(args);

    expect(result.has_value());
    expect(count == 10);
  };

  "ArgumentParser missing value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    parser.addArguments("-o");

    Vec<String> args   = { "testprog", "-o" };
    Result<>    result = parser.parseArgs(args);

    expect(!result.has_value());
  };

  "ArgumentParser unknown argument"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    Vec<String>    args   = { "testprog", "--unknown" };
    Result<>       result = parser.parseArgs(args);

    expect(!result.has_value());
  };

  return 0;
}
