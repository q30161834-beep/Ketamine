cmake_minimum_required(VERSION 3.16)
project(ketc VERSION 1.0.0 LANGUAGES C)

# ─── C Standard ────────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── Build Options ─────────────────────────────────────────────────────────────
option(KET_BUILD_TESTS   "Build test suite" ON)
option(KET_BUILD_SHARED  "Build shared library" OFF)
option(KET_ENABLE_ASAN   "Enable AddressSanitizer" OFF)
option(KET_ENABLE_UBSAN  "Enable UBSan" OFF)
option(KET_ENABLE_LTO    "Enable LTO" OFF)
option(KET_STRICT_WARNINGS "Enable -Wall -Wextra -Werror" ON)

# ─── Sources ───────────────────────────────────────────────────────────────────
set(KET_SOURCES
    src/main.c
    src/lexer.c
    src/parser.c
    src/arena.c
    src/diagnostics.c
    src/symbol_table.c
    src/typecheck.c
    src/borrow.c
    src/monomorph.c
    src/trait_resolve.c
    src/package.c
    src/lsp.c
    src/ir.c
    src/ir_optimize.c
    src/codegen.c
    src/native.c
    src/forth.c
    src/repl.c
    src/format.c
    src/logging.c
    src/docgen.c
    src/ffi_ownership.c
    src/monomorph_dispatch.c
    src/jit_layout.c
    src/runtime.c
    src/ffi_zero_copy.c
    src/dwarf.c
    src/parallel.c
    src/registry.c
    src/coverage.c
    src/profiler.c
    src/macros.c
    src/closures.c
    src/const_generics.c
    src/test_framework.c
    src/trait_associated.c
    src/build_system.c
)

set(KET_HEADERS
    src/include/types.h
    src/include/lexer.h
    src/include/parser.h
    src/include/arena.h
    src/include/codegen.h
    src/include/typecheck.h
    src/include/borrow.h
    src/include/monomorph.h
    src/include/trait_resolve.h
    src/include/package.h
    src/include/lsp.h
    src/include/ffi_ownership.h
    src/include/monomorph_dispatch.h
    src/include/jit_layout.h
    src/include/runtime.h
    src/include/ffi_zero_copy.h
    src/include/dwarf.h
    src/include/parallel.h
    src/include/registry.h
    src/include/coverage.h
    src/include/profiler.h
    src/include/macros.h
    src/include/closures.h
    src/include/const_generics.h
    src/include/test_framework.h
    src/include/trait_associated.h
    src/include/build_system.h
)

# ─── Executable ────────────────────────────────────────────────────────────────
add_executable(ketc ${KET_SOURCES} ${KET_HEADERS})

target_include_directories(ketc PRIVATE src)

# ─── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ketc PRIVATE /W4 /utf-8)
    if(KET_STRICT_WARNINGS)
        target_compile_options(ketc PRIVATE /WX)
    endif()
else()
    target_compile_options(ketc PRIVATE -Wall -Wextra)
    if(KET_STRICT_WARNINGS)
        target_compile_options(ketc PRIVATE -Werror -Wpedantic
            -Wshadow -Wstrict-prototypes -Wmissing-prototypes
            -Wconversion -Wno-sign-conversion)
    endif()
    if(KET_ENABLE_ASAN)
        target_compile_options(ketc PRIVATE -fsanitize=address)
        target_link_options(ketc PRIVATE -fsanitize=address)
    endif()
    if(KET_ENABLE_UBSAN)
        target_compile_options(ketc PRIVATE -fsanitize=undefined)
        target_link_options(ketc PRIVATE -fsanitize=undefined)
    endif()
    if(KET_ENABLE_LTO)
        target_compile_options(ketc PRIVATE -flto)
        target_link_options(ketc PRIVATE -flto)
    endif()
endif()

