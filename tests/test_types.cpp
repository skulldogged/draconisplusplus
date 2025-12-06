#include <boost/ut.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::types;

  "Type sizes"_test = [] -> void {
    expect(sizeof(u8) == 1_ul);
    expect(sizeof(u16) == 2_ul);
    expect(sizeof(u32) == 4_ul);
    expect(sizeof(u64) == 8_ul);
    expect(sizeof(i8) == 1_ul);
    expect(sizeof(i16) == 2_ul);
    expect(sizeof(i32) == 4_ul);
    expect(sizeof(i64) == 8_ul);
    expect(sizeof(f32) == 4_ul);
    expect(sizeof(f64) == 8_ul);
  };

  "Option helper Some"_test = [] -> void {
    Option<i32> opt = Some(42);

    expect(opt.has_value());
    expect(*opt == 42);

    Option<String> optStr = Some(String("test"));

    expect(optStr.has_value());
    expect(*optStr == String("test"));
  };

  "Result success"_test = [] -> void {
    Result<i32> res = 10;

    expect(res.has_value());
    expect(*res == 10);
  };

  "Result error"_test = [] -> void {
    using namespace draconis::utils::error;

    Result<i32> res = Err(DracError(DracErrorCode::NotFound, "test error"));

    expect(!res.has_value());
    expect(res.error().code == DracErrorCode::NotFound);
    expect(res.error().message == String("test error"));
  };

  "None constant"_test = [] -> void {
    Option<i32> opt = None;

    expect(!opt.has_value());
  };

  return 0;
}
