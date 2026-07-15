# Building the Ketamine Compiler

## Prerequisites

- C11 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- (Optional) LLVM tools for native codegen: `llc`, `clang`

## Quick Start

```bash
git clone https://github.com/ketamine-lang/ketamine.git
cd ketamine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Debug | Release, Debug, RelWithDebInfo |
| `KET_BUILD_TESTS` | ON | Build test suite |
| `KET_ENABLE_ASAN` | OFF | AddressSanitizer |
| `KET_ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer |
| `KET_ENABLE_LTO` | OFF | Link-time optimization |
| `KET_STRICT_WARNINGS` | ON | -Wall -Wextra -Werror |

## Debug Build

```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Test Build

```bash
cmake .. -DKET_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```