# ─── Release build optimization ────────────────────────────────────────────────
set_target_properties(ketc PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(ketc PRIVATE /O2 /GL)
        target_link_options(ketc PRIVATE /LTCG)
    else()
        target_compile_options(ketc PRIVATE -O3 -march=native -flto)
    endif()
endif()

# ─── Tests ─────────────────────────────────────────────────────────────────────
if(KET_BUILD_TESTS AND NOT MSVC)
    enable_testing()

    # Helper macro for adding test executables
    macro(add_ket_test name sources)
        add_executable(${name} ${sources})
        target_include_directories(${name} PRIVATE src)
        add_test(NAME ${name} COMMAND ${name})
    endmacro()

    # ── Unit Tests ──
    add_ket_test(test_lexer
        tests/test_lexer.c
        src/lexer.c src/arena.c src/diagnostics.c src/symbol_table.c
    )

    add_ket_test(test_parser
        tests/test_parser.c
        src/lexer.c src/parser.c src/arena.c src/diagnostics.c src/symbol_table.c
    )

    add_ket_test(test_typecheck
        tests/test_typecheck.c
        src/lexer.c src/parser.c src/typecheck.c src/arena.c
        src/diagnostics.c src/symbol_table.c
    )

    add_ket_test(test_borrowcheck
        tests/test_borrowcheck.c
        src/lexer.c src/parser.c src/typecheck.c src/borrow.c src/arena.c
        src/diagnostics.c src/symbol_table.c
    )

    add_ket_test(test_optimizer
        tests/test_optimizer.c
        src/lexer.c src/parser.c src/typecheck.c src/borrow.c src/arena.c
        src/diagnostics.c src/symbol_table.c src/ir.c src/ir_optimize.c
    )

    add_ket_test(test_lsp
        tests/test_lsp.c
        src/lexer.c src/parser.c src/arena.c src/lsp.c
    )

    add_ket_test(test_package
        tests/test_package.c
        src/arena.c src/diagnostics.c src/symbol_table.c src/package.c
    )

    add_ket_test(test_codegen
        tests/test_codegen.c
        src/lexer.c src/parser.c src/typecheck.c src/borrow.c src/arena.c
        src/diagnostics.c src/symbol_table.c src/ir.c src/ir_optimize.c src/codegen.c
    )

    # ── Integration Tests ──
    add_ket_test(test_integration
        tests/integration/test_compile.c
        src/lexer.c src/parser.c src/typecheck.c src/borrow.c src/arena.c
        src/diagnostics.c src/symbol_table.c src/ir.c src/ir_optimize.c src/codegen.c
    )

    add_ket_test(test_runner
        tests/runner/test_runner.c
        src/lexer.c src/parser.c src/typecheck.c src/borrow.c src/monomorph.c
        src/trait_resolve.c src/arena.c src/diagnostics.c src/symbol_table.c
        src/ir.c src/ir_optimize.c src/codegen.c
    )
    set_tests_properties(test_runner PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    # ── Custom target to run all tests ──
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 60
        DEPENDS test_lexer test_parser test_typecheck test_codegen
                test_borrowcheck test_optimizer test_lsp test_package
                test_integration test_runner
        COMMENT "Running all Ketamine tests..."
    )
endif()

# ─── Install ───────────────────────────────────────────────────────────────────
install(TARGETS ketc DESTINATION bin)
install(FILES ${KET_HEADERS} DESTINATION include/ketamine)
install(DIRECTORY stdlib/ DESTINATION lib/ketamine/stdlib)
install(DIRECTORY examples/ DESTINATION share/ketamine/examples)
install(FILES README.md DESTINATION share/doc/ketamine)

# ─── Custom target: format ────────────────────────────────────────────────────
if(UNIX)
    find_program(CLANG_FORMAT clang-format)
    if(CLANG_FORMAT)
        add_custom_target(format
            COMMAND ${CLANG_FORMAT} -i -style=file ${KET_SOURCES} ${KET_HEADERS}
            COMMENT "Formatting source code"
        )
    endif()
endif()

# ─── Post-build message ────────────────────────────────────────────────────────
message(STATUS "Ketamine Compiler v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Build with: cmake --build .")
message(STATUS "Run:       ./ketc --help")
