link_args = []

if host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
endif

executable(
  'glaze_http',
  'glaze_http/main.cpp',
  dependencies: [draconis_dep] + lib_deps,
  link_args: link_args,
  install: false,
)

executable(
  'mcp_server',
  'mcp_server/main.cpp',
  dependencies: [draconis_dep] + lib_deps,
  link_args: link_args,
  install: true,
)