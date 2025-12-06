# ================================ #
#   Draconis++ Plugin Build System  #
# ================================ #

# Only build dynamic plugins if:
# 1. Plugin support is enabled AND
# 2. We're NOT in precompiled config mode with static plugins
if not get_option('plugins').enabled()
  subdir_done()
endif

if get_option('precompiled_config') and get_option('static_plugins').length() > 0
  # Static plugins are compiled into the binary, don't build dynamic versions
  subdir_done()
endif

# Plugin build configuration
plugin_cpp_args = []
plugin_base_deps = [draconis_dep, glaze_dep]

# Add plugin-specific compiler arguments
plugin_cpp_args += [
  '-DDRAC_PLUGIN_BUILD=1',
  '-fvisibility=default', # Plugins need visible symbols
]

# Platform-specific plugin settings
if host_system == 'windows'
  plugin_suffix = '.dll'
  plugin_cpp_args += ['-DDRAC_PLUGIN_WINDOWS=1']
elif host_system == 'darwin'
  plugin_suffix = '.dylib'
  plugin_cpp_args += ['-DDRAC_PLUGIN_MACOS=1']
else
  plugin_suffix = '.so'
  plugin_cpp_args += ['-DDRAC_PLUGIN_UNIX=1']
endif

# Plugin installation directory
plugin_install_dir = get_option('libdir') / 'draconis++' / 'plugins'

# ========================== #
#   Dynamic Plugin Discovery #
# ========================== #

# Track built plugins for summary
plugins_built = []

# Known plugin directories - add new plugins here
# Each must have a meson.build that sets: plugin_name, plugin_sources, plugin_deps, plugin_platform
known_plugins = [
  'json_format',
  'markdown_format',
  'now_playing',
  'weather',
  'yaml_format',
]

foreach plugin_dir : known_plugins
  # Skip if directory doesn't exist (allows conditional plugin inclusion)
  if not fs.is_dir(plugin_dir)
    continue
  endif

  # Skip if no meson.build (not a buildable plugin)
  if not fs.is_file(plugin_dir / 'meson.build')
    continue
  endif

  plugin_name = ''
  plugin_sources = []
  plugin_deps = []
  plugin_platform = 'all'

  subdir(plugin_dir)

  # Check platform compatibility
  if plugin_platform != 'all' and plugin_platform != host_system
    message(
      'Skipping plugin "@0@" (requires @1@, building on @2@)'.format(plugin_name, plugin_platform, host_system),
    )
    continue
  endif

  # Build the plugin
  shared_library(
    plugin_name,
    plugin_sources,
    name_prefix: '',
    name_suffix: plugin_suffix.substring(1),
    dependencies: plugin_base_deps + plugin_deps,
    cpp_args: plugin_cpp_args + ['-O3', '-DNDEBUG'],
    install: true,
    install_dir: plugin_install_dir,
  )

  plugins_built += plugin_name
endforeach
