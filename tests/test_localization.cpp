#include <boost/ut.hpp>

#include <Drac++/Utils/Localization.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::localization;

  "TranslationManager singleton"_test = [] -> void {
    TranslationManager& transMgr = GetTranslationManager();

    expect(transMgr.getCurrentLanguage() == "en" or transMgr.getCurrentLanguage().empty());
  };

  "Get key fallback"_test = [] -> void {
    TranslationManager& transMgr = GetTranslationManager();

    String val = transMgr.get("NON_EXISTENT_KEY_XYZ_123");
    expect(val == "NON_EXISTENT_KEY_XYZ_123");
  };

  "Localization helpers"_test = [] -> void {
    String val = _("NON_EXISTENT_KEY_HELPER");
    expect(val == "NON_EXISTENT_KEY_HELPER");
  };

  "Formatting helpers"_test = [] -> void {
    i32    arg = 123;
    String val = _format("KEY_{}", arg);
    expect(val == "KEY_123");
  };

  return 0;
}
