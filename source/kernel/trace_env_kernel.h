/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef SOURCE_KERNEL_INTERNAL_TRACE_ENV_KERNEL_H
#define SOURCE_KERNEL_INTERNAL_TRACE_ENV_KERNEL_H

#include <asm/atomic.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#define MIN min
#define MAX max

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t) -1)
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##ULL
#endif

typedef atomic_t env_atomic;
typedef atomic64_t env_atomic64;

static inline int env_atomic_read(const env_atomic *a) {
    return atomic_read(a);
}

static inline void env_atomic_set(env_atomic *a, int i) {
    atomic_set(a, i);
}

static inline void env_atomic_add(int i, env_atomic *a) {
    atomic_add(i, a);
}

static inline void env_atomic_sub(int i, env_atomic *a) {
    atomic_sub(i, a);
}

static inline bool env_atomic_sub_and_test(int i, env_atomic *a) {
    return atomic_sub_and_test(i, a);
}

static inline void env_atomic_inc(env_atomic *a) {
    atomic_inc(a);
}

static inline void env_atomic_dec(env_atomic *a) {
    atomic_dec(a);
}

static inline bool env_atomic_dec_and_test(env_atomic *a) {
    return atomic_dec_and_test(a);
}

static inline bool env_atomic_inc_and_test(env_atomic *a) {
    return atomic_inc_and_test(a);
}

static inline int env_atomic_add_return(int i, env_atomic *a) {
    return atomic_add_return(i, a);
}

static inline int env_atomic_sub_return(int i, env_atomic *a) {
    return atomic_sub_return(i, a);
}

static inline int env_atomic_inc_return(env_atomic *a) {
    return atomic_inc_return(a);
}

static inline int env_atomic_dec_return(env_atomic *a) {
    return atomic_dec_return(a);
}

static inline int env_atomic_cmpxchg(env_atomic *a, int old, int new_value) {
    return atomic_cmpxchg(a, old, new_value);
}

static inline int env_atomic_add_unless(env_atomic *a, int i, int u) {
    return atomic_add_unless(a, i, u);
}

static inline u64 env_atomic64_read(const env_atomic64 *a) {
    return atomic64_read(a);
}

static inline void env_atomic64_set(env_atomic64 *a, u64 i) {
    atomic64_set(a, i);
}

static inline void env_atomic64_add(u64 i, env_atomic64 *a) {
    atomic64_add(i, a);
}

static inline void env_atomic64_sub(u64 i, env_atomic64 *a) {
    atomic64_sub(i, a);
}

static inline void env_atomic64_inc(env_atomic64 *a) {
    atomic64_inc(a);
}

static inline u64 env_atomic64_inc_return(env_atomic64 *a) {
    return atomic64_inc_return(a);
}

static inline void env_atomic64_dec(env_atomic64 *a) {
    atomic64_dec(a);
}

static inline u64 env_atomic64_cmpxchg(atomic64_t *a, u64 old, u64 new) {
    return atomic64_cmpxchg(a, old, new);
}

static inline long env_atomic64_xchg(env_atomic64 *a, long new) {
    return atomic64_xchg(a, new);
}

/* *** LOGGING *** */

#define ENV_PRIu64 "llu"

#define ENV_WARN(cond, fmt...) WARN(cond, fmt)
#define ENV_WARN_ON(cond) WARN_ON(cond)

#define ENV_BUG() BUG()
#define ENV_BUG_ON(cond) BUG_ON(cond)

static inline void *env_malloc(size_t size, int flags) {
    return kmalloc(size, flags);
}

static inline void *env_zalloc(size_t size) {
    return kzalloc(size, GFP_KERNEL);
}

static inline void env_free(const void *ptr) {
    kfree(ptr);
}

/* *** STRINGS *** */
static inline void memcpy_s(void *dest,
                            size_t dmax,
                            const void *src,
                            size_t smax) {
    BUG_ON(smax > dmax);
    memcpy(dest, src, smax);
}

static inline void memset_s(void *dest, size_t len, uint8_t value) {
    memset(dest, value, len);
}

#endif  // SOURCE_KERNEL_INTERNAL_TRACE_ENV_KERNEL_H
