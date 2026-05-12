/*
 * ncr_unicode - utilities for working with unicode (UTF-8, UCS-4) strings
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#ifndef _53994d0d2b6d47028f271c81f3b8f52a_
#define _53994d0d2b6d47028f271c81f3b8f52a_

#include <type_traits>
#include <string>
#include "ncr/types.hpp"

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
		u8     b0  = static_cast<u8>(utf8[i]);
		u32    cp  = 0;
		size_t len = 0;
		// Tight bounds on the first continuation byte. These encode the Unicode
		// 15.0 Table 3-7 well-formedness constraints. We'll reject overlong
		// forms (E0, F0), surrogates U+D800..U+DFFF (ED), and codepoints above
		// U+10FFFF (F4). All other continuation bytes lie in 0x80..0xBF.
		u8 lo = 0x80, hi = 0xBF;

		if      (b0 <= 0x7F) { cp = b0;        len = 1; }
		else if (b0 <  0xC2) { throw std::runtime_error("Invalid UTF-8 byte sequence."); }
		else if (b0 <= 0xDF) { cp = b0 & 0x1F; len = 2; }
		else if (b0 == 0xE0) { cp = b0 & 0x0F; len = 3; lo = 0xA0; }
		else if (b0 <= 0xEC) { cp = b0 & 0x0F; len = 3; }
		else if (b0 == 0xED) { cp = b0 & 0x0F; len = 3; hi = 0x9F; }
		else if (b0 <= 0xEF) { cp = b0 & 0x0F; len = 3; }
		else if (b0 == 0xF0) { cp = b0 & 0x07; len = 4; lo = 0x90; }
		else if (b0 <= 0xF3) { cp = b0 & 0x07; len = 4; }
		else if (b0 == 0xF4) { cp = b0 & 0x07; len = 4; hi = 0x8F; }
		else                 { throw std::runtime_error("Invalid UTF-8 byte sequence."); }

		if (i + len > utf8.size())
			throw std::runtime_error("Truncated UTF-8 byte sequence.");

		for (size_t k = 1; k < len; ++k) {
			u8 bk  = static_cast<u8>(utf8[i + k]);
			u8 clo = (k == 1) ? lo : 0x80;
			u8 chi = (k == 1) ? hi : 0xBF;
			if (bk < clo || bk > chi)
				throw std::runtime_error("Invalid UTF-8 continuation byte.");
			cp = (cp << 6) | (bk & 0x3F);
		}
		i += len;

		if constexpr (N == 0) {
			result.data.push_back(cp);
		}
		else {
			if (n >= N)
				throw std::runtime_error("Input string exceeds fixed-width UCS-4 string size.");
			result.data[n++] = cp;
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
