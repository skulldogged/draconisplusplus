# ========================= #
#   Draconis++ Build System  #
# ========================= #

# Use PowerShell on Windows
set windows-shell := ["cmd.exe", "/c"]

# Default recipe - show available commands
default:
    @just --list

# Build directory
build_dir := "build"

# Platform-specific executable path
exe_path := if os() == "windows" { ".\\build\\src\\CLI\\draconis++.exe" } else { "./build/src/CLI/draconis++" }

# ===================== #
#   Setup & Configure   #
# ===================== #

# Configure the build (run once before building)
setup:
    meson setup {{build_dir}}

# Reconfigure with custom options (e.g., just configure -Dweather=disabled)
configure *ARGS:
    meson setup {{build_dir}} --reconfigure {{ARGS}}

# Wipe build directory and reconfigure
wipe *ARGS:
    meson setup {{build_dir}} --wipe {{ARGS}}

# ===================== #
#   Build Commands      #
# ===================== #

# Build the project
build:
    meson compile -C {{build_dir}}

# Build with verbose output
build-verbose:
    meson compile -C {{build_dir}} -v

# Build in release mode
release:
    meson setup {{build_dir}} --buildtype=release --reconfigure
    meson compile -C {{build_dir}}

# ===================== #
#   Run Commands        #
# ===================== #

# Build and run the main binary
run: build
    {{exe_path}}

# Run with arguments (e.g., just run-args --help)
run-args *ARGS: build
    {{exe_path}} {{ARGS}}

# ===================== #
#   Test Commands       #
# ===================== #

# Run all tests
test:
    meson test -C {{build_dir}}

# Run tests with verbose output
test-verbose:
    meson test -C {{build_dir}} -v

# Run a specific test (e.g., just test-one test_name)
test-one NAME:
    meson test -C {{build_dir}} {{NAME}}

# ===================== #
#   Clean Commands      #
# ===================== #

# Clean build artifacts (keeps configuration)
clean:
    meson compile -C {{build_dir}} --clean

# Clean and rebuild
rebuild: clean build

# ===================== #
#   Development         #
# ===================== #

# Format all source files
format:
    clang-format -i src include plugins

lint:
    clang-tidy -p {{build_dir}}

# Generate documentation with Doxygen
docs:
    doxygen Doxyfile

# ===================== #
#   Installation        #
# ===================== #

# Install to system
install: build
    meson install -C {{build_dir}}

# Install to custom prefix (e.g., just install-prefix ~/.local)
install-prefix PREFIX: build
    meson install -C {{build_dir}} --destdir {{PREFIX}}

# ===================== #
#   Information         #
# ===================== #

# Show build configuration summary
info:
    meson configure {{build_dir}}

# Show project introspection data
introspect:
    meson introspect {{build_dir}} --projectinfo
