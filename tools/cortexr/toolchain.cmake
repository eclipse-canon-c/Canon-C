# toolchain.cmake -- the tests for bare-metal Cortex-R5F and Cortex-R52
# with Ubuntu's arm-none-eabi-gcc and picolibc, run on QEMU's models of the
# two cores (VERIFY-113). Set by tools/cortexr/cortexr.py, which says what
# they hold:
#   CANON_CR_FLAGS   the core's flags, and the defines of every test
#   CANON_CR_QEMU    the command before the binary, ;-separated: qemu.sh
#                    and QEMU's name of the core
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
# CMake's checks compile only: they need no start file or linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_canon_flags "$ENV{CANON_CR_FLAGS}")
if(_canon_flags STREQUAL "")
  message(FATAL_ERROR "CANON_CR_FLAGS is empty: run tools/cortexr/cortexr.py")
endif()
set(CMAKE_C_FLAGS_INIT "${_canon_flags} --specs=picolibc.specs")
# picolibc's semihosting start file and library, which carry the output
# and main's status to QEMU, and picolibc's linker script, its memory set
# inside the RAM qemu.sh gives QEMU's empty machine (64 MB from 0): 16 MB
# of flash from 0, then 16 MB of RAM, a 256 KB stack among it. cortexr.py's
# LINK links its probe the same way: the two must stay the same.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--oslib=semihost --crt0=semihost -Wl,--defsym=__flash=0x0 -Wl,--defsym=__flash_size=0x1000000 -Wl,--defsym=__ram=0x1000000 -Wl,--defsym=__ram_size=0x1000000 -Wl,--defsym=__stack_size=0x40000")
set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{CANON_CR_QEMU}")
