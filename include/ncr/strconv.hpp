/*
 * strconv.hpp - collection of functions to convert types to strings
 *
 * SPDX-FileCopyrightText: 2024-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#ifndef _f53b5d05a7dd47668fbad51a033a87b7_
#define _f53b5d05a7dd47668fbad51a033a87b7_

#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <cstdint>
#include <span>
#include <type_traits>

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
