/*
 * ncr_unicode - utilities for working with unicode (UTF-8, UCS-4) strings
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <type_traits>
#include <string>
#include <ncr/ncr_types.hpp>

namespace ncr {

// forward declarations
template <size_t N = 0> struct ucs4string;
template <size_t N = 0> struct utf8string;

template <typename T, size_t N = 0> ucs4string<N> to_ucs4(const T &t);
template <typename T, size_t N = 0> utf8string<N> to_utf8(const T &t);
template <typename T, size_t N = 0> std::string to_string(const T &t);


/*
 * ucs4string - fixed width Unicode string based on UCS-4
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


template <>
struct ucs4string<0>
{
	std::vector<u32>
		data;
};


template <size_t N = 0>
ucs4string<N>
to_ucs4(const std::array<u32, N> &ucs4)
{
	ucs4string<N> result;
	if constexpr (N != 0)
		result.data = Container(ucs4.begin(), ucs4.end());
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
		std::copy(ucs4.begin(), ucs4.end(), result.data);
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


template <size_t N = 0>
std::ostream&
operator<<(std::ostream& os, const ucs4string<N> &ucs4)
{
	os << to_string(ucs4);
	return os;
}



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
	using Container = std::conditional_t<N == 0, std::vector<u8>, std::array<u8, N>>;

	std::array<u8, N>
		data {};
};


template <>
struct utf8string<0>
{
	std::vector<u8>
		data {};
};


template <size_t N = 0>
utf8string<N>
to_utf8(const std::array<u8, N> &utf8)
{
	utf8string<N> result;
	if constexpr (N != 0)
		result.data = Container(utf8.begin(), utf8.end());
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
		std::copy(utf8.begin(), utf8.end(), result.data);
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


template <size_t N = 0>
std::ostream&
operator<<(std::ostream& os, const utf8string<N> &utf8)
{
	os << to_string(utf8);
	return os;
}


} // ncr
