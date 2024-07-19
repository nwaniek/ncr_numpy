/*
 * utils.hpp - ncr utility functions and macros
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <iomanip>
#include <iostream>
#include <optional>
#include "types.hpp"

namespace ncr {


/*
 * hexdump - print an u8_vector similar to hex editor displays
 */
inline void
hexdump(std::ostream& os, const u8_vector &data)
{
	// record current formatting
	std::ios old_state(nullptr);
	old_state.copyfmt(std::cout);

	const size_t bytes_per_line = 16;
	for (size_t offset = 0; offset < data.size(); offset += bytes_per_line) {
		os << std::setw(8) << std::setfill('0') << std::hex << offset << ": ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			// missing bytes will be replaced with whitespace
			if (offset + i < data.size())
				os << std::setw(2) << std::setfill('0') << std::hex << static_cast<i32>(data[offset + i]) << ' ';
			else
				os << "   ";
		}
		os << " | ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			if (offset + i >= data.size())
				break;

			// non-printable characters will be replaced with '.'
			char c = data[offset + i];
			if (c < 32 || c > 126)
				c = '.';
			os << c;
		}
		os << "\n";
	}
	// reset to previous state
	std::cout.copyfmt(old_state);
}


/*
 * The following two tables define converters from string to other types. The
 * first table contains the types for which a 'standard implementation' for the
 * template specialization can be generated, whereas the second table contains
 * those for which manual implementations of the conversion function is defined
 * below.
 *
 * The lists are also used to generate functions with C-names, in case a user
 * does or can not use the templated version, or in case a C interface needs to
 * be used. Note that the C-named functions still return an std::optional, so
 * they cannot directly be externed to C.
 */
#define STOX_STANDARD_TYPES(_) \
	_(stoi, int)           \
	_(stou, unsigned)      \
	_(stof, float)         \
	_(stod, double)

#define STOX_SPECIAL_TYPES(_)  \
	_(stob, bool)          \
	_(stoc, char)          \
	_(stos, std::string)


/*
 * str_to_type<T> - signature for the templated conversion function
 */
template <typename T>
std::optional<T>
str_to_type(std::string);


/*
 * str_to_type<bool> - convert from string to bool.
 *
 * This function allows to pass in '0', '1', 'false', 'true' to get
 * corresponding results in boolean type.
 */
template <>
inline std::optional<bool>
str_to_type(std::string s)
{
	bool result;

	std::istringstream is(s);
	is >> result;

	if (is.fail()) {
		is.clear();
		is >> std::boolalpha >> result;
	}
	return is.fail() ? std::nullopt : std::optional<bool>{result};
}


/*
 * str_to_type<char> - convert a string to char
 *
 * More precisely, this function extracts the first character from the string
 * (if available) and \0 otherwise.
 */
template<>
inline std::optional<char>
str_to_type(std::string s)
{
	if (!s.length())
		return '\0';
	return s[0];
}


/*
 * str_to_type<std::string> - return the argument
 *
 * This function simply returns the argument, as a conversion from string to
 * string is its identity.
 */
template <>
inline std::optional<std::string>
str_to_type(std::string s)
{
	return s;
}


/*
 * automatically generate code for standard type implementations, because they
 * are all the same. This could also be achieved by having the implementation in
 * the base template case, but then the template would also be defined for other
 * types, which might not be desirable. Hence, rather let the compiler throw an
 * error in case someone tries to call str_to_type on a type which is not
 * known.
 */
#define X_TEMPLATE_IMPL(_1, T)                                      \
	template <>                                                     \
	inline std::optional<T>                                         \
	str_to_type(std::string s)                                  \
	{                                                               \
		T result;                                                   \
		std::istringstream is(s);                                   \
		is >> result;                                               \
		return is.fail() ? std::nullopt : std::optional<T>{result}; \
	}

STOX_STANDARD_TYPES(X_TEMPLATE_IMPL)
#undef X_TEMPLATE_IMPL


// generate alias functions with c-like function names for all types
#define X_ALIAS_IMPL(FN_NAME, T)      \
	inline std::optional<T>           \
	FN_NAME(std::string s) {          \
		return str_to_type<T>(s); \
	}

