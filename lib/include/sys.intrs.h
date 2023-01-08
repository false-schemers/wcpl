/* webassembly intrinsics */

#pragma once

/* extra integer operations */
#define __builtin_clz32(x) ((unsigned(*)(unsigned))asm(i32.clz)(x))
#define __builtin_ctz32(x) ((unsigned(*)(unsigned))asm(i32.ctz)(x))
#define __builtin_popcnt32(x) ((unsigned(*)(unsigned))asm(i32.popcnt)(x))
#define __builtin_rotl32(x, n) ((unsigned(*)(unsigned, unsigned))asm(i32.rotl)(x, n))
#define __builtin_rotr32(x, n) ((unsigned(*)(unsigned, unsigned))asm(i32.rotr)(x, n))
#define __builtin_clz64(x) ((unsigned long long(*)(unsigned long long))asm(i64.clz)(x))
#define __builtin_ctz64(x) ((unsigned long long(*)(unsigned long long))asm(i64.ctz)(x))
#define __builtin_popcnt64(x) ((unsigned long long(*)(unsigned long long))asm(i64.popcnt)(x))
#define __builtin_rotl64(x, n) ((unsigned long long(*)(unsigned long long, unsigned long long))asm(i64.rotl)(x, n))
#define __builtin_rotr64(x, n) ((unsigned long long(*)(unsigned long long, unsigned long long))asm(i64.rotr)(x, n))

/* reinterpret casts */
#define __builtin_asuint32(x) ((unsigned(*)(float))asm(i32.reinterpret_f32)(x))
#define __builtin_asfloat(x) ((float(*)(unsigned))asm(f32.reinterpret_i32)(x))
#define __builtin_asuint64(x) ((unsigned long long(*)(double))asm(i64.reinterpret_f64)(x))
#define __builtin_asdouble(x) ((double(*)(unsigned long long))asm(f64.reinterpret_i64)(x))

/* memory operations (return no value) */
#define __builtin_memset(d, v, n) ((void(*)(void*,unsigned,size_t))asm(memory.fill)(d, v, n))
#define __builtin_bzero(d, n) ((void(*)(void*,unsigned,size_t))asm(memory.fill)(d, 0, n))
#define __builtin_memcpy(d, s, n) ((void(*)(void*,void*,size_t))asm(memory.copy)(d, s, n))
#define __builtin_memmove(d, s, n) ((void(*)(void*,void*,size_t))asm(memory.copy)(d, s, n))


