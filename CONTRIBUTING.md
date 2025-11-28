# Contributing to Draconis++

Thank you for your interest in contributing to Draconis++! This document provides guidelines and information to help you get started.

## Table of Contents

- [Getting Started](#getting-started)
- [Architecture Overview](#architecture-overview)
- [Custom Type System](#custom-type-system)
- [Platform-Specific Implementation Guide](#platform-specific-implementation-guide)
- [Code Style Guidelines](#code-style-guidelines)
- [Submitting Changes](#submitting-changes)

---

## Getting Started

### Prerequisites

- **C++26 compiler** (Clang 17+, GCC 13+, or MSVC 19.36+)
- **Meson** (1.1+) and **Ninja**
- **just** (optional, for simplified commands)

### Building

```bash
# Using just (recommended)
just setup    # Configure the build
just build    # Build the project
just test     # Run tests
just run      # Build and run

# Using Meson directly
meson setup build
meson compile -C build
meson test -C build
```

### Configuration Options

Key build options can be set via `meson configure`:

| Option             | Type    | Default   | Description                           |
|--------------------|---------|-----------|---------------------------------------|
| `weather`          | feature | enabled   | Weather data fetching                 |
| `nowplaying`       | feature | enabled   | Now playing media info                |
| `packagecount`     | feature | enabled   | Package count functionality           |
| `caching`          | feature | enabled   | Caching system                        |
| `plugins`          | feature | enabled   | Plugin support                        |
| `precompiled_config` | bool  | false     | Use compile-time configuration        |

---

### Layer Responsibilities

#### `include/Drac++/` — Public API

Headers exposed to consumers of the library. These define the stable interface.

- **Core/**: System data structures (`System.hpp`, `Package.hpp`)
- **Services/**: Service interfaces for external data (weather, packages)
- **Utils/**: Type aliases, error handling, macros

#### `src/Lib/` — Library Implementation

The core library containing all platform-agnostic and platform-specific code.

- **Core/**: Cross-platform implementations
- **OS/**: Platform-specific code (Windows, Linux, macOS, BSD, etc.)
- **Services/**: External service integrations
- **Wrappers/**: Thin wrappers around third-party libraries

#### `src/CLI/` — Command-Line Interface

The user-facing application that consumes the library.

- **Config/**: TOML configuration parsing and management
- **Core/**: `SystemInfo` class that aggregates all system data
- **UI/**: Terminal output formatting and theming

---

## Custom Type System

Draconis++ uses custom type aliases defined in `include/Drac++/Utils/Types.hpp` to improve code readability and express intent more clearly.

### Why Custom Types?

1. **Consistency**: Uniform naming across the codebase
2. **Expressiveness**: Types like `Option<T>` and `Result<T>` convey meaning
3. **Safety**: Wrapper types prevent common errors
4. **Rust-like ergonomics**: Familiar patterns for developers from other languages

### Primitive Types

| Alias   | Standard Type     | Description              |
|---------|-------------------|--------------------------|
| `u8`    | `std::uint8_t`    | 8-bit unsigned integer   |
| `u16`   | `std::uint16_t`   | 16-bit unsigned integer  |
| `u32`   | `std::uint32_t`   | 32-bit unsigned integer  |
| `u64`   | `std::uint64_t`   | 64-bit unsigned integer  |
| `i8`    | `std::int8_t`     | 8-bit signed integer     |
| `i16`   | `std::int16_t`    | 16-bit signed integer    |
| `i32`   | `std::int32_t`    | 32-bit signed integer    |
| `i64`   | `std::int64_t`    | 64-bit signed integer    |
| `f32`   | `float`           | 32-bit floating-point    |
| `f64`   | `double`          | 64-bit floating-point    |
| `usize` | `std::size_t`     | Unsigned size type       |
| `isize` | `std::ptrdiff_t`  | Signed size type         |

### String Types

| Alias       | Standard Type        | Description                |
|-------------|----------------------|----------------------------|
| `String`    | `std::string`        | Owning string              |
| `StringView`| `std::string_view`   | Non-owning string view     |
| `WString`   | `std::wstring`       | Wide string (Windows)      |

### Container Types

| Alias              | Standard Type                  | Description                     |
|--------------------|--------------------------------|---------------------------------|
| `Vec<T>`           | `std::vector<T>`               | Dynamic array                   |
| `Array<T, N>`      | `std::array<T, N>`             | Fixed-size array                |
| `Span<T>`          | `std::span<T>`                 | Non-owning view of sequence     |
| `Map<K, V>`        | `std::map<K, V>`               | Ordered map                     |
| `UnorderedMap<K,V>`| `std::unordered_map<K, V>`     | Hash map                        |
| `Pair<T1, T2>`     | `std::pair<T1, T2>`            | Pair of values                  |

### Smart Pointers

| Alias               | Standard Type              | Description              |
|---------------------|----------------------------|--------------------------|
| `UniquePointer<T>`  | `std::unique_ptr<T>`       | Unique ownership         |
| `SharedPointer<T>`  | `std::shared_ptr<T>`       | Shared ownership         |

### Result Types (Error Handling)

| Alias         | Standard Type              | Description                        |
|---------------|----------------------------|------------------------------------|
| `Option<T>`   | `std::optional<T>`         | Value that may be absent           |
| `Result<T,E>` | `std::expected<T, E>`      | Value or error                     |
| `Err<E>`      | `std::unexpected<E>`       | Error wrapper for Result           |
| `None`        | `std::nullopt`             | Empty Option value                 |

### Helper Functions

```cpp
// Create an Option with a value
Option<int> x = Some(42);

// Create an empty Option
Option<int> y = None;

// Return success from a function returning Result
return Result<String>{"success"};

// Return error from a function returning Result
return Err(DracError("something went wrong"));
```

### Synchronization

| Alias      | Standard Type             | Description           |
|------------|---------------------------|-----------------------|
| `Mutex`    | `std::mutex`              | Mutex lock            |
| `LockGuard`| `std::lock_guard<Mutex>`  | RAII lock guard       |
| `Future<T>`| `std::future<T>`          | Asynchronous result   |

### Usage Guidelines

1. **Always use custom types** in new code
2. **Import types** in `.cpp` files using:
   ```cpp
   using namespace draconis::utils::types;
   ```
3. **Avoid** `using namespace` in header files — use fully qualified names
4. **Prefer** `Result<T>` over exceptions for error handling

---

## Platform-Specific Implementation Guide

Platform-specific code lives in `src/Lib/OS/`. Each platform has its own implementation file.

### Supported Platforms

| File          | Platform                        |
|---------------|---------------------------------|
| `Windows.cpp` | Windows 10/11                   |
| `Linux.cpp`   | Linux (glibc/musl)              |
| `macOS.cpp`   | macOS 12+                       |
| `BSD.cpp`     | FreeBSD, OpenBSD, NetBSD        |
| `Haiku.cpp`   | Haiku OS                        |
| `Serenity.cpp`| SerenityOS                      |
| `Switch.cpp`  | Nintendo Switch (devkitPro)     |

### Adding Platform-Specific Code

1. **Check for existing abstraction** in `include/Drac++/Core/`
2. **Implement the function** in the appropriate `OS/*.cpp` file
3. **Use preprocessor guards** when necessary:

```cpp
#ifdef __linux__
// Linux-specific implementation
#elifdef _WIN32
// Windows-specific implementation
#elifdef __APPLE__
// macOS-specific implementation
#endif
```

### Platform Detection Macros

The build system defines these macros:

| Macro                  | Condition                    |
|------------------------|------------------------------|
| `DRAC_ARCH_X86_64`     | x86_64 architecture          |
| `DRAC_ARCH_AARCH64`    | ARM64 architecture           |
| `DRAC_ARCH_64BIT`      | 64-bit pointer size          |
| `DRAC_DEBUG`           | Debug build                  |

### Example: Adding a New System Info Function

1. **Declare in header** (`include/Drac++/Core/System.hpp`):
   ```cpp
   namespace draconis::core {
     auto GetBatteryLevel() -> Result<u8>;
   }
   ```

2. **Implement per-platform** (`src/Lib/OS/Windows.cpp`):
   ```cpp
   namespace draconis::core {
     auto GetBatteryLevel() -> Result<u8> {
       SYSTEM_POWER_STATUS status;
       if (!GetSystemPowerStatus(&status))
         return Err(DracError("Failed to get power status"));
       return status.BatteryLifePercent;
     }
   }
   ```

3. **Implement for other platforms** or provide a fallback:
   ```cpp
   // Linux.cpp
   auto GetBatteryLevel() -> Result<u8> {
     // Read from /sys/class/power_supply/...
   }
   ```

### Windows-Specific Notes

- Use `WString` for Windows API calls requiring `wchar_t*`
- Convert with `ConvertWStringToUTF8()` / `ConvertUTF8ToWString()`
- Link against: `dwmapi`, `windowsapp`, `setupapi`, `dxgi`

### macOS-Specific Notes

- Objective-C++ code goes in `src/Lib/OS/macOS/`
- Use `.mm` extension for Objective-C++ files
- Link frameworks via `appleframeworks` dependency

### Linux-Specific Notes

- Optional dependencies: `xcb`, `wayland-client`, `dbus-1`, `pugixml`
- Check feature flags: `DRAC_USE_XCB`, `DRAC_USE_WAYLAND`
- Read system info from `/proc/`, `/sys/`, `/etc/`

---

## Code Style Guidelines

### General Rules

- **C++26** standard
- **2-space indentation** (configured in `.clang-format`)
- **`constexpr`/`consteval`** where possible
- **`noexcept`** for non-throwing functions
- **`[[nodiscard]]`** for functions whose return value matters

### Naming Conventions

| Element       | Style           | Example                |
|---------------|-----------------|------------------------|
| Types/Classes | PascalCase      | `SystemInfo`           |
| Functions     | PascalCase      | `GetCpuInfo()`         |
| Variables     | camelCase       | `cpuCount`             |
| Constants     | SCREAMING_CASE  | `MAX_BUFFER_SIZE`      |
| Namespaces    | lowercase       | `draconis::core`       |

### Function Declarations

```cpp
// Use trailing return type with 'fn' macro (defined as 'auto')
fn GetSystemName() -> Result<String>;

// Mark pure functions as [[nodiscard]]
[[nodiscard]] fn CalculateHash(StringView input) -> u64;

// Use noexcept when appropriate
fn SafeOperation() noexcept -> bool;
```

### Error Handling

```cpp
// Prefer Result<T> over exceptions
fn ReadFile(StringView path) -> Result<String> {
  std::ifstream file(path);
  if (!file)
    return Err(DracError("Failed to open file"));
  // ...
  return content;
}

// Propagate errors explicitly
fn ProcessData() -> Result<Data> {
  auto content = ReadFile("config.toml");
  if (!content)
    return Err(content.error());
  // Process content...
}
```

---

## Submitting Changes

### Workflow

1. **Fork** the repository
2. **Create a branch** for your feature/fix
3. **Make changes** following the style guidelines
4. **Test** your changes: `just test`
5. **Format** code: `just format`
6. **Submit** a pull request

### Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add battery level monitoring
fix: correct memory leak in cache manager
docs: update contributing guide
refactor: simplify OS detection logic
perf: optimize string conversion on Windows
```

### Pull Request Guidelines

- **Descriptive title** summarizing the change
- **Reference issues** if applicable (`Fixes #123`)
- **Include tests** for new functionality
- **Update documentation** if adding public API
- **Keep changes focused** — one feature/fix per PR

---

## Questions?

If you have questions or need help, feel free to open an issue or discussion on the repository.

Thank you for contributing to Draconis++!
