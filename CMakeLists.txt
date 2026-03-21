cmake_minimum_required(VERSION 3.10)
project(Canon-C
    VERSION 1.2.0
    LANGUAGES C
    DESCRIPTION "Explicit semantic standard library for C99"
)

# ---------------------------------------------------------------------------
# Language standard — C99 strict, no compiler extensions
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Always export compile commands — required for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_SANITIZERS "Enable ASan + UBSan (GCC/Clang) or ASan (MSVC)" OFF)
option(ENABLE_FUZZING    "Enable libFuzzer fuzz targets (Clang only)"       OFF)

# ---------------------------------------------------------------------------
# Warning flags — per compiler
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /WX
        /wd4127   # C4127: conditional expression is constant
                  # Fired by EXPECT(sizeof(...) == N) and similar
                  # compile-time-constant test assertions. Suppressed
                  # globally for test code — not a real defect here.
        /wd4996   # C4996: 'strncpy' / other "unsafe" CRT functions.
                  # contract_test.c intentionally tests boundary
                  # behaviour of standard C string functions.
        /wd4146   # C4146: unary minus applied to unsigned type.
                  # types_test.c tests deliberate unsigned negation
                  # to verify wrap-around behaviour.
        /wd4308   # C4308: negative integral constant converted to unsigned.
                  # Companion diagnostic to C4146 — same source.
    )
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# Sanitizer flags
# Mutually exclusive with fuzzing — libFuzzer manages its own instrumentation
#
# NOTE: MSVC ASan (/fsanitize=address) requires clang_rt.asan*.dll to be
# on PATH at runtime. This DLL is not automatically deployed in CI
# environments and causes exit code 0xc0000135 (DLL not found). MSVC
# builds therefore skip sanitizers entirely — memory safety on Windows
# is covered by Dr. Memory in the dr-memory CI job instead.
# ---------------------------------------------------------------------------
if(ENABLE_SANITIZERS AND ENABLE_FUZZING)
    message(FATAL_ERROR
        "ENABLE_SANITIZERS and ENABLE_FUZZING are mutually exclusive. "
        "libFuzzer manages its own sanitizer instrumentation.")
endif()

if(ENABLE_SANITIZERS)
    if(MSVC)
        # Intentionally skipped — see note above.
        message(STATUS "MSVC detected: skipping ASan (DLL deployment not supported in CI)")
    else()
        # GCC/Clang: ASan + UBSan
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ---------------------------------------------------------------------------
# Fuzzing flags — Clang only
# ---------------------------------------------------------------------------
if(ENABLE_FUZZING)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "ENABLE_FUZZING requires Clang. "
            "Current compiler: ${CMAKE_C_COMPILER_ID}")
    endif()
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()

