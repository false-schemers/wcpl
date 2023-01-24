/* LLVM's wasm_simd128.h-style macros for basic WASM operators */

#pragma once
#include <stdint.h>

/* int32_t wasm_i32_const(c) __REQUIRE_CONSTANT(c); */
#define wasm_i32_const(c) ((int32_t(*)())asm(i32.const c)())

/* int64_t wasm_i64_const(c) __REQUIRE_CONSTANT(c); */
#define wasm_i64_const(c) ((int64_t(*)())asm(i64.const c)())

/* float32_t wasm_f32_const(c) __REQUIRE_CONSTANT(c); */
#define wasm_f32_const(c) ((float32_t(*)())asm(f32.const c)())

/* float64_t wasm_f64_const(c) __REQUIRE_CONSTANT(c); */
#define wasm_f64_const(c) ((float64_t(*)())asm(f64.const c)())


/* int32_t wasm_i32_load(const void *p); */
#define wasm_i32_load(p) ((int32_t(*)(const void*))asm(i32.load offset=0 align=4)(p))

/* int64_t wasm_i64_load(const void *p); */
#define wasm_i64_load(p) ((int64_t(*)(const void*))asm(i64.load offset=0 align=8)(p))

/* float32_t wasm_f32_load(const void *p); */
#define wasm_f32_load(p) ((float32_t(*)(const void*))asm(f32.load offset=0 align=4)(p))

/* float64_t wasm_f64_load(const void *p); */
#define wasm_f64_load(p) ((float64_t(*)(const void*))asm(f64.load offset=0 align=8)(p))

/* int32_t wasm_i32_load8(const void *p); */
#define wasm_i32_load8(p) ((int32_t(*)(const void*))asm(i32.load8_s offset=0 align=1)(p))

/* uint32_t wasm_u32_load8(const void *p); */
#define wasm_u32_load8(p) ((uint32_t(*)(const void*))asm(i32.load8_u offset=0 align=1)(p))

/* int32_t wasm_i32_load16(const void *p); */
#define wasm_i32_load16(p) ((int32_t(*)(const void*))asm(i32.load16_s offset=0 align=2)(p))

/* uint32_t wasm_u32_load16(const void *p); */
#define wasm_u32_load16(p) ((uint32_t(*)(const void*))asm(i32.load16_u offset=0 align=2)(p))

/* int64_t wasm_i64_load8(const void *p); */
#define wasm_i64_load8(p) ((int64_t(*)(const void*))asm(i64.load8_s offset=0 align=1)(p))

/* uint64_t wasm_u64_load8(const void *p); */
#define wasm_u64_load8(p) ((uint64_t(*)(const void*))asm(i64.load8_u offset=0 align=1)(p))

/* int64_t wasm_i64_load16(const void *p); */
#define wasm_i64_load16(p) ((int64_t(*)(const void*))asm(i64.load16_s offset=0 align=2)(p))

/* uint64_t wasm_u64_load16(const void *p); */
#define wasm_u64_load16(p) ((uint64_t(*)(const void*))asm(i64.load16_u offset=0 align=2)(p))

/* int64_t wasm_i64_load32(const void *p); */
#define wasm_i64_load32(p) ((int64_t(*)(const void*))asm(i64.load32_s offset=0 align=4)(p))

/* uint64_t wasm_u64_load32(const void *p); */
#define wasm_u64_load32(p) ((uint64_t(*)(const void*))asm(i64.load32_u offset=0 align=4)(p))

/* void wasm_i32_store(void *p, int32_t a); */
#define wasm_i32_store(p, a) ((void(*)(void*, int32_t))asm(i32.store offset=0 align=4)(p, a))

/* void wasm_i64_store(void *p, int64_t a); */
#define wasm_i64_store(p, a) ((void(*)(void*, int64_t))asm(i64.store offset=0 align=8)(p, a))

/* void wasm_f32_store(void *p, float32_t a); */
#define wasm_f32_store(p, a) ((void(*)(void*, float32_t))asm(f32.store offset=0 align=4)(p, a))

/* void wasm_f64_store(void *p, float64_t a); */
#define wasm_f64_store(p, a) ((void(*)(void*, float64_t))asm(f64.store offset=0 align=8)(p, a))

