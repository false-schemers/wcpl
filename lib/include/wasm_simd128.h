/* LLVM-style WASM SIMD128 */

#pragma once

/* v128_t is built-in */
#include <stdint.h>

/* v128_t wasm_v128_load(const void *p); */
#define wasm_v128_load(p) ((v128_t(*)(const void*))asm(v128.load offset=0 align=16)(p))

/* v128_t wasm_v128_load8_splat(const void *p); */
#define wasm_v128_load8_splat(p) ((v128_t(*)(const void*))asm(v128.load8_splat offset=0 align=1)(p))

/* v128_t wasm_v128_load16_splat(const void *p); */
#define wasm_v128_load16_splat(p) ((v128_t(*)(const void*))asm(v128.load16_splat offset=0 align=2)(p))

/* v128_t wasm_v128_load32_splat(const void *p); */
#define wasm_v128_load32_splat(p) ((v128_t(*)(const void*))asm(v128.load32_splat offset=0 align=4)(p))

/* v128_t wasm_v128_load64_splat(const void *p); */
#define wasm_v128_load64_splat(p) ((v128_t(*)(const void*))asm(v128.load64_splat offset=0 align=8)(p))

/* v128_t wasm_i16x8_load8x8(const void *p); */
#define wasm_i16x8_load8x8(p) ((v128_t(*)(const void*))asm(v128.load8x8_s offset=0 align=1)(p))

/* v128_t wasm_u16x8_load8x8(const void *p); */
#define wasm_u16x8_load8x8(p) ((v128_t(*)(const void*))asm(v128.load8x8_u offset=0 align=1)(p))

/* v128_t wasm_i32x4_load16x4(const void *p); */
#define wasm_i32x4_load16x4(p) ((v128_t(*)(const void*))asm(v128.load16x4_s offset=0 align=2)(p))

/* v128_t wasm_u32x4_load16x4(const void *p); */
#define wasm_u32x4_load16x4(p) ((v128_t(*)(const void*))asm(v128.load16x4_u offset=0 align=2)(p))

/* v128_t wasm_i64x2_load32x2(const void *p); */
#define wasm_i64x2_load32x2(p) ((v128_t(*)(const void*))asm(v128.load32x2_s offset=0 align=4)(p))

/* v128_t wasm_u64x2_load32x2(const void *p); */
#define wasm_u64x2_load32x2(p) ((v128_t(*)(const void*))asm(v128.load32x2_u offset=0 align=4)(p))

/* v128_t wasm_v128_load32_zero(const void *p); -- loads lane 0 only? */
#define wasm_v128_load32_zero(p) ((v128_t(*)(int32_t))asm(v128.load32_zero offset=0 align=4 0)(p))

/* v128_t wasm_v128_load64_zero(const void *p); -- loads lane 0 only? */
#define wasm_v128_load64_zero(p) ((v128_t(*)(int32_t))asm(v128.load64_zero offset=0 align=8 0)(p))

/* v128_t wasm_v128_load8_lane(const void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_load8_lane(p, v, i) ((v128_t(*)(const void*, v128_t))asm(v128.load8_lane offset=0 align=1 i)(p, v))

/* v128_t wasm_v128_load16_lane(const void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_load16_lane(p, v, i) ((v128_t(*)(const void*, v128_t))asm(v128.load16_lane offset=0 align=2 i)(p, v))

/* v128_t wasm_v128_load32_lane(const void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_load32_lane(p, v, i) ((v128_t(*)(const void*, v128_t))asm(v128.load32_lane offset=0 align=4 i)(p, v))

/* v128_t wasm_v128_load64_lane(const void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_load64_lane(p, v, i) ((v128_t(*)(const void*, v128_t))asm(v128.load64_lane offset=0 align=8 i)(p, v))

/* void wasm_v128_store(void *p, v128_t a); */
#define wasm_v128_store(p, a) ((void(*)(void*, v128_t))asm(v128.store offset=0 align=16)(p, a))

/* void wasm_v128_store8_lane(void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_store8_lane(p, v, i) ((v128_t(*)(void*, v128_t))asm(v128.store8_lane offset=0 align=1 i)(p, v))

/* void wasm_v128_store16_lane(void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_store16_lane(p, v, i) ((v128_t(*)(void*, v128_t))asm(v128.store16_lane offset=0 align=2 i)(p, v))

/* void wasm_v128_store32_lane(void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_store32_lane(p, v, i) ((v128_t(*)(void*, v128_t))asm(v128.store32_lane offset=0 align=4 i)(p, v))

/* void wasm_v128_store64_lane(void *p, v128_t v, int i) __REQUIRE_CONSTANT(i); */
#define wasm_v128_store64_lane(p, v, i) ((v128_t(*)(void*, v128_t))asm(v128.store64_lane offset=0 align=8 i)(p, v))


/* v128_t wasm_i8x16_const(int8_t c0, ..., int8_t c15) __REQUIRE_CONSTANT(c0, ..., c15); */
#define wasm_i8x16_const(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  ((simd<int8_t, 16>){c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15})

/* v128_t wasm_u8x16_const(uint8_t c0, ..., uint8_t c15) __REQUIRE_CONSTANT(c0, ..., c15); */
#define wasm_u8x16_const(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  ((simd<uint8_t, 16>){c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15})

/* v128_t wasm_i16x8_const(int16_t c0, ..., int16_t c7) __REQUIRE_CONSTANT(c0, ..., c7); */
#define wasm_i16x8_const(c0, c1, c2, c3, c4, c5, c6, c7) \
  ((simd<int16_t, 8>){c0, c1, c2, c3, c4, c5, c6, c7})

/* v128_t wasm_u16x8_const(uint16_t c0, ..., uint16_t c7) __REQUIRE_CONSTANT(c0, ..., c7); */
#define wasm_u16x8_const(c0, c1, c2, c3, c4, c5, c6, c7) \
  ((simd<uint16_t, 8>){c0, c1, c2, c3, c4, c5, c6, c7})

/* v128_t wasm_i32x4_const(int32_t c0, int32_t c1, int32_t c2, int32_t c3) __REQUIRE_CONSTANT(c0, ..., c3); */
#define wasm_i32x4_const(c0, c1, c2, c3) \
  ((simd<int32_t, 4>){c0, c1, c2, c3})

/* v128_t wasm_u32x4_const(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) __REQUIRE_CONSTANT(c0, ..., c3); */
#define wasm_u32x4_const(c0, c1, c2, c3) \
  ((simd<uint32_t, 4>){c0, c1, c2, c3})

/* v128_t wasm_i64x2_const(int64_t c0, int64_t c1) __REQUIRE_CONSTANT(c0, c1); */
#define wasm_i64x2_const(c0, c1) \
  ((simd<int64_t, 2>){c0, c1})

/* v128_t wasm_u64x2_const(uint64_t c0, uint64_t c1) __REQUIRE_CONSTANT(c0, c1); */
#define wasm_u64x2_const(c0, c1) \
  ((simd<uint64_t, 2>){c0, c1})

