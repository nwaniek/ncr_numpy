/*
 * string.hpp - common string manipulation functions
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <cstdarg>
#include <string>
#include <vector>

namespace ncr {


/*
 * strpad - pad a string with whitespace to make it at least length chars long
 */
inline std::string
strpad(const std::string& str, size_t length)
{
    return str + std::string(std::max(length - str.size(), size_t(0)), ' ');
}


/*
 * ltrim - trim a string on the left, removing leading whitespace characters
 */
inline std::string&
ltrim(std::string &s, const char *ws = " \n\t\r")
{
	s.erase(0, s.find_first_not_of(ws));
	return s;
}

/*
 * rtrim - trim a string on the right, removing trailing whitespace
 */
inline std::string&
rtrim(std::string &s, const char *ws = " \n\t\r")
{
	s.erase(s.find_last_not_of(ws) + 1);
	return s;
}


/*
 * trim - remove leading and trailing whitespace
 */
inline std::string&
trim(std::string &s, const char *ws = " \n\t\r")
{
	return ltrim(rtrim(s, ws), ws);
}


inline
const std::string
strformat(const char *const fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);

	// reliably acquire the size using a copy of the variable argument array,
	// and a functionally reliable call to mock the formatting
	va_list va_copy;
	va_copy(va_copy, va_args);
	const int len = std::vsnprintf(NULL, 0, fmt, va_copy);
	va_end(va_copy);

	// return a formatted string without risk of memory mismanagement and
	// without assuming any compiler or platform specific behavior using a
	// vector with zero termination at the end
	std::vector<char> str(len + 1);
	std::vsnprintf(str.data(), str.size(), fmt, va_args);
	va_end(va_args);

	// return the string
	return std::string(str.data(), len);
}


} // ncr::