/* void wasm_i32_store8(void *p, int32_t a); */
#define wasm_i32_store8(p, a) ((void(*)(void*, int32_t))asm(i32.store8 offset=0 align=1)(p, a))

/* void wasm_i32_store16(void *p, int32_t a); */
#define wasm_i32_store16(p, a) ((void(*)(void*, int32_t))asm(i32.store16 offset=0 align=2)(p, a))

/* void wasm_i64_store8(void *p, int64_t a); */
#define wasm_i64_store8(p, a) ((void(*)(void*, int64_t))asm(i64.store8 offset=0 align=1)(p, a))

/* void wasm_i64_store16(void *p, int64_t a); */
#define wasm_i64_store16(p, a) ((void(*)(void*, int64_t))asm(i64.store16 offset=0 align=2)(p, a))

/* void wasm_i64_store32(void *p, int64_t a); */
#define wasm_i64_store32(p, a) ((void(*)(void*, int64_t))asm(i64.store32 offset=0 align=4)(p, a))


/* int32_t wasm_i32_eqz(int32_t a); */
#define wasm_i32_eqz(a) ((int32_t(*)(int32_t))asm(i32.eqz)(a))

/* int32_t wasm_i32_eq(int32_t a, int32_t b); */
#define wasm_i32_eq(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.eq)(a, b))

/* int32_t wasm_i32_ne(int32_t a, int32_t b); */
#define wasm_i32_ne(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.ne)(a, b))

/* int32_t wasm_i32_lt(int32_t a, int32_t b); */
#define wasm_i32_lt(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.lt_s)(a, b))

/* int32_t wasm_u32_lt(uint32_t a, uint32_t b); */
#define wasm_u32_lt(a, b) ((int32_t(*)(uint32_t, uint32_t))asm(i32.lt_u)(a, b))

/* int32_t wasm_i32_gt(int32_t a, int32_t b); */
#define wasm_i32_gt(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.gt_s)(a, b))

/* int32_t wasm_u32_gt(uint32_t a, uint32_t b); */
#define wasm_u32_gt(a, b) ((int32_t(*)(uint32_t, uint32_t))asm(i32.gt_u)(a, b))

/* int32_t wasm_i32_le(int32_t a, int32_t b); */
#define wasm_i32_le(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.le_s)(a, b))

/* int32_t wasm_u32_le(uint32_t a, uint32_t b); */
#define wasm_u32_le(a, b) ((int32_t(*)(uint32_t, uint32_t))asm(i32.le_u)(a, b))

/* int32_t wasm_i32_ge(int32_t a, int32_t b); */
#define wasm_i32_ge(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.ge_s)(a, b))

/* int32_t wasm_u32_ge(uint32_t a, uint32_t b); */
#define wasm_u32_ge(a, b) ((int32_t(*)(uint32_t, uint32_t))asm(i32.ge_u)(a, b))

/* int32_t wasm_i64_eqz(int64_t a); */
#define wasm_i64_eqz(a) ((int32_t(*)(int64_t))asm(i64.eqz)(a))

/* int32_t wasm_i64_eq(int64_t a, int64_t b); */
#define wasm_i64_eq(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.eq)(a, b))

/* int32_t wasm_i64_ne(int64_t a, int64_t b); */
#define wasm_i64_ne(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.ne)(a, b))

/* int32_t wasm_i64_lt(int64_t a, int64_t b); */
#define wasm_i64_lt(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.lt_s)(a, b))

/* int32_t wasm_u64_lt(uint64_t a, uint64_t b); */
#define wasm_u64_lt(a, b) ((int32_t(*)(uint64_t, uint64_t))asm(i64.lt_u)(a, b))

/* int32_t wasm_i64_gt(int64_t a, int64_t b); */
#define wasm_i64_gt(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.gt_s)(a, b))

/* int32_t wasm_u64_gt(uint64_t a, uint64_t b); */
#define wasm_u64_gt(a, b) ((int32_t(*)(uint64_t, uint64_t))asm(i64.gt_u)(a, b))

/* int32_t wasm_i64_le(int64_t a, int64_t b); */
#define wasm_i64_le(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.le_s)(a, b))

