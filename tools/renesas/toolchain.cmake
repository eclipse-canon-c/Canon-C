# toolchain.cmake -- the tests for a Renesas target with the toolchain of
# tools/renesas/build.sh, run under GDB's simulator of the target
# (VERIFY-101, VERIFY-102). Set by tools/renesas/renesas.py, which says
# what each variable holds:
#   CANON_RS_TARGET  the toolchain's target, v850-elf for RH850, rx-elf for RX
#   CANON_RS_FLAGS   the variant's flags, and the defines of every test
#   CANON_RS_LINK    what each test links besides its objects: the object of
#                    tools/renesas/open.c (RH850); -msim, and the object of
#                    tools/msp430/tmpfile.c with -Wl,--wrap=tmpfile (RX);
#                    -B and the folder of the corrected crt files (rx-be)
#   CANON_RS_RUN     the simulator's command before the binary, ;-separated
# renesas.py sets CMAKE_C_FLAGS_DEBUG itself where the variant has its own
# Debug flags (RX: -O0, without -g).
set(CMAKE_SYSTEM_NAME Generic)
set(_canon_target "$ENV{CANON_RS_TARGET}")
set(_canon_flags "$ENV{CANON_RS_FLAGS}")
if(_canon_target STREQUAL "" OR _canon_flags STREQUAL "")
  message(FATAL_ERROR "CANON_RS_TARGET or CANON_RS_FLAGS is empty: run tools/renesas/renesas.py")
endif()
string(REGEX REPLACE "-.*" "" _canon_processor "${_canon_target}")
set(CMAKE_SYSTEM_PROCESSOR "${_canon_processor}")
set(CMAKE_C_COMPILER "${_canon_target}-gcc")
# CMake's checks compile only: newlib's start file links them, but nothing
# of theirs is run.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# newlib's start file, library and system calls, and the linker's own
# script for the target, as the compiler links them by default (with -msim,
# libgloss's system calls for the simulator, at RX); and CANON_RS_LINK's
# objects, taken before libc.a's.
set(CMAKE_C_FLAGS_INIT "${_canon_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "$ENV{CANON_RS_LINK}")
set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{CANON_RS_RUN}")
