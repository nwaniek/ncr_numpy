/*
 * utils.hpp - ncr utility functions and macros
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <type_traits>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "types.hpp"

namespace ncr {

/*
 * memory_guard - A simple memory scope guard
 *
 * This template allows to pass in several pointers which will be deleted when
 * the guard goes out of scope. This is the same as using a unique_ptr into
 * which a pointer is moved, but with slightly less verbose syntax. This is
 * particularly useful to scope members of structs / classes.
 *
 * Example:
 *
 *     struct MyStruct {
 *         SomeType *var;
 *
 *         void some_function
 *         {
 *             // for some reason, var should live only as long as some_function
 *             // is running. This can be useful in the case of recursive
 *             // functions to which sending all the context or state variables
 *             // is inconvenient, and save them in the surrounding struct. An
 *             // alternative, maybe even a preferred way, is to use PODs that
 *             // contain state and use free functions. Still, the memory guard
 *             // might be handy
 *             var = new SomeType{};
 *             memory_guard<SomeType> guard(var);
 *
 *             ...
 *
 *             // the guard will call delete on `var' once it drops out of scope
 *         }
 *     };
 *
 * Example with unique_ptr:
 *
 *            // .. struct is same as above
 *            var = new SomeType{};
 *            std::unique_ptr<SomeType>(std::move(*var));
 *
 * Yes, this only saves a few characters to type. However, memory_guard works
 * with an arbitrary number of arguments.
 */
template <typename... Ts>
struct memory_guard;

template <>
struct memory_guard<> {};

template <typename T, typename... Ts>
struct memory_guard<T, Ts...> : memory_guard<Ts...>
{
	T *ptr = nullptr;
	memory_guard(T *_ptr, Ts *...ptrs) : memory_guard<Ts...>(ptrs...), ptr(_ptr) {}
	~memory_guard() { if (ptr) delete ptr; }
};


/*
 * to_underlying - convert a type to its underlying type
 *
 * This will be available in C++23 as std::to_underlying. Until then, use the
 * implementation here.
 */
template <typename E>
constexpr typename std::underlying_type<E>::type
to_underlying(E e) noexcept {
	return static_cast<typename std::underlying_type<E>::type>(e);
}


/*
 * NCR_DEFINE_ENUM_FLAG_OPERATORS - define all binary operators used for flags
 *
 * This macro expands into functions for bit-wise and binary operations on
 * enums, e.g. given two enum values a and b, one might want to write `a |= b;`.
 * With the macro below, this will be possible.
 */
#define NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_T) \
	inline ENUM_T operator~(ENUM_T a) { return static_cast<ENUM_T>(~ncr::to_underlying(a)); } \
	inline ENUM_T operator|(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T operator&(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T operator^(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); } \
	inline ENUM_T& operator|=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T& operator&=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T& operator^=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); }


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
 * get_file_size - get the file size of an ifstream in bytes
 */
inline u64
get_file_size(std::ifstream &is)
{
	auto ip = is.tellg();
	is.seekg(0, std::ios::end);
	auto res = is.tellg();
	is.seekg(ip);
	return static_cast<u64>(res);
}


/*
 * buffer_from_file - fill an u8_vector by (binary) reading a file
 */
inline bool
buffer_from_file(std::filesystem::path filepath, u8_vector &buffer)
{
	std::ifstream fstream(filepath, std::ios::binary | std::ios::in);
	if (!fstream) {
		std::cerr << "Error opening file: " << filepath << std::endl;
		return false;
	}

	// resize buffer and read
	auto filesize = get_file_size(fstream);
	buffer.resize(filesize);
	fstream.read(reinterpret_cast<char*>(buffer.data()), filesize);

	// check for errors
	if (!fstream) {
		std::cerr << "Error reading file: " << filepath << std::endl;
		return false;
	}
	return true;
}


} // ncr::