/* int32_t wasm_u64_le(uint64_t a, uint64_t b); */
#define wasm_u64_le(a, b) ((int32_t(*)(uint64_t, uint64_t))asm(i64.le_u)(a, b))

/* int32_t wasm_i64_ge(int64_t a, int64_t b); */
#define wasm_i64_ge(a, b) ((int32_t(*)(int64_t, int64_t))asm(i64.ge_s)(a, b))

/* int32_t wasm_u64_ge(uint64_t a, uint64_t b); */
#define wasm_u64_ge(a, b) ((int32_t(*)(uint64_t, uint64_t))asm(i64.ge_u)(a, b))

/* int32_t wasm_f32_eq(float32_t a, float32_t b); */
#define wasm_f32_eq(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.eq)(a, b))

/* int32_t wasm_f32_ne(float32_t a, float32_t b); */
#define wasm_f32_ne(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.ne)(a, b))

/* int32_t wasm_f32_lt(float32_t a, float32_t b); */
#define wasm_f32_lt(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.lt)(a, b))

/* int32_t wasm_f32_gt(float32_t a, float32_t b); */
#define wasm_f32_gt(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.gt)(a, b))

/* int32_t wasm_f32_le(float32_t a, float32_t b); */
#define wasm_f32_le(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.le)(a, b))

/* int32_t wasm_f32_ge(float32_t a, float32_t b); */
#define wasm_f32_ge(a, b) ((int32_t(*)(float32_t, float32_t))asm(f32.ge)(a, b))

/* int32_t wasm_f64_eq(float64_t a, float64_t b); */
#define wasm_f64_eq(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.eq)(a, b))

/* int32_t wasm_f64_ne(float64_t a, float64_t b); */
#define wasm_f64_ne(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.ne)(a, b))

/* int32_t wasm_f64_lt(float64_t a, float64_t b); */
#define wasm_f64_lt(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.lt)(a, b))

/* int32_t wasm_f64_gt(float64_t a, float64_t b); */
#define wasm_f64_gt(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.gt)(a, b))

/* int32_t wasm_f64_le(float64_t a, float64_t b); */
#define wasm_f64_le(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.le)(a, b))

/* int32_t wasm_f64_ge(float64_t a, float64_t b); */
#define wasm_f64_ge(a, b) ((int32_t(*)(float64_t, float64_t))asm(f64.ge)(a, b))


/* int32_t wasm_i32_clz(int32_t a); */
#define wasm_i32_clz(a) ((int32_t(*)(int32_t))asm(i32.clz)(a))

/* int32_t wasm_i32_ctz(int32_t a); */
#define wasm_i32_ctz(a) ((int32_t(*)(int32_t))asm(i32.ctz)(a))

/* int32_t wasm_i32_popcnt(int32_t a); */
#define wasm_i32_popcnt(a) ((int32_t(*)(int32_t))asm(i32.popcnt)(a))

/* int32_t wasm_i32_add(int32_t a, int32_t b); */
#define wasm_i32_add(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.add)(a, b))

/* int32_t wasm_i32_sub(int32_t a, int32_t b); */
#define wasm_i32_sub(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.sub)(a, b))

/* int32_t wasm_i32_mul(int32_t a, int32_t b); */
#define wasm_i32_mul(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.mul)(a, b))

/* int32_t wasm_i32_div(int32_t a, int32_t b); */
#define wasm_i32_div(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.div_s)(a, b))

/* uint32_t wasm_u32_div(uint32_t a, uint32_t b); */
#define wasm_u32_div(a, b) ((uint32_t(*)(uint32_t, uint32_t))asm(i32.div_u)(a, b))

/* int32_t wasm_i32_rem(int32_t a, int32_t b); */
#define wasm_i32_rem(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.rem_s)(a, b))

/* uint32_t wasm_u32_rem(uint32_t a, uint32_t b); */
#define wasm_u32_rem(a, b) ((uint32_t(*)(uint32_t, uint32_t))asm(i32.rem_u)(a, b))

/* int32_t wasm_i32_and(int32_t a, int32_t b); */
#define wasm_i32_and(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.and)(a, b))

