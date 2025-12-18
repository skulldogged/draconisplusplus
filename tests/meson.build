# ============ #
#  Unit Tests  #
# ============ #

ut_dep = dependency(
  'boost.ut',
  include_type: 'system',
  allow_fallback: false,
)

test_deps = [ut_dep, draconis_dep] + lib_deps

# Environment utilities tests
test_env = executable(
  'test_env',
  'test_env.cpp',
  dependencies: test_deps,
)
test('Environment Utils', test_env)

# Types tests
test_types = executable(
  'test_types',
  'test_types.cpp',
  dependencies: test_deps,
)
test('Types', test_types)

# Error handling tests
test_error = executable(
  'test_error',
  'test_error.cpp',
  dependencies: test_deps,
)
test('Error Handling', test_error)

# Localization tests
test_localization = executable(
  'test_localization',
  'test_localization.cpp',
  dependencies: test_deps,
)
test('Localization', test_localization)

# Argument Parser tests
test_argparser = executable(
  'test_argparser',
  'test_argparser.cpp',
  dependencies: test_deps,
)
test('Argument Parser', test_argparser)
