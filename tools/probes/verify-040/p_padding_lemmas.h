/*
 * p_padding_lemmas.h — VERIFY-040 round 3, block 7: the bit-level steps on
 * bitset's padding word, stated as ACSL lemmas, each a goal of its own.
 *
 * Block 7 ("Bit-level identities on the padding word", docs/deviations.md)
 * files four goals of frama-c-bitset: bitset_pad after bitset_set's,
 * bitset_clear's and bitset_toggle's single-bit store into the last word,
 * and after bitset_clear_padding's mask. Its Retirement names lemmas for the
 * identities. The three below state the steps the four goals take, bit by
 * bit, in the form (x & (1 << j)) == 0 that WP's theory ties to a bit test:
 *
 *   bitset_shr_zero_bits  an integer that shifts right by r to 0 has no
 *                         bit j from r on;
 *   bitset_bits_shr_zero  a u64 with no bit from r to 63 shifts right by r
 *                         to 0;
 *   bitset_mask_bits      the mask that bitset_clear_padding builds,
 *                         2^r - 1, has no bit j from r on.
 *
 * Under WP 25, with Z3 and CVC4, the four goals prove with the three as
 * hypotheses, and none of them without bitset_bits_shr_zero. The identities
 * in the code's own forms, assumed, do not close them: in each goal WP puts
 * the mask before the word, as the operand of |, & or ^, and in a lemma that
 * states the store as the code writes it the word comes first.
 *
 * They are tried here, by the round 3 probe (round3_probe.py), and not in
 * data/bitset.h: a lemma WP does not prove is an annotation WP assumes
 * without proving it, which VERIFY-040 does not admit, and every goal of
 * the unit that declared it could rest on it. A lemma the CI's provers prove
 * can go into data/bitset.h, with a prediction of its own; while a goal of
 * block 7 stays, the block is needed.
 *
 * Run from the repository root (see round3_probe.py for the commands).
 */
#include "vmacros/vdrivers/bitset_verify.h"

/*@ lemma bitset_shr_zero_bits:
      \forall integer x, r, j;
        0 <= r <= j && (x >> r) == 0 ==> (x & (1 << j)) == 0;

    lemma bitset_bits_shr_zero:
      \forall u64 x; \forall integer r;
        0 <= r <= 63
        && (\forall integer j; r <= j < 64 ==> (x & (1 << j)) == 0)
        ==> (x >> r) == 0;

    lemma bitset_mask_bits:
      \forall integer r, j;
        0 < r <= j < 64
        ==> ((u64)((u64)((u64)1 << r) - 1) & (1 << j)) == 0;
*/
