# Now Playing Plugin
# Provides currently playing media information from the system
# - Windows: NPSM (Now Playing Session Manager) COM API
# - macOS: MediaRemote private framework
# - Linux/BSD: MPRIS over DBus

set_variable('plugin_name', 'now_playing')
set_variable('plugin_sources', files('now_playing.cpp'))

# Platform-specific dependencies
if host_system in ['linux', 'freebsd', 'dragonfly', 'netbsd']
  # Linux/BSD use DBus for MPRIS
  set_variable(
    'plugin_deps',
    [
      dependency('dbus-1', include_type: 'system'),
    ],
  )
else
  # Windows and macOS have no additional dependencies
  set_variable('plugin_deps', [])
endif

set_variable('plugin_platform', 'all')