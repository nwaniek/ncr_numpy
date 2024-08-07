/*
 * ncr_bits.hpp - bit operations
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <bit>
#include "types.hpp"

// figure out if there are builtins or system functions available for byte
// swapping.
//
// TODO: Note that this is particularly ugly, and a better way might be to
// define the functions for each system / compiler in a particular header and
// let the build system or user decide which to pull in. For the time being,
// have everything in here.
//
// XXX: extend to other systems, see e.g. https://github.com/google/cityhash/blob/8af9b8c2b889d80c22d6bc26ba0df1afb79a30db/src/city.cc#L50

// take the compiler built in bswaps if possible
#if __has_builtin(__builtin_bswap16)
	#define ncr_bswap_16(x) __builtin_bswap16(x)
	#define NCR_HAS_BSWAP16
#endif
#if __has_builtin(__builtin_bswap32)
	#define ncr_bswap_32(x) __builtin_bswap32(x)
	#define NCR_HAS_BSWAP32
#endif
#if __has_builtin(__builtin_bswap64)
	#define ncr_bswap_64(x) __builtin_bswap64(x)
	#define NCR_HAS_BSWAP64
#endif

// only pull in headers when really needed
#if defined(_MSC_VER) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <stdlib.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   _byteswap_short(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   _byteswap_long(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   _byteswap_uint64(x)
		#define NCR_HAS_BSWAP64
	#endif
#elif defined(__APPLE__) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <libkern/OSByteOrder.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   OSSwapInt16(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   OSSwapInt32(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   OSSwapInt64(x)
		#define NCR_HAS_BSWAP64
	#endif
#elif defined(__linux__) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <byteswap.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   bswap_16(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   bswap_32(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   bswap_64(x)
		#define NCR_HAS_BSWAP64
	#endif
#endif

namespace ncr {

/*
 * bswap - byteswap (useful for changing endianness)
 *
 * The template will prefer any builtins that are available, and only fall back
 * to a default and potentially slower implementation of byte swapping when
 * necessary.
 *
 * TODO: for C++23, can also use std::byteswap, but this will pull in a ton of
 * headers
 */
template <typename T> inline T bswap(T);

template <> inline
u8 bswap<u8> (u8 val)
{
	return val;
}


template <> inline
u16 bswap<u16>(u16 val)
{
	#if defined(NCR_HAS_BSWAP16)
		return ncr_bswap_16(val);
	#else
		return ((val & 0xFF00) >> 8u)  |
		       ((val & 0x00FF) << 8u);
	#endif
}


template <> inline
u32 bswap<u32>(u32 val)
{
	#if defined(NCR_HAS_BSWAP32)
		return ncr_bswap_32(val);
	#else
		return ((val & 0xFF000000) >> 24u) |
		       ((val & 0x00FF0000) >> 8u)  |
		       ((val & 0x0000FF00) << 8u)  |
		       ((val & 0x000000FF) << 24u);
    #endif
}


template <> inline
u64 bswap<u64>(u64 val)
{
	#if defined(NCR_HAS_BSWAP64)
		return ncr_bswap_64(val);
	#else
		return ((val & 0xFF00000000000000) >> 56u) |
		       ((val & 0x00FF000000000000) >> 40u) |
		       ((val & 0x0000FF0000000000) >> 24u) |
		       ((val & 0x000000FF00000000) >> 8u)  |
		       ((val & 0x00000000FF000000) << 8u)  |
		       ((val & 0x0000000000FF0000) << 24u) |
		       ((val & 0x000000000000FF00) << 40u) |
		       ((val & 0x00000000000000FF) << 56u);
	#endif
}


template <> inline
f32 bswap<f32>(f32 val)
{
	u32 tmp = bswap<u32>(std::bit_cast<u32, f32>(val));
	return std::bit_cast<f32>(tmp);
}


template <> inline
f64 bswap<f64>(f64 val)
{
	u64 tmp = bswap<u64>(std::bit_cast<u64, f64>(val));
	return std::bit_cast<f64>(tmp);
}


template <> inline
std::complex<f32> bswap<std::complex<f32>>(std::complex<f32> val)
{
	u32 ureal = bswap<u32>(std::bit_cast<u32, f32>(real(val)));
	u32 uimag = bswap<u32>(std::bit_cast<u32, f32>(imag(val)));
	return std::complex<f32>(std::bit_cast<f32, u32>(ureal), std::bit_cast<f32, u32>(uimag));
}


template <> inline
std::complex<f64> bswap<std::complex<f64>>(std::complex<f64> val)
{
	u64 ureal = bswap<u64>(std::bit_cast<u64, f64>(real(val)));
	u64 uimag = bswap<u64>(std::bit_cast<u64, f64>(imag(val)));
	return std::complex<f64>(std::bit_cast<f64, u64>(ureal), std::bit_cast<f64, u64>(uimag));
}


/*
 * flag_is_set - test if a flag is set in an unsigned value.
 *
 * This function evaluates if a certain flag, i.e. bit pattern, is present in v.
 */
template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline bool
flag_is_set(const T v, const U flag)
{
	return (v & flag) == flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
set_flag(const T v, const U flag)
{
	return v | flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
clear_flag(const T v, const U flag)
{
	return v & ~flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
toggle_flag(const T v, const U flag)
{
	return v ^ flag;
}


/*
 * bitmask - create bitmask of given length at offset
 */
template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
constexpr U
bitmask(U offset, U length)
{
	// casting -1 to unsigned produces a value with 1s everywhere (i.e.
	// 0xFFF...F)
	return ~(U(-1) << length) << offset;
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
set_bits(U dest, U offset, U length, U bits)
{
	U mask = bitmask<U>(offset, length);
	return (dest & ~mask) | ((bits << offset) & mask);
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
get_bits(U src, U offset, U length)
{
	return (src & bitmask<U>(offset, length)) >> offset;
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
toggle_bits(U src, U offset, U length)
{
	U mask = bitmask<U>(offset, length);
	return src ^ mask;
}


/*
 * bit_is_set - test if the Nth bit is set in a variable, where N starts at 0
 *
 * This function evaluates if the Nth bit is present in variable v.
 */
template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline bool
bit_is_set(const T v, const U N)
{
	return (v & 1 << N) > 0;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
set_bit(const T v, const U N)
{
	return v | (1 << N);
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
clear_bit(const T v, const U N)
{
	return v & ~(1 << N);
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
toggle_bit(const T v, const U N)
{
	return v ^ (1 << N);
}

}
