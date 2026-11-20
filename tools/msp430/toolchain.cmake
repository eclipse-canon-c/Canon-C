# toolchain.cmake -- the tests for MSP430X with TI's msp430-elf-gcc, run
# under msp430-elf-run, the GDB simulator the toolchain ships (VERIFY-096).
# Set by tools/msp430/msp430.py, which says what they hold:
#   CANON_MSP_FLAGS   the memory model's flags, and the defines of every test
#   CANON_MSP_LINK    linked into each test: tmpfile.c's object, with its
#                     -Wl,--wrap=tmpfile (VERIFY-098)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR msp430)
set(CMAKE_C_COMPILER msp430-elf-gcc)
# CMake's checks compile only: they need no simulator runtime.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_canon_flags "$ENV{CANON_MSP_FLAGS}")
if(_canon_flags STREQUAL "")
  message(FATAL_ERROR "CANON_MSP_FLAGS is empty: run tools/msp430/msp430.py")
endif()
# The model's flags reach the link too (CMake passes the C flags there), so
# that -mlarge selects the large model's libraries and linker script.
set(CMAKE_C_FLAGS_INIT "${_canon_flags}")
# -msim: newlib's simulator runtime (libsim), whose system calls the
# simulator carries out, and the simulator's linker script (msp430-sim.ld,
# or msp430xl-sim.ld with -mlarge).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-msim $ENV{CANON_MSP_LINK}")
set(CMAKE_CROSSCOMPILING_EMULATOR msp430-elf-run)