# ---------------------------------------------------------------------------
# Helper: canon_c_test(NAME <n> SRC <path> INCLUDES <dirs...> [FUZZABLE])
#
# Always registers a unit test binary.
#
# When FUZZABLE is present and ENABLE_FUZZING=ON, also registers a fuzz
# twin named fuzz_<n> from the same source file, compiled with
# CANON_FUZZING defined. The source file gates on this:
#
#   #ifdef CANON_FUZZING
#     int LLVMFuzzerTestOneInput(...) { ... }   // fuzz entry point
#   #else
#     int main(void) { ... }                    // unit test entry point
#   #endif
#
# All special cases (libm, forced assertions, include quirks) are handled
# inside the source file — not here.
#
# Usage:
#   canon_c_test(
#       NAME     checked_test
#       SRC      test/core/primitives/checked_test.c
#       INCLUDES core/primitives
#       FUZZABLE
#   )
#
#   canon_c_test(
#       NAME     contract_test
#       SRC      test/core/primitives/contract_test.c
#       INCLUDES core/primitives
#   )
# ---------------------------------------------------------------------------
function(canon_c_test)
    cmake_parse_arguments(T "FUZZABLE" "NAME;SRC" "INCLUDES" ${ARGN})

    # --- Unit test binary ---------------------------------------------------
    add_executable(${T_NAME} ${T_SRC})

    foreach(inc IN LISTS T_INCLUDES)
        target_include_directories(${T_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/${inc})
    endforeach()

    add_test(NAME ${T_NAME} COMMAND ${T_NAME})

    # --- Fuzz twin (only when FUZZABLE and ENABLE_FUZZING=ON) ---------------
    if(T_FUZZABLE AND ENABLE_FUZZING)
        set(FUZZ_NAME "fuzz_${T_NAME}")

        add_executable(${FUZZ_NAME} ${T_SRC})

        foreach(inc IN LISTS T_INCLUDES)
            target_include_directories(${FUZZ_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/${inc})
        endforeach()

        # Activates LLVMFuzzerTestOneInput, suppresses main()
        target_compile_definitions(${FUZZ_NAME} PRIVATE CANON_FUZZING)

        # Link against libFuzzer
        target_link_options(${FUZZ_NAME} PRIVATE -fsanitize=fuzzer)

        # 30s CTest timeout — -max_total_time=25 leaves 5s margin
        add_test(NAME ${FUZZ_NAME} COMMAND ${FUZZ_NAME} -max_total_time=25)
        set_tests_properties(${FUZZ_NAME} PROPERTIES TIMEOUT 30)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# core/primitives/ tests
#
# Note on INCLUDES:
#   - "core/primitives" is always included so that flat includes like
#     #include "bits.h" resolve correctly.
#   - "." (repo root) is added for any header that uses a path-qualified
#     include such as #include "core/primitives/types.h". This affects
#     compare.h, ptr.h, and any future header that follows the same
#     convention.
# ---------------------------------------------------------------------------

canon_c_test(
    NAME     bits_test
    SRC      test/core/primitives/bits_test.c
    INCLUDES core/primitives
    FUZZABLE
)

canon_c_test(
    NAME     checked_test
    SRC      test/core/primitives/checked_test.c
    INCLUDES core/primitives
    FUZZABLE
)

canon_c_test(
    NAME     compare_test
    SRC      test/core/primitives/compare_test.c
    INCLUDES core/primitives .
    FUZZABLE
)

canon_c_test(
    NAME     contract_test
    SRC      test/core/primitives/contract_test.c
    INCLUDES core/primitives
)

canon_c_test(
    NAME     limits_test
    SRC      test/core/primitives/limits_test.c
    INCLUDES core/primitives
)

canon_c_test(
    NAME     types_test
    SRC      test/core/primitives/types_test.c
    INCLUDES core/primitives
)

canon_c_test(
    NAME     ptr_test
    SRC      test/core/primitives/ptr_test.c
    INCLUDES core/primitives .
    FUZZABLE
)

# ---------------------------------------------------------------------------
# core/ tests                          — uncomment when ready (v1.3.0)
# ---------------------------------------------------------------------------
canon_c_test(
    NAME arena_test 
    SRC test/core/arena_test.c 
    INCLUDES core/primitives core . 
    FUZZABLE
)

canon_c_test(
    NAME pool_test      
    SRC test/core/pool_test.c      
    INCLUDES core/primitives core .
    FUZZABLE
)

# canon_c_test(NAME slice_test     SRC test/core/slice_test.c     INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME scope_test     SRC test/core/scope_test.c     INCLUDES core/primitives .)
# canon_c_test(NAME region_test    SRC test/core/region_test.c    INCLUDES core/primitives .)

canon_c_test(
    NAME memory_test 
    SRC test/core/memory_test.c 
    INCLUDES core/primitives core . 
    FUZZABLE
)

canon_c_test(
    NAME ownership_test 
    SRC test/core/ownership_test.c 
    INCLUDES core/primitives core .
    FUZZABLE
)

# ---------------------------------------------------------------------------
# semantics/ tests                     — uncomment when ready (v1.3.0)
# ---------------------------------------------------------------------------
# canon_c_test(NAME option_test SRC test/semantics/option_test.c INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME result_test SRC test/semantics/result_test.c INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME borrow_test SRC test/semantics/borrow_test.c INCLUDES core/primitives .)
# canon_c_test(NAME diag_test   SRC test/semantics/diag_test.c   INCLUDES core/primitives .)
# canon_c_test(NAME error_test  SRC test/semantics/error_test.c  INCLUDES core/primitives .)

# ---------------------------------------------------------------------------
# data/ tests                          — uncomment when ready (v1.3.0)
# ---------------------------------------------------------------------------
# canon_c_test(NAME vec_test            SRC test/data/vec_test.c            INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME deque_test          SRC test/data/deque_test.c          INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME hashmap_test        SRC test/data/hashmap_test.c        INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME array_test          SRC test/data/array_test.c          INCLUDES core/primitives .)
# canon_c_test(NAME stack_test          SRC test/data/stack_test.c          INCLUDES core/primitives .)
# canon_c_test(NAME queue_test          SRC test/data/queue_test.c          INCLUDES core/primitives .)
# canon_c_test(NAME priority_queue_test SRC test/data/priority_queue_test.c INCLUDES core/primitives .)
# canon_c_test(NAME bitset_test         SRC test/data/bitset_test.c         INCLUDES core/primitives .)
# canon_c_test(NAME stringbuf_test      SRC test/data/stringbuf_test.c      INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME range_test          SRC test/data/range_test.c          INCLUDES core/primitives .)

# ---------------------------------------------------------------------------
# data/convenience/ tests              — uncomment when ready (v1.3.0)
# ---------------------------------------------------------------------------
# canon_c_test(NAME dynvec_test    SRC test/data/convenience/dynvec_test.c    INCLUDES core/primitives .)
# canon_c_test(NAME smallvec_test  SRC test/data/convenience/smallvec_test.c  INCLUDES core/primitives .)
# canon_c_test(NAME dynstring_test SRC test/data/convenience/dynstring_test.c INCLUDES core/primitives . FUZZABLE)

# ---------------------------------------------------------------------------
# algo/ tests                          — uncomment when ready (v1.3.0)
# ---------------------------------------------------------------------------
# canon_c_test(NAME map_test     SRC test/algo/map_test.c     INCLUDES core/primitives .)
# canon_c_test(NAME filter_test  SRC test/algo/filter_test.c  INCLUDES core/primitives .)
# canon_c_test(NAME fold_test    SRC test/algo/fold_test.c    INCLUDES core/primitives .)
# canon_c_test(NAME find_test    SRC test/algo/find_test.c    INCLUDES core/primitives .)
# canon_c_test(NAME any_all_test SRC test/algo/any_all_test.c INCLUDES core/primitives .)
# canon_c_test(NAME sort_test    SRC test/algo/sort_test.c    INCLUDES core/primitives .)
# canon_c_test(NAME search_test  SRC test/algo/search_test.c  INCLUDES core/primitives .)
# canon_c_test(NAME unique_test  SRC test/algo/unique_test.c  INCLUDES core/primitives .)
# canon_c_test(NAME reverse_test SRC test/algo/reverse_test.c INCLUDES core/primitives .)

# ---------------------------------------------------------------------------
# util/ tests                          — uncomment when ready (v1.3.0)
# High-value fuzz targets: parse, string, stringbuf, hashmap
# ---------------------------------------------------------------------------
# canon_c_test(NAME string_test SRC test/util/string_test.c INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME split_test  SRC test/util/split_test.c  INCLUDES core/primitives .)
# canon_c_test(NAME join_test   SRC test/util/join_test.c   INCLUDES core/primitives .)
# canon_c_test(NAME view_test   SRC test/util/view_test.c   INCLUDES core/primitives .)
# canon_c_test(NAME intern_test SRC test/util/intern_test.c INCLUDES core/primitives .)
# canon_c_test(NAME log_test    SRC test/util/log_test.c    INCLUDES core/primitives .)
# canon_c_test(NAME file_test   SRC test/util/file_test.c   INCLUDES core/primitives .)
# canon_c_test(NAME parse_test  SRC test/util/parse_test.c  INCLUDES core/primitives . FUZZABLE)
# canon_c_test(NAME time_test   SRC test/util/time_test.c   INCLUDES core/primitives .)
# canon_c_test(NAME random_test SRC test/util/random_test.c INCLUDES core/primitives .)