/* int32_t wasm_i32_or(int32_t a, int32_t b); */
#define wasm_i32_or(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.or)(a, b))

/* int32_t wasm_i32_xor(int32_t a, int32_t b); */
#define wasm_i32_xor(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.xor)(a, b))

/* int32_t wasm_i32_shl(int32_t a, int32_t b); */
#define wasm_i32_shl(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.shl)(a, b))

/* int32_t wasm_i32_shr(int32_t a, int32_t b); */
#define wasm_i32_shr(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.shr_s)(a, b))

/* uint32_t wasm_u32_shr(uint32_t a, uint32_t b); */
#define wasm_u32_shr(a, b) ((uint32_t(*)(uint32_t, uint32_t))asm(i32.shr_u)(a, b))

/* int32_t wasm_i32_rotl(int32_t a, int32_t b); */
#define wasm_i32_rotl(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.rotl)(a, b))

/* int32_t wasm_i32_rotr(int32_t a, int32_t b); */
#define wasm_i32_rotr(a, b) ((int32_t(*)(int32_t, int32_t))asm(i32.rotr)(a, b))

/* int64_t wasm_i64_clz(int64_t a); */
#define wasm_i64_clz(a) ((int64_t(*)(int64_t))asm(i64.clz)(a))

/* int64_t wasm_i64_ctz(int64_t a); */
#define wasm_i64_ctz(a) ((int64_t(*)(int64_t))asm(i64.ctz)(a))

/* int64_t wasm_i64_popcnt(int64_t a); */
#define wasm_i64_popcnt(a) ((int64_t(*)(int64_t))asm(i64.popcnt)(a))

/* int64_t wasm_i64_add(int64_t a, int64_t b); */
#define wasm_i64_add(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.add)(a, b))

/* int64_t wasm_i64_sub(int64_t a, int64_t b); */
#define wasm_i64_sub(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.sub)(a, b))

/* int64_t wasm_i64_mul(int64_t a, int64_t b); */
#define wasm_i64_mul(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.mul)(a, b))

/* int64_t wasm_i64_div(int64_t a, int64_t b); */
#define wasm_i64_div(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.div_s)(a, b))

/* uint64_t wasm_u64_div(uint64_t a, uint64_t b); */
#define wasm_u64_div(a, b) ((uint64_t(*)(uint64_t, uint64_t))asm(i64.div_u)(a, b))

/* int64_t wasm_i64_rem(int64_t a, int64_t b); */
#define wasm_i64_rem(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.rem_s)(a, b))

/* uint64_t wasm_u64_rem(uint64_t a, uint64_t b); */
#define wasm_u64_rem(a, b) ((uint64_t(*)(uint64_t, uint64_t))asm(i64.rem_u)(a, b))

/* int64_t wasm_i64_and(int64_t a, int64_t b); */
#define wasm_i64_and(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.and)(a, b))

/* int64_t wasm_i64_or(int64_t a, int64_t b); */
#define wasm_i64_or(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.or)(a, b))

/* int64_t wasm_i64_xor(int64_t a, int64_t b); */
#define wasm_i64_xor(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.xor)(a, b))

/* int64_t wasm_i64_shl(int64_t a, int64_t b); */
#define wasm_i64_shl(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.shl)(a, b))

/* int64_t wasm_i64_shr(int64_t a, int64_t b); */
#define wasm_i64_shr(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.shr_s)(a, b))

/* uint64_t wasm_u64_shr(uint64_t a, uint64_t b); */
#define wasm_u64_shr(a, b) ((uint64_t(*)(uint64_t, uint64_t))asm(i64.shr_u)(a, b))

/* int64_t wasm_i64_rotl(int64_t a, int64_t b); */
#define wasm_i64_rotl(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.rotl)(a, b))

/* int64_t wasm_i64_rotr(int64_t a, int64_t b); */
#define wasm_i64_rotr(a, b) ((int64_t(*)(int64_t, int64_t))asm(i64.rotr)(a, b))

/* float32_t wasm_f32_abs(float32_t a); */
#define wasm_f32_abs(a) ((float32_t(*)(float32_t))asm(f32.abs)(a))

/* float32_t wasm_f32_neg(float32_t a); */
#define wasm_f32_neg(a) ((float32_t(*)(float32_t))asm(f32.neg)(a))

