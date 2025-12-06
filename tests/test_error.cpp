#include <boost/ut.hpp>

#include <Drac++/Utils/Error.hpp>

using namespace boost::ut;
using namespace draconis::utils::error;
using namespace draconis::utils::types;

namespace {
  auto fail_helper() -> Result<i32> {
    ERR(DracErrorCode::InvalidArgument, "fail");
  }

  auto succeed_helper() -> Result<i32> {
    return 42;
  }

  auto try_test_helper_fail() -> Result<i32> {
    i32 val = TRY(fail_helper());

    return val + 1; // Should not reach here
  }

  auto try_test_helper_success() -> Result<i32> {
    i32 val = TRY(succeed_helper());

    return val + 1; // Should be 43
  }
} // namespace

auto main() -> int {
  "DracError construction"_test = [] -> void {
    DracError err(DracErrorCode::NotFound, "Item not found");

    expect(err.code == DracErrorCode::NotFound);
    expect(err.message == String("Item not found"));
    expect(err.location.line() > 0);
  };

  "TRY macro success"_test = [] -> void {
    Result<i32> res = try_test_helper_success();

    expect(res.has_value());
    expect(*res == 43);
  };

  "TRY macro failure"_test = [] -> void {
#ifdef _MSC_VER
    try {
      [[maybe_unused]] Result<i32> res = try_test_helper_fail();
      expect(false); // Should have thrown
    } catch (const DracError& e) {
      expect(e.code == DracErrorCode::InvalidArgument);
      expect(e.message == String("fail"));
    }
#else
    Result<i32> res = try_test_helper_fail();

    expect(!res.has_value());
    expect(res.error().code == DracErrorCode::InvalidArgument);
    expect(res.error().message == String("fail"));
#endif
  };

  "ERR macro"_test = [] -> void {
    auto func = []() -> Result<void> {
      ERR(DracErrorCode::InternalError, "internal error");
    };

    Result<void> res = func();

    expect(!res.has_value());
    expect(res.error().code == DracErrorCode::InternalError);
  };

  return 0;
}
