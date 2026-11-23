#!/usr/bin/env bash
# qemu.sh CPU ELF -- run ELF on QEMU's model of CPU, cortex-r5f or
# cortex-r52, on QEMU's empty machine, through semihosting (VERIFY-113).
# That machine has no board around the CPU, and takes no -kernel: the
# generic loader puts the ELF into its RAM, 64 MB from address 0, and the
# CPU at the ELF's entry. toolchain.cmake sets picolibc's memory inside
# that RAM. CMake runs each test as "qemu.sh CPU TEST", the emulator first.
set -eu
cpu=$1 elf=$2
# A comma in a QEMU option's value is written twice.
exec qemu-system-arm -M none -cpu "$cpu" -m 64M -nographic -monitor none -serial none \
  -semihosting-config enable=on,target=native \
  -device "loader,file=${elf//,/,,},cpu-num=0"
