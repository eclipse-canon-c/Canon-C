/*
 * compare.h's comparators as total orders (VERIFY-084,
 * vmacros/vdrivers/compare_order_verify.h): for each of the 24, the
 * driver's six functions, whose assertions state that it is reflexive,
 * antisymmetric and transitive, which WP proves for every value. Here each
 * runs on every value, pair and triple of a few values of its type: an
 * integer type's bounds, 0 and 1, and -1 if it is signed; for f32 and f64,
 * a NaN, the infinities, -1, -0, +0 and 1. The comparators are called,
 * not inlined, so Eva checks the assertions at those values, and E-ACSL as
 * the program runs; a native build runs them, the assertions comments.
 */
#include <stdint.h>
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/compare_order_verify.h"
#include "eva_program.h"

/* Every value, pair and triple of V through the six functions of T. */
#define ORDER(T, V)                                                          \
    do {                                                                     \
        usize n_ = sizeof (V) / sizeof (V)[0];                               \
        for (usize i = 0u; i < n_; i++) {                                    \
            order_##T##_reflexive(&(V)[i], NULL);                            \
            order_##T##_desc_reflexive(&(V)[i], NULL);                       \
            for (usize j = 0u; j < n_; j++) {                                \
                order_##T##_antisymmetric(&(V)[i], &(V)[j], NULL);           \
                order_##T##_desc_antisymmetric(&(V)[i], &(V)[j], NULL);      \
                for (usize k = 0u; k < n_; k++) {                            \
                    order_##T##_transitive(&(V)[i], &(V)[j], &(V)[k], NULL); \
                    order_##T##_desc_transitive(&(V)[i], &(V)[j], &(V)[k],   \
                                                NULL);                       \
                }                                                            \
            }                                                                \
        }                                                                    \
    } while (0)

int main(void)
{
    int fails = 0;

    u8 vu8[] = { 0u, 1u, UINT8_MAX };
    u16 vu16[] = { 0u, 1u, UINT16_MAX };
    u32 vu32[] = { 0u, 1u, UINT32_MAX };
    u64 vu64[] = { 0u, 1u, UINT64_MAX };
    i8 vi8[] = { INT8_MIN, -1, 0, 1, INT8_MAX };
    i16 vi16[] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    i32 vi32[] = { INT32_MIN, -1, 0, 1, INT32_MAX };
    i64 vi64[] = { INT64_MIN, -1, 0, 1, INT64_MAX };
    usize vusize[] = { 0u, 1u, SIZE_MAX };
    isize visize[] = { PTRDIFF_MIN, -1, 0, 1, PTRDIFF_MAX };

    /* A quiet NaN, the infinities, -1, -0, +0 and 1, from their bits: no
       arithmetic makes them. */
    union { u32 bits; f32 value; } f32b[] = {
        { 0x7FC00000u }, { 0xFF800000u }, { 0xBF800000u }, { 0x80000000u },
        { 0x00000000u }, { 0x3F800000u }, { 0x7F800000u } };
    union { u64 bits; f64 value; } f64b[] = {
        { 0x7FF8000000000000u }, { 0xFFF0000000000000u }, { 0xBFF0000000000000u },
        { 0x8000000000000000u }, { 0x0000000000000000u }, { 0x3FF0000000000000u },
        { 0x7FF0000000000000u } };
    f32 vf32[7];
    f64 vf64[7];
    for (usize i = 0u; i < 7u; i++) {
        vf32[i] = f32b[i].value;
        vf64[i] = f64b[i].value;
    }

    ORDER(u8, vu8);
    ORDER(u16, vu16);
    ORDER(u32, vu32);
    ORDER(u64, vu64);
    ORDER(i8, vi8);
    ORDER(i16, vi16);
    ORDER(i32, vi32);
    ORDER(i64, vi64);
    ORDER(usize, vusize);
    ORDER(isize, visize);
    ORDER(f32, vf32);
    ORDER(f64, vf64);

    END(fails);
    return fails != 0;
}
