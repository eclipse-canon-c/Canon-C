/*
 * What every program here includes after the Canon-C file it uses
 * (tools/eva/README.md).
 *
 * CHECK(c) counts a check that fails, in the int `fails` that main
 * declares. END(fails) marks the end of main. Under Frama-C both call Eva
 * builtins, which print what Eva computed there, and tools/eva/eva.py reads
 * those lines: a check's line number if Eva finds that the check can fail,
 * and the values of `fails` at the end. Eva printing {0} there means that
 * Eva found every check to hold; no line means that no execution Eva
 * considered reached the end. Compiled by a C compiler, CHECK only counts,
 * END does nothing, and main returns 1 if a check failed.
 */
#ifndef CANON_EVA_PROGRAM_H
#define CANON_EVA_PROGRAM_H

#ifdef __FRAMAC__
void Frama_C_show_each_check_failed(int line);
void Frama_C_show_each_reached_end(int fails);
#define CHECK(c) do { if (!(c)) { Frama_C_show_each_check_failed(__LINE__); fails++; } } while (0)
#define END(n) Frama_C_show_each_reached_end(n)
#else
#define CHECK(c) do { if (!(c)) { fails++; } } while (0)
#define END(n) ((void)(n))
#endif

#endif /* CANON_EVA_PROGRAM_H */
