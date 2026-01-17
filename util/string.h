#ifndef CANON_UTIL_STRING_H
#define CANON_UTIL_STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Philosophy:
//    - Prefer non-allocating functions when possible
//    - All functions are null-safe
//    - Explicit success/failure reporting
//    - No hidden global state
// ─────────────────────────────────────────────────────────────────────────────

// === Non-allocating (borrowed / fixed-buffer) operations ===

static inline bool str_copy(char* restrict dst, size_t dst_size, const char* restrict src) {
    if (!dst || dst_size == 0 || !src) return false;
    size_t i = 0;
    while (i + 1 < dst_size && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return src[i] == '\0';  // true = full copy, false = truncated
}

static inline bool str_concat(char* restrict dst, size_t dst_size,
                             const char* restrict a, const char* restrict b) {
    if (!dst || dst_size == 0 || !a || !b) return false;

    size_t pos = 0;
    while (pos + 1 < dst_size && *a) {
        dst[pos++] = *a++;
    }
    while (pos + 1 < dst_size && *b) {
        dst[pos++] = *b++;
    }
    dst[pos] = '\0';
    return *a == '\0' && *b == '\0';
}

static inline bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static inline bool str_equals_ignore_case(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;

    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++; b++;
    }
    return *a == *b;
}

static inline bool str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    while (*prefix) {
        if (*s++ != *prefix++) return false;
    }
    return true;
}

static inline bool str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    size_t slen = strlen(s);
    size_t tlen = strlen(suffix);
    if (tlen > slen) return false;
    return memcmp(s + slen - tlen, suffix, tlen) == 0;
}

// === Allocating functions (caller must free) ===

static inline char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* p = (char*)malloc(len + 1);
    if (p) {
        memcpy(p, s, len + 1);
    }
    return p;
}

static inline char* str_concat_new(const char* a, const char* b) {
    if (!a || !b) return NULL;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char* p = (char*)malloc(la + lb + 1);
    if (p) {
        memcpy(p, a, la);
        memcpy(p + la, b, lb + 1);
    }
    return p;
}

// Very common utility - often missed
static inline char* str_sub_new(const char* s, size_t start, size_t len) {
    if (!s) return NULL;
    size_t slen = strlen(s);
    if (start >= slen) return NULL;

    size_t avail = slen - start;
    if (len > avail) len = avail;

    char* p = (char*)malloc(len + 1);
    if (p) {
        memcpy(p, s + start, len);
        p[len] = '\0';
    }
    return p;
}

static inline void str_free(char* p) {
    free(p);
}

#ifdef __cplusplus
}
#endif

#endif
