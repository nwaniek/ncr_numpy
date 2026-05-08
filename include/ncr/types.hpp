/*
 * types.hpp - basic types used in ncr
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE for more details
 */
#ifndef _909f868e37c64952a3871f2f678d0778_
#define _909f868e37c64952a3871f2f678d0778_

#ifndef NCR_TYPES
#define NCR_TYPES
#endif

#include <cstdint>
#include <span>

//@ncr-fusor-keep-includes-start
#ifndef NCR_TYPES_DISABLE_COMPLEX
	#include <complex>
#endif

#if __cplusplus >= 202302L
	#include <stdfloat>
#endif
#ifndef NCR_TYPES_DISABLE_VECTORS
	#include <vector>
	#ifdef NCR_TYPES_ENABLE_STD_RANGES
		#include <ranges>
	#endif
#endif
//@ncr-fusor-keep-includes-end

using i8           = std::int8_t;
using i16          = std::int16_t;
using i32          = std::int32_t;
using i64          = std::int64_t;
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
using i128         = __int128_t;
#else
using i128         = long long int;
#endif

using u8           = std::uint8_t;
using u16          = std::uint16_t;
using u32          = std::uint32_t;
using u64          = std::uint64_t;
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
using u128         = __uint128_t;
#else
using u128         = unsigned long long int;
#endif

#if __cplusplus >= 202302L
	using f16      = std::float16_t;
	using f32      = std::float32_t;
	using f64      = std::float64_t;
	using f128     = std::float128_t;
#else
	using f16      = _Float16;
	using f32      = float;
	using f64      = double;
	using f128     = __float128; // TODO: include test if this is available, and
								 // if not emit at least a warning
#endif

#ifndef NCR_REAL_TYPE
using real_t = float;
#else
using real_t = NCR_REAL_TYPE;
#endif

#ifndef NCR_TYPES_DISABLE_COMPLEX
#define NCR_TYPES_HAS_COMPLEX
using c64          = std::complex<f32>;
using c128         = std::complex<f64>;
using c256         = std::complex<f128>;
#endif

#ifndef NCR_TYPES_DISABLE_VECTORS
#define NCR_TYPES_HAS_VECTORS
using u8_vector          = std::vector<u8>;
using u8_iterator        = u8_vector::iterator;
using u8_const_iterator  = u8_vector::const_iterator;

using u16_vector         = std::vector<u16>;
using u16_iterator       = u16_vector::iterator;
using u16_const_iterator = u16_vector::const_iterator;

using u32_vector         = std::vector<u32>;
using u32_iterator       = u32_vector::iterator;
using u32_const_iterator = u32_vector::const_iterator;

using u64_vector         = std::vector<u64>;
using u64_iterator       = u64_vector::iterator;
using u64_const_iterator = u64_vector::const_iterator;

#ifdef NCR_TYPES_ENABLE_STD_RANGES
#define NCR_TYPES_HAS_STD_RANGES
using u8_subrange        = std::ranges::subrange<u8_iterator>;
using u8_const_subrange  = std::ranges::subrange<u8_const_iterator>;

using u16_subrange       = std::ranges::subrange<u16_iterator>;
using u16_const_subrange = std::ranges::subrange<u16_const_iterator>;

using u32_subrange       = std::ranges::subrange<u32_iterator>;
using u32_const_subrange = std::ranges::subrange<u32_const_iterator>;

using u64_subrange       = std::ranges::subrange<u64_iterator>;
using u64_const_subrange = std::ranges::subrange<u64_const_iterator>;
#endif
#endif

using u8_span = std::span<u8>;
using u8_const_span = std::span<const u8>;


// Compile-time invariants. We make assumptions about size and representation of
// the basic types that are easy to break silently when porting. Catch them at
// the include site instead of producing subtly wrong file output at runtime.
static_assert(sizeof(u8)  == 1, "ncr expects sizeof(u8) == 1");
static_assert(sizeof(u16) == 2, "ncr expects sizeof(u16) == 2");
static_assert(sizeof(u32) == 4, "ncr expects sizeof(u32) == 4");
static_assert(sizeof(u64) == 8, "ncr expects sizeof(u64) == 8");
static_assert(sizeof(f32) == 4, "ncr expects sizeof(f32) == 4");
static_assert(sizeof(f64) == 8, "ncr expects sizeof(f64) == 8");


#endif /* _909f868e37c64952a3871f2f678d0778_ */

