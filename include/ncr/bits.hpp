/*
 * bits.hpp - bit operations
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE for more details
 */
#ifndef _6029ff7cb97c498f8a26966c49a873fe_
#define _6029ff7cb97c498f8a26966c49a873fe_

#include <bit>
#include "ncr/types.hpp"
#include "ncr/bswapdefs.hpp"

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


#ifdef NCR_TYPES_HAS_COMPLEX

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

#endif


/*
 * unsigned_or_unsigned_enum - matches plain unsigned integer types and
 * enum types whose underlying type is unsigned. Centralises the long
 * requires-clause that all bit/flag helpers below shared.
 */
template <typename T>
concept unsigned_or_unsigned_enum =
	std::unsigned_integral<T> ||
	std::unsigned_integral<typename std::underlying_type<T>::type>;


/*
 * flag_is_set - test if a flag is set in an unsigned value.
 *
 * This function evaluates if a certain flag, i.e. bit pattern, is present in v.
 */
template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline bool
flag_is_set(const T v, const U flag)
{
	return (v & flag) == flag;
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
set_flag(const T v, const U flag)
{
	return v | flag;
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
clear_flag(const T v, const U flag)
{
	return v & ~flag;
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
toggle_flag(const T v, const U flag)
{
	return v ^ flag;
}


/*
 * bitmask - create bitmask of given length at offset
 */
template <unsigned_or_unsigned_enum U>
constexpr U
bitmask(U offset, U length)
{
	// casting -1 to unsigned produces a value with 1s everywhere (i.e.
	// 0xFFF...F)
	return ~(U(-1) << length) << offset;
}


template <unsigned_or_unsigned_enum U>
inline U
set_bits(U dest, U offset, U length, U bits)
{
	U mask = bitmask<U>(offset, length);
	return (dest & ~mask) | ((bits << offset) & mask);
}


template <unsigned_or_unsigned_enum U>
inline U
get_bits(U src, U offset, U length)
{
	return (src & bitmask<U>(offset, length)) >> offset;
}


template <unsigned_or_unsigned_enum U>
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
template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline bool
bit_is_set(const T v, const U N)
{
	return (v & 1 << N) > 0;
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
set_bit(const T v, const U N)
{
	return v | (1 << N);
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
clear_bit(const T v, const U N)
{
	return v & ~(1 << N);
}


template <unsigned_or_unsigned_enum T, unsigned_or_unsigned_enum U>
inline T
toggle_bit(const T v, const U N)
{
	return v ^ (1 << N);
}

}

#endif /* _6029ff7cb97c498f8a26966c49a873fe_ */
