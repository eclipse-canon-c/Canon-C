# toolchain.cmake -- the tests for bare-metal AArch64 with Ubuntu's
# aarch64-linux-gnu-gcc and picolibc, run under qemu-system-aarch64
# (VERIFY-099). Set by tools/aarch64/aarch64.py, which says what they hold:
#   CANON_A64_FLAGS   the core's flags, and the defines of every test
#   CANON_A64_QEMU    the QEMU command before the binary, ;-separated
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
# CMake's checks compile only: they need no start file or linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_canon_flags "$ENV{CANON_A64_FLAGS}")
if(_canon_flags STREQUAL "")
  message(FATAL_ERROR "CANON_A64_FLAGS is empty: run tools/aarch64/aarch64.py")
endif()
# picolibc.specs: picolibc's headers and libraries in place of glibc's,
# which the compiler is built for; the flags reach the link too.
set(CMAKE_C_FLAGS_INIT "${_canon_flags} --specs=picolibc.specs")
# picolibc's semihosting start file and library, which carry the output
# and main's status to QEMU, and picolibc's linker script, its memory set
# inside QEMU's virt RAM (0x40000000, 128 MB): 16 MB of flash, then 16 MB
# of RAM, a 256 KB stack among it. aarch64.py's LINK links its probe the
# same way: the two must stay the same.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--oslib=semihost --crt0=semihost -Wl,--defsym=__flash=0x40000000 -Wl,--defsym=__flash_size=0x1000000 -Wl,--defsym=__ram=0x41000000 -Wl,--defsym=__ram_size=0x1000000 -Wl,--defsym=__stack_size=0x40000")
set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{CANON_A64_QEMU}")
