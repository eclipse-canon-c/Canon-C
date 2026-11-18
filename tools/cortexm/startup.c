/*
 * startup.c -- start-up code for the tests under QEMU with newlib
 * (tools/cortexm/cortexm.py, VERIFY-089).
 *
 * A Cortex-M takes its stack from the vector table at reset; newlib's own
 * start file takes it from semihosting's SYS_HEAPINFO instead, which on
 * mps2-an386 QEMU answered with 0x22000000, apart from the 4 MB at
 * 0x20000000 that cortexm.ld links the program in. This file gives the
 * vector table, takes the stack from it, enables the FPU where there is
 * one, zeroes .bss, opens the semihosting handles for stdio, runs the
 * constructors and calls main. picolibc's semihosting start file does as
 * much for picolibc, and is used as it is.
 *
 * A fault ends the run through semihosting, with exit status 70.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* A linker symbol, the address the stack starts at (cortexm.ld), declared
 * as a function so that the vector table holds function pointers alone. */
extern void __stack_top(void);
extern unsigned long __bss_start__[];
extern unsigned long __bss_end__[];
extern int main(void);
extern void initialise_monitor_handles(void);
extern void __libc_init_array(void);

void canon_reset_handler(void);
void canon_fault_handler(void);
void _init(void);
void _fini(void);

/* newlib's constructors and destructors call _init and _fini, which the
 * start files it is linked without would give. */
void _init(void) {}
void _fini(void) {}

/* The vector table: the initial stack, reset, NMI, HardFault, MemManage,
 * BusFault, UsageFault, four reserved, SVCall, DebugMonitor, reserved,
 * PendSV, SysTick. */
__attribute__((section(".vectors"), used))
void (*const canon_vectors[16])(void) = {
    __stack_top, canon_reset_handler,
    canon_fault_handler, canon_fault_handler, canon_fault_handler,
    canon_fault_handler, canon_fault_handler, 0, 0, 0, 0,
    canon_fault_handler, canon_fault_handler, 0,
    canon_fault_handler, canon_fault_handler,
};

void canon_fault_handler(void)
{
    static const char msg[] = "FAULT: an exception was taken (tools/cortexm/startup.c)\n";
    (void)fwrite(msg, 1, sizeof msg - 1, stderr);
    _exit(70);
}

void canon_reset_handler(void)
{
    unsigned long *p;
#if defined(__ARM_FP)
    /* CPACR: full access to CP10 and CP11, the FPU. */
    *(volatile unsigned long *)0xE000ED88UL |= 0xFUL << 20;
    __asm volatile ("dsb\n\tisb" ::: "memory");
#endif
    for (p = __bss_start__; p < __bss_end__; ++p) { *p = 0; }
    initialise_monitor_handles();
    __libc_init_array();
    exit(main());
}
