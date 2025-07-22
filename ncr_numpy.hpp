/*
 * ncr_numpy.hpp - single file header for working with numpy files.
 *
 * NOTE: This file is an amalgamation of several individual header files that
 * provide functionality to work with numpy npy files. Combining all the
 * individual headers into one large header means that this is the only header
 * that is required to work with numpy files.
 *
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2023-2025 Nicolai Waniek <n@rochus.net>
 *
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define NCR_NUMPY_VERSION 0.5.6

#include <cstring>
#include <cassert>
#include <type_traits>
#include <functional>
#include <vector>
#include <iostream>
#include <iomanip>
#include <span>
#include <cstdint>
#include <complex>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <cstddef>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <optional>
#include <algorithm>
#include <memory>
#include <array>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <unordered_set>
#include <bit>
#include <zip.h>

/*
 * ncr/bswapdefs.hpp - definitions for bswap16, bswap32, and bswap64
 *
 * TODO: Note that this is particularly ugly, and a better way might be to
 * define the functions for each system / compiler in a particular header and
 * let the build system or user decide which to pull in. For the time being,
 * have everything in here.
 *
 * XXX: extend to other systems, see e.g. https://github.com/google/cityhash/blob/8af9b8c2b889d80c22d6bc26ba0df1afb79a30db/src/city.cc#L50
 *
 */
#ifndef _ba6cf59bec5b4f2b92694c85e64f44cb_
#define _ba6cf59bec5b4f2b92694c85e64f44cb_


// figure out if there are builtins or system functions available for byte
// swapping, and if yes, take the compiler built in bswaps
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

// only pull in headers when really needed, and only those appropriate for the
// system this will be compiled on
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

#endif /* _ba6cf59bec5b4f2b92694c85e64f44cb_ */

/*
 * ncr_types.hpp - basic types used in ncr
 *
 */
#ifndef _909f868e37c64952a3871f2f678d0778_
#define _909f868e37c64952a3871f2f678d0778_

#ifndef NCR_TYPES
#define NCR_TYPES
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

using c64          = std::complex<f32>;
using c128         = std::complex<f64>;
using c256         = std::complex<f128>;

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


#endif /* _909f868e37c64952a3871f2f678d0778_ */

/*
 * ncr_unicode - utilities for working with unicode (UTF-8, UCS-4) strings
 *
 */
#ifndef _53994d0d2b6d47028f271c81f3b8f52a_
#define _53994d0d2b6d47028f271c81f3b8f52a_


namespace ncr {

// forward declarations
template <size_t N = 0> struct ucs4string;
template <size_t N = 0> struct utf8string;

template <typename T, size_t N = 0> ucs4string<N> to_ucs4(const T &t);
template <typename T, size_t N = 0> utf8string<N> to_utf8(const T &t);
template <typename T, size_t N = 0> std::string to_string(const T &t);


/*
 * ucs4string<N> - fixed-length unicode string in UCS-4 encoding
 *
 * This is a fixed-length unicode string that uses std::array as internal
 * storage container.
 *
 * When working with python/numpy, one can assume that most strings are likely
 * encoded as UCS-4 strings, meaning 4 bytes per character.
 *
 * The storage format of UCS-4 strings represent Unicode code points of a fixed
 * 4 byte width. Each code point directly corresponds to a single 4-byte value.
 * Thus, each element in data represents one code point.
 *
 * Example: 'A' (U+0041) is stored as '0x00000041'
 *          '€' (U+20AC) is stored as '0x000020AC'
 */
template <size_t N>
struct ucs4string
{
	std::array<u32, N>
		data;
};


/*
 * ucs4string<0> - variable-length with unicode string in UCS-4 encoding
 *
 * This is a variable-length unicode string, its internal storage container is
 * std::vector. For more details about UCS-4, see ucs4string<N>.
 */
template <>
struct ucs4string<0>
{
	std::vector<u32>
		data;
};


template <typename T>
struct is_ucs4string : std::false_type {};

template <size_t N>
struct is_ucs4string<ucs4string<N>> : std::true_type {};

template <typename T>
struct ucs4string_size : std::integral_constant<u64, 0> {};

template <size_t N>
struct ucs4string_size<ucs4string<N>>: std::integral_constant<u64, N> {};

template <typename T>
struct ucs4string_bytesize : std::integral_constant<u64, 0> {};

template <size_t N>
struct ucs4string_bytesize<ucs4string<N>>: std::integral_constant<u64, N * 4> {};



template <size_t N = 0>
ucs4string<N>
to_ucs4(const std::array<u32, N> &ucs4)
{
	ucs4string<N> result;
	if constexpr (N != 0) {
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(ucs4.begin(), ucs4.end(), result.data.begin());
	}
	return result;
}


template <size_t N = 0>
ucs4string<N>
to_ucs4(const std::vector<u32> &ucs4)
{
	ucs4string<N> result;
	if constexpr (N == 0) {
		result.data.assign(ucs4.begin(), ucs4.end());
	}
	else {
		if (ucs4.size() > N)
			throw std::runtime_error("Input string exceeds fixed-width UCS-4 string size.");
		// make sure the entire array is zero initialized
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(ucs4.begin(), ucs4.end(), result.data.begin());
	}
	return result;
}


template <size_t N = 0>
ucs4string<N>
to_ucs4(const std::string &utf8)
{
	ucs4string<N> result;
	if constexpr (N != 0)
		std::fill(result.data.begin(), result.data.end(), 0);

	size_t i = 0;
	size_t n = 0;
	while (i < utf8.size()) {
		u32 codepoint = 0;
		u8 byte		  = utf8[i];

		if (byte <= 0x7F) {
			codepoint = byte;
			i += 1;
		}
		else if (byte <= 0xBF) {
			throw std::runtime_error("Invalid UTF-8 byte sequence.");
		}
		else if (byte <= 0xDF) {
			codepoint = byte & 0x1F;
			codepoint = (codepoint << 6) | (utf8[i + 1] & 0x3F);
			i += 2;
		}
		else if (byte <= 0xEF) {
			codepoint = byte & 0x0F;
			codepoint = (codepoint << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
			i += 3;
		}
		else if (byte <= 0xF7) {
			codepoint = byte & 0x07;
			codepoint = (codepoint << 18) | ((utf8[i + 1] & 0x3F) << 12) |
				((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
			i += 4;
		}
		else {
			throw std::runtime_error("Invalid UTF-8 byte sequence.");
		}

		if constexpr (N == 0) {
			result.data.push_back(codepoint);
		}
		else {
			if (n >= N)
				throw std::runtime_error("Input string exceeds fixed-width UCS-4 string size.");
			result.data[n++] = codepoint;
		}
	}
	return result;
}


template <size_t N = 0>
ucs4string<N>
to_ucs4(const char *utf8)
{
	return to_ucs4<N>(std::string(utf8));
}


template <size_t N = 0, size_t M = 0>
ucs4string<N>
to_ucs4(const utf8string<M> &utf8)
{
	return to_ucs4<N>(to_string(utf8));
}


template <size_t N = 0>
std::string
to_string(const ucs4string<N> &ucs4)
{
	std::string utf8_string;
	for (auto ch : ucs4.data) {
		if (ch == 0) break;

		if (ch <= 0x7F) {
			utf8_string += static_cast<char>(ch);
		} else if (ch <= 0x7FF) {
			utf8_string += static_cast<char>(0xC0 | (ch >> 6));
			utf8_string += static_cast<char>(0x80 | (ch & 0x3F));
		} else if (ch <= 0xFFFF) {
			utf8_string += static_cast<char>(0xE0 | (ch >> 12));
			utf8_string += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
			utf8_string += static_cast<char>(0x80 | (ch & 0x3F));
		} else if (ch <= 0x10FFFF) {
			utf8_string += static_cast<char>(0xF0 | (ch >> 18));
			utf8_string += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
			utf8_string += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
			utf8_string += static_cast<char>(0x80 | (ch & 0x3F));
		}
	}
	return utf8_string;
}


#ifdef NCR_ENABLE_STREAM_OPERATORS
template <size_t N = 0>
std::ostream&
operator<<(std::ostream& os, const ucs4string<N> &ucs4)
{
	os << to_string(ucs4);
	return os;
}
#endif



/*
 * utf8string - fixed with UTF-8 string
 *
 * The storage format of UTF-8 is a variable length sequence of bytes, where
 * each character can use 1 to 4 bytes. The content is a byte sequence that
 * represents Unicode characters. Note that Unicode characters are not Unicode
 * code points, but rather a byte representation of these code points
 *
 * Example: 'A' (U+0041) is stored as 0x41 (1 byte)
 *          '€' (U+20AC) is stored as 0xE2 0x82 0xAC (3 bytes)
 */
template <size_t N>
struct utf8string
{
	std::array<u8, N>
		data;
};


template <>
struct utf8string<0>
{
	std::vector<u8>
		data;
};


template <typename T>
struct is_utf8string : std::false_type {};

template <size_t N>
struct is_utf8string<utf8string<N>> : std::true_type {};

template <typename T>
struct utf8string_size : std::integral_constant<u64, 0> {};

template <size_t N>
struct utf8string_size<utf8string<N>>: std::integral_constant<u64, N> {};


template <size_t N = 0>
utf8string<N>
to_utf8(const std::array<u8, N> &utf8)
{
	utf8string<N> result;
	if constexpr (N != 0) {
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(utf8.begin(), utf8.end(), result.data.begin());
	}
	return result;
}

template <size_t N = 0>
utf8string<N>
to_utf8(const std::vector<u8> &utf8)
{
	utf8string<N> result;
	if constexpr (N == 0) {
		result.data.assign(utf8.begin(), utf8.end());
	}
	else {
		if (utf8.size() > N)
			throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
		// make sure the entire array is zero initialized
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(utf8.begin(), utf8.end(), result.data.begin());
	}
	return result;
}


template <size_t N = 0>
utf8string<N>
to_utf8(const std::string &utf8)
{
	utf8string<N> result;
	if constexpr (N == 0) {
		result.data.assign(utf8.begin(), utf8.end());
	}
	else {
		if (utf8.size() > N)
			throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
		// make sure the entire array is zero initialized
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(utf8.begin(), utf8.end(), result.data.begin());
	}
	return result;
}


template <size_t N = 0>
utf8string<N>
to_utf8(const char *utf8)
{
	return to_utf8<N>(std::string(utf8));
}


template <size_t N = 0>
utf8string<N>
to_utf8(const ucs4string<N> &ucs4)
{
	utf8string<N> result;
	std::string tmp = to_string(ucs4);
	if constexpr (N == 0) {
		result.data.assign(tmp.begin(), tmp.end());
	}
	else {
		if (tmp.size() > N)
			throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
		// make sure the entire array is zero initialized
		std::fill(result.data.begin(), result.data.end(), 0);
		std::copy(tmp.begin(), tmp.end(), result.data.begin());
	}
	return result;
}

template <size_t N = 0>
std::string
to_string(const utf8string<N> &utf8)
{
	std::string result;
	result.assign(utf8.data.begin(), utf8.data.end());
	return result;
}


#ifdef NCR_ENABLE_STREAM_OPERATORS
template <size_t N = 0>
std::ostream&
operator<<(std::ostream& os, const utf8string<N> &utf8)
{
	os << to_string(utf8);
	return os;
}
#endif


} // ncr

#endif /* _53994d0d2b6d47028f271c81f3b8f52a_ */

/*
 * ncr/ndindex.hpp - functions to calculate indexes of N-D arrays
 */
#ifndef _8840dbd5d93342cda97ac95e4a7ed8b3_
#define _8840dbd5d93342cda97ac95e4a7ed8b3_


#ifndef NCR_TYPES
using u8 = std::uint8_t;
#endif


namespace ncr {

/*
 * byte_order - byte order indicator
 */
enum class byte_order {
	little,
	big,
	not_relevant,
	invalid,
	// TODO: determine if native = little is correct
	native = little,
};


inline char
to_char(byte_order o)
{
	switch (o) {
		case byte_order::little:       return '<';
		case byte_order::big:          return '>';
		case byte_order::not_relevant: return '|';

		// TODO: set a fail state for invalid
		case byte_order::invalid:      return '!';
	}
	return '!';
}


inline byte_order
to_byte_order(const u8 chr)
{
	switch (chr) {
	case '>': return byte_order::big;
	case '<': return byte_order::little;
	case '=': return byte_order::native;
	case '|': return byte_order::not_relevant;
	default:  return byte_order::invalid;
	};
}



#ifdef NCR_ENABLE_STREAM_OPERATORS
// operator<< usually used in std::cout
// TODO: remove or disable?
inline std::ostream&
operator<<(std::ostream &os, const byte_order bo)
{
	switch (bo) {
		case byte_order::little:       os << "little";       break;
		case byte_order::big:          os << "big";          break;
		case byte_order::not_relevant: os << "not_relevant"; break;
		case byte_order::invalid:      os << "invalid";      break;

		// this should never happen
		default: os.setstate(std::ios_base::failbit);
	}
	return os;
}
#endif


/*
 * storage_order - storage order of data in a dtype
 */
enum class storage_order {
	// linear storage in which consecutive elements form the columns, also
	// called 'fortran-order'
	col_major,

	// linear storage in which consecutive elements form the rows of data,
	// also called c-order
	row_major,
};


#ifdef NCR_ENABLE_STREAM_OPERATORS
/*
 * operator<< - pretty print the storage order as text
 */
inline std::ostream&
operator<<(std::ostream &os, const storage_order order)
{
	switch (order) {
		case storage_order::col_major: os << "col_major"; break;
		case storage_order::row_major: os << "row_major"; break;
	}
	return os;
}
#endif


template <typename T = size_t>
std::vector<T>
unravel_index(T index, const std::vector<T>& shape, storage_order order)
{
	size_t n = shape.size();
	std::vector<T> indices(n);

	switch (order) {
	case storage_order::row_major:
		{
			size_t i = n;
			while (i > 0) {
				--i;
				indices[i] = static_cast<T>(index % shape[i]);
				index /= shape[i];
			}
		}
		break;

	case storage_order::col_major:
		for (size_t i = 0; i < n; ++i) {
			indices[i] = index % shape[i];
			index /= shape[i];
		}
		break;
	}

	return indices;
}



template <typename T = size_t>
std::vector<T>
unravel_index_strided(size_t offset, const std::vector<T> &strides, storage_order order)
{
	std::vector<T> indices(strides.size());

	switch (order) {
	case storage_order::row_major:
		for (size_t i = 0; i < strides.size(); ++i) {
			size_t currentStride = strides[i];
			size_t currentIndex = offset / currentStride;
			offset %= currentStride;
			indices[i] = static_cast<T>(currentIndex);
		}
		break;

	case storage_order::col_major:
		for (size_t i = strides.size(); i-- > 0;) {
			size_t currentStride = strides[i];
			size_t currentIndex = offset / currentStride;
			offset %= currentStride;
			indices[i] = static_cast<T>(currentIndex);
		}
		break;
	}

	return indices;
}


/*
 * the strides for array with dimensions N_1 x N_2 x ... x N_d and
 * index tuple (n_1, n_2, ..., n_d), n_k ∈ [0, N_k - 1] can be computed as
 * follows:

 * formula for row-major: sum_{k=1}^d (prod_{l=k+1}^d N_l) * n_k
 * formula for col-major: sum_{k=1}^d (prod_{l=1}^{k-1} N_l) * n_k
 */


/*
 * stride_row_major - get the l-th stride of an array of given shape
 */
template <typename T = size_t>
T
stride_row_major(const std::vector<T> &shape, ssize_t l)
{
	size_t s = 1;
	for (; ++l < (ssize_t)shape.size(); )
		s *= shape[l];
	return s;
}


/*
 * stride_col_major - get the k-th stride of an array of given shape
 */
template <typename T = size_t>
T
stride_col_major(const std::vector<T> &shape, ssize_t k)
{
	size_t s = 1;
	for (; --k >= 0; )
		s *= shape[k];
	return s;
}


/*
 * compute_strides - compute the strides of an array of given shape and storage order
 */
template <typename T = size_t, bool single_loop = true>
void
compute_strides(const std::vector<T> &shape, std::vector<T> &strides, storage_order order = storage_order::row_major)
{
	strides.resize(shape.size());

	if constexpr (single_loop) {
		T total = 1;
		switch (order) {
		case storage_order::row_major:
			for (size_t i = shape.size(); i-- > 0; ) {
				strides[i] = total;
				total *= shape[i];
			}
			break;

		case storage_order::col_major:
			for (size_t i = 0; i < shape.size(); ++i) {
				strides[i] = total;
				total *= shape[i];
			}
			break;
		}
	}
	else {
		T (*fptr)(const std::vector<T> &shape, ssize_t) =
			order == storage_order::row_major ?
				&stride_row_major<T> :
				&stride_col_major<T> ;
		for (size_t k = 0; k < shape.size(); k++)
			strides[k] = (*fptr)(shape, k);
	}
}



} // ncr::

#endif /* _8840dbd5d93342cda97ac95e4a7ed8b3_ */

/*
 * ncr/dtype.hpp - data type definition for numpy nd-arrays
 */
#ifndef _f7e9e094e0ba4453850c999f0e7f2a56_
#define _f7e9e094e0ba4453850c999f0e7f2a56_



// TODO: decide if this should remain in the numpy namespace or not
namespace ncr { namespace numpy {

/*
 * dtype - data type description of elements in the ndarray
 *
 * In case of structured arrays, only the values in the fields might be properly
 * filled in. Note also that structured arrays can have types with arbitrarily
 * deep nesting of sub-structures. To determine if a (sub-)dtype is a structured
 * array, you can query is_structured_array(), which simply tests if there are
 * fields within this particular dtype.
 *
 * Note, however, that nested fields themselves are dtypes. That means that
 * fields which are leaves of a structured array, and therefore basic types,
 * will return false in a call to is_structured_array.
 *
 * Further note that structured arrays might contain types with mixed
 * endianness.
 */
struct dtype
{
	// name of the field in case of strutured arrays. for basic types this is
	// empty.
	std::string
		name = "";

	// byte order of the data
	byte_order
		endianness = byte_order::native;

	// single character type code (see table at start of this file)
	u8
		type_code = 0;

	// size of the data in bytes, e.g. bytes of an integer or characters in a
	// unicode string
	u32
		size = 0;

	// size of an item in bytes in this dtype (e.g. U16 is a 16-character
	// unicode string, each character using 4 bytes.  hence, item_size = 64
	// bytes).
	u64
		item_size = 0;

	// offset of the field if this is a field in a structured array, otherwise
	// this will be (most likely) 0
	u64
		offset = 0;

	// numpy's shape has python 'int', which is commonly a 64bit integer. see
	// python's sys.maxsize to get the maximum value, log(sys.maxsize,2)+1 will
	// tell the number of bits used on a machine. Here, we simply assume that
	// a u64 is enough.
	std::vector<u64>
		shape = {};

	// structured arrays will contain fields, which are themselves dtypes.
	//
	// Note: We store them in a vector because we need to retain the insert
	// order. We could also use a map instead, but then we would have to make
	// sure that we somehow store the insert order.
	std::vector<dtype>
		fields = {};

	// map field names to indexes. while small structured arrays won't
	// necessarily benefit from this, larger structured arrays might gain some
	// speedup in getting values from fields.
	std::unordered_map<std::string, size_t>
		field_indexes  = {};
};


inline bool
is_structured_array(const dtype &dt)
{
	return !dt.fields.empty();
}


inline
const dtype*
find_field(const dtype &dt, const std::string& field_name)
{
	auto it = dt.field_indexes.find(field_name);
	if (it == dt.field_indexes.end())
		return nullptr;
	return &dt.fields[it->second];
}


template <typename First, typename... Rest>
const dtype*
find_field_recursive(const dtype &dt, const First& first, const Rest&... rest)
{
	const dtype* next_dt = find_field(dt, first);
	if (!next_dt)
		return nullptr;

	if constexpr (sizeof...(rest) == 0)
		return next_dt;
	else
		return find_field_recursive(*next_dt, rest...);
}


template <typename T, typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, dtype>>>
dtype&
add_field(dtype &dt, T &&field)
{
	dt.fields.emplace_back(std::forward<T>(field));
	dtype& result = dt.fields.back();
	dt.field_indexes.insert({result.name, dt.fields.size() - 1});
	return result;
}


template <typename Func>
void
for_each_field(dtype &dt, Func &&func)
{
	for (auto& field: dt.fields)
		func(field);
}


template <typename Func>
void
for_each_field(const dtype &dt, Func &&func)
{
	for (const auto& field: dt.fields)
		func(field);
}


template <typename Func>
void
for_each(std::vector<dtype> &fields, Func &&func)
{
	for (auto& field: fields)
		func(field);
}

template <typename Func>
void
for_each(const std::vector<dtype> &fields, Func &&func)
{
	for (const auto& field: fields)
		func(field);
}


//
// basic dtypes
//
inline dtype dtype_int16()  { return {.type_code = 'i', .size=2, .item_size=2}; }
inline dtype dtype_int32()  { return {.type_code = 'i', .size=4, .item_size=4}; }
inline dtype dtype_int64()  { return {.type_code = 'i', .size=8, .item_size=8}; }

inline dtype dtype_uint16() { return {.type_code = 'u', .size=2, .item_size=2}; }
inline dtype dtype_uint32() { return {.type_code = 'u', .size=4, .item_size=4}; }
inline dtype dtype_uint64() { return {.type_code = 'u', .size=8, .item_size=8}; }

inline dtype dtype_float16() { return {.type_code = 'f', .size=2, .item_size=2}; }
inline dtype dtype_float32() { return {.type_code = 'f', .size=4, .item_size=4}; }
inline dtype dtype_float64() { return {.type_code = 'f', .size=8, .item_size=8}; }


//
// dtype selectors, required for automatic compile time dtype selection in
// ndarray_t constructors further below
//
template <typename T>
struct dtype_selector;

template <> struct dtype_selector<i16> { static dtype get() { return dtype_int16(); } };
template <> struct dtype_selector<i32> { static dtype get() { return dtype_int32(); } };
template <> struct dtype_selector<i64> { static dtype get() { return dtype_int64(); } };

template <> struct dtype_selector<u16> { static dtype get() { return dtype_uint16(); } };
template <> struct dtype_selector<u32> { static dtype get() { return dtype_uint32(); } };
template <> struct dtype_selector<u64> { static dtype get() { return dtype_uint64(); } };

template <> struct dtype_selector<f16> { static dtype get() { return dtype_float16(); } };
template <> struct dtype_selector<f32> { static dtype get() { return dtype_float32(); } };
template <> struct dtype_selector<f64> { static dtype get() { return dtype_float64(); } };


//
// forward declarations (required due to indirect recursion)
//
inline void serialize_dtype(std::ostream &s, const dtype &dt);
inline void serialize_dtype_descr(std::ostream &s, const dtype &dt);
inline void serialize_dtype_fields(std::ostream &s, const dtype &dt);
inline void serialize_dtype_typestr(std::ostream &s, const dtype &dt);
inline void serialize_fortran_order(std::ostream &s, storage_order o);
inline void serialize_shape(std::ostream &s, const u64_vector &shape);


inline void
serialize_dtype_typestr(std::ostream &s, const dtype &dt)
{
	s << "'" << to_char(dt.endianness) << dt.type_code << dt.size << "'";
}


inline void
serialize_shape(std::ostream &s, const u64_vector &shape)
{
	s << "(";
	for (auto size: shape)
		s << size << ",";
	s << ")";
}


inline void
serialize_dtype_fields(std::ostream &s, const dtype &dt)
{
	s << "[";
	size_t i = 0;
	for (auto &f: dt.fields) {
		if (i++ > 0) s << ", ";
		serialize_dtype(s, f);
	}
	s << "]";
}


inline void
serialize_dtype(std::ostream &s, const dtype &dt)
{
	s << "('" << dt.name << "', ";
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else {
		serialize_dtype_typestr(s, dt);
		if (dt.shape.size() > 0) {
			s << ", ";
			serialize_shape(s, dt.shape);
		}
	}
	s << ")";
}


inline void
serialize_dtype_descr(std::ostream &s, const dtype &dt)
{
	s << "'descr': ";
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else
		serialize_dtype_typestr(s, dt);
}

inline void
serialize_fortran_order(std::ostream &s, storage_order o)
{
	s << "'fortran_order': " << (o == storage_order::col_major ? "True" : "False");
}


#ifdef NCR_ENABLE_STREAM_OPERATORS
/*
 * operator<< - pretty print a dtype
 */
inline std::ostream&
operator<< (std::ostream &os, const dtype &dt)
{
	std::ostringstream s;
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else
		serialize_dtype_typestr(s, dt);
	os << s.str();
	return os;
}
#endif


}} // ncr::numpy::


#endif /* _f7e9e094e0ba4453850c999f0e7f2a56_ */

#ifndef _65fc1481d8d149029547d3932c93f2e0_
#define _65fc1481d8d149029547d3932c93f2e0_



/*
 * NCR_DEFINE_ENUM_FLAG_OPERATORS - define all binary operators used for flags
 *
 * This macro expands into functions for bit-wise and binary operations on
 * enums, e.g. given two enum values a and b, one might want to write `a |= b;`.
 * With the macro below, this will be possible.
 */
#define NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_T) \
	inline ENUM_T operator~(ENUM_T a)              { return static_cast<ENUM_T>(~ncr::to_underlying(a)); } \
	inline ENUM_T operator|(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T operator&(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T operator^(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); } \
	inline ENUM_T& operator|=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T& operator&=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T& operator^=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); }


/*
 * NCR_DEFINE_FUNCTION_ALIAS - define a function alias for another function
 *
 * Using perfect forwarding, this creates a function with a novel name that
 * forwards all the arguments to the original function
 *
 * Note: In case there are multiple overloaded functions, this macro can be used
 *       _after_ the last overloaded function itself.
 */
#define NCR_DEFINE_FUNCTION_ALIAS(ALIAS_NAME, ORIGINAL_NAME)           \
	template <typename... Args>                                        \
	inline auto ALIAS_NAME(Args &&... args)                            \
		noexcept(noexcept(ORIGINAL_NAME(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME(std::forward<Args>(args)...))        \
	{                                                                  \
		return ORIGINAL_NAME(std::forward<Args>(args)...);             \
	}


/*
 * NCR_DEFINE_FUNCTION_ALIAS_EXT - similar as above, but with additional
 * template arguments that are not captured in the case above.
 *
 * For instance, if one implements a function that gets specialized on its
 * return type, then the this could be used.
 *
 * Example:
 *
 *     // some template which has a template arguemnt for the return type
 *     template <typename T, typename U> T ncr_some_fun(int x);
 *
 *     // specialization
 *     template <typename U>
 *     float ncr_some_fun(int x)
 *     {
 *     	return (float)x;
 *     }
 *
 *     NCR_DEFINE_SHORT_NAME_EXT(some_fun, ncr_some_fun)
 */
#define NCR_DEFINE_FUNCTION_ALIAS_EXT(ALIAS_NAME, ORIGINAL_NAME)                 \
	template <typename... Args2, typename... Args>                               \
	inline auto ALIAS_NAME(Args &&... args)                                      \
		noexcept(noexcept(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))        \
	{                                                                            \
		return ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...);             \
	}

#define NCR_DEFINE_TYPE_ALIAS(ALIAS_NAME, ORIGINAL_NAME) \
	using ALIAS_NAME = ORIGINAL_NAME


/*
 * NCR_DEFINE_SHORT_NAME - define a short name for a longer one
 *
 * This allows to easily define short function names, e.g. without the ncr_
 * prefix, for a given function. Not the the alias definition will only take
 * place if NCR_ENABLE_SHORT_NAMES is defined.
 */
#ifdef NCR_ENABLE_SHORT_NAMES
	#define NCR_DEFINE_SHORT_FN_NAME(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS(SHORT_NAME, LONG_NAME)

	#define NCR_DEFINE_SHORT_FN_NAME_EXT(SHORT_FN_NAME, LONG_FN_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS_EXT(SHORT_FN_NAME, LONG_FN_NAME)

	#define NCR_DEFINE_SHORT_TYPE_ALIAS(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_TYPE_ALIAS(SHORT_NAME, LONG_NAME)
#else
	#define NCR_DEFINE_SHORT_FN_NAME(_0, _1)
	#define NCR_DEFINE_SHORT_FN_NAME_EXT(_0, _1)
	#define NCR_DEFINE_SHORT_TYPE_ALIAS(_0, _1)
#endif


/*
 * Count the number of arguments to a variadic macro. Up to 64 arguments are
 * supported
 */
#define NCR_COUNT_ARGS2(X,_64,_63,_62,_61,_60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41,_40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21,_20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) N
#define NCR_COUNT_ARGS(...) NCR_COUNT_ARGS2(0, __VA_ARGS__ ,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)


/*
 * Suppress warnings for unused arguments. Up to 10 arguments are supported in
 * the variadic version NCR_UNUSED
 */
#define NCR_UNUSED_1(X)        (void)X;
#define NCR_UNUSED_2(X0, X1)   NCR_UNUSED_1(X0); NCR_UNUSED_1(X1)
#define NCR_UNUSED_3(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_2(__VA_ARGS__)
#define NCR_UNUSED_4(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_3(__VA_ARGS__)
#define NCR_UNUSED_5(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_4(__VA_ARGS__)
#define NCR_UNUSED_6(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_5(__VA_ARGS__)
#define NCR_UNUSED_7(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_6(__VA_ARGS__)
#define NCR_UNUSED_8(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_7(__VA_ARGS__)
#define NCR_UNUSED_9(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_8(__VA_ARGS__)
#define NCR_UNUSED_10(X0, ...) NCR_UNUSED_1(X0); NCR_UNUSED_9(__VA_ARGS__)

#define NCR_UNUSED_INDIRECT3(N, ...)  NCR_UNUSED_ ## N(__VA_ARGS__)
#define NCR_UNUSED_INDIRECT2(N, ...)  NCR_UNUSED_INDIRECT3(N, __VA_ARGS__)
#define NCR_UNUSED(...)               NCR_UNUSED_INDIRECT2(NCR_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)



/*
 * macro to define an enum class of a specific underlying type, and also
 * generating a template specialization that returns the number of values in the
 * enum class.
 */
template <typename T> constexpr size_t enum_count();

#define NCR_ENUM_CLASS(EnumName, UnderlyingType, ...) \
	enum class EnumName : UnderlyingType { \
		__VA_ARGS__ \
	}; \
	template<> constexpr size_t enum_count<EnumName>() { return NCR_COUNT_ARGS(__VA_ARGS__); }

/*
 * A simple define to reduce the verbosity to declare a tuple. This is
 * particularly useful, for instance, in calls to random.hpp:random_coord.
 * In this example, the template accepts a variadic number of tuples, e.g.
 *
 *     auto xlim = std::tuple{0, 1};
 *     auto ylim = std::tuple{0, 10};
 *     auto coord = random_coord(rng, xlim, ylim);
 *
 * It would be better to avoid the temporary variables. However,
 * brace-initializers wont work, as there is no clear (read: acceptably sane)
 * way to turn an initializer_list into a tuple. With the following macro, it is
 * actually possible to succinctly write
 *
 *     auto coord = random_coord(_T(0, 1), _T(0, 10));
 *
 * without any local declaration of temporaries, or overly long calls that
 * include the specific tuple type.
 */
#ifndef _tup
	#define _tup(...) std::tuple{__VA_ARGS__}
#endif



namespace ncr {

/*
 * ensure at compile time that one or more types are PODs
 */
template <typename T>
constexpr void ensure_pod1() {
	static_assert(std::is_trivial_v<T>,         "Type is not trivial!");
	static_assert(std::is_standard_layout_v<T>, "Type does not have a standard layout!");
}
template <typename... Types>
constexpr void ensure_pod() { (ensure_pod1<Types>(), ...); }


/*
 * compile time count of elements in an array. If standard library is used,
 * could also use std::size instead.
 */
template <std::size_t N, class T>
constexpr std::size_t len(T(&)[N]) { return N; }


/*
 * to_underlying - Get the underlying type of some type
 *
 * This is an implementation of C++23's to_underlying function, which is not yet
 * available in C++20 but handy for casting enum-structs to their underlying
 * type (see NCR_DEFINE_ENUM_FLAG_OPERATORS for an example).
 */
template <typename E>
constexpr typename std::underlying_type<E>::type
to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}


/*
 * get_index_of - get the index of a pointer to T in a vector of T
 *
 * This function returns an optional to indicate if the pointer to T was found
 * or not.
 */
template <typename T>
inline std::optional<size_t>
get_index_of(std::vector<T*> vec, T *needle)
{
	for (size_t i = 0; i < vec.size(); i++)
		if (vec[i] == needle)
			return i;
	return {};
}


/*
 * determine if a container contains a certain element or not
 */
template <typename ContainerT, typename U>
inline bool
contains(const ContainerT &container, const U &needle)
{
	auto it = std::find(container.begin(), container.end(), needle);
	return it != container.end();
}


/*
 * hexdump - generate a hexdump for a buffer similar to hex editors
 */
inline void
hexdump(std::ostream& os, const std::vector<uint8_t> &data)
{
	std::ios old_state(nullptr);
	old_state.copyfmt(os);

	const size_t bytes_per_line = 16;
	for (size_t offset = 0; offset < data.size(); offset += bytes_per_line) {
		os << std::setw(8) << std::setfill('0') << std::hex << offset << ": ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			// missing bytes will be replaced with whitespace
			if (offset + i < data.size())
				os << std::setw(2) << std::setfill('0') << std::hex << static_cast<i32>(data[offset + i]) << ' ';
			else
				os << "   ";
		}
		os << " | ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			if (offset + i >= data.size())
				break;

			// non-printable characters will be replaced with '.'
			char c = data[offset + i];
			if (c < 32 || c > 126)
				c = '.';
			os << c;
		}
		os << "\n";
	}
	// reset to default
	os << std::setfill(os.widen(' '));
	os.copyfmt(old_state);
}


} // namespace ncr

