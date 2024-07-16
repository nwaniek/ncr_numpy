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


// forward declaration
template <size_t N = 0> struct utf8string;


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
template <size_t N = 0>
struct ucs4string
{
	using Container = std::conditional_t<N == 0, std::vector<u32>, std::array<u32, N>>;

	Container
		data {};


	ucs4string() {}


	ucs4string(const std::array<u32, N> &ucs4)
	{
		// if N == 0, then we received nothing in the constructor and don't need
		// to care about the data
		if constexpr (N != 0)
			data = Container(ucs4.begin(), ucs4.end());
	}


	ucs4string(const std::vector<u32> &ucs4)
	{
		if constexpr (N == 0) {
			data.assign(ucs4.begin(), ucs4.end());
		}
		else {
			if (ucs4.size() > N)
				throw std::runtime_error("Input string exceeds fixed-width UCS-4 string size.");
			data = {};
			std::copy(ucs4.begin(), ucs4.end(), data);
		}
	}


	ucs4string(const std::string &utf8)   { from_utf8(utf8); }


	template <size_t M>
	ucs4string(const utf8string<M> &utf8) { from_utf8(utf8.to_string()); }


	/*
	 * from_utf8 - fill the string from a utf8 string
	 */
	void
	from_utf8(const std::string &utf8)
	{
		if constexpr (N == 0)
			data.clear();
		else
			data = {};

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
				data.push_back(codepoint);
			}
			else {
				if (n >= N)
					throw std::runtime_error("Input string exceeds fixed-width UCS-4 string size.");
				data[n++] = codepoint;
			}
		}
	}


	/*
	 * to_string - convert the ucs4 string to utf8 string (inside an std::string)
	 */
	std::string
	to_string() const
	{
		std::string utf8_string;
		for (auto ch : data) {
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


	friend std::ostream&
	operator<<(std::ostream& os, const ucs4string<N> &str) {
		os << str.to_string();
		return os;
	}
};


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

	Container
		data {};


	utf8string() {}

	utf8string(const std::array<u8, N> &utf8)
	{
		if constexpr (N != 0)
			data = Container(utf8.begin(), utf8.end());
	}


	utf8string(const std::vector<u8> &utf8) {
		if constexpr (N == 0) {
			data.assign(utf8.begin(), utf8.end());
		}
		else {
			if (utf8.size() > N)
				throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
			data = {};
			std::copy(utf8.begin(), utf8.end(), data);
		}
	}


	utf8string(const std::string &utf8) {
		if constexpr (N == 0) {
			data.assign(utf8.begin(), utf8.end());
		}
		else {
			if (utf8.size() > N)
				throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
			data = {};
			std::copy(utf8.begin(), utf8.end(), data.begin());
		}
	}


	utf8string(const ucs4string<N> &ucs4) {
		std::string tmp = ucs4.to_string();
		if constexpr (N == 0) {
			data.assign(tmp.begin(), tmp.end());
		}
		else {
			if (tmp.size() > N)
				throw std::runtime_error("Input string exceeds fixed-width UTF-8 string size.");
			std::copy(tmp.begin(), tmp.end(), data.begin());
		}
	}


	/*
	 * to_string - convert to std::string
	 */
	std::string
	to_string() const
	{
		return std::string(data.begin(), data.end());
	}


	friend std::ostream&
	operator<<(std::ostream& os, const utf8string<N> &utf8_str) {
		os << utf8_str.to_string();
		return os;
	}
};

