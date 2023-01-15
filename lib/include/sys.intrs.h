/* webassembly intrinsics */

#pragma once

/* extra integer operations */
#define __builtin_clz(x) ((int(*)(unsigned))asm(i32.clz)(x))
#define __builtin_ctz(x) ((int(*)(unsigned))asm(i32.ctz)(x))
#define __builtin_popcount(x) ((int(*)(unsigned))asm(i32.popcnt)(x))
#define __builtin_rotleft(x, n) ((int(*)(unsigned, unsigned))asm(i32.rotl)(x, n))
#define __builtin_rotright(x, n) ((int(*)(unsigned, unsigned))asm(i32.rotr)(x, n))
#define __builtin_clzll(x) ((long long(*)(unsigned long long))asm(i64.clz)(x))
#define __builtin_ctzll(x) ((long long(*)(unsigned long long))asm(i64.ctz)(x))
#define __builtin_popcountll(x) ((long long(*)(unsigned long long))asm(i64.popcnt)(x))
#define __builtin_rotleftll(x, n) ((long long(*)(unsigned long long, unsigned long long))asm(i64.rotl)(x, n))
#define __builtin_rotrightll(x, n) ((long long(*)(unsigned long long, unsigned long long))asm(i64.rotr)(x, n))

/* reinterpret casts */
#define __builtin_asuint32(x) ((unsigned(*)(float))asm(i32.reinterpret_f32)(x))
#define __builtin_asfloat(x) ((float(*)(unsigned))asm(f32.reinterpret_i32)(x))
#define __builtin_asuint64(x) ((unsigned long long(*)(double))asm(i64.reinterpret_f64)(x))
#define __builtin_asdouble(x) ((double(*)(unsigned long long))asm(f64.reinterpret_i64)(x))

/* memory operations (return no value) */
#define __builtin_memset(d, v, n) ((void(*)(void*, unsigned, size_t))asm(memory.fill)(d, v, n))
#define __builtin_memcpy(d, s, n) ((void(*)(void*, void*, size_t))asm(memory.copy)(d, s, n)) /* checks overlap */
#define __builtin_memmove(d, s, n) ((void(*)(void*, void*, size_t))asm(memory.copy)(d, s, n)) /* ditto */

/* short aliases, generic versions */
#define _clz(x) (generic((x), \
    unsigned long long: __builtin_clzll(x), \
             long long: __builtin_clzll(x), \
               default: __builtin_clz(x)))
#define _ctz(x) (generic((x), \
    unsigned long long: __builtin_ctzll(x), \
             long long: __builtin_ctzll(x), \
               default: __builtin_ctz(x)))
#define _popcnt(x) (generic((x), \
    unsigned long long: __builtin_popcountll(x), \
             long long: __builtin_popcountll(x), \
               default: __builtin_popcount(x)))
#define _rotl(x, n) (generic((x), \
    unsigned long long: __builtin_rotleftll(x, n), \
             long long: __builtin_rotleftll(x, n), \
               default: __builtin_rotleft(x, n)))
#define _rotr(x, n) (generic((x), \
    unsigned long long: __builtin_rotrightll(x, n), \
             long long: __builtin_rotrightll(x, n), \
               default: __builtin_rotright(x, n)))
#define _fasu(x) (generic((x), \
                 float: __builtin_asuint32(x)))
#define _dasull(x) (generic((x), \
                double: __builtin_asuint64(x)))
#define _uasf(x) (generic((x), \
              unsigned: __builtin_asfloat(x)))
#define _ullasd(x) (generic((x), \
    unsigned long long: __builtin_asdouble(x)))
#define _bfill(d, v, n) (__builtin_memset(d, v, n))
#define _bzero(d, n)    (__builtin_memset(d, 0, n))
#define _bcopy(d, s, n) (__builtin_memmove(d, s, n))