#endif /* _65fc1481d8d149029547d3932c93f2e0_ */

/*
 * ncr/npyerror.hpp - return codes used throughout ncr::numpy
 *
 *
 */

#ifndef _8c9e4fd8e3de4665b327b3e0a6481c9f_
#define _8c9e4fd8e3de4665b327b3e0a6481c9f_


namespace ncr { namespace numpy {

#define NCR_NUMPY_ERROR_CODE_LIST(_)                                          \
	_(ok                                     , 0)                             \
	/* warnings about missing fields. Note that not all fields are required   \
	 * and it might not be a problem for an application if they are not       \
	 * present. However, inform the user about this state */                  \
	_(warning_missing_descr                  , 1ul << 0)                      \
	_(warning_missing_fortran_order          , 1ul << 1)                      \
	_(warning_missing_shape                  , 1ul << 2)                      \
	/* error codes. in particular for nested/structured arrays, it might be   \
	 * helpful to know precisely what went wrong. */                          \
	_(error_wrong_filetype                   , 1ul << 3)                      \
	_(error_file_not_found                   , 1ul << 4)                      \
	_(error_file_exists                      , 1ul << 5)                      \
	_(error_file_open_failed                 , 1ul << 6)                      \
	_(error_file_truncated                   , 1ul << 7)                      \
	_(error_file_write_failed                , 1ul << 8)                      \
	_(error_file_read_failed                 , 1ul << 9)                      \
	_(error_file_close                       , 1ul << 10)                     \
	_(error_unsupported_file_format          , 1ul << 11)                     \
	_(error_duplicate_array_name             , 1ul << 12)                     \
	/* */                                                                     \
	_(error_magic_string_invalid             , 1ul << 13)                     \
	_(error_version_not_supported            , 1ul << 14)                     \
	_(error_header_invalid_length            , 1ul << 15)                     \
	_(error_header_truncated                 , 1ul << 16)                     \
	_(error_header_parsing_error             , 1ul << 17)                     \
	_(error_header_invalid                   , 1ul << 18)                     \
	_(error_header_empty                     , 1ul << 19)                     \
	/* */                                                                     \
	_(error_descr_invalid                    , 1ul << 20)                     \
	_(error_descr_invalid_type               , 1ul << 21)                     \
	_(error_descr_invalid_string             , 1ul << 22)                     \
	_(error_descr_invalid_data_size          , 1ul << 23)                     \
	_(error_descr_list_empty                 , 1ul << 24)                     \
	_(error_descr_list_invalid_type          , 1ul << 25)                     \
	_(error_descr_list_incomplete_value      , 1ul << 26)                     \
	_(error_descr_list_invalid_value         , 1ul << 27)                     \
	_(error_descr_list_invalid_shape         , 1ul << 28)                     \
	_(error_descr_list_invalid_shape_value   , 1ul << 29)                     \
	_(error_descr_list_subtype_not_supported , 1ul << 30)                     \
	/* */                                                                     \
	_(error_fortran_order_invalid_value      , 1ul << 31)                     \
	_(error_shape_invalid_value              , 1ul << 32)                     \
	_(error_shape_invalid_shape_value        , 1ul << 33)                     \
	_(error_item_size_mismatch               , 1ul << 34)                     \
	_(error_data_size_mismatch               , 1ul << 35)                     \
	_(error_unavailable                      , 1ul << 36)                     \
    /* */                                                                     \
	_(error_mmap_failed                      , 1ul << 37)                     \
	_(error_seek_failed                      , 1ul << 38)                     \
	_(error_reader_not_open                  , 1ul << 39)                     \
	_(error_invalid_item_offset              , 1ul << 40)                     \
	_(error_invalid_data_pointer             , 1ul << 41)                     \
	_(error_munmap_failed                    , 1ul << 42)                     \

#define NCR_NUMPY_ERROR_CODE_ENUM_ENTRY(NAME, VALUE) \
	NAME = VALUE,

#define NCR_NUMPY_ERROR_CODE_STRINGIFY(NAME, VALUE) \
	{result::NAME, #NAME},

// need to bring enum_count into this namespace for the MACRO to work (TODO:
// fix this)
template <typename T> constexpr size_t enum_count();
NCR_ENUM_CLASS(result, u64, NCR_NUMPY_ERROR_CODE_LIST(NCR_NUMPY_ERROR_CODE_ENUM_ENTRY))

NCR_DEFINE_ENUM_FLAG_OPERATORS(result);

// map from error code to string for pretty printing the error code. This is a
// bit more involved than just listing the strings, because result codes can be
// OR-ed together, i.e. a result code might have several codes that are set.
constexpr inline std::array<std::pair<result, const char*>, enum_count<result>()>
result_strings = {{
	NCR_NUMPY_ERROR_CODE_LIST(NCR_NUMPY_ERROR_CODE_STRINGIFY)
}};


inline bool
is_error(result r)
{
	return
		r != result::ok &&
		r != result::warning_missing_descr &&
		r != result::warning_missing_shape &&
		r != result::warning_missing_fortran_order;
}


struct ErrorContext
{
	result      res;
	const char *failed_function;
};


}} // ncr::numpy::

#endif /* _8c9e4fd8e3de4665b327b3e0a6481c9f_ */

/*
 * ncr/npybuffers.hpp - some buffer backends for npyfile and ndarray
 *
 *
 */


#ifndef _69a274a94acf465aaa21a9e5046fa6ed_
#define _69a274a94acf465aaa21a9e5046fa6ed_


// mmap



namespace ncr { namespace numpy {


/*
 * mmap'ed file based buffer
 *
 * TODO: also store the offset (in bytes) for the actual data
 */
struct mmap_buffer
{
	uint8_t* data        = nullptr;
	size_t   size        = 0;
	size_t   position    = 0;
	size_t   data_offset = 0;
};


inline result
open(const char *filepath, mmap_buffer* buf)
{
	if (!buf)
		return result::error_invalid_data_pointer;

	int fd = ::open(filepath, O_RDONLY);
	if (fd == -1) {
		return result::error_file_open_failed;
	}

	buf->size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	buf->data = (uint8_t*)mmap(NULL, buf->size, PROT_READ, MAP_PRIVATE, fd, 0);
	::close(fd);
	if (buf->data == MAP_FAILED) {
		buf->size = 0;
		buf->data = nullptr;
		return result::error_mmap_failed;
	}
	buf->position = 0;
	return result::ok;
}


inline result
close(mmap_buffer* buf)
{
	if (!buf)
		return result::error_invalid_data_pointer;

	if (munmap(buf->data, buf->size) == -1)
		return result::error_munmap_failed;

	buf->size = 0;
	buf->data = nullptr;
	buf->position = 0;
	return result::ok;
}


inline void
release(mmap_buffer* buf)
{
	if (!buf)
		return;

	close(buf);
	delete buf;
}



/*
 * raw array based buffer
 */
struct raw_buffer
{
	uint8_t* data;
	size_t   size;
};


inline raw_buffer*
make_raw_buffer(size_t N)
{
	auto *buffer = new raw_buffer();
	buffer->data = new uint8_t[N]{};
	buffer->size = N;
	return buffer;
}


inline result
release(raw_buffer* buf)
{
	if (!buf)
		return result::error_invalid_data_pointer;

	delete[] buf->data;
	delete buf;
	return result::ok;
}


/*
 * vector based buffer
 */
struct vector_buffer
{
	std::vector<uint8_t> data;
};


inline vector_buffer*
make_vector_buffer(size_t N)
{
	auto* buf = new vector_buffer();
	buf->data.resize(N);
	return buf;
}

inline vector_buffer*
make_vector_buffer(u8_vector&& other)
{
	auto* buf = new vector_buffer();
	buf->data = std::move(other);
	return buf;
}



inline result
release(vector_buffer* buf)
{
	if (!buf)
		return result::error_invalid_data_pointer;

	delete buf;
	return result::ok;
}



/*
 * npybuffer - simple frontend for different buffers
 *
 * In case of a regular file I/O or no file I/O at all, an ndarray will simply
 * store its data within a vector. however, as soon as a file is opened in mmap
 * mode, we want the actual data to be accessed via the memory mapped file. to
 * make this flexibly possible within ndarray, internally it only sets the data
 * pointer corresponding to what the data source actually is (i.e. a vector,
 * mmap, etc.). all of this could be implanted directly in ndarray, but there's
 * almost zero cost to separate npybuffer from ndarray and keep the interface
 * small and tidy.
 *
 * Why not std::variant? because the types are *very* simple, and there's no
 * need for type checking beyond testing the enum flag in any part of the code.
 * Also, this provides a few custom functions
 *
 * TODO: maybe rename
 * TODO: maybe provide constructors to move stuff in
 */
struct npybuffer
{
	enum class type : uint8_t {
		raw,
		vector,
		mmap
	};