STOX_STANDARD_TYPES(X_ALIAS_IMPL)
STOX_SPECIAL_TYPES(X_ALIAS_IMPL)
#undef X_ALIAS_IMPL

#undef STOX_SPECIAL_TYPES
#undef STOX_STANDARD_TYPES



#define XTOS_STANDARD_TYPES(_) \
	_(itos, int)           \
	_(utos, unsigned)      \
	_(ftos, float)         \
	_(dtos, double)        \
	_(ctos, char)          \

#define XTOS_SPECIAL_TYPES(_)  \
	_(btos, bool)          \
	_(stos, std::string)


// TODO: r-value?
template <typename T>
inline std::optional<std::string>
type_to_str(T &value);


template <>
inline std::optional<std::string>
type_to_str(bool &value)
{
	std::ostringstream os;
	os << std::boolalpha << value;
	return os.fail() ? std::nullopt : std::optional{os.str()};
}


template <>
inline std::optional<std::string>
type_to_str(std::string &value)
{
	return value;
}


// TODO: find nicer workaround for this and auto-generate it
template <typename T>
inline void
_v_to_os(std::ostringstream &os, std::vector<T> vs)
{
	size_t i = 0;
	for (auto &v : vs) {
		if (i > 0)
			os << " ";
		os << v;
		i++;
	}
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<int> &vs)
{
	std::ostringstream os;
	_v_to_os<int>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<unsigned> &vs)
{
	std::ostringstream os;
	_v_to_os<unsigned>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<float> &vs)
{
	std::ostringstream os;
	_v_to_os<float>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}


#define X_TEMPLATE_IMPL(_1, T)                                     \
	template <>                                                    \
	inline std::optional<std::string>                              \
	type_to_str(T &value)                                      \
	{                                                              \
		std::ostringstream os;                                     \
		os << value;                                               \
		return os.fail() ? std::nullopt : std::optional{os.str()}; \
	}

XTOS_STANDARD_TYPES(X_TEMPLATE_IMPL)
#undef X_TEMPLATE_IMPL

// generate alias functions with c-like function names
#define X_ALIAS_IMPL(FN_NAME, T)       \
	inline std::optional<std::string>  \
	FN_NAME(T &value) {                \
		return type_to_str(value); \
	}

XTOS_STANDARD_TYPES(X_ALIAS_IMPL)
XTOS_SPECIAL_TYPES(X_ALIAS_IMPL)
#undef X_ALIAS_IMPL

#undef XTOS_SPECIAL_TYPES
#undef XTOS_STANDARD_TYPES


/*
 * A simple define to reduce the verbosity to declare a tuple. This is
 * particularly useful, for instance, in calls to random.hpp:random_coord.
 * In this example, the template accepts a variadic number of tuples, e.g.
 *
 *     auto xlim = std::tuple{0, 1};
 *     auto ylim = std::tuple{0, 10};
 *     auto coord = random_coord(rng, xlim, ylim);
 *
 * It would be better to avoid the temporary variables. However,
 * brace-initializers wont work, as there is no clear (read: acceptably sane)
 * way to turn an initializer_list into a tuple. With the following macro, it is
 * actually possible to succinctly write
 *
 *     auto coord = random_coord(_T(0, 1), _T(0, 10));
 *
 * without any local declaration of temporaries, or overly long calls that
 * include the specific tuple type.
 */
#ifndef _tup
	#define _tup(...) std::tuple{__VA_ARGS__}
#endif


/*
 * get_index_of - get the index of a pointer to T in a vector of T
 *
 * This function returns an optional to indicate if the pointer to T was found
 * or not.
 */
template <typename T>
inline std::optional<size_t>
get_index_of(std::vector<T*> vec, T *needle)
{
	for (size_t i = 0; i < vec.size(); i++)
		if (vec[i] == needle)
			return i;
	return {};
}


/*
 * determine if a container contains a certain element or not
 */
template <typename ContainerT, typename U>
inline bool
contains(const ContainerT &container, const U &needle)
{
	auto it = std::find(container.begin(), container.end(), needle);
	return it != container.end();
}



} // ncr::
