#include <boost/ut.hpp>

#include <Drac++/Utils/Env.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::env;
  using namespace draconis::utils::error;
  using namespace draconis::utils::types;

  "GetEnv returns NotFound for missing variable"_test = [] -> void {
    Result<String> result = GetEnv("DRAC_TEST_NONEXISTENT_VAR_12345");

    expect(!result.has_value());
    expect(result.error().code == DracErrorCode::NotFound);
  };

  "SetEnv and GetEnv round-trip"_test = [] -> void {
    SetEnv("DRAC_TEST_VAR", "test_value");
    Result<String> result = GetEnv("DRAC_TEST_VAR");

    expect(result.has_value());
    expect(*result == String("test_value"));

    UnsetEnv("DRAC_TEST_VAR");
  };

  "UnsetEnv removes variable"_test = [] -> void {
    SetEnv("DRAC_TEST_VAR2", "value");
    UnsetEnv("DRAC_TEST_VAR2");

    Result<String> result = GetEnv("DRAC_TEST_VAR2");

    expect(!result.has_value());
  };

  return 0;
}