	// tagged union
	type type;
	union {
		raw_buffer*    raw;
		vector_buffer* vector;
		mmap_buffer*   mmap;
	};

	npybuffer(enum type _t) : type(_t) {}

	// pointer to the actual array data
	inline u8*
	get_data_ptr()
	{
		switch (type) {
		case type::raw:
			return raw->data;
		case type::vector:
			return vector->data.data();
		case type::mmap:
			return mmap->data + mmap->data_offset;
		}
		return nullptr;
	}

	// pointer to the actual data. this is the same as with get_data_ptr except
	// for mmap buffers
	inline u8*
	get_raw_data_ptr()
	{
		switch (type) {
		case type::raw:
			return raw->data;
		case type::vector:
			return vector->data.data();
		case type::mmap:
			return mmap->data;
		}
		return nullptr;
	}

	size_t
	get_data_size()
	{
		switch (type) {
		case type::raw:
			return raw->size;
		case type::vector:
			return vector->data.size();
		case type::mmap:
			return mmap->size;
		}
		return 0;
	}

	void
	release() {
		switch (type) {
		case type::raw:
			ncr::numpy::release(raw);
			break;
		case type::vector:
			ncr::numpy::release(vector);
			break;
		case type::mmap:
			ncr::numpy::release(mmap);
			break;
		}
	}
};



}} // ncr::numpy

#endif /* _69a274a94acf465aaa21a9e5046fa6ed_ */

/*
 * ndarray.hpp - n-dimensional array implementation
 *
 *
 * While the ndarray is tightly integrated with ncr_numpy, it is not within the
 * ncr::numpy namespace. The reason is that the ndarray implementation here is
 * generic enough that it can be used outside of numpy and the purpose of the
 * numpy namespace.
 *
 * ndarray is not a template. This was a design decision to avoid template creep
 * in ncr::numpy. That is, operator() does not return an explicit type such das
 * f32, because there is no such thing as ndarray<f32>. In contrast, operator()
 * returns an ndarray_item, which itself only encapsulates an u8 subrange. To
 * avoid the intermediary ndarray_item, use the ndarray's value() function.
 *
 *
 * TODO: determine if size-matching needs to be equal instead of smaller-equal
 *       (see e.g. ndarray.value)
 * TODO: combine ndarray_item with ndarray_t::proxy if possible (and sensible)
 * TODO: broadcasting / ellipsis
 */
#ifndef _719685da6c474222b60a9d28795719db_
#define _719685da6c474222b60a9d28795719db_




namespace ncr { namespace numpy {



/*
 * forward declarations
 */
struct ndarray_item;
struct ndarray;


// using u8_span = std::span<const u8>;


/*
 * ndarray_item - items returned from ndarray's operator()
 *
 * This is an indirection to make the syntax to convert values slightly more
 * elegant. Instead of this indirection, it is also possible to implement range
 * views on top of the data ranges returned from ndarray's get function, or to
 * use ndarray's .value() function.
 *
 * The indirection allows to write the following code:
 *		array(row, col) = 123.f;
 *	and
 *		f32 f = array(row, col).as<float>();
 *
 * While the second example is only marginally shorter than
 *		f32 f = array.value<float>(row, col);
 * the first clearly is.
 *
 * However, beware of temporaries!
 */
struct ndarray_item
{
	ndarray_item() = delete;

	explicit
	ndarray_item(u8* data, size_t size, struct dtype &_dt)
		: _data(data)
		, _size(size)
		, _dtype(_dt)
		{}

	explicit
	ndarray_item(u8_span&& span, struct dtype& _dt)
		: _data(span.data())
		, _size(span.size())
		, _dtype(_dt)
		{}


	template <typename T>
	T
	as() const {
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_size != sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) mismatch with item size (" << _size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T val;
		std::memcpy(&val, _data, sizeof(T));
		return val;
	}


	template <typename T>
	void
	operator=(T value)
	{
		if (_size != sizeof(T)) {
			std::ostringstream s;
			s << "Value size (" << sizeof(T) << " bytes) mismatch with item size (" << _size << " bytes)";
			throw std::length_error(s.str());
		}
		std::memcpy(_data, &value, sizeof(T));
	}


	inline
	u8_const_span
	span() const {
		return u8_const_span(_data, _size);
	}


	inline
	const u8*
	data() const {
		return _data;
	}


	inline
	size_t
	bytesize() const {
		return _size;
	}


	inline
	const struct dtype&
	dtype() const {
		return _dtype;
	}


	template <typename T, typename... Args>
	static
	const T
	field(const ndarray_item &item, Args&&... args);


	template<typename T, typename... Args>
	const T
	get_field(Args&&... args) const {
		return field<T>(*this, std::forward<Args>(args)...);
	}


private:
	// the data subrange within the ndarray
	u8*
		_data;

	// the size of the subrange. this might be required frequently, so we store
	// it once
	const size_t
		_size;

	// the data type of the item (equal to the data type of its ndarray)
	const struct dtype &
		_dtype;
};


template <typename T, typename = void>
struct field_extractor
{
	static const T
	get_field(const ndarray_item &item, const dtype& dt)
	{
		auto range_size = item.bytesize();
		if ((dt.offset + sizeof(T)) > range_size) {
			std::ostringstream s;
			s << "Target type size (" << sizeof(T) << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, item.data() + dt.offset, sizeof(T));
		return value;
	}
};


template <typename T>
struct field_extractor<T, std::enable_if_t<is_ucs4string<T>::value>>
{
	static const T
	get_field(const ndarray_item &item, const dtype& dt)
	{
		constexpr auto N = ucs4string_size<T>::value;
		constexpr auto B = ucs4string_bytesize<T>::value;
		auto range_size = item.bytesize();
		if ((dt.offset + B) > range_size) {
			std::ostringstream s;
			s << "Target string size (" << B << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		std::array<u32, N> arr;
		std::memcpy(arr.data(), item.data() + dt.offset, sizeof(arr));
		return to_ucs4<N>(arr);
	}
};


template <typename T, typename... Args>
const T
ndarray_item::field(const ndarray_item &item, Args&&... args)
{
	const struct dtype *dt = find_field_recursive(item.dtype(), args...);
	if (!dt)
		throw std::runtime_error("Field not found: " + (... + ('/' + std::string(args))));
	return field_extractor<T>::get_field(item, *dt);
}


/*
 * ndarray - basic ndarray without a lot of functionality
 *
 * TODO: documentation
 */
struct ndarray
{
	enum class result {
		ok,
		value_error
	};

	ndarray() {}

	// non-copyable
	ndarray(const ndarray&) = delete;
	ndarray& operator=(const ndarray&) = delete;

	// move constructor
	ndarray(ndarray&& other) noexcept
	: _dtype(other._dtype)
	, _shape(other._shape)
	, _size(other._size)
	, _order(other._order)
	, _strides(other._strides)
	, _data_ptr(other._data_ptr)
	, _data_size(other._data_size)
	, _buffer(other._buffer)
	{
		// make sure other._bufptr is not pointing anywhere, or we'll end up
		// releasing _bufptr too often
		other._buffer = nullptr;
	}

	// move assignment
	ndarray& operator=(ndarray&& other) noexcept
	{
		_dtype     = other._dtype;
		_shape     = other._shape;
		_size      = other._size;
		_order     = other._order;
		_strides   = other._strides;
		_data_ptr  = other._data_ptr;
		_data_size = other._data_size;
		_buffer    = other._buffer;

		// make sure other._bufptr is not pointing anywhere, or we'll end up
		// releasing _bufptr too often
		other._buffer = nullptr;

		return *this;
	}

	// TODO: copy, which needs to explicitly be called and copy resources

	// TODO: default data type
	ndarray(std::initializer_list<u64> shape,
	        struct dtype dt = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dt), _shape{shape}, _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	// TODO: default data type
	ndarray(u64_vector shape,
	        struct dtype dt = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dt), _shape(shape), _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	ndarray(struct dtype &&dt,
	        u64_vector &&shape,
	        u8_vector &&buffer,
	        storage_order o = storage_order::row_major)
	: _dtype(std::move(dt)) , _shape(std::move(shape)) , _order(o)
	{
		_from_vector_rvalue(std::move(buffer));
		_compute_size();
		_compute_strides();
	}


	/*
	 * assign - assign new data to this array
	 *
	 * Note that this will clear all existing data beforehand
	 */
	void
	assign(dtype &&dt,
	       u64_vector &&shape,
	       npybuffer *buffer,
	       storage_order o = storage_order::row_major)
	{
		// tidy up first
		_shape.clear();
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// assign values
		_dtype = std::move(dt);
		_shape = std::move(shape);
		_order = o;
		_from_npybuffer(buffer);

		// recompute size and strides
		_compute_size();
		_compute_strides();
	}


	/*
	 * unravel - unravel a given index for this particular array
	 */
	template <typename T = size_t>
	u64_vector
	unravel(size_t index)
	{
		return unravel_index<u64>(index, _shape, _order);
	}


	/*
	 * get - get the u8 span in the data buffer for an element
	 */
	template <typename ...Indexes>
	u8_span
	get(Indexes... index)
	{
		// Number of indices must match number of dimensions
		assert(_shape.size() == sizeof...(Indexes));

		// test if indexes are out of bounds. we don't handle negative indexes
		if (sizeof...(Indexes) > 0) {
			{
				size_t i = 0;
				bool valid_index = ((index >= 0 && (size_t)index < _shape[i++]) && ...);
				if (!valid_index)
					throw std::out_of_range("Index out of bounds\n");
			}

			// this ravels the index, i.e. turns it into a flat index. note that
			// in contrast to numpy.ndarray.strides, _strides contains only
			// number of elements, not bytes. the bytes will be multiplied in
			// below when extracting u8_subrange
			size_t i = 0;
			size_t offset = 0;
			((offset += index * _strides[i], i++), ...);

			return u8_span(_data_ptr + _dtype.item_size * offset, _dtype.item_size);
		}
		else
			// TODO: evaluate if this is the correct response here
			return u8_span();
	}


	/*
	 * get - get the u8 subrange in the data buffer for an element
	 */
	inline u8_span
	get(u64_vector indexes)
	{
		// TODO: don't assert, throw exception
		assert(indexes.size() == _shape.size());
		if (indexes.size() > 0) {
			size_t offset = 0;
			for (size_t i = 0; i < indexes.size(); i++) {
				if (indexes[i] >= _shape[i])
					throw std::out_of_range("Index out of bounds\n");

				// update offset
				offset += indexes[i] * _strides[i];
			}
			return u8_span(_data_ptr + _dtype.item_size * offset, _dtype.item_size);
		}
		else
			// TODO: like above, evaluate if this is the correct response
			return u8_span();
	}


	/*
	 * operator() - convenience function to avoid template creep in ncr::numpy
	 *
	 * when only reading values, use .value() instead to avoid an intermediary
	 * ndarray_item. when writing values
	 */
	template <typename... Indexes>
	inline ndarray_item
	operator()(Indexes... index)
	{
		return ndarray_item(this->get(index...), _dtype);
	}


	/*
	 * operator() - convenience function to avoid template creep in ncr::numpy
	 *
	 * this function accepts a vector of indexes to access an array element at
	 * the specified location
	 */
	inline ndarray_item
	operator()(u64_vector indexes)
	{
		return ndarray_item(this->get(indexes), _dtype);
	}


	/*
	 * value - access the value at a given index
	 *
	 * This function returns a reference, which makes it possible to change the
	 * value.
	 */
	template <typename T, typename... Indexes>
	inline T
	value(Indexes... index)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, this->get(index...).data(), sizeof(T));
		return value;
	}


	/*
	 * value - access the value at a given index
	 *
	 * Given a vector of indexes, this function returns a reference to the value
	 * at this index. This makes it possible to change the value within the
	 * array's data buffer.
	 *
	 * Note: This function throws if the size of T is larger than elements
	 *       stored in the array, of if the indexes are out of bounds.
	 */
	template <typename T>
	inline T
	value(u64_vector indexes)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, this->get(indexes).data(), sizeof(T));
		return value;
	}


	/*
	 * transform - transform a value
	 *
	 * This is useful, for instance, when the data stored in the array is not in
	 * the same storage_order as the system that is using the data.
	 */
	template <typename T, typename Func = std::function<T (T)>, typename... Indexes>
	inline T
	transform(Func func, Indexes... index)
	{
		T val = value<T>(index...);
		return func(val);
	}


	/*
	 * apply - apply a function to each value in the array
	 *
	 * This function applies a user-specified function to each element of the
	 * array. The user-specified function will receive a constant subrange of u8
	 * containing the array element, and is expected to return a vector
	 * containing u8 of the same size as the range. If there is a size-mismatch,
	 * this function will throw an std::length_error.
	 *
	 * TODO: provide an apply function which also passes the element index back
	 * to the transformation function
	 */
	template <typename Func = std::function<u8_vector (u8_span)>>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = _dtype.item_size;
		while (offset < _data_size) {
			auto span = u8_span(_data_ptr + offset, stride);
			auto new_value = func(span);
			if (new_value.size() != span.size())
				throw std::length_error("Invalid size of result");
			std::copy(new_value.begin(), new_value.end(), _data_ptr + offset);
			offset += stride;
		}
	}


	/*
	 * apply - apply a function to each value in the array given a type T
	 *
	 * Note: no size checking is performed. As a consequence, it is possible to
	 * call transform<i32>(...) on an array that stores values of types with
	 * different size, e.g. i16 or i64.
	 */
	template <typename T, typename Func = std::function<T (T)>>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = sizeof(T);
		while (offset < _data_size) {
			T tmp;
			std::memcpy(&tmp, _data_ptr + offset, sizeof(T));
			tmp = func(tmp);
			std::memcpy(_data_ptr + offset, &tmp, sizeof(T));
			offset += stride;
		}
	}


	/*
	 * map - call a function for each element
	 *
	 * map calls a function for each item of the array, passing the item to the
	 * function that is given to map. The provided function will also receive
	 * the flat-index of the item, which can be used on the caller-side to get
	 * the multi-index (via ndarray::unravel).
	 */
	template <typename Func = std::function<void (const ndarray_item&, size_t)>>
	inline void
	map(Func func)
	{
		for (size_t i = 0; i < _size; i++) {
			func(ndarray_item(
					_data_ptr + _dtype.item_size * i,
					_dtype.item_size,
					_dtype), i);
		}
	}


	template <typename T>
	T
	max()
	{
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}

		auto stride = sizeof(T);
		auto nelems = _data_size / stride;

		T _max;
		std::memcpy(&_max, &_data_ptr[0], sizeof(T));
		for (size_t i = 1; i < nelems; i++) {
			T val;
			std::memcpy(&val, &_data_ptr[i * stride], sizeof(T));
			if (val > _max)
				_max = val;
		}
		return _max;
	}


	// TODO: provide variant with vector argument
	template <typename... Lengths>
	result
	reshape(Lengths... length)
	{
		size_t n_elems = (length * ...);
		if (n_elems != _size)
			return result::value_error;

		// set the shape
		_shape.resize(sizeof...(Lengths));
		size_t i = 0;
		((_shape[i++] = length), ...);

		// re-compute strides
		_compute_strides();
		return result::ok;
	}


	inline std::string
	get_type_description() const
	{
		std::ostringstream s;
		s << "{";
		serialize_dtype_descr(s, dtype());
		s << ", ";
		serialize_fortran_order(s, _order);
		if (_shape.size() > 0) {
			s << ", 'shape': ";
			serialize_shape(s, _shape);
		}
		s << ", ";
		// TODO: optional fields of the array interface
		s << "}";
		return s.str();
	}

	void
	release()
	{
		_release_buffer();
	}

	//
	// property getters
	//
	const struct dtype& dtype()    const { return _dtype;     }
	storage_order       order()    const { return _order;     }
	const u64_vector&   shape()    const { return _shape;     }
	const u8*           data()     const { return _data_ptr;  }
	size_t              size()     const { return _size;      }
	size_t              bytesize() const { return _data_size; }

