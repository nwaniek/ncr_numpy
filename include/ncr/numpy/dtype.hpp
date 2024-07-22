/*
 * dtype.hpp - dtype struct definition and functions
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#pragma once

#include <algorithm>
#include <unordered_map>
#include <ncr/core/types.hpp>
#include "core.hpp"

namespace ncr { namespace numpy {


/*
 * dtype - data type description of elements in the ndarray
 *
 * In case of structured arrays, only the values in the fields might be properly
 * filled in. Note also that structured arrays can have types with arbitrarily
 * deep nesting of sub-structures. To determine if a (sub-)dtype is a structured
 * array, you can query is_structured_array(), which simply tests if there are
 * fields within this particular dtype.
 *
 * Note, however, that nested fields themselves are dtypes. That means that
 * fields which are leaves of a structured array, and therefore basic types,
 * will return false in a call to is_structured_array.
 *
 * Furet note that structured arrays might contain types with mixed endianness.
 */
struct dtype
{
	// name of the field in case of strutured arrays. for basic types this is
	// empty.
	std::string
		name       = "";

	// byte order of the data
	byte_order
		endianness = byte_order::native;

	// single character type code (see table at start of this file)
	u8
		type_code  = 0;

	// size of the data in bytes, e.g. bytes of an integer or characters in a
	// unicode string
	u32
		size       = 0;

	// size of an item in bytes in this dtype (e.g. U16 is a 16-character
	// unicode string, each character using 4 bytes.  hence, item_size = 64
	// bytes).
	u64
		item_size  = 0;

	// offset of the field if this is a field in a structured array, otherwise
	// this will be (most likely) 0
	u64
		offset     = 0;

	// numpy's shape has python 'int', which is commonly a 64bit integer. see
	// python's sys.maxsize to get the maximum value, log(sys.maxsize,2)+1 will
	// tell the number of bits used on a machine. Here, we simply assume that
	// a u64 is enough.
	std::vector<u64>
		shape      = {};

	// structured arrays will contain fields, which are themselves dtypes.
	//
	// Note: We store them in a vector because we need to retain the insert
	// order. We could also use a map instead, but then we would have to make
	// sure that we somehow store the insert order.
	std::vector<dtype>
		fields     = {};

	std::unordered_map<std::string, size_t>
		field_indexes  = {};
};


inline bool
is_structured_array(const dtype &dt)
{
	return !dt.fields.empty();
}


inline
const dtype*
find_field(const dtype &dt, const std::string& field_name)
{
	auto it = dt.field_indexes.find(field_name);
	if (it == dt.field_indexes.end())
		return nullptr;
	return &dt.fields[it->second];
}


template <typename First, typename... Rest>
const dtype*
get_nested_dtype(const dtype &dt, const First& first, const Rest&... rest)
{
	const dtype* next_dt = find_field(dt, first);
	if (!next_dt)
		return nullptr;

	if constexpr (sizeof...(rest) == 0)
		return next_dt;
	else
		return get_nested_dtype(*next_dt, rest...);
}


template <typename T>
dtype&
add_field(dtype &dt, T &&field)
{
	dt.fields.push_back(std::forward<T>(field));
	dtype& result = dt.fields.back();
	dt.field_indexes.insert({result.name, dt.fields.size() - 1});
	return result;
}


template <typename Func>
void
for_each_field(dtype &dt, Func &&func)
{
	std::for_each(dt.fields.begin(), dt.fields.end(), std::forward<Func>(func));
}


template <typename Func>
void
for_each_field(const dtype &dt, Func &&func)
{
	std::for_each(dt.fields.begin(), dt.fields.end(), std::forward<Func>(func));
}


template <typename Func>
void
for_each(std::vector<dtype> &fields, Func &&func)
{
	std::for_each(fields.begin(), fields.end(), std::forward<Func>(func));
}

template <typename Func>
void
for_each(const std::vector<dtype> &fields, Func &&func)
{
	std::for_each(fields.begin(), fields.end(), std::forward<Func>(func));
}


//
// basic dtypes
//
inline dtype dtype_int16()  { return {.type_code = 'i', .size=2, .item_size=2}; }
inline dtype dtype_int32()  { return {.type_code = 'i', .size=4, .item_size=4}; }
inline dtype dtype_int64()  { return {.type_code = 'i', .size=8, .item_size=8}; }

inline dtype dtype_uint16() { return {.type_code = 'u', .size=2, .item_size=2}; }
inline dtype dtype_uint32() { return {.type_code = 'u', .size=4, .item_size=4}; }
inline dtype dtype_uint64() { return {.type_code = 'u', .size=8, .item_size=8}; }

inline dtype dtype_float16() { return {.type_code = 'f', .size=2, .item_size=2}; }
inline dtype dtype_float32() { return {.type_code = 'f', .size=4, .item_size=4}; }
inline dtype dtype_float64() { return {.type_code = 'f', .size=8, .item_size=8}; }


//
// forward declarations (required due to indirect recursion)
//
inline void serialize_dtype(std::ostream &s, const dtype &dt);
inline void serialize_dtype_descr(std::ostream &s, const dtype &dt);
inline void serialize_dtype_fields(std::ostream &s, const dtype &dt);
inline void serialize_dtype_typestr(std::ostream &s, const dtype &dt);
inline void serialize_fortran_order(std::ostream &s, storage_order o);
inline void serialize_shape(std::ostream &s, const u64_vector &shape);


inline void
serialize_dtype_typestr(std::ostream &s, const dtype &dt)
{
	s << "'" << to_char(dt.endianness) << dt.type_code << dt.size << "'";
}


inline void
serialize_shape(std::ostream &s, const u64_vector &shape)
{
	s << "(";
	for (auto size: shape)
		s << size << ",";
	s << ")";
}


inline void
serialize_dtype_fields(std::ostream &s, const dtype &dt)
{
	s << "[";
	size_t i = 0;
	for (auto &f: dt.fields) {
		if (i++ > 0) s << ", ";
		serialize_dtype(s, f);
	}
	s << "]";
}


inline void
serialize_dtype(std::ostream &s, const dtype &dt)
{
	s << "('" << dt.name << "', ";
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else {
		serialize_dtype_typestr(s, dt);
		if (dt.shape.size() > 0) {
			s << ", ";
			serialize_shape(s, dt.shape);
		}
	}
	s << ")";
}


inline void
serialize_dtype_descr(std::ostream &s, const dtype &dt)
{
	s << "'descr': ";
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else
		serialize_dtype_typestr(s, dt);
}

inline void
serialize_fortran_order(std::ostream &s, storage_order o)
{
	s << "'fortran_order': " << (o == storage_order::col_major ? "True" : "False");
}


/*
 * operator<< - pretty print a dtype
 */
inline std::ostream&
operator<< (std::ostream &os, const dtype &dt)
{
	std::ostringstream s;
	if (is_structured_array(dt))
		serialize_dtype_fields(s, dt);
	else
		serialize_dtype_typestr(s, dt);
	os << s.str();
	return os;
}


}} // ncr::numpy
