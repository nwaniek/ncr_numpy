/*
 * declarations.hpp - interface and template declarations (without implementation)
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */
#pragma once

namespace ncr { namespace numpy {

// test to see if something is a structured array
template <typename T> bool is_structured_array(const T&);

}} // ncr::numpy