private:
	// _data stores the type information of the array
	struct dtype
		_dtype;

	// _shape contains the shape of the array, meaning the size of each
	// dimension. Example: a shape of [2,3] would mean an array of size 2x3,
	// i.e. with 2 rows and 3 columns.
	u64_vector
		_shape;

	// _size contains the number of elements in the array
	size_t
		_size  = 0;

	// storage order used in this array. by default this corresponds to
	// row_major (or 'C' order). Alternatively, this could be col_major (or
	// 'Fortran' order).
	storage_order
		_order = storage_order::row_major;

	// _strides is the tuple (or vector) of elements in each dimension when
	// traversing the array. Note that this differs from numpy's ndarray.strides
	// in that _strides contains number of elements and *not* number of bytes.
	// the bytes depend on _dtype.item_size, and will be usually multiplied in
	// only after the number of elements to skip are determined. See get<> for
	// an example
	u64_vector
		_strides;

	u8*
		_data_ptr = nullptr;

	size_t
		_data_size = 0;

	npybuffer*
		_buffer = nullptr;


	/*
	 * _compute_strides - compute the strides for this particular ndarray
	 */
	void
	_compute_strides()
	{
		compute_strides(_shape, _strides, _order);
	}


	/*
	 * _compute_size - compute the number of elements in the array
	 */
	void
	_compute_size()
	{
		// TODO: verify that dtype.item_size and computed _size match
		if (_shape.size() > 0) {
			auto prod = 1;
			for (auto &s: _shape)
				prod *= s;
			_size = prod;
		}
		else {
			if (_dtype.item_size > 0) {
				// infer from data and itemsize if possible
				_size = _data_size / _dtype.item_size;
				// set shape to 1-dimensional of _size count
				if (_size > 0) {
					_shape.clear();
					_shape.push_back(_size);
				}
			}
			else {
				// can't do anything with dtype.item_size 0
				_size = 0;
			}
		}
	}


	// _resize - resize _data for _size many items
	//
	// Note that this should only be called in the constructor after setting
	// _dtype and _shape and after a call of _compute_size
	void
	_resize()
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		if (!_size)
			return;
		_alloc_buffer(_size * _dtype.item_size);
	}


	void
	_from_npybuffer(npybuffer *buffer)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		_buffer = buffer;
		if (_buffer) {
			_data_ptr  = _buffer->get_data_ptr();
			_data_size = _buffer->get_data_size();
		}
	}


	void
	_from_vector_rvalue(u8_vector&& vec)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// move the vec into a new npybuffer of suitable type
		_buffer = new npybuffer(npybuffer::type::vector);
		_buffer->vector = make_vector_buffer(std::move(vec));
		_data_ptr  = _buffer->get_data_ptr();
		_data_size = _buffer->get_data_size();
	}


	void
	_alloc_buffer(size_t N)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// by default, we allocate a vector buffer
		_buffer = new npybuffer(npybuffer::type::vector);
		_buffer->vector = make_vector_buffer(N);
		_data_ptr  = _buffer->get_data_ptr();
		_data_size = _buffer->get_data_size();
	}


	void
	_release_buffer()
	{
		if (_buffer) {
			_buffer->release();
			delete _buffer;
			_buffer = nullptr;
		}
		_data_size = 0;
		_data_ptr = nullptr;
	}
};


inline void
release(ndarray &arr)
{
	arr.release();
}


inline bool
is_structured_array(const ndarray &arr)
{
	return !arr.dtype().fields.empty();
}


/*
 * ndarray_t - simple typed facade for ndarray
 *
 * This template wraps an ndarray and provides new operator() which return
 * direct references to the underlying data. These facades are helpful when the
 * data type of an ndarray is properly known in advance and can be easily
 * converted to T. This is the case for all basic types.
 */
template <typename T>
struct ndarray_t : ndarray
{
	struct proxy {
		ndarray_t<T>& array;
		std::vector<size_t> indices;

		proxy(ndarray_t<T>& arr, std::vector<size_t> idx) : array(arr), indices(idx) {}

		// assignment operator so that an item can be used as lvalue
		proxy& operator=(const T& value)
		{
			auto range = array.get(indices);
			std::memcpy(range.data(), &value, sizeof(T));
			return *this;
		}

		// conversion operator to type T to allow using an item in an expression
		operator T() const
		{
			T val;
			auto range = array.get(indices);
			std::memcpy(&val, range.data(), sizeof(T));
			return val;
		}
	};

	// default constructor (without arguments), select dtype based on T
	ndarray_t()
	: ndarray(u64_vector{}, dtype_selector<T>::get(), storage_order::row_major) {}

	// constructor for shape and storage order, setting dtype based on T
	template <typename... Shape, typename = std::enable_if_t<sizeof...(Shape) != 0>>
	ndarray_t(Shape... shape, storage_order so = storage_order::row_major)
	: ndarray({static_cast<u64>(shape)...}, dtype_selector<T>::get(), so) {}

	// constructor for shape as an initializer list and storage order, setting dtype based on T
	ndarray_t(std::initializer_list<u64> shape, storage_order so = storage_order::row_major)
	: ndarray(shape, dtype_selector<T>::get(), so) {}

	// constructor for shape vector and storage order, setting dtype based on T
	ndarray_t(u64_vector shape, storage_order so = storage_order::row_major)
	: ndarray(shape, dtype_selector<T>::get(), so) {}

	// constructor for pre-allocated buffer and storage order, setting dtype based on T
	ndarray_t(u64_vector shape, u8_vector buffer, storage_order so = storage_order::row_major)
	: ndarray(dtype_selector<T>::get(), std::move(shape), std::move(buffer), so) {}

	template <typename... Indexes>
	inline proxy
	operator()(Indexes... index)
	{
		return proxy(*this, {static_cast<size_t>(index)...});
	}

	inline proxy
	operator()(u64_vector indexes)
	{
		return proxy(*this, indexes);
	}
};

template <typename T>
inline void
release(ndarray_t<T> &arr)
{
	arr.release();
}


/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T, typename Func = std::function<T (T)>>
void
print_tensor(std::ostream &os, ndarray &arr, std::string indent, u64_vector &indexes, size_t dim, Func transform)
{
	auto shape = arr.shape();
	auto len   = shape.size();

	if (len == 0) {
		os << "[]";
		return;
	}

	if (dim == len - 1) {
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			if (i > 0)
				os << ", ";
			os << std::setw(2) << transform(arr.value<T>(indexes));
		}
		os << "]";
	}
	else {
		if (dim == 0)
			os << indent;
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			// indent
			if (i > 0)
				os << indent << std::setw(dim+1) << "";
			print_tensor<T>(os, arr, indent, indexes, dim+1, transform);
			if (shape[dim] > 1) {
				if (i < shape[dim] - 1)
					os << ",\n";
			}
		}
		os << "]";
	}
}


/*
 * print_tensor - print an ndarray to an ostream
 */
template <typename T, typename Func = std::function<T (T)>>
void
print_tensor(ndarray &arr, std::string indent="", Func transform = [](T v){ return v; }, std::ostream &os = std::cout)
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor<T>(os, arr, indent, indexes, 0, transform);
}


/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T>
void
print_tensor(std::ostream &os, ndarray_t<T> &arr, std::string indent, u64_vector &indexes, size_t dim)
{
	auto shape = arr.shape();
	auto len   = shape.size();

	if (len == 0) {
		os << "[]";
		return;
	}

	if (dim == len - 1) {
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			if (i > 0)
				os << ", ";
			os << std::setw(2) << arr(indexes);
		}
		os << "]";
	}
	else {
		if (dim == 0)
			os << indent;
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			// indent
			if (i > 0)
				os << indent << std::setw(dim+1) << "";
			print_tensor(os, arr, indent, indexes, dim+1);
			if (shape[dim] > 1) {
				if (i < shape[dim] - 1)
					os << ",\n";
			}
		}
		os << "]";
	}
}


/*
 * print_tensor - print an ndarray to an ostream
 */
template <typename T>
void print_tensor(ndarray_t<T> &arr, std::string indent="")
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor(std::cout, arr, indent, indexes, 0);
}



}} // ncr


#endif /* _719685da6c474222b60a9d28795719db_ */

/*
 * string_conversions.hpp - collection of functions to convert types to strings
 *
 */

#ifndef _f53b5d05a7dd47668fbad51a033a87b7_
#define _f53b5d05a7dd47668fbad51a033a87b7_


namespace ncr {


struct strfmtopts {
	const char *sep = ", ";
	const char *beg = "[";
	const char *end = "]";
};


template <typename Iterator, typename Sentinel>
std::string
to_string(Iterator iter, Sentinel sentinel, const strfmtopts& opts = {})
{
	using ValueType = typename std::iterator_traits<Iterator>::value_type;

	std::ostringstream oss;
	oss << opts.beg;
	bool first = true;
	for (Iterator it = iter; it != sentinel; ++it) {
		if (!first)
			oss << opts.sep;

		if constexpr (std::is_same_v<ValueType, uint8_t>)
			oss << static_cast<int>(*it);
		else
			oss << *it;

		first = false;
	}
	oss << opts.end;
	return oss.str();
}

template <typename T>
std::string to_string(const std::vector<T>& vec, const strfmtopts& opts = {})
{
	return to_string(vec.begin(), vec.end(), opts);
}

template <typename T, std::size_t N>
std::string to_string(const std::array<T, N>& arr, const strfmtopts& opts = {})
{
	return to_string(arr.begin(), arr.end(), opts);
}

template <typename T>
std::string to_string(const std::span<T>& span,  const strfmtopts& opts = {})
{
	return to_string(span.begin(), span.end(), opts);
}

// Optionally, overload for C-style arrays
template <typename T, std::size_t N>
std::string to_string(const T (&arr)[N], const strfmtopts& opts = {})
{
	return to_string(std::begin(arr), std::end(arr), opts);
}


} // ncr::

#endif /* _f53b5d05a7dd47668fbad51a033a87b7_ */

/*
 * pyparser - a simple parser for python data
 *
 */


#ifndef _f03a19a69cac46f38404d117df9d9c37_
#define _f03a19a69cac46f38404d117df9d9c37_



namespace ncr { namespace numpy {


inline bool
equals(const u8_const_span &subspan, const std::string_view &str)
{
	return (subspan.size() == str.size()) &&
	       std::equal(subspan.begin(), subspan.end(), str.begin());
}


inline bool
equals(u8_const_iterator first, u8_const_iterator last, const std::string_view &str)
{
	return equals(u8_const_span(&*first, std::distance(first, last)), str);
}

inline bool
equals(u8* first, u8* last, const std::string_view &str)
{
	return equals(u8_const_span(&*first, std::distance(first, last)), str);
}

inline bool
equals(const u8* first, const u8* last, const std::string_view &str)
{
	return equals(u8_const_span(&*first, std::distance(first, last)), str);
}

// basic types
enum class TokenType : u8 {
	Unknown,
	// punctuations / separators
	// Dot,            // XXX: currently not handled explicitly
	// Ellipsis,       // XXX: currently not handled explicitly
	LeftBrace,        // { begin set | dict
	RightBrace,       // } end set | dict
	LeftBracket,      // [ begin list
	RightBracket,     // ] end list
	LeftParen,        // ( begin tuple
	RightParen,       // ) end tuple
	ValueSeparator,   // ,
	KVSeparator,      // : between key and value pairs
	// literals of known type
	StringLiteral,    // a string ...
	IntegerLiteral,   // an integer number
	FloatLiteral,     // a floating point number
	BoolLiteral,      // True or False
	NoneLiteral,      // None
	// others
	// Identifier,     // XXX: currently not supported
	// Keyword,        // XXX: currently not supported
	// Operator,       // XXX: currently not supported
};


/*
 * mapping of a punctuation symbol to its type
 */
struct Punctuation {
	const u8 sym          = 0;
	const TokenType ttype = TokenType::Unknown;
};


/*
 * list of all punctuations
 */
inline constexpr Punctuation punctuations[] = {
	// TODO: dot and ellipsis
	{'{', TokenType::LeftBrace},
	{'}', TokenType::RightBrace},
	{'[', TokenType::LeftBracket},
	{']', TokenType::RightBracket},
	{'(', TokenType::LeftParen},
	{')', TokenType::RightParen},
	{':', TokenType::KVSeparator},
	{',', TokenType::ValueSeparator},
};


/*
 * list of all literals
 */
inline constexpr TokenType literals[] = {
	TokenType::StringLiteral,
	TokenType::IntegerLiteral,
	TokenType::FloatLiteral,
	TokenType::BoolLiteral,
	TokenType::NoneLiteral,
};


/*
 * is_literal - evaluate if the token type is literal
 */
inline constexpr bool
is_literal(const TokenType &t) {
	for (auto &l: literals)
		if (l == t) return true;
	return false;
}


// determine the punctuation type of the symbol under the cursor
inline bool
get_punctuation_type(u8 sym, TokenType &t)
{
	for (auto &p: punctuations)
		if (p.sym == sym) {
			t = p.ttype;
			return true;
		}
	return false;
}


// determine if a string is an integer number or not.
// TODO: maybe adapt std::from_chars, as this might circumvent using an
// std::string, or maybe parse the numbers manually.
// TODO: also allow users to have the ability to use
// boost::lexical_cast. But I don't see why we should pull in anything
// from boost for one or two lines of code.
inline bool
is_integer_literal(std::string str, u64 &value)
{
	char *end;
	value = std::strtol(str.c_str(), &end, 10);
	return *end == '\0';
}


// determine if a string is a floating point number or not.
// TODO: maybe adapt std::from_chars, as this might circumvent using an
// std::string, or maybe parse the numbers manually
// TODO: also allow users to have the ability to use
// boost::lexical_cast. But I don't see why we should pull in anything
// from boost for one or two lines of code.
inline bool
is_float_literal(std::string str, double &value)
{
	// TODO: maybe adapt std::from_chars, as this might circumvent using an
	// std::string. However, from_chars for float will be only in C++23
	char *end;
	value = std::strtod(str.c_str(), &end);
	return *end == '\0';
}


// determine if the range given by [first,last) is a literal true or literal
// false
inline bool
is_bool_literal(u8_const_iterator first, u8_const_iterator last, bool &value)
{
	if (equals(first, last, "False")) { value = false; return true; }
	if (equals(first, last, "True"))  { value = true;  return true; }
	return false;
}

inline bool
is_bool_literal(const u8* first , const u8* last, bool &value)
{
	auto subspan = u8_const_span(first, std::distance(first, last));

	if (equals(subspan, "False")) { value = false; return true; }
	if (equals(subspan, "True"))  { value = true;  return true; }
	return false;
}


inline bool
is_whitespace(u8 sym)
{
	return sym == ' ' || sym == '\n' || sym == '\t';
}



/*
 * token - representes a token read from an input vector
 */
struct Token
{
	// the type of this token
	TokenType ttype = TokenType::Unknown;

	// explicitly store the iterators for this token relative to the input data.
	// this makes debugging and data extraction slightly easier
	const u8* begin;
	const u8* end;
	u8_const_span span() const { return u8_const_span(&*begin, std::distance(begin, end)); }

	// in case of numbers or single symbols, we also directly store the
	// (converted) value. In case of numerical values, this is a byproduct of
	// testing for numerical values. In case of single symbols, it can make
	// access a bit faster than going through the iterator
	union {
		u8   sym;
		u64  l;
		f64  d;
		bool b;
	} value;
};


inline bool
equals(const Token &tok, const std::string_view &str)
{
	return equals(u8_const_span(&*tok.begin, std::distance(tok.begin, tok.end)), str);
}


/*
 * TODO: for debugging purposes only
 */
inline
const char*
to_string(const TokenType &type)
{
	using namespace ncr;

	switch (type) {
	case TokenType::StringLiteral:  return "string";
	//case token_type::dot:             return "dot";
	//case token_type::ellipsis:        return "ellipsis";
	case TokenType::ValueSeparator: return "delimiter";
	case TokenType::LeftBrace:      return "braces_left";
	case TokenType::RightBrace:     return "braces_right";
	case TokenType::LeftBracket:    return "brackets_left";
	case TokenType::RightBracket:   return "brackets_right";
	case TokenType::LeftParen:      return "parens_left";
	case TokenType::RightParen:     return "parens_right";
	case TokenType::KVSeparator:    return "colon";
	case TokenType::IntegerLiteral: return "integer";
	case TokenType::FloatLiteral:   return "floating_point";
	case TokenType::BoolLiteral:    return "boolean";
	case TokenType::NoneLiteral:    return "none";
	case TokenType::Unknown:        return "unknown";
	}

	return "";
}


/*
 * TODO: for debugging purposes only
 */
inline
std::string
to_string(const Token &token)
{
	std::ostringstream oss;
	oss << "token type: " << to_string(token.ttype) << ", value: " << ncr::to_string(token.span(), {.sep=" ", .beg="", .end=""});
	return oss.str();
}


inline constexpr bool
is_number(Token &tok)
{
	return
		tok.ttype == TokenType::FloatLiteral ||
		tok.ttype == TokenType::IntegerLiteral;
}


inline constexpr bool
is_string(Token &tok)
{
	return
		tok.ttype == TokenType::StringLiteral;
}


inline constexpr bool
is_delimiter(Token &tok)
{
	return
		tok.ttype == TokenType::ValueSeparator;
}


/*
 * tokenizer - a backtracking tokenizer to lex tokens from an input vector
 *
 * This tokenizer uses a very basic implementation by stepping through the input
 * vector and extracting valid tokens, i.e. those sequences of characters mostly
 * surrounded by whitespace, punctutations, or other delimiters, and putting
 * them into a token struct. Backtracking is implemented using a std::vector as
 * buffer, which simply stores each token that was extracted from the input.
 * This makes backtracking effective and simple to implement, with the drawback
 * of having a growing vector (currently, no .consume() is implemented because
 * that would require some more housekeeping).
 *
 * Could this parser be implemented any other way? Sure! For instance, we could
 * roll a template-based tokenizer, but I'm too lazy to do that for real right
 * now. If anyone is interested and has too much time on their hand, here's a
 * starting point for the interested coder:
 *
 *
 *     template <char C>
 *     struct Char {
 *         enum { value = C };
 *         static constexpr bool match(char c) { return c == C; }
 *     };
 *
 *
 *     template <typename... Ts>
 *     struct Or {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if any values match
 *             return (Ts::match(arg) || ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct And {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if all values match
 *             return (Ts::match(arg) && ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct Not {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if all values match
 *             return !(Ts::match(arg) || ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct Seq {
 *         template <typename... Args>
 *         static bool match(Args... args) {
 *             // use fold expression to test if all values match
 *             return (Ts::match(args) && ...);
 *         }
 *
 *         template <typename Arg>
 *         static constexpr bool match(const std::vector<Arg> &arg) {
 *             // determine if size matches
 *             if (arg.size() != sizeof...(Ts)) return false;
 *             // use fold expression to test if all values match
 *             size_t i = 0;
 *             return (Ts::match(arg[i++]) && ...);
 *         }
 *
 *         template <typename I>
 *         static constexpr bool match(const std::ranges::subrange<I> &arg) {
 *             // determine if size matches
 *             if (arg.size() != sizeof...(Ts)) return false;
 *             // use fold expression to test if all values match
 *             size_t i = 0;
 *             return (Ts::match(arg[i++]) && ...);
 *         }
 *     };
 *
 *
 *     // build parse table
 *     using Whitespace = Or<Char<'\n'>, Char<' '>, Char<'\t'>>;
 *     using Letter     = Or<Char<'a'>, Char<'b'>>; // this is of course entirely incomplete
 *     using If         = Seq<Char<'i'>, Char<'f'>>;
 *     using Else       = Seq<Char<'e'>, Char<'l'>, Char<'s'>, Char<'e'>>;
 *     using IfElse     = Or<If, Else>;
 *     // a ton more declarations
 *
 *     // This can be then used as follows:
 *     std::vector<unsigned> v{'i', 'f', 'g'};
 *     std::cout << "expected false: " << If::match(v) << "\n";
 *     std::ranges::subrange range(v.begin(), std::prev(v.end()));
 *     std::cout << "expected true:  " << If::match(range) << "\n";
 *     std::cout << "expected true:  " << IfElse::match(range) << "\n";
 *
 * I welcome any patches that replace the tokenizer with the template version.
 * Such a patch must (!) include a performance comparison that clearly shows an
 * advantage of the template version to convince me that it's worth applying,
 * though.
 */
struct Tokenizer
{
	// reference to the input data
	const u8* data_start;
	const u8* data_end;

