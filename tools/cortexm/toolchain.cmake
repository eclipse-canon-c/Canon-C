# toolchain.cmake -- the tests for Cortex-M with arm-none-eabi-gcc, run
# under QEMU (VERIFY-089). Set by tools/cortexm/cortexm.py, which says
# what each variable holds:
#   CANON_CM_FLAGS    the CPU's flags
#   CANON_CM_LIBC     newlib or picolibc
#   CANON_CM_SUPPORT  newlib only: the start-up object and linker script
#   CANON_CM_QEMU     the QEMU command before the binary, ;-separated
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
# Nothing links without the start-up code: CMake's checks compile only.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_canon_flags "$ENV{CANON_CM_FLAGS}")
set(_canon_libc "$ENV{CANON_CM_LIBC}")
set(_canon_support "$ENV{CANON_CM_SUPPORT}")
if(_canon_flags STREQUAL "")
  message(FATAL_ERROR "CANON_CM_FLAGS is empty: run tools/cortexm/cortexm.py")
endif()
if(_canon_libc STREQUAL "picolibc")
  # picolibc's own linker script and its semihosting start file, which
  # reports a fault and passes main's status to QEMU.
  set(CMAKE_C_FLAGS_INIT "${_canon_flags} --specs=picolibc.specs")
  set(CMAKE_EXE_LINKER_FLAGS_INIT
      "--oslib=semihost --crt0=semihost -Wl,--defsym=__flash_size=0x400000 -Wl,--defsym=__ram_size=0x400000 -Wl,--defsym=__stack_size=0x40000")
elseif(_canon_libc STREQUAL "newlib")
  # newlib with rdimon, its semihosting library, and the start-up code of
  # tools/cortexm/startup.c in place of newlib's.
  set(CMAKE_C_FLAGS_INIT "${_canon_flags}")
  set(CMAKE_EXE_LINKER_FLAGS_INIT
      "--specs=rdimon.specs -nostartfiles -T ${_canon_support}/cortexm.ld ${_canon_support}/startup.o")
else()
  message(FATAL_ERROR "CANON_CM_LIBC: newlib or picolibc, not '${_canon_libc}'")
endif()
set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{CANON_CM_QEMU}")
