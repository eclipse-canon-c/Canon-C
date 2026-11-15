/*
 * The markers of eva_program.h, for E-ACSL's runs (tools/eva/eacsl.py,
 * VERIFY-083).
 *
 * Under Frama-C, CHECK and END call Eva's builtins. E-ACSL instruments a
 * program as Frama-C reads it, so the instrumented program calls them too,
 * and these define them: each check that fails, and the end of main with
 * the count of failed checks. Compiled as it is, not instrumented.
 */
#include <stdio.h>

void Frama_C_show_each_check_failed(int line);
void Frama_C_show_each_reached_end(int fails);

void Frama_C_show_each_check_failed(int line)
{
    printf("CHECK failed at line %d\n", line);
    fflush(stdout);
}

void Frama_C_show_each_reached_end(int fails)
{
    printf("END reached, fails %d\n", fails);
    fflush(stdout);
}
