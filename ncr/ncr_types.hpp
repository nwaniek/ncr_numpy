/*
 * ncr_types.hpp - basic types used in ncr
 *
 * SPDX-FileCopyrightText: 2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#pragma once

#ifndef NCR_TYPES
#define NCR_TYPES
#endif

#ifndef NCR_TYPES_DISABLE_VECTORS
#define __NCR_TYPES_ENABLE_VECTORS true
#else
#define __NCR_TYPES_ENABLE_VECTORS false
#endif

#include <cstdint>
#include <complex>
#if __cplusplus >= 202302L
	#include <stdfloat>
#endif
#if __NCR_TYPES_ENABLE_VECTORS
	#include <ranges>
	#include <vector>
#endif


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

using c64          = std::complex<f32>;
using c128         = std::complex<f64>;
using c256         = std::complex<f128>;

#if __NCR_TYPES_ENABLE_VECTORS
using u8_vector         = std::vector<u8>;
using u8_iterator       = u8_vector::iterator;
using u8_subrange       = std::ranges::subrange<u8_iterator>;
using u8_const_iterator = u8_vector::const_iterator;
using u8_const_subrange = std::ranges::subrange<u8_const_iterator>;

using u16_vector         = std::vector<u16>;
using u16_iterator       = u16_vector::iterator;
using u16_subrange       = std::ranges::subrange<u16_iterator>;
using u16_const_iterator = u16_vector::const_iterator;
using u16_const_subrange = std::ranges::subrange<u16_const_iterator>;

using u32_vector         = std::vector<u32>;
using u32_iterator       = u32_vector::iterator;
using u32_subrange       = std::ranges::subrange<u32_iterator>;
using u32_const_iterator = u32_vector::const_iterator;
using u32_const_subrange = std::ranges::subrange<u32_const_iterator>;

using u64_vector         = std::vector<u64>;
using u64_iterator       = u64_vector::iterator;
using u64_subrange       = std::ranges::subrange<u64_iterator>;
using u64_const_iterator = u64_vector::const_iterator;
using u64_const_subrange = std::ranges::subrange<u64_const_iterator>;
#endif