	// iterators to track start and end of a token. also called 'cursor'
	const u8 * tok_start;
	const u8 * tok_end;

	// buffer and position within the buffer for all tokens that were read. Note
	// that the buffer is not pruned at the moment and thus lives as long as the
	// tokenizer. For most inputs that this tokenizer will see, this currently
	// does not pose any issue. In the future, a .parse() function might be
	// implemented which takes care of the buffer growing out-of-bounds.
	using RestorePoint = size_t;
	std::vector<Token> buffer;
	size_t             buffer_pos {0};

	// different tokenizer result
	enum class result : u8 {
		ok,
		end_of_input,
		incomplete_token,
		invalid_token,
	};


	Tokenizer(const u8* begin, const u8* end)
	 	 : data_start(begin), data_end(end)
	 	 , tok_start(begin), tok_end(end) {}

	// tokenizer(u8_span &_data) : data(_data), tok_start(data.begin()), tok_end(data.begin()) {}


	bool
	eof() {
		// try to read a token. if it's all whitespace at the end, then we'll
		// get a corresponding result. if not, then backtrack. This prevents
		// parsing fails when there's no more input.
		// TODO: better to propagate EOF from within the parse_* methods (see
		//       also comment in parse()
		RestorePoint rp;
		Token tok;
		if (tok_start == data_end || get_next_token(tok, &rp) == result::end_of_input)
			return true;
		restore(rp);
		return false;
	}


	result
	__fetch_token(Token &tok)
	{
		if (tok_start == data_end)
			return result::end_of_input;

		// ignore whitespace
		if (is_whitespace(*tok_start)) {
			do {
				++tok_start;
				if (tok_start == data_end)
					break;
			} while (is_whitespace(*tok_start));
			tok_end = tok_start;
			if (tok_start == data_end)
				return result::end_of_input;
		}

		// TODO: number start, -,+,number,. -> would need to parse the entire
		//       number manually, though

		/*
		 * TODO: dot and ellipsis
		 */
		/*
		// determine if this is an ellipsis, because python interprets '...'
		// as a single token!
			tok_end = tok_start;
			u16 accum = 1;
			while (++tok_end < dlen) {
				if (data[tok_end] != '.')
					break;
				++accum;
			}
			if (accum != 1 && accum != 3) {
				return result::invalid_token;
			}
		*/

		// punctuations
		TokenType ttype;
		if (get_punctuation_type(*tok_start, ttype)) {
			tok.ttype     = ttype;
			tok.begin     = tok_start;
			tok.end       = tok_start + 1;
			// TODO: this is wrong in the case of an ellipsis
			tok.value.sym = *tok_start;
			++tok_start;
			tok_end = tok_start;
			return result::ok;
		}

		// string token
		if (*tok_start == '\'' || *tok_start == '\"') {
			u8 str_delim = *tok_start;
			tok_end = tok_start;
			while (++tok_end != data_end) {
				if (*tok_end == str_delim)
					break;
			}
			tok.ttype = TokenType::StringLiteral;
			// range excludes the surrounding ''
			tok.begin = tok_start + 1;
			tok.end   = tok_end;

			// test if string is finished
			if (tok_end == data_end || *tok_end != str_delim)
				return result::incomplete_token;

			tok_start = ++tok_end;
			return result::ok;
		}

		// read everything until a punctuation or whitespace
		while (tok_end != data_end) {
			TokenType ttype;
			if (*tok_end == ' ' || get_punctuation_type(*tok_end, ttype))
				break;
			++tok_end;
		}
		if (tok_end > tok_start) {
			tok.begin = tok_start;
			tok.end   = tok_end;

			// TODO: avoid using a temporary string, and use something that
			// actually uses system locales to determine numbers based on a
			// locale's decimal point settings
			std::string tmp(tok.begin, tok.end);
			if (is_integer_literal(tmp, tok.value.l))
				tok.ttype = TokenType::IntegerLiteral;
			else if (is_float_literal(tmp, tok.value.d))
				tok.ttype = TokenType::FloatLiteral;
			else if (is_bool_literal(tok.begin, tok.end, tok.value.b))
				tok.ttype = TokenType::BoolLiteral;
			else if (equals(u8_const_span(tok.begin, std::distance(tok.begin, tok.end)), "None"))
				tok.ttype = TokenType::NoneLiteral;
			else
				// we could not determine this type.
				// TODO: maybe return an error code
				tok.ttype = TokenType::Unknown;
		}
		tok_start = tok_end;
		return result::ok;
	};


	/*
	 * get_next_token - get the next token
	 *
	 * This will return a tokenizer result and store the next token in the
	 * passed in reference tok. In case a restore_point is passed in to bpoint,
	 * then bpoint will be set to the token *before* the one returned in tok.
	 * This makes it possible to reset the tokenizer to before the read symbol
	 * in case the symbol is unexpected during parsing.
	 *
	 * Note: Internally, the tokenizer stores all read tokens in a buffer which
	 * grows over time. While this is not ideal, and a mechanism that parses
	 * the tokens and prevents unbounded growth of the buffer, it's currently
	 * not required. On systems with significantly limited memory, this might
	 * become an issue, though.
	 */
	result
	get_next_token(Token &tok, RestorePoint *bpoint =nullptr)
	{
		if (bpoint != nullptr)
			*bpoint = buffer_pos;

		if (buffer_pos < buffer.size()) {
			tok = buffer[buffer_pos++];
			return Tokenizer::result::ok;
		}

		if (buffer.empty() || buffer_pos >= buffer.size()) {
			Token _tok;
			if (__fetch_token(_tok) == Tokenizer::result::ok) {
				buffer.push_back(_tok);
				tok = buffer.back();
				buffer_pos = buffer.size();
				return Tokenizer::result::ok;
			}
		}

		return Tokenizer::result::end_of_input;
	}

	// backup points to memoize where the
	RestorePoint backup()             { return buffer_pos;   }
	void restore(RestorePoint bpoint) { buffer_pos = bpoint; }
};





/*
 * parser - A simple recursive descent parser (RDP)
 *
 * The parser uses a backtracking tokenizer to be able to -- in principle --
 * parse LL(k) languages. However, it is reduced to the purpose of what is
 * required.
 *
 * Note: In theory, an RDP for LL(k) languages with backtracking can suffer from
 * an exponential runtime due to backtracking. However, this is rarely an issue
 * besides theoretical considerations and nasty language grammars of obscure and
 * mostly theoretical programming languages. If ever this becomes a problem, the
 * parser type can be changed, of course. Until then, I'll stick to RDP and
 * ignore discussions around this issue.
 */
struct PyParser
{
	enum class result : u8 {
		ok,             // parsing succeeded
		failure,        // failure parsing a context / type / object
		syntax_error,    // syntax error while parsing
		incomplete      // parsing encountered an incomplete context / type / object
	};

	/*
	 * the type of object that was parsed
	 */
	enum class Type : u8 {
		Uninitialized,  // something we don't yet know (default value)

		None,           // the none-type for the keyword "None"
		String,
		Integer,
		FloatingPoint,
		Boolean,
		KVPair,         // a key:value pair
		Tuple,          // tuples of the form (value0, value1, ...)
		List,           // lists of the form [value0, value1, ...]
		Set,            // sets of the form {value0, value1, ...}
		Dict,           // dict of the form {key0:value0, key1:value1, ...}

		Symbol,         // anything like {}[], etc. we return parse_results for
		                // symbols even though they don't specify a particular
		                // type, because we might want to extract the beginning
		                // and end of certain groups. This keeps the interface
		                // somewhat small. A parse result of type symbol should
		                // never end up directly or indirectly in the
		                // root_context. This allows to store everything the
		                // parser returns in a parse result, i.e. it will not
		                // require a different data type. Could have used an
		                // std::vector or similar container, though.

		RootContext,    // root context, contains everything that was parsed
		                // successfully
	};


	/*
	 * a parse result
	 *
	 * A parse result is treated as a context. A context can be a value, or
	 * contain other parse results as nodes. For instance, a list contains the
	 * elements of the list in the nodes vector. Generally, the parse result
	 * contains information about the type, the range of the result, as well as
	 * embedded nodes. In the case of 'basic types' (integer, double, bool), the
	 * value is accessible directly via the value union.
	 *
	 * XXX: maybe use an std::variant to get more type checking into the parser
	 */
	struct ParseResult {
		using ParseResultNodes = std::vector<std::unique_ptr<ParseResult>>;

		// parse status of this result / context
		result
			status {result::failure};

		// data type of this result / context
		Type
			dtype  {Type::Uninitialized};

		// where this type / context starts in the input range
		const u8*
			begin;

		// where this type / context ends
		const u8*
			end;

		// in case of a group (kvpairs, lists, etc.), this contains the group's
		// children. In case of a key-value pair, there will be 2 children:
		// first the key, then the value
		ParseResultNodes nodes;

		// for 'basic types', this contains the actual value
		union {
			i64  l;
			f64  d;
			bool b;
		} value;

		// access to the range within the input which this parse_result captures
		u8_const_span span() { return u8_const_span(begin, std::distance(begin, end)); }


		inline bool
		equals(const std::string_view &str)
		{
			return ::ncr::numpy::equals(this->begin, this->end, str);
		}
	};


	/*
	 * parse a token of a particular type Type
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <TokenType TokenType, PyParser::Type ParserType>
	std::unique_ptr<ParseResult>
	parse_token_type(Tokenizer &tokens)
	{
		Token tok;
		Tokenizer::RestorePoint rp;
		if (tokens.get_next_token(tok, &rp) == Tokenizer::result::ok && tok.ttype == TokenType) {
			auto ptr = std::make_unique<ParseResult>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == Type::Boolean)
				ptr->value.b = tok.value.b;
			if constexpr (ParserType == Type::Integer)
				ptr->value.l = tok.value.l;
			if constexpr (ParserType == Type::FloatingPoint)
				ptr->value.d = tok.value.d;

			return ptr;
		}

		tokens.restore(rp);
		return {};
	}


	/*
	 * parse a token that evalutes Fn to true.
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <PyParser::Type ParserType, typename F>
	std::unique_ptr<ParseResult>
	parse_token_fn(Tokenizer &tokens, F fn)
	{
		Token tok;
		Tokenizer::RestorePoint rp;
		if (tokens.get_next_token(tok, &rp) == Tokenizer::result::ok && fn(tok)) {
			auto ptr = std::make_unique<ParseResult>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == Type::Integer)
				ptr->value.l = tok.value.l;

			return ptr;
		}

		tokens.restore(rp);
		return {};
	}


	// symbols. result of these parse instructions will be ignored, but for the
	// sake of completeness, we still specify a parser type
	inline std::unique_ptr<ParseResult> parse_delimiter(Tokenizer &tokens) { return parse_token_fn<Type::Symbol>(tokens, is_delimiter);                  }
	inline std::unique_ptr<ParseResult> parse_colon(Tokenizer &tokens)     { return parse_token_type<TokenType::KVSeparator,   Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lbracket(Tokenizer &tokens)  { return parse_token_type<TokenType::LeftBracket,   Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rbracket(Tokenizer &tokens)  { return parse_token_type<TokenType::RightBracket,  Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lbrace(Tokenizer &tokens)    { return parse_token_type<TokenType::LeftBrace,     Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rbrace(Tokenizer &tokens)    { return parse_token_type<TokenType::RightBrace,    Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lparen(Tokenizer &tokens)    { return parse_token_type<TokenType::LeftParen,     Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rparen(Tokenizer &tokens)    { return parse_token_type<TokenType::RightParen,    Type::Symbol>(tokens);  }

	// types / literals
	inline std::unique_ptr<ParseResult> parse_number(Tokenizer &tokens)    { return parse_token_fn<Type::Integer>(tokens, is_number);                    }
	inline std::unique_ptr<ParseResult> parse_string(Tokenizer &tokens)    { return parse_token_type<TokenType::StringLiteral, Type::String>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_bool(Tokenizer &tokens)      { return parse_token_type<TokenType::BoolLiteral,   Type::Boolean>(tokens); }
	inline std::unique_ptr<ParseResult> parse_none(Tokenizer &tokens)      { return parse_token_type<TokenType::NoneLiteral,   Type::None>(tokens);    }


	std::unique_ptr<ParseResult>
	parse_kvpair(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		// parse the key
		auto      key = parse_string(tokens);
		if (!key) key = parse_number(tokens);
		if (!key) key = parse_tuple(tokens);
		if (!key) {
			tokens.restore(rp);
			return {};
		}

		// parse :
		if (!parse_colon(tokens)) {
			tokens.restore(rp);
			return {};
		}

		// parse the value
		auto        value = parse_none(tokens);
		if (!value) value = parse_bool(tokens);
		if (!value) value = parse_number(tokens);
		if (!value) value = parse_string(tokens);
		if (!value) value = parse_tuple(tokens);
		if (!value) value = parse_list(tokens);
		if (!value) value = parse_set(tokens);
		if (!value) value = parse_dict(tokens);
		if (!value) {
			// failed to parse kv pair, backtrack out
			tokens.restore(rp);
			return {};
		}

		// package up the result
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::ok;
		ptr->dtype = Type::KVPair;
		ptr->begin = key->begin;
		ptr->end   = value->end;
		ptr->nodes.push_back(std::move(key));
		ptr->nodes.push_back(std::move(value));
		return ptr;
	}


	std::unique_ptr<ParseResult>
	parse_tuple(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lparen = parse_lparen(tokens);
		if (!lparen) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Tuple;
		ptr->begin  = lparen->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rparen = parse_rparen(tokens);
			if (rparen) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rparen->end;
				return ptr;
			}

			// almost everything is allowed in tuples. we only care about those
			// types that we have implemented (so far), though.
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_list(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) elem = parse_dict(tokens);
			if (!elem) {
				// failed to parse tuple, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_list(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbracket = parse_lbracket(tokens);
		if (!lbracket) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::List;
		ptr->begin  = lbracket->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbracket = parse_rbracket(tokens);
			if (rbracket) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rbracket->end;
				return ptr;
			}

			// almost everything is allowed in a list
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_list(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) elem = parse_dict(tokens);
			if (!elem) {
				// failed to parse list, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_set(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbrace = parse_lbrace(tokens);
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Set;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace(tokens);
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// allowed types in a set are all immutable and hashable types. we
			// don't support arbitrary hashable objects, so we only need to
			// check for immutable things that we know
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) {
				// failed to parse set, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_dict(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbrace = parse_lbrace(tokens);
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Dict;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace(tokens);
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// sets are surprisingly simple beasts to tame, because they only
			// want to eat kv pairs
			auto kv_pair = parse_kvpair(tokens);
			if (!kv_pair) {
				// failed to parse dict, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(kv_pair));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_expression(Tokenizer &tokens)
	{
		// parse the things we know (and care about) in order they are specified
		// in python's formal grammar
		auto         result = parse_tuple(tokens);
		if (!result) result = parse_list(tokens);
		if (!result) result = parse_set(tokens);
		if (!result) result = parse_dict(tokens);

		return result;
	}


	std::unique_ptr<ParseResult>
	parse(u8_const_span input)
	{
		Tokenizer tokens{input.data(), input.data() + input.size()};

		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::RootContext;
		// TODO: currently we don't store begin nor end for the root context.
		// maybe this should change

		// properly initialize the parser state
		while (!tokens.eof()) {
			auto expr = parse_expression(tokens);
			if (!expr)
				return {};
			ptr->nodes.push_back(std::move(expr));
		}
		ptr->status = result::ok;
		return ptr;
	}

	std::unique_ptr<ParseResult>
	parse(const u8_vector &input)
	{
		return parse(u8_const_span(input.data(), input.size()));
	}

};


}} // ncr::numpy

#endif /* _f03a19a69cac46f38404d117df9d9c37_ */

/*
 * ncr_bits.hpp - bit operations
 *
 */
#ifndef _6029ff7cb97c498f8a26966c49a873fe_
#define _6029ff7cb97c498f8a26966c49a873fe_


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

#endif /* _6029ff7cb97c498f8a26966c49a873fe_ */

/*
 * ncr_zip - zip backend interface declaration
 *
 */
#ifndef _8d2a79e5218b40e3807880febfa294a0_
#define _8d2a79e5218b40e3807880febfa294a0_

#ifndef NCR_TYPES
#include <cstdint>
using u8 = std::uint8_t;
using u32 = std::uint32_t;
#endif

#ifndef NCR_TYPES_HAS_VECTORS
#include <vector>
using u8_vector = std::vector<u8>;
#endif


namespace ncr {

/*
 * zip - namespace for arbitrary zip backend implementations
 *
 * Everyone and their grandma has their favourite zip backend. Let it be some
 * custom rolled zlib backend, zlib-ng, minizip, libzip, etc. To provide some
 * flexibility in the backend and allow projects to avoid pulling in too many
 * dependencies (if they already use a backend), specify only an interface here.
 * It is up to the user to select which backend to use. For an example backend
 * implementation, see ncr_zip_impl_libzip.hpp, which implements a libzip
 * backend. Follow the implementation there to implement alternative backends.
 * Make sure to include the appropriate one you want.
 */
namespace zip {
	// (partially) translated errors from within a zip backend. they are mostly
	// inspired from working with libzip
	enum class result {
		ok,

