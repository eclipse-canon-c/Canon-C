/* tools/renesas/psim_start.S -- the start file of a test under GDB's
 * PowerPC simulator, psim, in place of libgloss's sim-crt0.o (VERIFY-119).
 *
 * At -msim GCC links libgloss's sim-crt0.o, which defines __atexit, a word
 * for an old EABI convention that nothing of GCC 13 reads; newlib 4.4
 * defines __atexit too, the head of its list of atexit functions
 * (sys/reent.h). Any program then fails to link: "multiple definition of
 * `__atexit'". This start file does what libgloss's does, without that
 * word: it ends the stack's back chain, opens a frame, calls main, and
 * calls exit with main's value. renesas.py assembles it for the variant
 * as sim-crt0.o in a folder that -B puts before GCC's own, under the
 * variant's multilib folder, where the driver looks for it first.
 */
	.section ".text"
	.globl	_start
	.type	_start, @function
_start:
	li	0, 0
	stw	0, 0(1)		/* the back chain ends here */
	stwu	1, -64(1)	/* a frame for main's caller */
	bl	main
	bl	exit		/* main's value, in r3 */
	.size	_start, . - _start