/* float32_t wasm_f32_ceil(float32_t a); */
#define wasm_f32_ceil(a) ((float32_t(*)(float32_t))asm(f32.ceil)(a))

/* float32_t wasm_f32_floor(float32_t a); */
#define wasm_f32_floor(a) ((float32_t(*)(float32_t))asm(f32.floor)(a))

/* float32_t wasm_f32_trunc(float32_t a); */
#define wasm_f32_trunc(a) ((float32_t(*)(float32_t))asm(f32.trunc)(a))

/* float32_t wasm_f32_nearest(float32_t a); */
#define wasm_f32_nearest(a) ((float32_t(*)(float32_t))asm(f32.nearest)(a))

/* float32_t wasm_f32_sqrt(float32_t a); */
#define wasm_f32_sqrt(a) ((float32_t(*)(float32_t))asm(f32.sqrt)(a))

/* float32_t wasm_f32_add(float32_t a, float32_t b); */
#define wasm_f32_add(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.add)(a, b))

/* float32_t wasm_f32_sub(float32_t a, float32_t b); */
#define wasm_f32_sub(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.sub)(a, b))

/* float32_t wasm_f32_mul(float32_t a, float32_t b); */
#define wasm_f32_mul(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.mul)(a, b))

/* float32_t wasm_f32_div(float32_t a, float32_t b); */
#define wasm_f32_div(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.div)(a, b))

/* float32_t wasm_f32_min(float32_t a, float32_t b); */
#define wasm_f32_min(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.min)(a, b))

/* float32_t wasm_f32_max(float32_t a, float32_t b); */
#define wasm_f32_max(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.max)(a, b))

/* float32_t wasm_f32_copysign(float32_t a, float32_t b); */
#define wasm_f32_copysign(a, b) ((float32_t(*)(float32_t, float32_t))asm(f32.copysign)(a, b))

/* float64_t wasm_f64_abs(float64_t a); */
#define wasm_f64_abs(a) ((float64_t(*)(float64_t))asm(f64.abs)(a))

/* float64_t wasm_f64_neg(float64_t a); */
#define wasm_f64_neg(a) ((float64_t(*)(float64_t))asm(f64.neg)(a))

/* float64_t wasm_f64_ceil(float64_t a); */
#define wasm_f64_ceil(a) ((float64_t(*)(float64_t))asm(f64.ceil)(a))

/* float64_t wasm_f64_floor(float64_t a); */
#define wasm_f64_floor(a) ((float64_t(*)(float64_t))asm(f64.floor)(a))

/* float64_t wasm_f64_trunc(float64_t a); */
#define wasm_f64_trunc(a) ((float64_t(*)(float64_t))asm(f64.trunc)(a))

/* float64_t wasm_f64_nearest(float64_t a); */
#define wasm_f64_nearest(a) ((float64_t(*)(float64_t))asm(f64.nearest)(a))

/* float64_t wasm_f64_sqrt(float64_t a); */
#define wasm_f64_sqrt(a) ((float64_t(*)(float64_t))asm(f64.sqrt)(a))

/* float64_t wasm_f64_add(float64_t a, float64_t b); */
#define wasm_f64_add(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.add)(a, b))

/* float64_t wasm_f64_sub(float64_t a, float64_t b); */
#define wasm_f64_sub(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.sub)(a, b))

/* float64_t wasm_f64_mul(float64_t a, float64_t b); */
#define wasm_f64_mul(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.mul)(a, b))

/* float64_t wasm_f64_div(float64_t a, float64_t b); */
#define wasm_f64_div(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.div)(a, b))

/* float64_t wasm_f64_min(float64_t a, float64_t b); */
#define wasm_f64_min(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.min)(a, b))

/* float64_t wasm_f64_max(float64_t a, float64_t b); */
#define wasm_f64_max(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.max)(a, b))

/* float64_t wasm_f64_copysign(float64_t a, float64_t b); */
#define wasm_f64_copysign(a, b) ((float64_t(*)(float64_t, float64_t))asm(f64.copysign)(a, b))