		warning_backend_ptr_not_null,

		error_invalid_filepath,
		error_invalid_argument,
		error_invalid_state,

		error_archive_not_open,
		error_invalid_file_index,
		error_file_not_found,
		error_file_deleted,
		error_memory,
		error_write,
		error_read,
		error_compression_failed,

		error_end_of_file,
		error_file_close,

		internal_error,
	};

	// mode for file opening.
	// TODO: currently, reading and writing are treated mutually exclusive. not
	//       clear if this is the best way to treat this. Also, when opening for
	//       writing, the file will be truncated at least in the libzip backend
	enum class filemode : unsigned {
		read   = 1 << 0,
		write  = 1 << 1
	};

	// a zip backend might require to store state between calls, e.g. when
	// opening a file to store an (internal) file pointer. this needs to be
	// opaque to the interface and is handled here in a separate (implementation
	// specific) struct.
	struct backend_state;

	// common interface for any zip backend
	struct backend_interface {
		// create a backend state
		result (*make)(backend_state **);

		// release a backend pointer.
		result (*release)(backend_state **);

		// open an archive
		result (*open)(backend_state *, const std::filesystem::path filepath, filemode mode);

		// close an archive
		result (*close)(backend_state *);

		// return the list of files contained within an archive
		result (*get_file_list)(backend_state *, std::vector<std::string> &list);

		// read a given filename from an archive. Note that the filename relates to
		// a file within the archive, not on the local filesystem. The
		// decompressed/read file should be stored in `buffer'.
		result (*read)(backend_state *, const std::string filename, u8_vector &buffer);

		// write a buffer to an already opened zip archive. the compression level
		// depends on the backend. for zlib and many zlib based libraries, 0 is
		// most the default compression level, and other compression levels range
		// from 1 to 9, with 1 being the fastest (but weakest) compression and 9 the
		// slowest (but strongest).
		//
		// Note: the backend implementation will take ownership of the buffer. that
		// is, the buffer will be moved to the function. The reason is that
		// (currently) the buffer is created locally and its livetime might be
		// shorter than what the backend requires. With transfer of ownership, the
		// backend can make sure that the buffer survives as long as required. For
		// an example of this behavior, see ncr_zip_impl_libzip.hpp
		result (*write)(backend_state *, const std::string filename, u8_vector &&buffer, bool compress, u32 compression_level);
	};

	// get an interface for the backend
	extern backend_interface& get_backend_interface();

} // zip::

} // ncr::

#endif /* _8d2a79e5218b40e3807880febfa294a0_ */

#ifndef _9750a253a01642ea81d4721d4c92ad7c_
#define _9750a253a01642ea81d4721d4c92ad7c_

/*
 * npy - read/write numpy files
 *
 */







namespace ncr { namespace numpy {


/*
 * forward declarations and typedefs
 */
struct npyfile;
struct npzfile;
enum class source_type: u16;


/*
 * concepts
 */
template <typename T>
concept NDArray = std::derived_from<T, ndarray>;

template<typename F>
concept GenericReaderCallback = requires(F f, const dtype& dt, const u64_vector &shape, const storage_order &order, u64 index, u8_vector item) {
    { f(dt, shape, order, index, std::move(item)) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallbackFlat = requires(F f, u64 index, T value) {
	{ f(index, value) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallbackMulti = requires(F f, u64_vector index, T value) {
	{ f(std::move(index), value) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallback = TypedReaderCallbackFlat<T, F> || TypedReaderCallbackMulti<T, F>;

template <typename F>
concept ArrayPropertiesCallback = requires(F f, const dtype &dt, const u64_vector &shape, const storage_order &order) {
	{ f(dt, shape, order) } -> std::same_as<bool>;
};

template <typename Range, typename Tp>
concept Writable = requires(Range r, Tp value) {
	// ensure writable to the iterator
	{ *std::begin(r) = value };
	// ensure there's and end iterator
	{ std::end(r) };
	// ensure its a valid range
	{ std::begin(r) != std::end(r) };
};

template <typename T, typename OutputRange>
concept Readable = requires(T source, OutputRange &&dest, std::size_t size) {
	{ source.read(dest, size) } -> std::same_as<std::size_t>;
	{ source.eof() } -> std::same_as<bool>;
	// TODO: maybe also fail()
};

template <typename T>
concept Viewable = requires(T source, std::size_t size) {
	{ source.view(size) } -> std::same_as<std::span<uint8_t>>;
};


/*
 * npyfile - file information of a numpy file
 *
 */
struct npyfile
{
	// numpy files begin with a magic string of 6 bytes, followed by two bytes
	// bytes that identify the version of the file.
	static constexpr u8
		magic_byte_count            {6};

	static constexpr u8
		version_byte_count          {2};

	// the header size field is either 2 or 4 bytes, depending on the version.
	u8
		header_size_byte_count      {0};

	// header size in bytes
	u32
		header_size                 {0};

	// data offset relative to the original file
	u64
		data_offset                 {0};

	// data (i.e. payload) size. Note that the data size is the size of the raw
	// numpy array data which follows the header. Not to be confused with the
	// data size in dtype
	u64
		data_size                   {0};

	// file size of the entire file. Note that this is only known when reading
	// from buffers or streams which support seekg (not necessarily the case for
	// named pipes or tcp streams).
	u64
		file_size                   {0};

	// storage for the magic string.
	u8
		magic[magic_byte_count]     = {};

	// storage for the version
	u8
		version[version_byte_count] = {};

	// the numpy header which describes which data type is stored in this numpy
	// array and how it is stored. Essentially this is a string representation
	// of a python dictionary. Note that the dict can be nested.
	u8_vector
		header;

	// prepare for streaming support via non-seekable streams
	bool
		streaming                   {false};
};


/*
 * clear - clear a npyfile
 *
 * This is useful when a npyfile should be used multiple times. to avoid
 * breaking the POD structure of npyfile, this is a free function.
 */
inline void
release(npyfile &npy)
{
	npy.header_size_byte_count = 0;
	npy.header_size            = 0;
	npy.data_offset            = 0;
	npy.data_size              = 0;
	npy.file_size              = 0;
	std::memset(npy.magic,   0, npyfile::magic_byte_count * sizeof(u8));
	std::memset(npy.version, 0, npyfile::version_byte_count * sizeof(u8));
	npy.header.clear();
	npy.streaming              = false;
}


/*
 * npzfile - container for (compressed) archive files
 *
 * Each file in the npz archive itself is an npy file. This struct is returned
 * when loading arrays from a zip archive. Note that the container will have
 * ownership of the arrays and npyfiles stored within.
 */
struct npzfile
{
	// operator[] is mapped to the array, because this is what people expect
	// from using numpy
	ndarray& operator[](std::string name)
	{
		auto where = arrays.find(name);
		if (where == arrays.end())
			throw std::runtime_error(std::string("Key error: No array with name \"") + name + std::string("\""));
		return *where->second.get();
	}

	// the names of all arrays in this file
	std::vector<std::string> names;

	// the npy files associated with each name
	std::map<std::string, std::unique_ptr<npyfile>> npys;

	// the actual array associated with each name
	std::map<std::string, std::unique_ptr<ndarray>> arrays;
};


// retain POD-like structure of npzfile and provide a free function to clear it
inline void
release(npzfile &npz)
{
	for (auto &arr: npz.arrays)
		arr.second->release();

	npz.names.clear();
	npz.npys.clear();
	npz.arrays.clear();
}



/*
 * to_string - returns a string representation of a result code
 *
 * Note that a result might contain not only a single code, but several codes
 * that are set (technically by OR-ing them). As such, this function returns a
 * string which will contain all string representations for all codes,
 * concatenated by " | ".
 */
inline
std::string
to_string(result res)
{
	if (res == result::ok)
		return result_strings[0].second;

	std::ostringstream oss;
	bool first = true;
	for (size_t i = 1; i < result_strings.size(); ++i) {
		const auto& [enum_val, str] = result_strings[i];
		if ((res & enum_val) != enum_val)
			continue;
		if (!first)
			oss << " | ";
		oss << str;
		first = false;
	}
	return oss.str();
}


/*
 * buffer_read - wrapper for vectors/buffers to make them a ReadableSource
 */
struct buffer_reader
{
	buffer_reader(u8_vector &data) : _data(data), _pos(0) {}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
        auto last = std::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (_pos + size > _data.size()) ? _data.size() - _pos : size;
		std::copy_n(_data.begin() + _pos, size, first);
		_pos += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	inline bool
	eof() noexcept {
		return _pos >= _data.size();
	}

	u8_vector   _data;
	std::size_t _pos;
};


/*
 * ifstream_reader - wrapper for ifstreams to make them a ReadableSource
 */
struct ifstream_reader
{
	ifstream_reader(std::ifstream &stream) : _stream(stream), _eof(false), _fail(false) {}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last = std::end(dest);
		size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));

		_stream.read(reinterpret_cast<char *>(&(*first)), size);
		_fail = _stream.fail();
		_eof  = _stream.eof();
		return static_cast<std::size_t>(_stream.gcount());
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span(dest, size), size);
	}

	inline bool
	eof() noexcept {
		return _eof;
	}

	inline bool
	fail() noexcept {
		return _fail;
	}

	std::ifstream &_stream;
	bool _eof;
	bool _fail;
};


/*
 * read_magic_string - read (and validate) the magic string from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::magic)>
result
read_magic_string(Reader &source, npyfile &npy)
{
	constexpr u8 magic[] = {0x93, 'N', 'U', 'M', 'P', 'Y'};
	if (source.read(npy.magic, npyfile::magic_byte_count) != npyfile::magic_byte_count)
		return result::error_magic_string_invalid;

	if (!std::equal(npy.magic, npy.magic + npyfile::magic_byte_count, magic))
		return result::error_magic_string_invalid;

	return result::ok;
}


/*
 * read_version - read (and validate) the version from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::version)>
result
read_version(Reader &source, npyfile &npy)
{
	if (source.read(npy.version, npyfile::version_byte_count) != npyfile::version_byte_count)
		return result::error_file_truncated;

	// currently, only 1.0 and 2.0 are supported
	if ((npy.version[0] != 0x01 && npy.version[0] != 0x02) || (npy.version[1] != 0x00))
		return result::error_version_not_supported;

	// set the size byte count, which depends on the version
	if (npy.version[0] == 0x01)
		npy.header_size_byte_count = 2;
	else
		npy.header_size_byte_count = 4;

	return result::ok;
}


/*
 * read_header_length - read (and validate) the header length from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, u8*>
result
read_header_length(Reader &source, npyfile &npy)
{
	npy.header_size = 0;

	// read bytes and convert bytes to size_t
	u8 elem = 0;
	size_t i = 0;
	while (i < npy.header_size_byte_count) {
		if (source.read(&elem, 1) != 1)
			return result::error_file_truncated;
		npy.header_size |= static_cast<size_t>(elem) << (i * 8);
		++i;
	}

	/*
	 * Note: the above could be replaced with the following that reads all
	 * header bytes into an std::array<u8, 4>. Still undecided which is better
	 */
	// if (source.read(npy.header_size_bytes, npy.header_size_byte_count) != npy.header_size_byte_count)
	// 	return result::error_file_truncated;
	// for (size_t i = 0; i < npy.header_size_byte_count; i++)
	// 	npy.header_size |= static_cast<size_t>(npy.header_size_bytes[i]) << (i * 8);


	// validate the length: len(magic string) + 2 + len(length) + HEADER_LEN must be divisible by 64
	npy.data_offset = npyfile::magic_byte_count + npy.version_byte_count + npy.header_size_byte_count + npy.header_size;
	if (npy.data_offset % 64 != 0)
		return result::error_header_invalid_length;

	return result::ok;
}


/*
 * read_header - read the header from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::header)>
result
read_header(Reader &source, npyfile &npy)
{
	npy.header.resize(npy.header_size);
	if (source.read(npy.header, npy.header_size) != npy.header_size)
		return result::error_file_truncated;
	if (npy.header.size() < npy.header_size)
		return result::error_header_truncated;
	return result::ok;
}


/*
 * parse_descr_string - turn a string from the parser result for the descirption string into a dtype
 */
inline result
parse_descr_string(PyParser::ParseResult *descr, dtype &dt)
{
	// sanity check: test if the data type is actually a string or not
	if (descr->dtype != PyParser::Type::String)
		return result::error_descr_invalid_string;

	if (std::distance(descr->begin, descr->end) < 3)
		return result::error_descr_invalid_string;

	// first character is the byte order
	dt.endianness = to_byte_order(descr->begin[0]);
	// second character is the type code
	dt.type_code  = descr->begin[1];
	// remaining characters are the byte size of the data type
	std::string str(descr->begin + 2, descr->end);

	// TODO: use something else than strtol
	char *end;
	dt.size = std::strtol(str.c_str(), &end, 10);
	if (*end != '\0') {
		dt.size = 0;
		return result::error_descr_invalid_data_size;
	}
	return result::ok;
}


/*
 * parse_descr_list - turn a list (from a description string parse result) into a dtype
 */
inline result
parse_descr_list(PyParser::ParseResult *descr, dtype &dt)
{
	// the descr field of structured arrays is a list of tuples (n, t, s), where
	// n is the name of the field, t is the type, s is the shape. Note that the
	// shape is optional. Moreover, t could be a list of sub-types, which might
	// recursively contain further subtypes.

	if (descr->nodes.size() == 0)
		return result::error_descr_list_empty;

	for (auto &node: descr->nodes) {
		// check data type of the node
		if (node->dtype != PyParser::Type::Tuple)
			return result::error_descr_list_invalid_type;

		// needs at least 2 subnodes, i.e. tuple (n, t)
		if (node->nodes.size() < 2)
			return result::error_descr_list_incomplete_value;

		// can have at most 3 subnodes, i.e. tuple (n, t, s)
		if (node->nodes.size() > 3)
			return result::error_descr_list_invalid_value;

		// first field: name
		auto &field = add_field(dt, dtype{.name = std::string(node->nodes[0]->begin, node->nodes[0]->end)});
		// dt.fields.push_back({.name = std::string(node->nodes[0]->begin, node->nodes[0]->end)});
		// auto &field = dt.fields.back();

		// second field: type description, which is either a string, or in case
		// of sub-structures it is again a list of tuples, which we can parse
		// recursively
		result res;
		switch (node->nodes[1]->dtype) {
			// string?
			case PyParser::Type::String:
				if ((res = parse_descr_string(node->nodes[1].get(), field)) != result::ok)
					return res;
				break;

			// recursively go through the list
			case PyParser::Type::List:
				if ((res = parse_descr_list(node->nodes[1].get(), field)) != result::ok)
					return res;
				break;

			// currently, other entries are not supported
			default:
				return result::error_descr_list_subtype_not_supported;
		}

		// third field (optional): shape
		if (node->nodes.size() > 2) {
			// test the type. must be a tuple
			if (node->nodes[2]->dtype != PyParser::Type::Tuple)
				return result::error_descr_list_invalid_shape;

			for (auto &n: node->nodes[2]->nodes) {
				// must be an integer value
				if (n->dtype != PyParser::Type::Integer)
					return result::error_descr_list_invalid_shape_value;
				field.shape.push_back(n->value.l);
			}
		}
	}

	return result::ok;
}


/*
 * parse_descr - turn a parser result into a dtype
 */
inline result
parse_descr(PyParser::ParseResult *descr, dtype &dt)
{
	if (!descr)
		return result::error_descr_invalid;

	switch (descr->dtype) {
		case PyParser::Type::String: return parse_descr_string(descr, dt);
		case PyParser::Type::List:   return parse_descr_list(descr, dt);
		default:                     return result::error_descr_invalid_type;
	}
}


/*
 * parse_header - parse the header string of a .npy file
 */
inline result
parse_header(npyfile &npy, dtype &dt, storage_order &order, u64_vector &shape)
{
	// the header of a numpy file is an ASCII-string terminated by \n and padded
	// with 0x20 (whitespace), i.e. string\x20\x20\x20...\n, where string is a
	// literal expression of a Python dictionary.
	//
	// Generally, numpy files have a header dict with fields descr, fortran_order
	// and shape. For 'simple' arrays, descr contains a string with a
	// representation of the stored type. For structured arrays, 'descr'
	// consists of a list of tuples (n, t, s), where n is the name of the field,
	// t is the type, s is the [optional] shape.
	//
	// Example: {'descr': '<f8', 'fortran_order': False, 'shape': (3, 3), }
	//
	// see https://numpy.org/doc/stable/reference/arrays.interface.html#arrays-interface
	// for more information
	//
	// This function uses a (partial) python parser. on success, it will examine the
	// parse result and turn it into classical dtype information. While this
	// information is already within the parse result, there's no need to
	// transport all of the parsing details the the user. Also, it shouldn't
	// take much time to convert and thus have negligible impact on performance.

	// try to parse the header
	PyParser parser;
	auto pres = parser.parse(npy.header);
	if (!pres)
		return result::error_header_parsing_error;

	// header must be one parse-node of type dict
	if (pres->nodes.size() != 1 || pres->nodes[0]->dtype != PyParser::Type::Dict)
		return result::error_header_invalid;

	// the dict itself must have child-nodes
	auto &root_dict = pres->nodes[0];
	if (root_dict->nodes.size() == 0)
		return result::error_header_empty;

	// the result code contains warnings for all fields. they will be disabled
	// during parsing below if they are discovered
	result res = result::warning_missing_descr | result::warning_missing_fortran_order | result::warning_missing_shape;

	// we are not to assume any order of the entries in the dict (albeit they
	// are normally ordered alphabetically). The parse result of the entires are
	// kvpairs, each having the key as node 0 and value as node 1
	for (auto &kv: root_dict->nodes) {
		// check the parsed type for consistency
		if (kv->dtype != PyParser::Type::KVPair || kv->nodes.size() != 2)
			return result::error_header_invalid;

		// descr, might be a string or a list of tuples
		if (kv->nodes[0]->equals("descr")) {
			auto tmp = parse_descr(kv->nodes[1].get(), dt);
			if (tmp != result::ok)
				return tmp;
			res &= ~result::warning_missing_descr;
		}

		// determine if the array data is in fortran order or not
		if (kv->nodes[0]->equals("fortran_order")) {
			if (kv->nodes[1]->dtype != PyParser::Type::Boolean)
				return result::error_fortran_order_invalid_value;
			order = kv->nodes[1]->value.b ? storage_order::col_major : storage_order::row_major;
			res &= ~result::warning_missing_fortran_order;
		}

		// read the shape of the array (NOTE: this is *not* the shape of a data
		// type, but the shape of the array)
		if (kv->nodes[0]->equals("shape")) {
			if (kv->nodes[1]->dtype != PyParser::Type::Tuple)
				return result::error_shape_invalid_value;

			// read each shape value
			shape.clear();
			for (auto &n: kv->nodes[1]->nodes) {
				// must be an integer value
				if (n->dtype != PyParser::Type::Integer)
					return result::error_shape_invalid_shape_value;
				shape.push_back(n->value.l);
			}
			res &= ~result::warning_missing_shape;
		}
	}

	return res;
}


/*
 * compute_item_size - compute the item size of a (possibly nested) dtype
 */
inline result
compute_item_size(dtype &dt, u64 offset = 0)
{
	dt.offset = offset;

	// for simple arrays, we simple report the item size
	if (!is_structured_array(dt)) {
		// most types have their 'width' in bytes given directly in the
		// descr string, which is already stored in dtype.size. However, unicode
		// strings and objects differ in that the size that is given in the
		// dtype is not the size in bytes, but the number of 'elements'. In case
		// of unicude, U16 means a unicode string with 16 unicode characters,
		// each character taking up 4 bytes.
		u64 multiplier;
		switch (dt.type_code) {
			case 'O': multiplier = 8; break;
			case 'U': multiplier = 4; break;
			default:  multiplier = 1;
		}
		dt.item_size = multiplier * dt.size;

		// if there's a shape attached, multiply it in
		for (auto s: dt.shape)
			dt.item_size *= s;
	}
	else {
		// the item_size for structured arrays is (often) 0, in which case we simply
		// update. If the item_size is not 0, double check to determine if there is
		// an item-size mismatch.
		u64 subsize = 0;
		for (auto &field: dt.fields) {
			result res;
			if ((res = compute_item_size(field, dt.offset + subsize)) != result::ok)
				return res;
			subsize += field.item_size;
		}
		if (dt.item_size != 0 && dt.item_size != subsize)
			return result::error_item_size_mismatch;
		dt.item_size = subsize;
	}
	return result::ok;
}


inline result
validate_data_size(const npyfile &npy, const dtype &dt)
{
	// TODO: for streaming data, we cannot decide this (we don't know yet how
	// much data there will be)
	if (npy.streaming)
		return result::ok;

	// detect if data is truncated
	if (npy.data_size % dt.item_size != 0)
		return result::error_data_size_mismatch;

	// size = npy.data_size / dt.item_size;
	return result::ok;
}


/*
 * compute_data_size - compute the size of the data in a ReadableSource (if possible)
 */
template <typename Reader>
inline result
compute_data_size(Reader &source, npyfile &npy)
{
	// TODO: implement for other things or use another approach to externalize
	//       type detection
	if constexpr (std::is_same_v<Reader, buffer_reader>) {
		npy.data_size = source._data.size() - source._pos;
	}
	else {
		npy.data_size = 0;
	}
	return result::ok;
}


inline result
from_stream(std::istream &)
{
	// TODO: for streaming data, we need read calls in between to fetch the
	// next amount of data from the streambuf_iterator.
	return result::error_unavailable;
}


template <typename Reader>
// requires Readable<Reader, OutputRange>
inline result
process_file_header(Reader &source, npyfile &npy, dtype &dt, u64_vector &shape, storage_order &order)
{
	auto res = result::ok;

	// read stuff
	if ((res |= read_magic_string(source,  npy)    , is_error(res))) return res;
	if ((res |= read_version(source, npy)          , is_error(res))) return res;
	if ((res |= read_header_length(source, npy)    , is_error(res))) return res;
	if ((res |= read_header(source, npy)           , is_error(res))) return res;

	// parse + compute stuff
	if ((res |= parse_header(npy, dt, order, shape), is_error(res))) return res;
	if ((res |= compute_item_size(dt)              , is_error(res))) return res;
	if ((res |= compute_data_size(source, npy)     , is_error(res))) return res;
	if ((res |= validate_data_size(npy, dt)        , is_error(res))) return res;

	return res;
}


inline result
from_buffer(u8_vector &&buffer, npyfile &npy, ndarray &dest)
{
	auto res = result::ok;

	// setup the npyfile struct as non-streaming
	npy.streaming = false;

	// parts of the array description (will be moved into the array later)
	dtype         dt;
	u64_vector    shape;
	storage_order order;

	// wrap the buffer so that it becomes a ReadableSource
	auto source = buffer_reader(buffer);
	if ((res = process_file_header(source, npy, dt, shape, order), is_error(res))) return res;

	// erase the entire header block. what's left is the raw data of the ndarray
	buffer.erase(buffer.begin(), buffer.begin() + npy.data_offset);

	// create a new npybuffer with a vector backend. we can move the data right
	// into it
	npybuffer* npybuf = new npybuffer(npybuffer::type::vector);
	npybuf->vector    = make_vector_buffer(std::move(buffer));

	// build the ndarray from the data that we read by moving into it. we also
	// transfer ownership of the npybuf to the array. the user is responsible to
	// call release on the array
	dest.assign(std::move(dt),
				std::move(shape),
				npybuf,
				order);

	return res;
}


inline bool
is_zip_file(std::istream &is)
{
	// each zip file starts with a local file header signature of 4 bytes and
	// the value 0x04034b50. For more information see the ZIP file format
	// specification: https://pkware.cachefly.net/webdocs/APPNOTE/APPNOTE-6.3.9.TXT
	u8 buffer[4];
	is.read((char*)buffer, 4);
	return buffer[0] == 0x50 &&
	       buffer[1] == 0x4b &&
	       buffer[2] == 0x03 &&
	       buffer[3] == 0x04;
}


inline result
from_zip_archive(std::filesystem::path filepath, npzfile &npz)
{
	// get a zip backend
	zip::backend_state *zip_state      = nullptr;
	zip::backend_interface zip_backend = zip::get_backend_interface();

	zip_backend.make(&zip_state);
	if (zip_backend.open(zip_state, filepath, zip::filemode::read) != zip::result::ok) {
		zip_backend.release(&zip_state);
		return result::error_file_open_failed;
	}

	std::vector<std::string> file_list;
	if (zip_backend.get_file_list(zip_state, file_list) != zip::result::ok) {
		zip_backend.close(zip_state);
		zip_backend.release(&zip_state);
		// TODO: better error return value
		return result::error_file_read_failed;
	}

	// for each archive file, decompress and parse the numpy array
	for (auto &fname: file_list) {
		u8_vector buffer;
		if (zip_backend.read(zip_state, fname, buffer) != zip::result::ok) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return result::error_file_read_failed;
		}

		// remove ".npy" from array name
		std::string array_name = fname.substr(0, fname.find_last_of("."));

		// get a npy file and array
		npyfile *npy = new npyfile{};
		ndarray *array = new ndarray{};
		result res;
		if ((res = from_buffer(std::move(buffer), *npy, *array)) != result::ok) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return res;
		}

		// store the information in an npz_file
		npz.names.push_back(array_name);
		npz.npys.insert(std::make_pair(array_name, std::unique_ptr<npyfile>(npy)));
		npz.arrays.insert(std::make_pair(array_name, std::unique_ptr<ndarray>(array)));
	}

