# ------- #
#  Files  #
# ------- #
main_app_sources = files('CLI.cpp', 'Config/Config.cpp', 'Core/SystemInfo.cpp', 'UI/UI.cpp', 'main.cpp')

# ------------------------- #
#  Link/ObjC Configuration  #
# ------------------------- #
link_args = []
objc_args = []

if host_system == 'darwin'
  objc_args += ['-fobjc-arc']
elif host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
elif host_system == 'haiku'
  link_args += ['-lpackage', '-lbe']
endif

# ------------------- #
#  Executable Target  #
# ------------------- #
# Use draconis_dep_whole to ensure static plugin self-registration isn't stripped
dracpp_exe = executable(
  'draconis++',
  main_app_sources,
  dependencies: [draconis_dep_whole] + lib_deps,
  link_args: link_args,
  objc_args: objc_args,
  install: true,
)

# ------------------------------ #
#  macOS MediaRemote Codesigning #
# ------------------------------ #
# Some plugins (e.g., now_playing) require the binary to be signed with a
# com.apple.* identifier to access private frameworks on macOS 15.4+.
# The codesign requirement is declared in the plugin's plugin.json.
#
# This is safe for local/source builds but will NOT work for Notarized distribution.
#
# We use 'sh -c' with MESON_INSTALL_DESTDIR_PREFIX to respect DESTDIR during install.
if host_system == 'darwin' and plugin_requires_codesign
  find_program('codesign', required: true)

  meson.add_install_script(
    'sh', '-c',
    'codesign --force -s - --identifier com.apple.draconisplusplus "$MESON_INSTALL_DESTDIR_PREFIX/' + get_option('bindir') + '/draconis++"',
  )
endif
