/*
 * units - Unit specifications
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * This file contains unit literal specifications that are used throghout NCR,
 * and which are handy for other projects. Note that the base unit for distance
 * measurements is cm, while for time it is ms.
 */

#pragma once

namespace ncr {

// time is represented in ms, use the operators to automatically convert between
// units
constexpr long double operator "" _ns  (long double ns)  { return ns / 1000000.0L; }
constexpr long double operator "" _µs  (long double µs)  { return µs / 1000.0L; }
constexpr long double operator "" _ms  (long double ms)  { return ms; }
constexpr long double operator "" _s   (long double s)   { return s * 1000.0L; }
constexpr long double operator "" _min (long double min) { return min * 60.0L * 1000.0L; }

// space is represented in cm
constexpr long double operator "" _µm  (long double µm)  { return µm  *   0.0001L; }
constexpr long double operator "" _mm  (long double mm)  { return mm  *   0.01L; }
constexpr long double operator "" _cm  (long double cm)  { return cm; }
constexpr long double operator "" _dm  (long double dm)  { return dm  *  10.0L; }
constexpr long double operator "" _m   (long double m)   { return m   * 100.0L; }

// voltages are expressed relative to mV
constexpr long double operator "" _mV  (long double mV)  { return mV; }
constexpr long double operator "" _V   (long double V)   { return V * 1000.0L; }

} // ncr::