	// close the zip backend and release it again
	zip_backend.close(zip_state);
	zip_backend.release(&zip_state);
	return result::ok;
}

inline result
open_fstream(std::filesystem::path filepath, std::ifstream &fstream)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (!fs::exists(filepath))
		return result::error_file_not_found;

	// attempt to read
	fstream.open(filepath, std::ios::binary);
	if (!fstream)
		return result::error_file_open_failed;

	return result::ok;
}


inline result
from_npz(std::filesystem::path filepath, npzfile &npz)
{
	// open the file for file type test
	result res;
	std::ifstream f;
	if ((res = open_fstream(filepath, f), is_error(res)))
		return res;

	// test if this is a PKzip file, also close it again
	bool test = is_zip_file(f);
	f.close();
	if (!test)
		return result::error_wrong_filetype;

	// let the zip backend handle this file from now on
	return from_zip_archive(filepath, npz);
}


/*
 * open_npy - attempt to open an npy file.
 *
 * This function opens a file and examines the first few bytes to test if it is
 * a zip file (e.g. used in .npz). If that's the case, it returns an error,
 * because handling .npz files is different regarding how data is read and where
 * it will be written to. If the file is not a zip file, this function resets
 * the read cursor to before the first byte, and returns OK. If it later turns
 * out that this is, in fact, not a .npy file, other functions will return
 * errors because parsing the .npy header will (most likely) fail.
 */
inline result
open_npy(std::filesystem::path filepath, std::ifstream &file)
{
	result res;
	if ((res = open_fstream(filepath, file), is_error(res)))
		return res;

	// test if this is a PKzip file, and if yes then we exit early. for loading
	// npz files, use from_npz
	if (is_zip_file(file))
		return result::error_wrong_filetype;

	file.seekg(0);
	return res;
}


#ifndef NCR_HAS_GET_FILE_SIZE
#define NCR_HAS_GET_FILE_SIZE
inline u64
get_file_size(std::ifstream &is)
{
	auto ip = is.tellg();
	is.seekg(0, std::ios::end);
	auto res = is.tellg();
	is.seekg(ip);
	return static_cast<u64>(res);
}
#endif


/*
 * from_npy_ifstream - read an already opened ifstream into an ndarray
 */
template <NDArray NDArrayType, bool unsafe_read = true>
result
from_npy_ifstream(std::ifstream &file, NDArrayType &array, npyfile *npy = nullptr)
{
	result res = result::ok;

	// read the file into a vector. the c++ iostream interface is horrible to
	// work with and considered bad design by many developers. We'll load the
	// file into a vector (which is not the fastest), but then working with it
	// is reasonably simple
	auto filesize = get_file_size(file);
	u8_vector buf(filesize);
	if constexpr (unsafe_read)
		file.read(reinterpret_cast<char*>(buf.data()), filesize);
	else
		buf.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	// if the caller didnt pass in a preallocated object, we'll use a local one.
	// this way avoids allocating an object, as _tmp is already present on the
	// stack. it also doesn't tamper with the npy pointer
	npyfile _tmp;
	npyfile *npy_ptr = npy ? npy : &_tmp;

	// Note the change in argument order!
	res = from_buffer(std::move(buf), *npy_ptr, array);

	// done
	return res;
}


/*
 * from_nyp - read a file into a container
 *
 * When reading a file into an ndarray, we read the file in one go into a buffer
 * and then process it.
 */
template <NDArray NDArrayType, bool unsafe_read = true>
result
from_npy(std::filesystem::path filepath, NDArrayType &array, npyfile *npy = nullptr)
{
	// try to open the file
	result res = result::ok;
	std::ifstream file;
	if ((res = open_npy(filepath, file), is_error(res))) return res;
	return from_npy_ifstream<NDArrayType, unsafe_read>(file, array, npy);
}



template <typename T, typename F, typename G>
result
from_npy_callback(std::filesystem::path filepath, G array_properties_callback, F data_callback, npyfile *npy = nullptr)
{
	// try to open the file
	result res = result::ok;
	std::ifstream file;
	if ((res = open_npy(filepath, file), is_error(res))) return res;

	// see comment in from_npy for NDArrayType
	npyfile _tmp;
	npyfile *npy_ptr = npy ? npy : &_tmp;

	// process the file header and extract properties of the array
	dtype         dt;
	u64_vector    shape;
	storage_order order;
	auto source = ifstream_reader(file);
	if ((res = process_file_header(source, *npy_ptr, dt, shape, order), is_error(res))) return res;
	if constexpr (ArrayPropertiesCallback<G>) {
		bool cb_result = array_properties_callback(dt, shape, order);
		if (!cb_result)
			return res;
	}

	// at this point we know the item size, and can read items from the file
	// until we hit eof
	for (u64 i = 0;; ++i) {
		u8_vector buffer(dt.item_size, 0);
		size_t bytes_read = source.read(buffer, dt.item_size);
		if (bytes_read != dt.item_size) {
			// EOF -> nothing more to read, w'ere in a good state
			if (bytes_read == 0 && source.eof())
				break;
			else {
				// there was some failure while reading. this might also be set
				// when trying to read more bytes than available
				// TODO: determine when this might happen
				if (source.fail())
					res = result::error_file_read_failed;

				// the file is truncated, there were not enough bytes for
				// another item.
				else
					res = result::error_file_truncated;

				break;
			}
		}

		// select the right callback variant. if the callback returns false, the
		// user wants an early exit
		if constexpr (GenericReaderCallback<F>) {
			if (!data_callback(dt, shape, order, i, std::move(buffer)))
				break;
		}
		else if constexpr (TypedReaderCallbackFlat<T, F>) {
			// when the callback returns false, the user wants an early exit
			T value;
			std::memcpy(&value, buffer.data(), sizeof(T));
			if (!data_callback(i, value))
				break;
		}
		else if constexpr (TypedReaderCallbackMulti<T, F>) {
			// when the callback returns false, the user wants an early exit
			u64_vector multi_index = unravel_index(i, shape, order);
			T value;
			std::memcpy(&value, buffer.data(), sizeof(T));
			if (!data_callback(multi_index, value))
				break;
		}
		else {
			static_assert(GenericReaderCallback<F> || TypedReaderCallback<T, F>,
						  "The provided function does not satisfy any of the required concepts.");
		}
	}
	return res;
}

template <typename F> requires GenericReaderCallback<F>
result
from_npy(std::filesystem::path filepath, F callback, npyfile *npy = nullptr)
{
	return from_npy_callback<void>(
		std::move(filepath),
		nullptr,
		std::forward<F>(callback),
		npy);
}


template <typename T, typename F> requires TypedReaderCallback<T, F>
result
from_npy(std::filesystem::path filepath, F callback, npyfile *npy = nullptr)
{
	return from_npy_callback<T, F>(
		std::move(filepath),
		nullptr,
		std::forward<F>(callback),
		npy);
}


template <typename T, typename G, typename F>
requires ArrayPropertiesCallback<G> && TypedReaderCallback<T, F>
result
from_npy(std::filesystem::path filepath, G array_properties_callback, F data_callback, npyfile *npy = nullptr)
{
	return from_npy_callback<T, F, G>(
		std::move(filepath),
		std::forward<G>(array_properties_callback),
		std::forward<F>(data_callback),
		npy);
}


/*
 * load - high level API which tries to load whatever file is given
 *
 * In case the file cannot be loaded, i.e. it's not an npz or npy file, the
 * variant will hold a corresponding error code
 *
 * TODO: memory-mapped variant, which then moves the file descriptor *into* the
 *       array (this way the array will be backed by the memory mapped data,
 *       similar to numpy.memmap)
 */
inline result
load(std::filesystem::path filepath, ndarray &arr)
{
	return from_npy(filepath, arr);
}


inline result
loadz(std::filesystem::path filepath, npzfile &npz)
{
	return from_npz(filepath, npz);
}


/*
 * to_npy_buffer - construct a npy file compatible buffer from ndarray
 */
inline result
to_npy_buffer(const ndarray &arr, u8_vector &buffer)
{
	// initialize default header structure
	buffer = {
		// magic string
		0x93, 'N', 'U', 'M', 'P', 'Y',
		// version (2.0)
		0x02, 0x00,
		// space for header size (version 2.0 -> 4 bytes)
		0x00, 0x00, 0x00, 0x00
	};

	// write the header string
	std::string typedescr = arr.get_type_description();
	std::copy(typedescr.begin(), typedescr.end(), std::back_inserter(buffer));

	// the entire header must be divisible by 64 -> find next bigger. Common
	// formula is simply (((N + divisor - 1) / divisor) * divisor), where
	// divisor = 64.  However, we need to adapt for +1 for the trailing \n that
	// terminates the header
	size_t bufsize = buffer.size();
	size_t total_header_length = ((bufsize + 64) / 64) * 64;

	// fill white whitespace (0x20) and trailing \n
	buffer.resize(total_header_length);
	std::fill(buffer.begin() + bufsize, buffer.end() - 1, 0x20);
	buffer[buffer.size() - 1] = '\n';

	// write the header length (in little endian). Note that the eader length
	// itself is the length without magic string, size, and version bytes and
	// not the total header length!
	size_t header_length = total_header_length
		- ncr::numpy::npyfile::magic_byte_count
		- ncr::numpy::npyfile::version_byte_count
		- 4; // four bytes for the header size (version 2.0 file)

	// do we need to byteswap the header length?
	u8 *buf_hlen = buffer.data() + ncr::numpy::npyfile::magic_byte_count + ncr::numpy::npyfile::version_byte_count;
	if constexpr (std::endian::native == std::endian::big) {
		u32 swapped_length = ncr::bswap<u32>(header_length);
		std::memcpy(buf_hlen, &swapped_length, sizeof(u32));
	}
	else
		std::memcpy(buf_hlen, &header_length, sizeof(u32));

	// copy the rest of the array
	const u8* ptr = arr.data();
	const size_t size = arr.bytesize();
	buffer.insert(buffer.end(), ptr, ptr + size);

	return result::ok;
}


inline result
save(std::filesystem::path filepath, const ndarray &arr, bool overwrite=false)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return result::error_file_exists;

	std::ofstream fstream;
	fstream.open(filepath, std::ios::binary | std::ios::out);
	if (!fstream)
		return result::error_file_open_failed;

	// turn the array into a numpy buffer
	result res;
	u8_vector buffer;
	if ((res = to_npy_buffer(arr, buffer)) != result::ok)
		return res;

	// write to file
	fstream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	// size_t is most likely 64bit, but just to make sure cast it again. tellp()
	// can easily be more than 32bit...
	if (fstream.bad() || static_cast<u64>(fstream.tellp()) != static_cast<u64>(buffer.size()))
		return result::error_file_write_failed;

	return result::ok;
}


/*
 * savez_arg - helper object to capture arguments to savez* and save_npz
 */
struct savez_arg
{
	std::string name;
	ndarray&    array;
};


/*
 * save_npz - save arrays to an npz file
 */
inline result
to_zip_archive(std::filesystem::path filepath, std::vector<savez_arg> args, bool compress, bool overwrite=false, u32 compression_level=0)
{
	namespace fs = std::filesystem;

	// detect if there are any name clashes
	std::unordered_set<std::string> _set;
	for (auto &arg: args) {
		if (_set.contains(arg.name))
			return result::error_duplicate_array_name;
		_set.insert(arg.name);
	}

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return result::error_file_exists;

	zip::backend_state *zip_state        = nullptr;
	zip::backend_interface zip_interface = zip::get_backend_interface();

	zip_interface.make(&zip_state);
	if (zip_interface.open(zip_state, filepath, zip::filemode::write) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return result::error_file_open_failed;
	}

	// write all arrays. append .npy to each argument name
	for (auto &arg: args) {
		u8_vector buffer;
		to_npy_buffer(arg.array, buffer);
		std::string name = arg.name + ".npy";
		if (zip_interface.write(zip_state, name, std::move(buffer), compress, compression_level) != zip::result::ok) {
			zip_interface.release(&zip_state);
			return result::error_file_write_failed;
		}
	}

	if (zip_interface.close(zip_state) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return result::error_file_close;
	}

	zip_interface.release(&zip_state);
	return result::ok;
}


/*
 * savez - save name/array pairs to an uncompressed npz file
 */
inline result
savez(std::filesystem::path filepath, std::vector<savez_arg> args, bool overwrite=false)
{
	return to_zip_archive(filepath, std::forward<decltype(args)>(args), false, overwrite);
}


/*
 * savez_compressed - save to compressed npz file
 */
inline result
savez_compressed(std::filesystem::path filepath, std::vector<savez_arg> args, bool overwrite=false, u32 compression_level = 0)
{
	return to_zip_archive(filepath, std::forward<decltype(args)>(args), true, overwrite, compression_level);
}


/*
 * savez - save unamed arrays to an uncompressed npz file
 *
 * Note that the arrays will receive a name of the format arr_i, where i is the
 * position in the args vector
 */
inline result
savez(std::filesystem::path filepath, std::vector<std::reference_wrapper<ndarray>> args, bool overwrite=false)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), false, overwrite);
}