/* v128_t wasm_f32x4_const(float32_t c0, float32_t c1, float32_t c2, float32_t c3) __REQUIRE_CONSTANT(c0, ..., c3); */
#define wasm_f32x4_const(c0, c1, c2, c3) \
  ((simd<float32_t, 4>){c0, c1, c2, c3})

/* v128_t wasm_f64x2_const(float64_t c0, float64_t c1) __REQUIRE_CONSTANT(c0) __REQUIRE_CONSTANT(c1); */
#define wasm_f64x2_const(c0, c1) \
  ((simd<float64_t, 2>){c0, c1})


/* v128_t wasm_i8x16_const_splat(int8_t c) __REQUIRE_CONSTANT(c); */
#define wasm_i8x16_const_splat(c) (wasm_i8x16_const(c, c, c, c, c, c, c, c, c, c, c, c, c, c, c, c))

/* v128_t wasm_u8x16_const_splat(uint8_t c) __REQUIRE_CONSTANT(c); */
#define wasm_u8x16_const_splat(c) (wasm_u8x16_const(c, c, c, c, c, c, c, c, c, c, c, c, c, c, c, c))

/* v128_t wasm_i16x8_const_splat(int16_t c) __REQUIRE_CONSTANT(c); */
#define wasm_i16x8_const_splat(c) (wasm_i16x8_const(c, c, c, c, c, c, c, c))

/* v128_t wasm_u16x8_const_splat(uint16_t c) __REQUIRE_CONSTANT(c); */
#define wasm_u16x8_const_splat(c) (wasm_u16x8_const(c, c, c, c, c, c, c, c))

/* v128_t wasm_i32x4_const_splat(int32_t c) __REQUIRE_CONSTANT(c); */
#define wasm_i32x4_const_splat(c) (wasm_i32x4_const(c, c, c, c))

/* v128_t wasm_u32x4_const_splat(uint32_t c) __REQUIRE_CONSTANT(c); */
#define wasm_u32x4_const_splat(c) (wasm_u32x4_const(c, c, c, c))

/* v128_t wasm_i64x2_const_splat(int64_t c) __REQUIRE_CONSTANT(c); */
#define wasm_i64x2_const_splat(c) (wasm_i64x2_const(c, c))

/* v128_t wasm_u64x2_const_splat(uint64_t c) __REQUIRE_CONSTANT(c); */
#define wasm_u64x2_const_splat(c) (wasm_u64x2_const(c, c))

/* v128_t wasm_f32x4_const_splat(float32_t c) __REQUIRE_CONSTANT(c); */
#define wasm_f32x4_const_splat(c) (wasm_f32x4_const(c, c, c, c))

/* v128_t wasm_f64x2_const_splat(float64_t c) __REQUIRE_CONSTANT(c); */
#define wasm_f64x2_const_splat(c) (wasm_f64x2_const(c, c))

/* v128_t wasm_i8x16_splat(int8_t a); */
#define wasm_i8x16_splat(a) ((v128_t(*)(int8_t))asm(i8x16.splat)(a))

/* v128_t wasm_u8x16_splat(uint8_t a); */
#define wasm_u8x16_splat(a) ((v128_t(*)(uint8_t))asm(i8x16.splat)(a))

/* int8_t wasm_i8x16_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_i8x16_extract_lane(a, i) ((int8_t(*)(v128_t))asm(i8x16.extract_lane_s i)(a))

/* uint8_t wasm_u8x16_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_u8x16_extract_lane(a, i) ((uint8_t(*)(v128_t))asm(i8x16.extract_lane_u i)(a))

/* v128_t wasm_i8x16_replace_lane(v128_t a, int i, int8_t b) __REQUIRE_CONSTANT(i); */
#define wasm_i8x16_replace_lane(a, i, b) ((v128_t(*)(v128_t, int8_t))asm(i8x16.replace_lane i)(a, b))

/* v128_t wasm_u8x16_replace_lane(v128_t a, int i, uint8_t b) __REQUIRE_CONSTANT(i); */
#define wasm_u8x16_replace_lane(a, i, b) ((v128_t(*)(v128_t, uint8_t))asm(i8x16.replace_lane i)(a, b))

/* v128_t wasm_i16x8_splat(int16_t a); */
#define wasm_i16x8_splat(a) ((v128_t(*)(int16_t))asm(i16x8.splat)(a))

/* v128_t wasm_u16x8_splat(uint16_t a); */
#define wasm_u16x8_splat(a) ((v128_t(*)(uint16_t))asm(i16x8.splat)(a))

/* int16_t wasm_i16x8_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_i16x8_extract_lane(a, i) ((int16_t(*)(v128_t))asm(i16x8.extract_lane_s i)(a))

/* uint16_t wasm_u16x8_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_u16x8_extract_lane(a, i) ((uint16_t(*)(v128_t))asm(i16x8.extract_lane_u i)(a))

/* v128_t wasm_i16x8_replace_lane(v128_t a, int i, int16_t b) __REQUIRE_CONSTANT(i); */
#define wasm_i16x8_replace_lane(a, i, b) ((v128_t(*)(v128_t, int16_t))asm(i16x8.replace_lane i)(a, b))

/* v128_t wasm_u16x8_replace_lane(v128_t a, int i, uint16_t b) __REQUIRE_CONSTANT(i); */
#define wasm_u16x8_replace_lane(a, i, b) ((v128_t(*)(v128_t, uint16_t))asm(i16x8.replace_lane i)(a, b))

/* v128_t wasm_i32x4_splat(int32_t a); */
#define wasm_i32x4_splat(a) ((v128_t(*)(int32_t))asm(i32x4.splat)(a))

/* v128_t wasm_u32x4_splat(uint32_t a); */
#define wasm_u32x4_splat(a) ((v128_t(*)(uint32_t))asm(i32x4.splat)(a))

/* int32_t wasm_i32x4_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_i32x4_extract_lane(a, i) ((int32_t(*)(v128_t))asm(i32x4.extract_lane i)(a))

/* uint32_t wasm_u32x4_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_u32x4_extract_lane(a, i) ((uint32_t(*)(v128_t))asm(i32x4.extract_lane i)(a))

/* v128_t wasm_i32x4_replace_lane(v128_t a, int i, int32_t b) __REQUIRE_CONSTANT(i); */
#define wasm_i32x4_replace_lane(a, i, b) ((v128_t(*)(v128_t, int32_t))asm(i32x4.replace_lane i)(a, b))

/* v128_t wasm_u32x4_replace_lane(v128_t a, int i, uint32_t b) __REQUIRE_CONSTANT(i); */
#define wasm_u32x4_replace_lane(a, i, b) ((v128_t(*)(v128_t, uint32_t))asm(i32x4.replace_lane i)(a, b))

/* v128_t wasm_i64x2_splat(int64_t a); */
#define wasm_i64x2_splat(a) ((v128_t(*)(int64_t))asm(i64x2.splat)(a))

