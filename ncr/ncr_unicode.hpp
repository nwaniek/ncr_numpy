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
	static constexpr size_t
		n    {N};

	std::conditional_t<N == 0, std::vector<u32>, std::array<u32, N>>
		data {0};

	ucs4string(std::array<u32, N> &input) : data(input) {}
	ucs4string(std::vector<u32>   &input) : data(input) {}

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
template <size_t N = 0>
struct utf8string
{
	static constexpr size_t
		n    {N};

	std::conditional_t<N == 0, std::vector<u32>, std::array<u32, N>>
		data {0};

	utf8string(std::array<u32, N> &input) : data(input) {}
	utf8string(std::vector<u32>   &input) : data(input) {}

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