/*
 * savez_compressed - save unamed arrays to a compressed npz file
 *
 * Note that the arrays will receive a name of the format arr_i, where i is the
 * position in the args vector
 */
inline result
savez_compressed(std::filesystem::path filepath, std::vector<std::reference_wrapper<ndarray>> args, bool overwrite=false, u32 compression_level=0)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), true, overwrite, compression_level);
}



template <typename... Args>
void
release(Args&&... args)
{
	(release(args), ...);
}



enum class source_type : u16 {
	mmap,
	fstream,
	buffered,
};


template <source_type>
struct npysource;


template<>
struct npysource<source_type::mmap>
{
	mmap_buffer* buf;

	inline result
	open(std::filesystem::path filepath)
	{
		if (buf)
			close();
		buf = new mmap_buffer();

		ncr::numpy::open(filepath.c_str(), buf);

		// try to open the foo
		int fd = ::open(filepath.c_str(), O_RDONLY);
		if (fd == -1) {
			return result::error_file_open_failed;
		}

		// try to memmap the foo
		buf->size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		buf->data = (uint8_t*)mmap(NULL, buf->size, PROT_READ, MAP_PRIVATE, fd, 0);
		::close(fd);
		if (buf->data == MAP_FAILED) {
			close();
			return result::error_mmap_failed;
		}
		buf->position = 0;
		return result::ok;
	}

	inline size_t
	size()
	{
		return buf->size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		switch (way) {
		case std::ios::beg:
			buf->position = offset;
			break;
		case std::ios::cur:
			buf->position = buf->position + offset;
			break;
		case std::ios::end:
			buf->position = buf->size - offset;
			break;
		}
		return result::ok;
	}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
        auto last = std::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (buf->position + size > buf->size) ? buf->size - buf->position : size;
		std::copy_n(buf->data + buf->position, size, first);
		buf->position += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	std::span<uint8_t>
	view(std::size_t size)
	{
		return std::span<uint8_t>(buf->data + buf->position,
						          buf->data + buf->position + size);
	}

	inline result
	close()
	{
		result res = result::ok;
		if ((res = numpy::close(buf), is_error(res))) return res;

		delete buf;
		buf = nullptr;
		return res;
	}

	inline bool
	eof() noexcept {
		return buf->position >= buf->size;
	}
};


template<>
struct npysource<source_type::fstream>
{
	std::ifstream fstream;
	size_t total_size;

	inline result
	open(std::filesystem::path filepath)
	{
		namespace fs = std::filesystem;

		// test if the file exists
		if (!fs::exists(filepath))
			return result::error_file_not_found;

		// attempt to open
		fstream.open(filepath, std::ios::binary);
		if (!(fstream))
			return result::error_file_open_failed;

		total_size = get_file_size(fstream);
		return result::ok;
	}

	inline size_t
	size()
	{
		return total_size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		fstream.seekg(offset, way);
		if (fstream.fail() or fstream.bad())
			return result::error_seek_failed;
		return result::ok;
	}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last = std::end(dest);
		size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));

		fstream.read(reinterpret_cast<char *>(&(*first)), size);
		return static_cast<std::size_t>(fstream.gcount());
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span(dest, size), size);
	}

	inline result
	close()
	{
		fstream.close();
		return result::ok;
	}

	inline bool
	eof() noexcept {
		// peek one character to check if this is eof in the stream
		fstream.peek();
		return fstream.eof();
	}

	inline bool
	fail() noexcept {
		return fstream.fail();
	}
};


template<>
struct npysource<source_type::buffered>
{
	vector_buffer *buffer;

	size_t    total_size;
	size_t    position;

	inline result
	open(std::filesystem::path filepath)
	{
		constexpr bool unsafe_read = false;

		std::ifstream fstream;
		fstream.open(filepath, std::ios::binary);
		if (!fstream)
			return result::error_file_open_failed;

		total_size = get_file_size(fstream);
		buffer = make_vector_buffer(total_size);

		if constexpr (unsafe_read)
			fstream.read(reinterpret_cast<char*>(buffer->data.data()), total_size);
		else
			buffer->data.assign(std::istreambuf_iterator<char>(fstream), std::istreambuf_iterator<char>());

		fstream.close();
		position   = 0;
		return result::ok;
	};

	inline size_t
	size()
	{
		return total_size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		switch (way) {
		case std::ios::beg:
			position = offset;
			break;
		case std::ios::cur:
			position = position + offset;
			break;
		case std::ios::end:
			position = total_size - offset;
			break;
		}
		if (position > total_size)
			return result::error_seek_failed;
		return result::ok;
	}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
        auto last = std::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (position + size > buffer->data.size()) ? buffer->data.size() - position : size;
		std::copy_n(buffer->data.begin() + position, size, first);
		position += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	inline std::span<uint8_t>
	view(std::size_t size)
	{
		return std::span<uint8_t>(buffer->data.begin() + position,
						          buffer->data.begin() + position + size);
	}

	inline result
	close()
	{
		delete buffer;
		buffer = nullptr;
		return result::ok;
	}

	inline bool
	eof() noexcept {
		return position >= total_size;
	}
};


template <typename Source>
struct npyreader_iterator_base
{
	using difference_type = std::ptrdiff_t;

	npyreader_iterator_base(Source& src, size_t item_sz, bool end = false)
		: source(src), item_size(item_sz), buffer(item_sz), is_end(end), dirty_buffer(true) {}

	npyreader_iterator_base& operator++() {
		seek_next();
		return *this;
	}

	npyreader_iterator_base operator++(int) {
		auto tmp = *this;
		++(*this);
		return tmp;
	}

	bool operator==(const npyreader_iterator_base& other) const { return is_end == other.is_end; }
	bool operator!=(const npyreader_iterator_base& other) const { return !(*this == other); }

protected:
	Source& source;
	size_t item_size;
	u8_vector buffer;
	bool is_end;
	bool dirty_buffer;

	void seek_next() {
		// in case of a buffered source, the call to read() moves the read
		// cursor forward already. to avoid moving over (seeking foward over) an
		// item, we check if the buffer is dirty or not. if the buffer is not
		// dirty, then we "just read" (as in past tense), and the source's read
		// cursor is already at the next item. if the buffer is dirty, then it's
		// not clear when the last actual read from the source happened.
		// In case of a Viewable source, we don't read into the buffer in the
		// first place to avoid copying
		bool do_seek = Viewable<Source> || dirty_buffer;
		if (do_seek)
			source.seek(item_size, std::ios::cur);

		if (source.eof())
			is_end = true;

		dirty_buffer = true;
	}

	void buffer_next_item() {
		// we only need to buffer the next item if the buffer_position is equals
		// the position (reading will move the source character pointer)
		if (!dirty_buffer)
			return;

		if (source.eof()) {
			is_end = true;
		} else {
			// read extracts the characters from the underlying source. need to
			// keep track of this
			auto bytes_read = source.read(buffer.data(), item_size);
			dirty_buffer = false;

			if (bytes_read != item_size) {
				is_end = true;
			}
		}
	}
};


template <typename Source>
struct npyreader_iterator : public npyreader_iterator_base<Source>
{
	using iterator_type     = npyreader_iterator<Source>;
	using iterator_category = std::input_iterator_tag;
	using value_type        = std::span<u8>;
	using pointer           = std::span<u8>*;
	using reference         = std::span<u8>;

	npyreader_iterator(Source& src, size_t item_sz, bool end = false)
		: npyreader_iterator_base<Source>(src, item_sz, end) {}

	reference operator*() {
		if constexpr (Viewable<Source>) {
			return this->source.view(this->item_size);
		} else {
			this->buffer_next_item();
			return this->buffer;
		}
	}
};


template <typename Source, typename T>
struct typed_npyreader_iterator : public npyreader_iterator_base<Source>
{
	using iterator_type     = typed_npyreader_iterator<Source, T>;
	using iterator_category = std::input_iterator_tag;
	using value_type        = T;
	using pointer           = T*;
	using reference         = T;

	typed_npyreader_iterator(Source& src, size_t item_sz, bool end = false)
		: npyreader_iterator_base<Source>(src, item_sz, end)
	{
		if (sizeof(T) != item_sz)
			throw std::runtime_error("Type size mismatch with item_size");
	}

	reference operator*() {
		T item;
		if constexpr (Viewable<Source>) {
			auto view = this->source.view(this->item_size);
			std::memcpy(&item, view.data(), sizeof(T));
		} else {
			this->buffer_next_item();
			std::memcpy(&item, this->buffer.data(), sizeof(T));
		}
		return item;
	}
};


template <source_type E = source_type::mmap>
struct npyreader
{
	using type        = npyreader<E>;
	using source_type = npysource<E>;
	using iterator    = npyreader_iterator<source_type>;

	dtype           dt;
	u64_vector      shape;
	storage_order   order;
	npyfile         npy;
	source_type     source;
	bool            is_open = false;

	iterator begin() { return iterator(source, dt.item_size); }
	iterator end()   { return iterator(source, dt.item_size, true); }

	result
	seek(size_t item_index)
	{
		if (!is_open)
			return result::error_reader_not_open;

		size_t offset = npy.data_offset + dt.item_size * item_index;
		if (offset > source.size())
			return result::error_invalid_item_offset;

		source.seek(offset);
		return result::ok;
	}

	template <typename T>
	struct typed_view {
		using source_type = npysource<E>;
		using iterator    = typed_npyreader_iterator<source_type, T>;

		typed_view(source_type &src, dtype& dtp)
		: source(src), dt(dtp) {}

		iterator begin() { return iterator(source, dt.item_size); }
		iterator end()   { return iterator(source, dt.item_size, true); }

		source_type& source;
		dtype&       dt;
	};

	template <typename T>
	auto as() {
		return typed_view<T>(source, dt);
	}

	template <typename T = std::span<uint8_t>> requires Viewable<source_type>
	T view()
	{
		if constexpr (std::is_same_v<T, std::span<uint8_t>>) {
			// forward the subrange from the source
			return source.view(dt.item_size);
		}
		else {
			if (sizeof(T) != dt.item_size)
				throw std::runtime_error("Type size mismatch with item_size");

			T value;
			auto buf = source.view(dt.item_size);
			std::memcpy(&value, buf.data(), sizeof(T));
			return value;
		}
	}
};


template <source_type E>
inline result
open(std::filesystem::path filepath, npyreader<E>& reader)
{
	result res;

	if ((res = reader.source.open(filepath),
		 is_error(res))) return res;
	if ((res = process_file_header(reader.source, reader.npy, reader.dt, reader.shape, reader.order),
		 is_error(res))) return res;

	reader.is_open = true;
	return res;
}


template <source_type E>
inline result
close(npyreader<E>& reader)
{
	auto res = reader.source.close();
	reader.is_open = false;
	return res;
}




}} // ncr::numpy


#endif /* _9750a253a01642ea81d4721d4c92ad7c_ */


/*
 * the zip implementation can be actively turned off by setting the compiler
 * flag NCR_NUMPY_DISABLE_ZIP_LIBZIP. This allows to develop custom other zip
 * backends and disable the libzip implementation that ships with ncr_numpy.
 */
#ifndef NCR_NUMPY_DISABLE_ZIP_LIBZIP
/*
 * zip_libzip.hpp - ncr zip backend based on libzip
 *
 */

#ifndef _ff3beaed1e3b48528794e7d803a82757_
#define _ff3beaed1e3b48528794e7d803a82757_

// TODO: get rid of iostream



namespace ncr { namespace zip {


/*
 * backend_state - libzip state
 */
struct backend_state
{
	// zip archive
	zip_t *zip {nullptr};

	// store all write buffers within the backend to make sure that the buffers
	// live long enough. zip_file_add does not directly read from the buffer,
	// and therefore a buffer might be invalid once writing actually happens
	std::vector<u8_vector> write_buffers;
};


/*
 * libzip_close - close a libzip backend sate
 */
inline result
libzip_close(backend_state *state)
{
	if (!state)
		return result::error_invalid_argument;
	if (state->zip != nullptr) {
		zip_close(state->zip);
		state->zip = nullptr;
	}
	return result::ok;
}


/*
 * libzip_get_file_list - get the list of files contained in an archive
 */
inline result
libzip_get_file_list(backend_state *state, std::vector<std::string> &list)
{
	if (!state)
		return result::error_invalid_state;
	if (!state->zip)
		return result::error_archive_not_open;

	zip_int64_t num_entries = zip_get_num_entries(state->zip, 0);
	for (zip_int64_t i = 0; i < num_entries; i++) {
		const char *fname = zip_get_name(state->zip, i, 0);
		if (fname == nullptr) {
			zip_error_t *error = zip_get_error(state->zip);
			// translate the error code
			switch (error->zip_err) {
				case ZIP_ER_MEMORY:  return result::error_memory;
				case ZIP_ER_DELETED: return result::error_file_deleted;
				case ZIP_ER_INVAL:   return result::error_invalid_file_index;
				default:             return result::internal_error;
			}
		}
		list.push_back(fname);
	}
	return result::ok;
}


/*
 * libzip_make - make a (libzip) backend state
 */
inline result
libzip_make(backend_state **state)
{
	if (state == nullptr)
		return result::error_invalid_argument;

	result res = result::ok;
	if (*state != nullptr)
		res = result::warning_backend_ptr_not_null;

	*state = new backend_state{};
	return res;
}


/*
 * libzip_open - open a file from an archive
 */
inline result
libzip_open(backend_state *state, const std::filesystem::path filepath, filemode mode)
{
	if (!state)
		return result::error_invalid_argument;

	// TODO: currently, when opening for writing, the file will be truncated if
	//       it already exists. maybe a better approach would be to check the
	//       file. However, this is done already on the callsite (see savez and
	//       savez_compressed's overwrite argument). determine if this should be
	//       kept this way or not
	int flags = (mode == filemode::read) ? ZIP_RDONLY : (ZIP_CREATE | ZIP_TRUNCATE);

	int err = 0;
	if ((state->zip = zip_open(filepath.c_str(), flags, &err)) == nullptr) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);

		// TODO: set a local error string that is part of the zip interface
		std::cerr << "cannot open zip archive " << filepath << ": " << zip_error_strerror(&error) << "\n";
		return result::error_invalid_filepath;
	}

	return result::ok;
}


/*
 * libzip_read - unzip a given filename (of an archive) into an u8 buffer
 */
inline result
libzip_read(backend_state *bptr, const std::string filename, u8_vector &buffer)
{
	if (!bptr)
		return result::error_invalid_state;
	if (!bptr->zip)
		return result::error_archive_not_open;

	// get the file index
	zip_int64_t fid;
	if ((fid = zip_name_locate(bptr->zip, filename.c_str(), 0)) < 0) {
		zip_error_t *error = zip_get_error(bptr->zip);
		// translate the error code
		switch (error->zip_err) {
			case ZIP_ER_MEMORY:  return result::error_memory;
			case ZIP_ER_INVAL:   return result::error_invalid_file_index;
			case ZIP_ER_NOENT:   return result::error_file_not_found;
			default:             return result::internal_error;
		}
	}

	// open the file pointer
	zip_file_t *fp = zip_fopen_index(bptr->zip, fid, 0);
	if (fp == nullptr) {
		zip_error_t *error = zip_get_error(bptr->zip);
		switch (error->zip_err) {
			case ZIP_ER_MEMORY: return result::error_memory;
			case ZIP_ER_READ:   return result::error_read;
			case ZIP_ER_WRITE:  return result::error_write;
			// TODO: translate more errors
			default:            return result::internal_error;
		}
	}

	// get stats to know how many bytes to read
	zip_stat_t stat;
	if (zip_stat_index(bptr->zip, fid, 0, &stat) < 0) {
		zip_error_t *error = zip_get_error(bptr->zip);
		// translate the error code
		switch (error->zip_err) {
			case ZIP_ER_INVAL:   return result::error_invalid_file_index;
			default:             return result::internal_error;
		}
	}

	// finally, read the file into the buffer
	buffer.resize(stat.size);
	zip_int64_t nread = zip_fread(fp, buffer.data(), stat.size);
	if (nread < 0) {
		// TODO: better error reporting
		return result::internal_error;
	}
	else if (nread == 0) {
		return result::error_end_of_file;
	}

	int err_code;
	if ((err_code = zip_fclose(fp)) != 0) {
		return result::error_file_close;
	}

	return result::ok;
}


/*
 * libzip_release - release the libzip backend state
 */
inline result
libzip_release(backend_state **bptr)
{
	if (bptr == nullptr || *bptr == nullptr)
		return result::error_invalid_argument;
	(*bptr)->write_buffers.clear();
	delete *bptr;
	bptr = nullptr;
	return result::ok;
}


/*
 * libzip_write - write a buffer to a previously open zip archive
 */
inline result
libzip_write(backend_state *bptr, const std::string name, u8_vector &&buffer, bool compress, u32 compression_level = 0)
{
	if (!bptr)
		return result::error_invalid_state;
	if (!bptr->zip)
		return result::error_archive_not_open;

	// move the buffer into the local list of write buffers. this will retain
	// the livetime of the buffer as long as required (cleared only during
	// release and after zip_close was called, or on error)
	bptr->write_buffers.push_back(std::move(buffer));
	void *data_ptr = bptr->write_buffers.back().data();
	zip_uint64_t size = bptr->write_buffers.back().size();

	// create a source from the buffer
	zip_source_t *source = zip_source_buffer(bptr->zip, data_ptr, size, 0);
	if (!source) {
		return result::error_write;
	}

	zip_int64_t fid;
	if ((fid = zip_file_add(bptr->zip, name.c_str(), source, ZIP_FL_ENC_UTF_8)) < 0) {
		zip_source_free(source);
		return result::error_write;
	}

	if (compress) {
		// Note: ZIP_CM_DEFLATE accepts compression levels 1 to 9, with 0
		// indicating "default". the values origin from zlib. python's zlib
		// backend until python 3.7 used zlib's "default" value, meaning that
		// for instance numpy arrays were compressed most likely with the
		// default value
		if (zip_set_file_compression(bptr->zip, fid, ZIP_CM_DEFLATE, compression_level) < 0) {
			return result::error_compression_failed;
		}
	}

	return result::ok;
}


/*
 * get_backend_interface - get the (libzip) backend interface
 */
inline backend_interface&
get_backend_interface()
{
	static backend_interface interface = {
		libzip_make,
		libzip_release,
		libzip_open,
		libzip_close,
		libzip_get_file_list,
		libzip_read,
		libzip_write
	};
	return interface;
}


}} // ncr::zip

#endif /* _ff3beaed1e3b48528794e7d803a82757_ */

#endif
