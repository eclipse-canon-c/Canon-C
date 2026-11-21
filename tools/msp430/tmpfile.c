/* tmpfile.c -- the tests' tmpfile() on MSP430X under the GDB simulator
 * (VERIFY-098). There newlib's own tmpfile() does not return (VERIFY-097's
 * file probe), so tools/msp430/msp430.py links each test with
 * -Wl,--wrap=tmpfile and this file: the tests' tmpfile() is a stream in
 * memory, newlib's fmemopen(), which they write, rewind and read back as a
 * temporary file, and which fclose() frees. NULL when it cannot be made,
 * as tmpfile() may return. The renesas job's RX variants link it too
 * (tools/renesas/renesas.py, VERIFY-102): GDB's RX simulator has no lseek,
 * so a file's stream does not rewind there. Nothing else links it. */
#include <stddef.h>
#include <stdio.h>

/* POSIX.1-2008's; newlib declares it beyond -std=c99 only. */
FILE *fmemopen(void *buf, size_t size, const char *mode);

FILE *__wrap_tmpfile(void);

/* Each test writes a few lines to its file; 1 KB holds several times that. */
#define CANON_MSP_TMPFILE_BYTES ((size_t)1024)

FILE *__wrap_tmpfile(void)
{
    return fmemopen(NULL, CANON_MSP_TMPFILE_BYTES, "w+");
}
