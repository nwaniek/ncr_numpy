/*
 * type_operators.hpp - special operators for basic types used in ncr
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#pragma once

#include <iostream>
#include <vector>
#include <ranges>


namespace ncr {

template <typename T>
inline std::ostream&
operator<<(std::ostream &os, const std::ranges::subrange<T> &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		os << (*it);
	return os;
}


template <typename T>
inline std::ostream&
operator<<(std::ostream &os, const std::vector<T> &vec)
{
	for (auto it = vec.begin(); it != vec.end(); ++it)
		os << (*it);
	return os;
}


} // ncr::
