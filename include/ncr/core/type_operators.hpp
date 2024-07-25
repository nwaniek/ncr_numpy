/*
 * type_operators.hpp - special operators for basic types used in ncr
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#pragma once

#include <iostream>
#include "types.hpp"


namespace ncr {

inline std::ostream&
operator<<(std::ostream &os, const u8_const_subrange &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		os << (*it);
	return os;
}


inline std::ostream&
operator<<(std::ostream &os, const u8_vector &vec)
{
	for (auto it = vec.begin(); it != vec.end(); ++it)
		os << (*it);
	return os;
}


} // ncr::
