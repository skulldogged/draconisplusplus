# Weather Plugin
# Provides weather information from various providers (OpenMeteo, MetNo, OpenWeatherMap)

set_variable('plugin_name', 'weather')
set_variable('plugin_sources', files('weather.cpp'))
set_variable('plugin_deps', [
  dependency('libcurl', include_type: 'system'),
  dependency('tomlplusplus', include_type: 'system'),
])
set_variable('plugin_platform', 'all')
