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

# Static plugin sources - for precompiled builds with static plugins
# Clone the plugins repo directly into the project root:
#   git clone https://github.com/skulldogged/draconisplusplus-plugins.git plugins
# Then build with: meson setup build -Dprecompiled_config=true -Dstatic_plugins=weather,now_playing
plugins_dir = meson.project_source_root() / 'plugins' / 'plugins'
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

# Whether any plugin requires codesigning (macOS only, for private framework access)
plugin_requires_codesign = false

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
    
    # Initialize empty list for static plugin dependencies
    static_plugin_deps = []

    foreach plugin_name : static_plugins
      # Dynamically find the plugin source file and metadata
      plugin_source_path = plugins_dir / plugin_name / (plugin_name + '.cpp')
      plugin_json_path = plugins_dir / plugin_name / 'plugin.json'
      
      # Check if the plugin source exists
      if fs.is_file(plugin_source_path)
        lib_all_sources += files(plugin_source_path)
      else
        error('Plugin source not found: "@0@". Expected file: @1@\nClone the plugins repo: git clone https://github.com/skulldogged/draconisplusplus-plugins.git plugins'.format(plugin_name, plugin_source_path))
      endif

      # Load plugin's plugin.json to get its dependencies and sources using Python
      if fs.is_file(plugin_json_path)
        # Use Python to parse JSON and output deps in a simple format
        # Format: DEP|name|include_type|static|platforms (one per line)
        # Format: SRC|filename|platforms (one per line)
        # Format: CODESIGN|true/false (one line if present)
        parse_result = run_command(
          'python', '-c', '''
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
for dep in data.get("deps", []):
    name = dep.get("name", "")
    inc_type = dep.get("include_type", "preserve")
    static = "true" if dep.get("static", False) else "false"
    platforms = ",".join(dep.get("platforms", []))
    print(f"DEP|{name}|{inc_type}|{static}|{platforms}")
for src in data.get("sources", []):
    filename = src.get("file", "")
    platforms = ",".join(src.get("platforms", []))
    print(f"SRC|{filename}|{platforms}")
if data.get("codesign", False):
    print("CODESIGN|true")
''',
          plugin_json_path,
          check: true,
        )
        
        parse_output = parse_result.stdout().strip()
        if parse_output != ''
          foreach line : parse_output.split('\n')
            parts = line.split('|')
            line_type = parts[0]
            
            if line_type == 'DEP'
              dep_name = parts[1]
              dep_include_type = parts[2]
              dep_static = parts[3] == 'true'
              dep_platforms_str = parts[4]
              
              # Check if this dependency is platform-specific
              if dep_platforms_str != ''
                dep_platforms = dep_platforms_str.split(',')
                # Only add if current platform is in the list
                if host_system in dep_platforms
                  static_plugin_deps += dependency(dep_name, include_type: dep_include_type, static: dep_static)
                endif
              else
                # No platform restriction, add unconditionally
                static_plugin_deps += dependency(dep_name, include_type: dep_include_type, static: dep_static)
              endif
            elif line_type == 'SRC'
              src_filename = parts[1]
              src_platforms_str = parts[2]
              
              # Check if this source is platform-specific
              should_add = false
              if src_platforms_str != ''
                src_platforms = src_platforms_str.split(',')
                if host_system in src_platforms
                  should_add = true
                endif
              else
                # No platform restriction, add unconditionally
                should_add = true
              endif
              
              if should_add
                src_full_path = plugins_dir / plugin_name / src_filename
                if fs.is_file(src_full_path)
                  lib_all_sources += files(src_full_path)
                else
                  warning('Plugin @0@: source file not found: @1@'.format(plugin_name, src_full_path))
                endif
              endif
            elif line_type == 'CODESIGN'
              # Plugin requires codesigning (only relevant on macOS)
              plugin_requires_codesign = true
            endif
          endforeach
        endif
      endif
    endforeach

    # Add static plugin dependencies to lib_deps
    lib_deps += static_plugin_deps
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
# Includes lib_deps so consumers get all transitive dependencies automatically
draconis_dep = declare_dependency(
  link_with : libdrac,
  dependencies : [includes_dep] + lib_deps,
  link_args : link_args,
  compile_args : static_plugin_cpp_args,
)

# Dependency with link_whole for the main CLI (ensures static plugin self-registration isn't stripped)
if get_option('precompiled_config') and static_plugins.length() > 0
  draconis_dep_whole = declare_dependency(
    link_whole : libdrac,
    dependencies : [includes_dep] + lib_deps,
    link_args : link_args,
    compile_args : static_plugin_cpp_args,
  )
else
  draconis_dep_whole = draconis_dep
endif
