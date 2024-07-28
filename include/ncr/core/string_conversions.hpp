/*
 * string_conversions.hpp - collection of functions to convert types to strings
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <string>
#include <vector>
#include <array>
#include <array>
#include <sstream>
#include <cstdint>

namespace ncr {


template <typename T>
std::string
to_string(const std::vector<T>& vec, const char *sep = ", ", const char *beg="[", const char *end="]")
{
	std::ostringstream oss;
	oss << beg;
	for (size_t i = 0; i < vec.size(); ++i) {
		if (i != 0) {
			oss << sep;
		}
		oss << vec[i];
	}
	oss << end;
	return oss.str();
}

template <typename T, std::size_t N>
std::string
to_string(const std::array<T, N>& vec, const char *sep = ", ", const char *beg="[", const char *end="]")
{
	std::ostringstream oss;
	oss << beg;
	for (size_t i = 0; i < vec.size(); ++i) {
		if (i != 0) {
			oss << sep;
		}
		oss << vec[i];
	}
	oss << end;
	return oss.str();
}

template <typename T>
std::string
to_string(const std::ranges::subrange<T> &range, const char *sep = ", ", const char *beg="[", const char *end="]")
{
	std::ostringstream oss;
	oss << beg;
	bool first = true;
	for (const auto &elem: range) {
		if (!first)
			oss << sep;

		if constexpr (std::is_same_v<typename T::value_type, uint8_t>)
			oss << static_cast<int>(elem);
		else
			oss << elem << sep;
	}
	oss << end;
	return oss.str();
}


} // ncr::
