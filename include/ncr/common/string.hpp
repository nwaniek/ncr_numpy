/*
 * string.hpp - common string manipulation functions
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <string>

namespace ncr {

/*
 * strpad - pad a string with whitespace to make it at least length chars long
 */
inline std::string
strpad(const std::string& str, size_t length)
{
    return str + std::string(std::max(length - str.size(), size_t(0)), ' ');
}


} // ncr::
