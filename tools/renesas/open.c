/* tools/renesas/open.c -- open() for newlib on the V850 under GDB's
 * simulator, its flags read where its callers pass them (VERIFY-101).
 *
 * newlib 4.4 defines the V850's _open with two fixed parameters
 * (libc/sys/sysnecv850/open.c), and calls it, from _open_r and through it
 * from open() and fopen(), through the prototype newlib declares for it,
 * int _open(const char *, int, ...). The two types are not compatible, and
 * GCC's V850 port passes the arguments differently: _open_r's call through
 * the variadic prototype puts the path in r6 and the flags and the mode on
 * the stack, and the fixed _open takes its flags from r7, which at that
 * call still holds the path's address. The simulator then opens the file
 * with the address's bits for flags: newlib's open() does not work there,
 * and nor does anything that opens a file.
 *
 * This _open has the variadic type its callers call it through, and passes
 * the path and the flags to the simulator's open system call as newlib's
 * does, with a mode of 0, as newlib's passes. renesas.py compiles it for the
 * variant and links it into each test: an object of the link, it is taken
 * before libc.a's. Nothing outside the renesas job links it.
 */
#include <sys/syscall.h>

int __trap0(int function, int p1, int p2, int p3);
int _open(const char *path, int flags, ...);

int _open(const char *path, int flags, ...)
{
    return __trap0(SYS_open, (int)path, flags, 0);
}