/* int32_t wasm_i32_wrap_i64(int64_t a); */
#define wasm_i32_wrap_i64(a) ((int32_t(*)(int64_t))asm(i32.wrap_i64)(a))

/* int32_t wasm_i32_trunc_f32(float32_t a); */
#define wasm_i32_trunc_f32(a) ((int32_t(*)(float32_t))asm(i32.trunc_f32_s)(a))

/* uint32_t wasm_u32_trunc_f32(float32_t a); */
#define wasm_u32_trunc_f32(a) ((uint32_t(*)(float32_t))asm(i32.trunc_f32_u)(a))

/* int32_t wasm_i32_trunc_f64(float64_t a); */
#define wasm_i32_trunc_f64(a) ((int32_t(*)(float64_t))asm(i32.trunc_f64_s)(a))

/* uint32_t wasm_u32_trunc_f64(float64_t a); */
#define wasm_u32_trunc_f64(a) ((uint32_t(*)(float64_t))asm(i32.trunc_f64_u)(a))

/* int64_t wasm_i64_extend_i32(int32_t a); */
#define wasm_i64_extend_i32(a) ((int64_t(*)(int32_t))asm(i64.extend_i32_s)(a))

/* uint64_t wasm_u64_extend_u32(uint32_t a); */
#define wasm_u64_extend_u32(a) ((uint64_t(*)(uint32_t))asm(i64.extend_i32_u)(a))

/* int64_t wasm_i64_trunc_f32(float32_t a); */
#define wasm_i64_trunc_f32(a) ((int64_t(*)(float32_t))asm(i64.trunc_f32_s)(a))

/* uint64_t wasm_u64_trunc_f32(float32_t a); */
#define wasm_u64_trunc_f32(a) ((uint64_t(*)(float32_t))asm(i64.trunc_f32_u)(a))

/* int64_t wasm_i64_trunc_f64(float64_t a); */
#define wasm_i64_trunc_f64(a) ((int64_t(*)(float64_t))asm(i64.trunc_f64_s)(a))

/* uint64_t wasm_u64_trunc_f64(float64_t a); */
#define wasm_u64_trunc_f64(a) ((uint64_t(*)(float64_t))asm(i64.trunc_f64_u)(a))

/* float32_t wasm_f32_convert_i32(int32_t a); */
#define wasm_f32_convert_i32(a) ((float32_t(*)(int32_t))asm(f32.convert_i32_s)(a))

/* float32_t wasm_f32_convert_u32(uint32_t a); */
#define wasm_f32_convert_u32(a) ((float32_t(*)(uint32_t))asm(f32.convert_i32_u)(a))

/* float32_t wasm_f32_convert_i64(int64_t a); */
#define wasm_f32_convert_i64(a) ((float32_t(*)(int64_t))asm(f32.convert_i64_s)(a))

/* float32_t wasm_f32_convert_u64(uint64_t a); */
#define wasm_f32_convert_u64(a) ((float32_t(*)(uint64_t))asm(f32.convert_i64_u)(a))

/* float32_t wasm_f32_demote_f64(float64_t a); */
#define wasm_f32_demote_f64(a) ((float32_t(*)(float64_t))asm(f32.demote_f64)(a))

/* float64_t wasm_f64_convert_i32(int32_t a); */
#define wasm_f64_convert_i32(a) ((float64_t(*)(int32_t))asm(f64.convert_i32_s)(a))

/* float64_t wasm_f64_convert_u32(uint32_t a); */
#define wasm_f64_convert_u32(a) ((float64_t(*)(uint32_t))asm(f64.convert_i32_u)(a))

/* float64_t wasm_f64_convert_i64(int64_t a); */
#define wasm_f64_convert_i64(a) ((float64_t(*)(int64_t))asm(f64.convert_i64_s)(a))

/* float64_t wasm_f64_convert_u64(uint64_t a); */
#define wasm_f64_convert_u64(a) ((float64_t(*)(uint64_t))asm(f64.convert_i64_u)(a))

/* float64_t wasm_f64_promote_f32(float32_t a); */
#define wasm_f64_promote_f32(a) ((float64_t(*)(float32_t))asm(f64.promote_f32)(a))

