# ------- #
#  Files  #
# ------- #

fs = import('fs')

# Structured source organization
lib_sources = {
  'base' : files('Localization.cpp'),
  'packages' : files('Services/Packages.cpp'),
  'plugins' : files('Core/PluginManager.cpp'),
}

# Static plugin sources - dynamically discovered from the plugins/ directory
# These are the same source files used for dynamic plugins, but compiled
# with DRAC_STATIC_PLUGIN_BUILD defined to use namespace exports
plugins_dir = meson.project_source_root() / 'plugins'
static_plugin_registry = files('Core/StaticPlugins.cpp')

# Platform-specific sources
platform_sources = {
  'darwin' : files('OS/macOS.cpp', 'OS/macOS/Bridge.mm'),
  'dragonfly' : files('OS/BSD.cpp'),
  'freebsd' : files('OS/BSD.cpp'),
  'haiku' : files('OS/Haiku.cpp'),
  'linux' : files('OS/Linux.cpp'),
  'netbsd' : files('OS/BSD.cpp'),
  'serenity' : files('OS/Serenity.cpp'),
  'windows' : files('OS/Windows.cpp'),
}

# Build final source list
lib_all_sources = lib_sources['base']

if get_option('packagecount').enabled()
  lib_all_sources += lib_sources['packages']
endif

# Add platform sources
lib_all_sources += platform_sources.get(host_system, files())

# Plugin configuration
static_plugin_cpp_args = []
static_plugins = get_option('static_plugins')

if get_option('precompiled_config')
  # In precompiled config mode, static_plugins is the single source of truth
  # If static_plugins is non-empty, plugins are enabled with those plugins compiled in
  # If static_plugins is empty, the plugin system is disabled entirely
  
  if static_plugins.length() > 0
    lib_all_sources += lib_sources['plugins']
    lib_all_sources += static_plugin_registry
    
    # Define DRAC_STATIC_PLUGIN_BUILD for all static plugins
    # Plugins self-register, so no per-plugin defines needed
    static_plugin_cpp_args += '-DDRAC_STATIC_PLUGIN_BUILD=1'
    
    foreach plugin_name : static_plugins
      # Dynamically find the plugin source file
      plugin_source_path = plugins_dir / plugin_name / (plugin_name + '.cpp')
      
      # Check if the plugin source exists
      if fs.is_file(plugin_source_path)
        lib_all_sources += files(plugin_source_path)
      else
        error('Plugin source not found: "@0@". Expected file: @1@'.format(plugin_name, plugin_source_path))
      endif
    endforeach
  endif
else
  # Non-precompiled mode: use the plugins feature option for dynamic plugin loading
  if get_option('plugins').enabled()
    lib_all_sources += lib_sources['plugins']
  endif
endif

# Link arguments
link_args = []

if get_option('use_linked_pci_ids') == true
  link_args += meson.project_source_root() / 'pci_ids.o'
endif

# ----------------- #
#  Static Library   #
# ----------------- #
libdrac = static_library(
  'drac++',
  lib_all_sources,
  dependencies : lib_deps,
  cpp_args : static_plugin_cpp_args,
  install : false,
)

# Standard dependency (for most targets)
draconis_dep = declare_dependency(
  link_with : libdrac,
  dependencies : [includes_dep],
  link_args : link_args,
  compile_args : static_plugin_cpp_args,
)

# Dependency with link_whole for the main CLI (ensures static plugin self-registration isn't stripped)
if get_option('precompiled_config') and static_plugins.length() > 0
  draconis_dep_whole = declare_dependency(
    link_whole : libdrac,
    dependencies : [includes_dep],
    link_args : link_args,
    compile_args : static_plugin_cpp_args,
  )
else
  draconis_dep_whole = draconis_dep
endif