/* v128_t wasm_u64x2_splat(uint64_t a); */
#define wasm_u64x2_splat(a) ((v128_t(*)(uint64_t))asm(i64x2.splat)(a))

/* int64_t wasm_i64x2_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_i64x2_extract_lane(a, i) ((int64_t(*)(v128_t))asm(i64x2.extract_lane i)(a))

/* uint64_t wasm_u64x2_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_u64x2_extract_lane(a, i) ((uint64_t(*)(v128_t))asm(i64x2.extract_lane i)(a))

/* v128_t wasm_i64x2_replace_lane(v128_t a, int i, int64_t b) __REQUIRE_CONSTANT(i); */
#define wasm_i64x2_replace_lane(a, i, b) ((v128_t(*)(v128_t, int64_t))asm(i64x2.replace_lane i)(a, b))

/* v128_t wasm_u64x2_replace_lane(v128_t a, int i, uint64_t b) __REQUIRE_CONSTANT(i); */
#define wasm_u64x2_replace_lane(a, i, b) ((v128_t(*)(v128_t, uint64_t))asm(i64x2.replace_lane i)(a, b))

/* v128_t wasm_f32x4_splat(float32_t a); */
#define wasm_f32x4_splat(a) ((v128_t(*)(float32_t))asm(f32x4.splat)(a))

/* float32_t wasm_f32x4_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_f32x4_extract_lane(a, i) ((float32_t(*)(v128_t))asm(f32x4.extract_lane i)(a))

/* v128_t wasm_f32x4_replace_lane(v128_t a, int i, float32_t b) __REQUIRE_CONSTANT(i); */
#define wasm_f32x4_replace_lane(a, i, b) ((v128_t(*)(v128_t, float32_t))asm(f32x4.replace_lane i)(a, b))

/* v128_t wasm_f64x2_splat(float64_t a); */
#define wasm_f64x2_splat(a) ((v128_t(*)(float64_t))asm(f64x2.splat)(a))

/* float64_t wasm_f64x2_extract_lane(v128_t a, int i) __REQUIRE_CONSTANT(i); */
#define wasm_f64x2_extract_lane(a, i) ((float64_t(*)(v128_t))asm(f64x2.extract_lane i)(a))

/* v128_t wasm_f64x2_replace_lane(v128_t a, int i, float64_t b) __REQUIRE_CONSTANT(i); */
#define wasm_f64x2_replace_lane(a, i, b) ((v128_t(*)(v128_t, float64_t))asm(f64x2.replace_lane i)(a, b))


/* v128_t wasm_i8x16_make(int8_t c0, ..., int8_t c15); */
#define wasm_i8x16_make(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane \
  (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane \
  (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane \
  (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane (wasm_i8x16_replace_lane \
  (wasm_i8x16_splat(c15), 14, c14), 13, c13), 12, c12), 11, c11), 10, c10), 9, c9), \
    8, c8), 7, c7), 6, c6), 5, c5), 4, c4), 3, c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_u8x16_make(uint8_t c0, ..., uint8_t c15); */
#define wasm_u8x16_make(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane \
  (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane \
  (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane \
  (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane (wasm_u8x16_replace_lane \
  (wasm_u8x16_splat(c15), 14, c14), 13, c13), 12, c12), 11, c11), 10, c10), 9, c9), \
    8, c8), 7, c7), 6, c6), 5, c5), 4, c4), 3, c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_i16x8_make(int16_t c0, ..., int16_t c7); */