/* int32_t wasm_i32_reinterpret_f32(float32_t a); */
#define wasm_i32_reinterpret_f32(a) ((int32_t(*)(float32_t))asm(i32.reinterpret_f32)(a))

/* int64_t wasm_i64_reinterpret_f64(float64_t a); */
#define wasm_i64_reinterpret_f64(a) ((int64_t(*)(float64_t))asm(i64.reinterpret_f64)(a))

/* float32_t wasm_f32_reinterpret_i32(int32_t a); */
#define wasm_f32_reinterpret_i32(a) ((float32_t(*)(int32_t))asm(f32.reinterpret_i32)(a))

/* float64_t wasm_f64_reinterpret_i64(int64_t a); */
#define wasm_f64_reinterpret_i64(a) ((float64_t(*)(int64_t))asm(f64.reinterpret_i64)(a))

/* int32_t wasm_i32_extend8(int32_t a); */
#define wasm_i32_extend8(a) ((int32_t(*)(int32_t))asm(i32.extend8_s)(a))

/* int32_t wasm_i32_extend16(int32_t a); */
#define wasm_i32_extend16(a) ((int32_t(*)(int32_t))asm(i32.extend16_s)(a))

/* int64_t wasm_i64_extend8(int64_t a); */
#define wasm_i64_extend8(a) ((int64_t(*)(int64_t))asm(i64.extend8_s)(a))

/* int64_t wasm_i64_extend16(int64_t a); */
#define wasm_i64_extend16(a) ((int64_t(*)(int64_t))asm(i64.extend16_s)(a))

/* int64_t wasm_i64_extend32(int64_t a); */
#define wasm_i64_extend32(a) ((int64_t(*)(int64_t))asm(i64.extend32_s)(a))

/* int32_t wasm_i32_trunc_sat_f32(float32_t a); */
#define wasm_i32_trunc_sat_f32(a) ((int32_t(*)(float32_t))asm(i32.trunc_sat_f32_s)(a))

/* uint32_t wasm_u32_trunc_sat_f32(float32_t a); */
#define wasm_u32_trunc_sat_f32(a) ((uint32_t(*)(float32_t))asm(i32.trunc_sat_f32_u)(a))

/* int32_t wasm_i32_trunc_sat_f64(float64_t a); */
#define wasm_i32_trunc_sat_f64(a) ((int32_t(*)(float64_t))asm(i32.trunc_sat_f64_s)(a))

/* uint32_t wasm_u32_trunc_sat_f64(float64_t a); */
#define wasm_u32_trunc_sat_f64(a) ((uint32_t(*)(float64_t))asm(i32.trunc_sat_f64_u)(a))

/* int64_t wasm_i64_trunc_sat_f32(float32_t a); */
#define wasm_i64_trunc_sat_f32(a) ((int64_t(*)(float32_t))asm(i64.trunc_sat_f32_s)(a))

/* uint64_t wasm_u64_trunc_sat_f32(float32_t a); */
#define wasm_u64_trunc_sat_f32(a) ((uint64_t(*)(float32_t))asm(i64.trunc_sat_f32_u)(a))

/* int64_t wasm_i64_trunc_sat_f64(float64_t a); */
#define wasm_i64_trunc_sat_f64(a) ((int64_t(*)(float64_t))asm(i64.trunc_sat_f64_s)(a))

/* uint64_t wasm_u64_trunc_sat_f64(float64_t a); */
#define wasm_u64_trunc_sat_f64(a) ((uint64_t(*)(float64_t))asm(i64.trunc_sat_f64_u)(a))


/* int32_t wasm_memory_size(); */
#define wasm_memory_size() ((int32_t(*)())asm(memory.size)())

/* int32_t wasm_memory_grow(int32_t a); */
#define wasm_memory_grow(a) ((int32_t(*)(int32_t))asm(memory.grow)(a))

/* void wasm_memory_fill(void *d, int32_t v, int32_t n); */
#define wasm_memory_fill(d, v, n) ((void(*)(void*, int32_t, int32_t))asm(memory.fill)(d, v, n))

/* void wasm_memory_copy(void *d, void *s, int32_t n); */
#define wasm_memory_copy(d, s, n) ((void(*)(void*, void*, int32_t))asm(memory.copy)(d, s, n))
