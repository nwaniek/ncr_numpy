/*
 * dtype.hpp - dtype struct definition and functions
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#pragma once

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
};


inline bool
is_structured_array(const dtype &dtype)
{
	return !dtype.fields.empty();
}


inline
const dtype*
find_field(const dtype &current_dtype, const std::string& field_name)
{
	for (const auto &field: current_dtype.fields) {
		if (field.name == field_name) {
			return &field;
		}
	}
	return nullptr;
}


template <typename First, typename... Rest>
const dtype*
get_nested_dtype(const dtype &current_dtype, const First& first, const Rest&... rest)
{
	const dtype* next_dtype = find_field(current_dtype, first);
	if (!next_dtype)
		return nullptr;

	if constexpr (sizeof...(rest) == 0)
		return next_dtype;
	else
		return get_nested_dtype(*next_dtype, rest...);
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
inline void serialize_dtype(std::ostream &s, const dtype &dtype);
inline void serialize_dtype_descr(std::ostream &s, const dtype &dtype);
inline void serialize_dtype_fields(std::ostream &s, const dtype &dtype);
inline void serialize_dtype_typestr(std::ostream &s, const dtype &dtype);
inline void serialize_fortran_order(std::ostream &s, storage_order o);
inline void serialize_shape(std::ostream &s, const dtype &dtype);


inline void
serialize_dtype_typestr(std::ostream &s, const dtype &dtype)
{
	s << "'" << to_char(dtype.endianness) << dtype.type_code << dtype.size << "'";
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
serialize_dtype_fields(std::ostream &s, const dtype &dtype)
{
	s << "[";
	size_t i = 0;
	for (auto &f: dtype.fields) {
		if (i++ > 0) s << ", ";
		serialize_dtype(s, f);
	}
	s << "]";
}


inline void
serialize_dtype(std::ostream &s, const dtype &dtype)
{
	s << "('" << dtype.name << "', ";
	if (is_structured_array(dtype))
		serialize_dtype_fields(s, dtype);
	else {
		serialize_dtype_typestr(s, dtype);
		if (dtype.shape.size() > 0) {
			s << ", ";
			serialize_shape(s, dtype.shape);
		}
	}
	s << ")";
}


inline void
serialize_dtype_descr(std::ostream &s, const dtype &dtype)
{
	s << "'descr': ";
	if (is_structured_array(dtype))
		serialize_dtype_fields(s, dtype);
	else
		serialize_dtype_typestr(s, dtype);
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
operator<< (std::ostream &os, const dtype &dtype)
{
	std::ostringstream s;
	if (is_structured_array(dtype))
		serialize_dtype_fields(s, dtype);
	else
		serialize_dtype_typestr(s, dtype);
	os << s.str();
	return os;
}


}} // ncr::numpy