#define wasm_i16x8_make(c0, c1, c2, c3, c4, c5, c6, c7) \
  (wasm_i16x8_replace_lane (wasm_i16x8_replace_lane (wasm_i16x8_replace_lane (wasm_i16x8_replace_lane \
  (wasm_i16x8_replace_lane (wasm_i16x8_replace_lane (wasm_i16x8_replace_lane \
  (wasm_i16x8_splat(c7), 6, c6), 5, c5), 4, c4), 3, c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_u16x8_make(uint16_t c0, ..., uint16_t c7); */
#define wasm_u16x8_make(c0, c1, c2, c3, c4, c5, c6, c7) \
  (wasm_u16x8_replace_lane (wasm_u16x8_replace_lane (wasm_u16x8_replace_lane (wasm_u16x8_replace_lane \
  (wasm_u16x8_replace_lane (wasm_u16x8_replace_lane (wasm_u16x8_replace_lane \
  (wasm_u16x8_splat(c7), 6, c6), 5, c5), 4, c4), 3, c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_i32x4_make(int32_t c0, int32_t c1, int32_t c2, int32_t c3); */
#define wasm_i32x4_make(c0, c1, c2, c3) \
  (wasm_i32x4_replace_lane(wasm_i32x4_replace_lane(wasm_i32x4_replace_lane \
  (wasm_i32x4_splat(c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_u32x4_make(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3); */
#define wasm_u32x4_make(c0, c1, c2, c3) \
  (wasm_u32x4_replace_lane(wasm_u32x4_replace_lane(wasm_u32x4_replace_lane \
  (wasm_u32x4_splat(c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_i64x2_make(int64_t c0, int64_t c1); */
#define wasm_i64x2_make(c0, c1) \
  (wasm_i64x2_replace_lane(wasm_i64x2_splat(c1), 0, c0))

/* v128_t wasm_u64x2_make(uint64_t c0, uint64_t c1); */
#define wasm_u64x2_make(c0, c1) \
  (wasm_u64x2_replace_lane(wasm_u64x2_splat(c1), 0, c0))

/* v128_t wasm_f32x4_make(float32_t c0, float32_t c1, float32_t c2, float32_t c3); */
#define wasm_f32x4_make(c0, c1, c2, c3) \
  (wasm_f32x4_replace_lane(wasm_f32x4_replace_lane(wasm_f32x4_replace_lane \
  (wasm_f32x4_splat(c3), 2, c2), 1, c1), 0, c0))

/* v128_t wasm_f64x2_make(float64_t c0, float64_t c1); */
#define wasm_f64x2_make(c0, c1) \
  (wasm_f64x2_replace_lane(wasm_f64x2_splat(c1), 0, c0))


/* v128_t wasm_i8x16_eq(v128_t a, v128_t b); */
#define wasm_i8x16_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.eq)(a, b))

/* v128_t wasm_i8x16_ne(v128_t a, v128_t b); */
#define wasm_i8x16_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.ne)(a, b))

/* v128_t wasm_i8x16_lt(v128_t a, v128_t b); */
#define wasm_i8x16_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.lt_s)(a, b))

/* v128_t wasm_u8x16_lt(v128_t a, v128_t b); */
#define wasm_u8x16_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.lt_u)(a, b))

/* v128_t wasm_i8x16_gt(v128_t a, v128_t b); */
#define wasm_i8x16_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.gt_s)(a, b))

/* v128_t wasm_u8x16_gt(v128_t a, v128_t b); */
#define wasm_u8x16_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.gt_u)(a, b))

/* v128_t wasm_i8x16_le(v128_t a, v128_t b); */
#define wasm_i8x16_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.le_s)(a, b))

/* v128_t wasm_u8x16_le(v128_t a, v128_t b); */
#define wasm_u8x16_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.le_u)(a, b))

/* v128_t wasm_i8x16_ge(v128_t a, v128_t b); */
#define wasm_i8x16_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.ge_s)(a, b))

/* v128_t wasm_u8x16_ge(v128_t a, v128_t b); */
#define wasm_u8x16_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.ge_u)(a, b))


/* v128_t wasm_i16x8_eq(v128_t a, v128_t b); */
#define wasm_i16x8_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.eq)(a, b))

/* v128_t wasm_i16x8_ne(v128_t a, v128_t b); */
#define wasm_i16x8_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.ne)(a, b))

/* v128_t wasm_i16x8_lt(v128_t a, v128_t b); */
#define wasm_i16x8_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.lt_s)(a, b))

/* v128_t wasm_u16x8_lt(v128_t a, v128_t b); */
#define wasm_u16x8_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.lt_u)(a, b))

/* v128_t wasm_i16x8_gt(v128_t a, v128_t b); */
#define wasm_i16x8_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.gt_s)(a, b))

/* v128_t wasm_u16x8_gt(v128_t a, v128_t b); */
#define wasm_u16x8_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.gt_u)(a, b))

/* v128_t wasm_i16x8_le(v128_t a, v128_t b); */
#define wasm_i16x8_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.le_s)(a, b))

/* v128_t wasm_u16x8_le(v128_t a, v128_t b); */
#define wasm_u16x8_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.le_u)(a, b))

/* v128_t wasm_i16x8_ge(v128_t a, v128_t b); */
#define wasm_i16x8_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.ge_s)(a, b))

/* v128_t wasm_u16x8_ge(v128_t a, v128_t b); */
#define wasm_u16x8_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.ge_u)(a, b))


/* v128_t wasm_i32x4_eq(v128_t a, v128_t b); */
#define wasm_i32x4_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.eq)(a, b))

/* v128_t wasm_i32x4_ne(v128_t a, v128_t b); */
#define wasm_i32x4_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.ne)(a, b))

/* v128_t wasm_i32x4_lt(v128_t a, v128_t b); */
#define wasm_i32x4_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.lt_s)(a, b))

/* v128_t wasm_u32x4_lt(v128_t a, v128_t b); */
#define wasm_u32x4_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.lt_u)(a, b))

/* v128_t wasm_i32x4_gt(v128_t a, v128_t b); */
#define wasm_i32x4_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.gt_s)(a, b))

/* v128_t wasm_u32x4_gt(v128_t a, v128_t b); */
#define wasm_u32x4_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.gt_u)(a, b))

/* v128_t wasm_i32x4_le(v128_t a, v128_t b); */
#define wasm_i32x4_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.le_s)(a, b))

/* v128_t wasm_u32x4_le(v128_t a, v128_t b); */
#define wasm_u32x4_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.le_u)(a, b))

/* v128_t wasm_i32x4_ge(v128_t a, v128_t b); */
#define wasm_i32x4_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.ge_s)(a, b))

/* v128_t wasm_u32x4_ge(v128_t a, v128_t b); */
#define wasm_u32x4_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.ge_u)(a, b))


/* v128_t wasm_i64x2_eq(v128_t a, v128_t b); */
#define wasm_i64x2_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.eq)(a, b))

/* v128_t wasm_i64x2_ne(v128_t a, v128_t b); */
#define wasm_i64x2_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.ne)(a, b))

/* v128_t wasm_i64x2_lt(v128_t a, v128_t b); */
#define wasm_i64x2_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.lt_s)(a, b))

/* v128_t wasm_i64x2_gt(v128_t a, v128_t b); */
#define wasm_i64x2_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.gt_s)(a, b))

/* v128_t wasm_i64x2_le(v128_t a, v128_t b); */
#define wasm_i64x2_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.le_s)(a, b))

/* v128_t wasm_i64x2_ge(v128_t a, v128_t b); */
#define wasm_i64x2_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.ge_s)(a, b))


/* v128_t wasm_f32x4_eq(v128_t a, v128_t b); */
#define wasm_f32x4_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.eq)(a, b))

/* v128_t wasm_f32x4_ne(v128_t a, v128_t b); */
#define wasm_f32x4_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.ne)(a, b))

/* v128_t wasm_f32x4_lt(v128_t a, v128_t b); */
#define wasm_f32x4_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.lt)(a, b))

/* v128_t wasm_f32x4_gt(v128_t a, v128_t b); */
#define wasm_f32x4_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.gt)(a, b))

/* v128_t wasm_f32x4_le(v128_t a, v128_t b); */
#define wasm_f32x4_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.le)(a, b))

/* v128_t wasm_f32x4_ge(v128_t a, v128_t b); */
#define wasm_f32x4_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.ge)(a, b))


/* v128_t wasm_f64x2_eq(v128_t a, v128_t b); */
#define wasm_f64x2_eq(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.eq)(a, b))

/* v128_t wasm_f64x2_ne(v128_t a, v128_t b); */
#define wasm_f64x2_ne(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.ne)(a, b))

/* v128_t wasm_f64x2_lt(v128_t a, v128_t b); */
#define wasm_f64x2_lt(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.lt)(a, b))

/* v128_t wasm_f64x2_gt(v128_t a, v128_t b); */
#define wasm_f64x2_gt(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.gt)(a, b))

/* v128_t wasm_f64x2_le(v128_t a, v128_t b); */
#define wasm_f64x2_le(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.le)(a, b))

/* v128_t wasm_f64x2_ge(v128_t a, v128_t b); */
#define wasm_f64x2_ge(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.ge)(a, b))


/* v128_t wasm_v128_not(v128_t a); */
#define wasm_v128_not(a) ((v128_t(*)(v128_t))asm(v128.not)(a))

/* v128_t wasm_v128_and(v128_t a, v128_t b); */
#define wasm_v128_and(a, b) ((v128_t(*)(v128_t, v128_t))asm(v128.and)(a, b))

/* v128_t wasm_v128_or(v128_t a, v128_t b); */
#define wasm_v128_or(a, b) ((v128_t(*)(v128_t, v128_t))asm(v128.or)(a, b))

/* v128_t wasm_v128_xor(v128_t a, v128_t b); */
#define wasm_v128_xor(a, b) ((v128_t(*)(v128_t, v128_t))asm(v128.xor)(a, b))

/* v128_t wasm_v128_andnot(v128_t a, v128_t b); */
#define wasm_v128_andnot(a, b) ((v128_t(*)(v128_t, v128_t))asm(v128.andnot)(a, b))

/* bool wasm_v128_any_true(v128_t a); */
#define wasm_v128_any_true(a) ((bool(*)(v128_t))asm(v128.any_true)(a))

/* v128_t wasm_v128_bitselect(v128_t a, v128_t b, v128_t mask); */
#define wasm_v128_bitselect(a, b, mask) ((v128_t(*)(v128_t, v128_t, v128_t))asm(v128.bitselect)(a, b, mask))


/* v128_t wasm_i8x16_abs(v128_t a); */
#define wasm_i8x16_abs(a) ((v128_t(*)(v128_t))asm(i8x16.abs)(a))

/* v128_t wasm_i8x16_neg(v128_t a); */
#define wasm_i8x16_neg(a) ((v128_t(*)(v128_t))asm(i8x16.neg)(a))

/* bool wasm_i8x16_all_true(v128_t a); */
#define wasm_i8x16_all_true(a) ((bool(*)(v128_t))asm(i8x16.all_true)(a))

/* uint32_t wasm_i8x16_bitmask(v128_t a); */
#define wasm_i8x16_bitmask(a) ((uint32_t(*)(v128_t))asm(i8x16.bitmask)(a))

/* v128_t wasm_i8x16_popcnt(v128_t a); */
#define wasm_i8x16_popcnt(a) ((v128_t(*)(v128_t))asm(i8x16.popcnt)(a))

/* v128_t wasm_i8x16_shl(v128_t a, uint32_t b); */
#define wasm_i8x16_shl(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i8x16.shl)(x, v))

/* v128_t wasm_i8x16_shr(v128_t a, uint32_t b); */
#define wasm_i8x16_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i8x16.shr_s)(x, v))

/* v128_t wasm_u8x16_shr(v128_t a, uint32_t b); */
#define wasm_u8x16_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i8x16.shr_u)(x, v))

/* v128_t wasm_i8x16_add(v128_t a, v128_t b); */
#define wasm_i8x16_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.add)(a, b))

/* v128_t wasm_i8x16_add_sat(v128_t a, v128_t b); */
#define wasm_i8x16_add_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.add_sat_s)(a, b))

/* v128_t wasm_u8x16_add_sat(v128_t a, v128_t b); */
#define wasm_u8x16_add_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.add_sat_u)(a, b))

/* v128_t wasm_i8x16_sub(v128_t a, v128_t b); */
#define wasm_i8x16_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.sub)(a, b))

/* v128_t wasm_i8x16_sub_sat(v128_t a, v128_t b); */
#define wasm_i8x16_sub_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.sub_sat_s)(a, b))

/* v128_t wasm_u8x16_sub_sat(v128_t a, v128_t b); */
#define wasm_u8x16_sub_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.sub_sat_u)(a, b))

/* v128_t wasm_i8x16_min(v128_t a, v128_t b); */
#define wasm_i8x16_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.min_s)(a, b))

/* v128_t wasm_u8x16_min(v128_t a, v128_t b); */
#define wasm_u8x16_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.min_u)(a, b))

/* v128_t wasm_i8x16_max(v128_t a, v128_t b); */
#define wasm_i8x16_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.max_s)(a, b))

/* v128_t wasm_u8x16_max(v128_t a, v128_t b); */
#define wasm_u8x16_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.max_u)(a, b))

/* v128_t wasm_u8x16_avgr(v128_t a, v128_t b); */
#define wasm_u8x16_avgr(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.avgr_u)(a, b))


/* v128_t wasm_i16x8_abs(v128_t a); */
#define wasm_i16x8_abs(a) ((v128_t(*)(v128_t))asm(i16x8.abs)(a))

/* v128_t wasm_i16x8_neg(v128_t a); */
#define wasm_i16x8_neg(a) ((v128_t(*)(v128_t))asm(i16x8.neg)(a))

/* bool wasm_i16x8_all_true(v128_t a); */
#define wasm_i16x8_all_true(a) ((bool(*)(v128_t))asm(i16x8.all_true)(a))

/* uint32_t wasm_i16x8_bitmask(v128_t a); */
#define wasm_i16x8_bitmask(a) ((uint32_t(*)(v128_t))asm(i16x8.bitmask)(a))

/* v128_t wasm_i16x8_shl(v128_t a, uint32_t b); */
#define wasm_i16x8_shl(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i16x8.shl)(x, v))

/* v128_t wasm_i16x8_shr(v128_t a, uint32_t b); */
#define wasm_i16x8_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i16x8.shr_s)(x, v))

/* v128_t wasm_u16x8_shr(v128_t a, uint32_t b); */
#define wasm_u16x8_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i16x8.shr_u)(x, v))

/* v128_t wasm_i16x8_add(v128_t a, v128_t b); */
#define wasm_i16x8_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.add)(a, b))

/* v128_t wasm_i16x8_add_sat(v128_t a, v128_t b); */
#define wasm_i16x8_add_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.add_sat_s)(a, b))

/* v128_t wasm_u16x8_add_sat(v128_t a, v128_t b); */
#define wasm_u16x8_add_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.add_sat_u)(a, b))

/* v128_t wasm_i16x8_sub(v128_t a, v128_t b); */
#define wasm_i16x8_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.sub)(a, b))

/* v128_t wasm_i16x8_sub_sat(v128_t a, v128_t b); */
#define wasm_i16x8_sub_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.sub_sat_s)(a, b))

/* v128_t wasm_u16x8_sub_sat(v128_t a, v128_t b); */
#define wasm_u16x8_sub_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.sub)(a, b))

/* v128_t wasm_i16x8_mul(v128_t a, v128_t b); */
#define wasm_i16x8_mul(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.mul)(a, b))

/* v128_t wasm_i16x8_min(v128_t a, v128_t b); */
#define wasm_i16x8_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.min_s)(a, b))

/* v128_t wasm_u16x8_min(v128_t a, v128_t b); */
#define wasm_u16x8_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.min_u)(a, b))

/* v128_t wasm_i16x8_max(v128_t a, v128_t b); */
#define wasm_i16x8_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.max_s)(a, b))

/* v128_t wasm_u16x8_max(v128_t a, v128_t b); */
#define wasm_u16x8_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.max_u)(a, b))

/* v128_t wasm_u16x8_avgr(v128_t a, v128_t b); */
#define wasm_u16x8_avgr(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.avgr_u)(a, b))


/* v128_t wasm_i32x4_abs(v128_t a); */
#define wasm_i32x4_abs(a) ((v128_t(*)(v128_t))asm(i32x4.abs)(a))

/* v128_t wasm_i32x4_neg(v128_t a); */
#define wasm_i32x4_neg(a) ((v128_t(*)(v128_t))asm(i32x4.neg)(a))

/* bool wasm_i32x4_all_true(v128_t a); */
#define wasm_i32x4_all_true(a) ((bool(*)(v128_t))asm(i32x4.all_true)(a))

/* uint32_t wasm_i32x4_bitmask(v128_t a); */
#define wasm_i32x4_bitmask(a) ((uint32_t(*)(v128_t))asm(i32x4.bitmask)(a))

/* v128_t wasm_i32x4_shl(v128_t a, uint32_t b); */
#define wasm_i32x4_shl(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i32x4.shl)(x, v))

/* v128_t wasm_i32x4_shr(v128_t a, uint32_t b); */
#define wasm_i32x4_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i32x4.shr_s)(x, v))

/* v128_t wasm_u32x4_shr(v128_t a, uint32_t b); */
#define wasm_u32x4_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i32x4.shr_u)(x, v))

/* v128_t wasm_i32x4_add(v128_t a, v128_t b); */
#define wasm_i32x4_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.add)(a, b))

/* v128_t wasm_i32x4_sub(v128_t a, v128_t b); */
#define wasm_i32x4_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.sub)(a, b))

/* v128_t wasm_i32x4_mul(v128_t a, v128_t b); */
#define wasm_i32x4_mul(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.mul)(a, b))

/* v128_t wasm_i32x4_min(v128_t a, v128_t b); */
#define wasm_i32x4_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.min_s)(a, b))

/* v128_t wasm_u32x4_min(v128_t a, v128_t b); */
#define wasm_u32x4_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.min_u)(a, b))

/* v128_t wasm_i32x4_max(v128_t a, v128_t b); */
#define wasm_i32x4_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.max_s)(a, b))

/* v128_t wasm_u32x4_max(v128_t a, v128_t b); */
#define wasm_u32x4_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.max_u)(a, b))

/* v128_t wasm_i32x4_dot_i16x8(v128_t a, v128_t b); */
#define wasm_i32x4_dot_i16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.dot_i16x8_s)(a, b))


/* v128_t wasm_i64x2_abs(v128_t a); */
#define wasm_i64x2_abs(a) ((v128_t(*)(v128_t))asm(i64x2.abs)(a))

/* v128_t wasm_i64x2_neg(v128_t a); */
#define wasm_i64x2_neg(a) ((v128_t(*)(v128_t))asm(i64x2.neg)(a))

/* bool wasm_i64x2_all_true(v128_t a); */
#define wasm_i64x2_all_true(a) ((bool(*)(v128_t))asm(i64x2.all_true)(a))

/* uint32_t wasm_i64x2_bitmask(v128_t a); */
#define wasm_i64x2_bitmask(a) ((uint32_t(*)(v128_t))asm(i64x2.bitmask)(a))

/* v128_t wasm_i64x2_shl(v128_t a, uint32_t b); */
#define wasm_i64x2_shl(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i64x2.shl)(x, v))

/* v128_t wasm_i64x2_shr(v128_t a, uint32_t b); */
#define wasm_i64x2_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i64x2.shr_s)(x, v))

/* v128_t wasm_u64x2_shr(v128_t a, uint32_t b); */
#define wasm_u64x2_shr(x, v) ((v128_t(*)(v128_t, uint32_t))asm(i64x2.shr_u)(x, v))

/* v128_t wasm_i64x2_add(v128_t a, v128_t b); */
#define wasm_i64x2_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.add)(a, b))

/* v128_t wasm_i64x2_sub(v128_t a, v128_t b); */
#define wasm_i64x2_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.sub)(a, b))

/* v128_t wasm_i64x2_mul(v128_t a, v128_t b); */
#define wasm_i64x2_mul(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.mul)(a, b))


/* v128_t wasm_f32x4_abs(v128_t a); */
#define wasm_f32x4_abs(a) ((v128_t(*)(v128_t))asm(f32x4.abs)(a))

/* v128_t wasm_f32x4_neg(v128_t a); */
#define wasm_f32x4_neg(a) ((v128_t(*)(v128_t))asm(f32x4.neg)(a))

/* v128_t wasm_f32x4_sqrt(v128_t a); */
#define wasm_f32x4_sqrt(a) ((v128_t(*)(v128_t))asm(f32x4.sqrt)(a))

/* v128_t wasm_f32x4_ceil(v128_t a); */
#define wasm_f32x4_ceil(a) ((v128_t(*)(v128_t))asm(f32x4.ceil)(a))

/* v128_t wasm_f32x4_floor(v128_t a); */
#define wasm_f32x4_floor(a) ((v128_t(*)(v128_t))asm(f32x4.floor)(a))

/* v128_t wasm_f32x4_trunc(v128_t a); */
#define wasm_f32x4_trunc(a) ((v128_t(*)(v128_t))asm(f32x4.trunc)(a))

/* v128_t wasm_f32x4_nearest(v128_t a); */
#define wasm_f32x4_nearest(a) ((v128_t(*)(v128_t))asm(f32x4.nearest)(a))

/* v128_t wasm_f32x4_add(v128_t a, v128_t b); */
#define wasm_f32x4_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.add)(a, b))

/* v128_t wasm_f32x4_sub(v128_t a, v128_t b); */
#define wasm_f32x4_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.sub)(a, b))

/* v128_t wasm_f32x4_mul(v128_t a, v128_t b); */
#define wasm_f32x4_mul(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.mul)(a, b))

/* v128_t wasm_f32x4_div(v128_t a, v128_t b); */
#define wasm_f32x4_div(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.div)(a, b))

/* v128_t wasm_f32x4_min(v128_t a, v128_t b); */
#define wasm_f32x4_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.min)(a, b))

/* v128_t wasm_f32x4_max(v128_t a, v128_t b); */
#define wasm_f32x4_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.max)(a, b))

/* v128_t wasm_f32x4_pmin(v128_t a, v128_t b); */
#define wasm_f32x4_pmin(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.pmin)(a, b))

/* v128_t wasm_f32x4_pmax(v128_t a, v128_t b); */
#define wasm_f32x4_pmax(a, b) ((v128_t(*)(v128_t, v128_t))asm(f32x4.pmax)(a, b))


/* v128_t wasm_f64x2_abs(v128_t a); */
#define wasm_f64x2_abs(a) ((v128_t(*)(v128_t))asm(f64x2.abs)(a))

/* v128_t wasm_f64x2_neg(v128_t a); */
#define wasm_f64x2_neg(a) ((v128_t(*)(v128_t))asm(f64x2.neg)(a))

/* v128_t wasm_f64x2_sqrt(v128_t a); */
#define wasm_f64x2_sqrt(a) ((v128_t(*)(v128_t))asm(f64x2.sqrt)(a))

/* v128_t wasm_f64x2_ceil(v128_t a); */
#define wasm_f64x2_ceil(a) ((v128_t(*)(v128_t))asm(f64x2.ceil)(a))

/* v128_t wasm_f64x2_floor(v128_t a); */
#define wasm_f64x2_floor(a) ((v128_t(*)(v128_t))asm(f64x2.floor)(a))

/* v128_t wasm_f64x2_trunc(v128_t a); */
#define wasm_f64x2_trunc(a) ((v128_t(*)(v128_t))asm(f64x2.trunc)(a))

/* v128_t wasm_f64x2_nearest(v128_t a); */
#define wasm_f64x2_nearest(a) ((v128_t(*)(v128_t))asm(f64x2.nearest)(a))

/* v128_t wasm_f64x2_add(v128_t a, v128_t b); */
#define wasm_f64x2_add(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.add)(a, b))

/* v128_t wasm_f64x2_sub(v128_t a, v128_t b); */
#define wasm_f64x2_sub(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.sub)(a, b))

/* v128_t wasm_f64x2_mul(v128_t a, v128_t b); */
#define wasm_f64x2_mul(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.mul)(a, b))

/* v128_t wasm_f64x2_div(v128_t a, v128_t b); */
#define wasm_f64x2_div(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.div)(a, b))

/* v128_t wasm_f64x2_min(v128_t a, v128_t b); */
#define wasm_f64x2_min(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.min)(a, b))

/* v128_t wasm_f64x2_max(v128_t a, v128_t b); */
#define wasm_f64x2_max(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.max)(a, b))

/* v128_t wasm_f64x2_pmin(v128_t a, v128_t b); */
#define wasm_f64x2_pmin(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.pmin)(a, b))

/* v128_t wasm_f64x2_pmax(v128_t a, v128_t b); */
#define wasm_f64x2_pmax(a, b) ((v128_t(*)(v128_t, v128_t))asm(f64x2.pmax)(a, b))


/* v128_t wasm_i32x4_trunc_sat_f32x4(v128_t a); */
#define wasm_i32x4_trunc_sat_f32x4(a) ((v128_t(*)(v128_t))asm(i32x4.trunc_sat_f32x4_s)(a))

/* v128_t wasm_u32x4_trunc_sat_f32x4(v128_t a); */
#define wasm_u32x4_trunc_sat_f32x4(a) ((v128_t(*)(v128_t))asm(i32x4.trunc_sat_f32x4_u)(a))

/* v128_t wasm_f32x4_convert_i32x4(v128_t a); */
#define wasm_f32x4_convert_i32x4(a) ((v128_t(*)(v128_t))asm(f32x4.convert_i32x4_s)(a))

/* v128_t wasm_f32x4_convert_u32x4(v128_t a); */
#define wasm_f32x4_convert_u32x4(a) ((v128_t(*)(v128_t))asm(f32x4.convert_i32x4_u)(a))

/* v128_t wasm_f64x2_convert_low_i32x4(v128_t a); */
#define wasm_f64x2_convert_low_i32x4(a) ((v128_t(*)(v128_t))asm(f64x2.convert_low_i32x4_s)(a))

/* v128_t wasm_f64x2_convert_low_u32x4(v128_t a); */
#define wasm_f64x2_convert_low_u32x4(a) ((v128_t(*)(v128_t))asm(f64x2.convert_low_i32x4_u)(a))

/* v128_t wasm_i32x4_trunc_sat_f64x2_zero(v128_t a); */
#define wasm_i32x4_trunc_sat_f64x2_zero(a) ((v128_t(*)(v128_t))asm(i32x4.trunc_sat_f64x2_s_zero)(a))

/* v128_t wasm_u32x4_trunc_sat_f64x2_zero(v128_t a); */
#define wasm_u32x4_trunc_sat_f64x2_zero(a) ((v128_t(*)(v128_t))asm(i32x4.trunc_sat_f64x2_u_zero)(a))

/* v128_t wasm_f32x4_demote_f64x2_zero(v128_t a); */
#define wasm_f32x4_demote_f64x2_zero(a) ((v128_t(*)(v128_t))asm(f32x4.demote_f64x2_zero)(a))

/* v128_t wasm_f64x2_promote_low_f32x4(v128_t a); */
#define wasm_f64x2_promote_low_f32x4(a) ((v128_t(*)(v128_t))asm(f64x2.promote_low_f32x4)(a))

/* kluge: __wasm_i8x16_shuffle defined below */
#define _wasm_i8x16_shuffle(a, b, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  (__wasm_i8x16_shuffle(a, b,
    (((c0  & 0xfULL) << 60) | ((c1 &  0xfULL) << 56) | ((c2  & 0xfULL) << 52) | ((c3  & 0xfULL) << 48) | \
     ((c4  & 0xfULL) << 44) | ((c5 &  0xfULL) << 40) | ((c6  & 0xfULL) << 36) | ((c7  & 0xfULL) << 32) | \
     ((c8  & 0xfULL) << 28) | ((c9 &  0xfULL) << 24) | ((c10 & 0xfULL) << 20) | ((c11 & 0xfULL) << 16) | \
     ((c12 & 0xfULL) << 12) | ((c13 & 0xfULL) <<  8) | ((c14 & 0xfULL) <<  4) | ((c15 & 0xfULL)))))

/* v128_t wasm_i8x16_shuffle(v128_t a, v128_t b, uint8_t c0, ..., uint8_t c15); __REQUIRE_CONSTANT(c0, ..., c15); */
#define wasm_i8x16_shuffle(a, b, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
  (_wasm_i8x16_shuffle(a, b, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15))

/* v128_t wasm_i16x8_shuffle(v128_t a, v128_t b, uint8_t c0, ..., uint8_t c7); __REQUIRE_CONSTANT(c0, ..., c7); */
#define wasm_i16x8_shuffle(a, b, c0, c1, c2, c3, c4, c5, c6,  c7) \
  (_wasm_i8x16_shuffle(a, b, (c0)*2, (c0)*2 + 1, (c1)*2, \
    (c1)*2 + 1, (c2)*2, (c2)*2 + 1, (c3)*2, (c3)*2 + 1, (c4)*2, \
    (c4)*2 + 1, (c5)*2, (c5)*2 + 1, (c6)*2, (c6)*2 + 1, (c7)*2, \
    (c7)*2 + 1))

/* v128_t wasm_i32x4_shuffle(v128_t a, v128_t b, uint8_t c0, ..., uint8_t c3); __REQUIRE_CONSTANT(c0, ..., c3); */
#define wasm_i32x4_shuffle(a, b, c0, c1, c2, c3) \
  (_wasm_i8x16_shuffle(a, b, (c0)*4, (c0)*4 + 1, (c0)*4 + 2, \
    (c0)*4 + 3, (c1)*4, (c1)*4 + 1, (c1)*4 + 2, (c1)*4 + 3, \
    (c2)*4, (c2)*4 + 1, (c2)*4 + 2, (c2)*4 + 3, (c3)*4, \
    (c3)*4 + 1, (c3)*4 + 2, (c3)*4 + 3))

/* v128_t wasm_i64x2_shuffle(v128_t a, v128_t b, uint8_t c0, uint8_t c1); __REQUIRE_CONSTANT(c0, c1); */
#define wasm_i64x2_shuffle(a, b, c0, c1) \
  (_wasm_i8x16_shuffle(a, b, (c0)*8, (c0)*8 + 1, (c0)*8 + 2, \
    (c0)*8 + 3, (c0)*8 + 4, (c0)*8 + 5, (c0)*8 + 6, (c0)*8 + 7, \
    (c1)*8, (c1)*8 + 1, (c1)*8 + 2, (c1)*8 + 3, (c1)*8 + 4, \
    (c1)*8 + 5, (c1)*8 + 6, (c1)*8 + 7))

#define __wasm_i8x16_shuffle(a, b, s) ((v128_t(*)(v128_t, v128_t))asm(i8x16.shuffle s)(a, b))


/* v128_t wasm_i8x16_swizzle(v128_t a, v128_t b); */
#define wasm_i8x16_swizzle(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.swizzle)(a, b))


/* v128_t wasm_i8x16_narrow_i16x8(v128_t a, v128_t b); */
#define wasm_i8x16_narrow_i16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.narrow_i16x8_s)(a, b))

/* v128_t wasm_u8x16_narrow_i16x8(v128_t a, v128_t b); */
#define wasm_u8x16_narrow_i16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i8x16.narrow_i16x8_u)(a, b))

/* v128_t wasm_i16x8_narrow_i32x4(v128_t a, v128_t b); */
#define wasm_i16x8_narrow_i32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.narrow_i32x4_s)(a, b))

/* v128_t wasm_u16x8_narrow_i32x4(v128_t a, v128_t b); */
#define wasm_u16x8_narrow_i32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.narrow_i32x4_u)(a, b))

/* v128_t wasm_i16x8_extend_low_i8x16(v128_t a); */
#define wasm_i16x8_extend_low_i8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extend_low_i8x16_s)(a))

/* v128_t wasm_i16x8_extend_high_i8x16(v128_t a); */
#define wasm_i16x8_extend_high_i8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extend_high_i8x16_s)(a))

/* v128_t wasm_u16x8_extend_low_u8x16(v128_t a); */
#define wasm_u16x8_extend_low_u8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extend_low_i8x16_u)(a))

/* v128_t wasm_u16x8_extend_high_u8x16(v128_t a); */
#define wasm_u16x8_extend_high_u8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extend_high_i8x16_u)(a))

/* v128_t wasm_i32x4_extend_low_i16x8(v128_t a); */
#define wasm_i32x4_extend_low_i16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extend_low_i16x8_s)(a))

/* v128_t wasm_i32x4_extend_high_i16x8(v128_t a); */
#define wasm_i32x4_extend_high_i16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extend_high_i16x8_s)(a))

/* v128_t wasm_u32x4_extend_low_u16x8(v128_t a); */
#define wasm_u32x4_extend_low_u16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extend_low_i16x8_u)(a))

/* v128_t wasm_u32x4_extend_high_u16x8(v128_t a); */
#define wasm_u32x4_extend_high_u16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extend_high_i16x8_u)(a))

/* v128_t wasm_i64x2_extend_low_i32x4(v128_t a); */
#define wasm_i64x2_extend_low_i32x4(a) ((v128_t(*)(v128_t))asm(i64x2.extend_low_i32x4_s)(a))


/* v128_t wasm_i64x2_extend_high_i32x4(v128_t a); */
#define wasm_i64x2_extend_high_i32x4(a) ((v128_t(*)(v128_t))asm(i64x2.extend_high_i32x4_s)(a))

/* v128_t wasm_u64x2_extend_low_u32x4(v128_t a); */
#define wasm_u64x2_extend_low_u32x4(a) ((v128_t(*)(v128_t))asm(i64x2.extend_low_i32x4_u)(a))

/* v128_t wasm_u64x2_extend_high_u32x4(v128_t a); */
#define wasm_u64x2_extend_high_u32x4(a) ((v128_t(*)(v128_t))asm(i64x2.extend_high_i32x4_u)(a))

/* v128_t wasm_i16x8_extadd_pairwise_i8x16(v128_t a); */
#define wasm_i16x8_extadd_pairwise_i8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extadd_pairwise_i8x16_s)(a))

/* v128_t wasm_u16x8_extadd_pairwise_u8x16(v128_t a); */
#define wasm_u16x8_extadd_pairwise_u8x16(a) ((v128_t(*)(v128_t))asm(i16x8.extadd_pairwise_i8x16_u)(a))

/* v128_t wasm_i32x4_extadd_pairwise_i16x8(v128_t a); */
#define wasm_i32x4_extadd_pairwise_i16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extadd_pairwise_i16x8_s)(a))

/* v128_t wasm_u32x4_extadd_pairwise_u16x8(v128_t a); */
#define wasm_u32x4_extadd_pairwise_u16x8(a) ((v128_t(*)(v128_t))asm(i32x4.extadd_pairwise_i16x8_u)(a))

/* v128_t wasm_i16x8_extmul_low_i8x16(v128_t a, v128_t b); */
#define wasm_i16x8_extmul_low_i8x16(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.extmul_low_i8x16_s)(a, b))

/* v128_t wasm_i16x8_extmul_high_i8x16(v128_t a, v128_t b); */
#define wasm_i16x8_extmul_high_i8x16(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.extmul_high_i8x16_s)(a, b))

/* v128_t wasm_u16x8_extmul_low_u8x16(v128_t a, v128_t b); */
#define wasm_u16x8_extmul_low_u8x16(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.extmul_low_i8x16_u)(a, b))

/* v128_t wasm_u16x8_extmul_high_u8x16(v128_t a, v128_t b); */
#define wasm_u16x8_extmul_high_u8x16(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.extmul_high_i8x16_u)(a, b))

/* v128_t wasm_i32x4_extmul_low_i16x8(v128_t a, v128_t b); */
#define wasm_i32x4_extmul_low_i16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.extmul_low_i16x8_s)(a, b))

/* v128_t wasm_i32x4_extmul_high_i16x8(v128_t a, v128_t b); */
#define wasm_i32x4_extmul_high_i16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.extmul_high_i16x8_s)(a, b))

/* v128_t wasm_u32x4_extmul_low_u16x8(v128_t a, v128_t b); */
#define wasm_u32x4_extmul_low_u16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.extmul_low_i16x8_u)(a, b))

/* v128_t wasm_u32x4_extmul_high_u16x8(v128_t a, v128_t b); */
#define wasm_u32x4_extmul_high_u16x8(a, b) ((v128_t(*)(v128_t, v128_t))asm(i32x4.extmul_high_i16x8_u)(a, b))

/* v128_t wasm_i64x2_extmul_low_i32x4(v128_t a, v128_t b); */
#define wasm_i64x2_extmul_low_i32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.extmul_low_i32x4_s)(a, b))

/* v128_t wasm_i64x2_extmul_high_i32x4(v128_t a, v128_t b); */
#define wasm_i64x2_extmul_high_i32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.extmul_high_i32x4_s)(a, b))

/* v128_t wasm_u64x2_extmul_low_u32x4(v128_t a, v128_t b); */
#define wasm_u64x2_extmul_low_u32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.extmul_low_i32x4_u)(a, b))

/* v128_t wasm_u64x2_extmul_high_u32x4(v128_t a, v128_t b); */
#define wasm_u64x2_extmul_high_u32x4(a, b) ((v128_t(*)(v128_t, v128_t))asm(i64x2.extmul_high_i32x4_u)(a, b))

/* v128_t wasm_i16x8_q15mulr_sat(v128_t a, v128_t b); */
#define wasm_i16x8_q15mulr_sat(a, b) ((v128_t(*)(v128_t, v128_t))asm(i16x8.q15mulr_sat_s)(a, b))
