/*
 * ncr_numpy - read/write numpy files
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

/*
 * This comment is primarily for informational purposes. The interested reader
 * might find it useful, though.
 *
 * numpy character type descriptions
 * =================================
 *
 * The following is a list of the built-in types in numpy. Note that the numpy
 * character is not necessarily used in the export. That is, numpy.save uses the
 * array interface protocol. This protocol specifies signed and unsigned
 * integers with an i and u, respectively, folllowed by the number of bytes. The
 * protocol does not use other characters, which can be used within numpy, such
 * as 'l' or 'q'.
 *
 * The following is the list of character symbols used in the array protocol
 * see also https://numpy.org/doc/stable/reference/arrays.dtypes.html#arrays-dtypes-constructing
 * and https://numpy.org/doc/stable/reference/arrays.interface.html#arrays-interface
 *
 *      't'                 bit field
 *      '?'                 boolean
 *      'b'                 (signed) byte
 *      'B'                 unsigned byte
 *      'i'                 (signed) integer
 *      'u'                 unsigned integer
 *      'f'                 floating-point
 *      'c'                 complex-floating point
 *      'm'                 timedelta
 *      'M'                 datetime
 *      'O'                 (Python) objects
 *      'S', 'a'            zero-terminated bytes (not recommended)
 *      'U'                 Unicode string
 *      'V'                 raw data (void)
 *
 * Moreover, the array interface description consists of 3 parts: character
 * describing the byte order, character code for the basic type, and an integer
 * describing the number of bytes the data type uses.
 *
 * Numpy accepts further characters in their dtype constructor. While they are
 * not found in the array protocol, it might be helpful to have the list and
 * their corresponding C++ types.
 *
 *      numpy name           numpy chr    c/c++ type or equivalent
 *
 *      byte                 b            i8
 *      short                h            i16
 *      int                  i            i32
 *      long                 l            i64
 *      long long            q            long long int
 *      unsigned byte        B            u8
 *      unsigned short       H            u16
 *      unsigned int         I            u32
 *      unsigned long        L            u64
 *      unsigned long long   Q            unsigned long long int
 *
 *      half                 e            std::float16_t / _Float16
 *      single               f            std::float32_t / float
 *      double               d            std::float64_t / double
 *      long double          g            std::float128_t / __float128 / long double
 *
 *      csingle              F            complex64 (2x float32 / float)
 *      cdouble              D            complex128 (2x float64 / double)
 *      clongdouble          G            complex256 (2x long double)
 *
 *      bool                 ?            bool
 *      datetime64           M            maybe use time_point in microseconds based on uint64_t
 *      timedelta64          m            maybe use duration in microseconds based on uint64_t
 *      object_              o
 *
 *      string_, bytes_      S
 *      unicode_, str_       U            unicode string
 *
 * For more information see https://numpy.org/doc/stable/reference/arrays.scalars.html#arrays-scalars-built-in
 *
 */


/*
 * when this is enable, then reading from a file stream will be implemented by
 * directly writing a number of bytes to an array buffer.  While this works on
 * most implementations, it is not guaranteed that a vector is contiguous. To
 * disable this behavior and use a safe variant via the vector's assign and an
 * istreambuf_iterator, set this define to false.
 */
#define NCR_FSTREAM_UNSAFE_READ true


/*
 * include hell
 */
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <map>
#include <variant>
#include <unordered_set>

#include <ncr/ncr_bits.hpp>
#include <ncr/ncr_types.hpp>
#include <ncr/ncr_pyparser.hpp>
#include <ncr/ncr_ndarray.hpp>
#include <ncr/ncr_numpy_zip.hpp>

#ifndef NCR_UTILS


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

#endif


namespace ncr { namespace numpy {

/*
 * npyfile - file information of a numpy file
 *
 */
struct npyfile
{
	// numpy files begin with a magic string of 6 bytes, followed by two bytes
	// bytes that identify the version of the file.
	static constexpr u8 magic_byte_count            = 6;
	static constexpr u8 version_byte_count          = 2;

	// the header size field is either 2 or 4 bytes, depending on the version.
	u8                  header_size_byte_count      = 0;

	// header size in bytes
	u32                 header_size                 = 0;

	// data offset relative to the original file
	u64                 data_offset                 = 0;

	// data (i.e. payload) size. Note that the data size is the size of the raw
	// numpy array data which follows the header. Not to be confused with the
	// data size in dtype
	u64                 data_size                   = 0;

	// file size of the entire file. Note that this is only known when reading
	// from buffers or streams which support seekg (not necessarily the case for
	// named pipes or tcp streams).
	u64                 file_size                   = 0;

	// storage for the magic string.
	u8                  magic[magic_byte_count]     = {};

	// storage for the version
	u8                  version[version_byte_count] = {};

	// the numpy header which describes which data type is stored in this numpy
	// array and how it is stored. Essentially this is a string representation
	// of a python dictionary. Note that the dict can be nested.
	u8_vector           header;

	// prepare for streaming support via non-seekable streams
	bool                streaming = false;
};


/*
 * clear - clear a npyfile
 *
 * This is useful when a npyfile should be used multiple times. to avoid
 * breaking the POD structure of npyfile, this is a free function.
 */
inline void
clear(npyfile &npy)
{
	npy.header_size_byte_count = 0;
	npy.header_size            = 0;
	npy.data_offset            = 0;
	npy.data_size              = 0;
	npy.file_size              = 0;
	for (size_t i = 0; i < npy.magic_byte_count; i++)   npy.magic[i]   = 0;
	for (size_t i = 0; i < npy.version_byte_count; i++) npy.version[i] = 0;
	npy.header.clear();
	npy.streaming              = false;
}


/*
 * npzfile - container for (compressed) archive files
 *
 * Each file in the npz archive itself is an npy file. This struct is returned
 * when loading arrays from a zip archive. Note that the container will have
 * ownership of the arrays and npyfiles stored within.
 */
struct npzfile
{
	// operator[] is mapped to the array, because this is what people expect
	// from using numpy
	ndarray& operator[](std::string name)
	{
		auto where = arrays.find(name);
		if (where == arrays.end())
			throw std::runtime_error(std::string("Key error: No array with name \"") + name + std::string("\""));
		return *where->second.get();
	}

	// the names of all arrays in this file
	std::vector<std::string> names;

	// the npy files associated with each name
	std::map<std::string, std::unique_ptr<npyfile>> npys;

	// the actual array associated with each name
	std::map<std::string, std::unique_ptr<ndarray>> arrays;
};


// retain POD-like structure of npzfile and provide a free function to clear it
inline void
clear(npzfile &npz)
{
	npz.names.clear();
	npz.npys.clear();
	npz.arrays.clear();
}


enum class result : u64 {
	// everything OK
	ok                                     = 0,
	// warnings about missing fields. Note that not all fields are required
	// and it might not be a problem for an application if they are not present.
	// However, inform the user about this state
	warning_missing_descr                  = 1ul << 0,
	warning_missing_fortran_order          = 1ul << 1,
	warning_missing_shape                  = 1ul << 2,
	// error codes. in particular for nested/structured arrays, it might be
	// helpful to know precisely what went wrong.
	error_wrong_filetype                   = 1ul << 3,
	error_file_not_found                   = 1ul << 4,
	error_file_exists                      = 1ul << 5,
	error_file_open_failed                 = 1ul << 6,
	error_file_truncated                   = 1ul << 7,
	error_file_write_failed                = 1ul << 8,
	error_file_read_failed                 = 1ul << 9,
	error_file_close                       = 1ul << 10,
	error_unsupported_file_format          = 1ul << 11,
	error_duplicate_array_name             = 1ul << 12,
	error_magic_string_invalid             = 1ul << 13,
	error_version_not_supported            = 1ul << 14,
	error_header_invalid_length            = 1ul << 15,
	error_header_truncated                 = 1ul << 16,
	error_header_parsing_error             = 1ul << 17,
	error_header_invalid                   = 1ul << 18,
	error_header_empty                     = 1ul << 19,
	error_descr_invalid                    = 1ul << 20,
	error_descr_invalid_type               = 1ul << 21,
	error_descr_invalid_string             = 1ul << 22,
	error_descr_invalid_data_size          = 1ul << 23,
	error_descr_list_empty                 = 1ul << 24,
	error_descr_list_invalid_type          = 1ul << 25,
	error_descr_list_incomplete_value      = 1ul << 26,
	error_descr_list_invalid_value         = 1ul << 27,
	error_descr_list_invalid_shape         = 1ul << 28,
	error_descr_list_invalid_shape_value   = 1ul << 29,
	error_descr_list_subtype_not_supported = 1ul << 30,
	error_fortran_order_invalid_value      = 1ul << 31,
	error_shape_invalid_value              = 1ul << 32,
	error_shape_invalid_shape_value        = 1ul << 33,
	error_item_size_mismatch               = 1ul << 34,
	error_data_size_mismatch               = 1ul << 35,
	error_unavailable                      = 1ul << 36,
};

NCR_DEFINE_ENUM_FLAG_OPERATORS(result);


inline bool
is_error(result r)
{
	return
		r != result::ok &&
		r != result::warning_missing_descr &&
		r != result::warning_missing_shape &&
		r != result::warning_missing_fortran_order;
}


inline std::ostream&
operator<< (std::ostream &os, result result)
{
	switch (result) {
		case result::ok:                                     os << "ok";                                     break;

		case result::warning_missing_descr:                  os << "warning_missing_descr";                  break;
		case result::warning_missing_fortran_order:          os << "warning_missing_fortran_order";          break;
		case result::warning_missing_shape:                  os << "warning_missing_shape";                  break;

		case result::error_wrong_filetype:                   os << "error_wrong_filetype";                   break;
		case result::error_file_not_found:                   os << "error_file_not_found";                   break;
		case result::error_file_exists:                      os << "error_file_exists";                      break;
		case result::error_file_open_failed:                 os << "error_file_open_failed";                 break;
		case result::error_file_truncated:                   os << "error_file_truncated";                   break;
		case result::error_file_write_failed:                os << "error_file_write_failed";                break;
		case result::error_file_read_failed:                 os << "error_file_read_failed";                 break;
		case result::error_file_close:                       os << "error_file_close";                       break;
		case result::error_unsupported_file_format:          os << "error_unsupported_file_format";          break;
		case result::error_duplicate_array_name:             os << "error_duplicate_array_name";             break;

		case result::error_magic_string_invalid:             os << "error_magic_string_invalid";             break;
		case result::error_version_not_supported:            os << "error_version_not_supported";            break;
		case result::error_header_invalid_length:            os << "error_header_invalid_length";            break;
		case result::error_header_truncated:                 os << "error_header_truncated";                 break;
		case result::error_header_parsing_error:             os << "error_header_parsing_error";             break;
		case result::error_header_invalid:                   os << "error_header_invalid";                   break;
		case result::error_header_empty:                     os << "error_header_empty";                     break;

		case result::error_descr_invalid:                    os << "error_descr_invalid";                    break;
		case result::error_descr_invalid_type:               os << "error_descr_invalid_type";               break;
		case result::error_descr_invalid_string:             os << "error_descr_invalid_string";             break;
		case result::error_descr_invalid_data_size:          os << "error_descr_invalid_data_size";          break;
		case result::error_descr_list_empty:                 os << "error_descr_list_empty";                 break;
		case result::error_descr_list_invalid_type:          os << "error_descr_list_invalid_type";          break;
		case result::error_descr_list_incomplete_value:      os << "error_descr_list_incomplete_value";      break;
		case result::error_descr_list_invalid_value:         os << "error_descr_list_invalid_value";         break;
		case result::error_descr_list_invalid_shape:         os << "error_descr_list_invalid_shape";         break;
		case result::error_descr_list_invalid_shape_value:   os << "error_descr_list_invalid_shape_value";   break;
		case result::error_descr_list_subtype_not_supported: os << "error_descr_list_subtype_not_supported"; break;

		case result::error_fortran_order_invalid_value:      os << "error_fortran_order_invalid_value";      break;
		case result::error_shape_invalid_value:              os << "error_shape_invalid_value";              break;
		case result::error_shape_invalid_shape_value:        os << "error_shape_invalid_shape_value";        break;
		case result::error_item_size_mismatch:               os << "error_item_size_mismatch";               break;
		case result::error_data_size_mismatch:               os << "error_data_size_mismatch";               break;
		case result::error_unavailable:                      os << "error_unavailable";                      break;

		// this should never happen
		default: os.setstate(std::ios_base::failbit);
	}
	return os;
}


inline byte_order
to_byte_order(const u8 chr)
{
	switch (chr) {
	case '>': return byte_order::big;
	case '<': return byte_order::little;
	case '=': return byte_order::native;
	case '|': return byte_order::not_relevant;
	default:  return byte_order::invalid;
	};
}


inline std::ostream&
operator<<(std::ostream &os, const byte_order &order)
{
	switch (order) {
	case byte_order::little:       os << '<'; break;
	case byte_order::big:          os << '>'; break;
	case byte_order::not_relevant: os << '|'; break;
	default:                       os.setstate(std::ios_base::failbit);
	};
	return os;
}


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


/*
 * istream interface is currently not directly supported because we read the
 * file in one go. Nevertheless, the functions below are kept as a starting
 * point for implementing streaming data
 */
#if 0
inline result
read_magic_string(std::istream &is, npy_file &finfo)
{
	// numpy file header begins with \x93NUMPY
	is.read((char*)finfo.magic, finfo.magic_byte_count);
	if (is.fail() && is.eof())
		return result::error_file_truncated;
	return result::ok;
}


inline result
read_version(std::istream &is, npy_file &finfo)
{
	// 2 bytes: major / minor version
	is.read((char*)finfo.version, finfo.version_byte_count);
	if (is.fail() && is.eof())
		return result::error_file_truncated;
	return result::ok;
}


inline result
read_header_length(std::istream &is, npy_file &finfo)
{
	// attempt to read the header length
	u8 buffer[4] = {};
	is.read((char*)buffer, finfo.size_byte_count);
	if (is.fail() && is.eof())
		return result::error_file_truncated;

	finfo.header_byte_count = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
	return result::ok;
}


inline result
read_header(std::istream &is, npy_file &finfo)
{
	// read the stream header into the buffer

	// read and parse the header
	finfo.header.resize(finfo.header_byte_count);
	is.read((char*)finfo.header.data(), finfo.header_byte_count);
	if (is.fail() && is.eof())
		return result::error_header_truncated;

	return result::ok;
}
#endif


inline result
validate_magic_string(const u8_vector &buffer, ssize_t &bufpos, npyfile &npy)
{
	if (buffer.size() - bufpos < npy.magic_byte_count)
		return result::error_magic_string_invalid;

	for (size_t i = 0; i < npy.magic_byte_count; i++)
		npy.magic[i] = buffer[bufpos++];

	if (npy.magic[0] != 0x93 ||
	    npy.magic[1] != 'N'  ||
	    npy.magic[2] != 'U'  ||
	    npy.magic[3] != 'M'  ||
	    npy.magic[4] != 'P'  ||
	    npy.magic[5] != 'Y')
		return result::error_magic_string_invalid;

	return result::ok;
}


inline result
validate_version(const u8_vector &buffer, ssize_t &bufpos, npyfile &npy)
{
	// TODO: more specific error
	if (buffer.size() - bufpos < npy.version_byte_count)
		return result::error_file_truncated;

	for (size_t i = 0; i < npy.version_byte_count; i++)
		npy.version[i] = buffer[bufpos++];

	// currently, only 1.0 and 2.0 are supported
	if ((npy.version[0] != 0x01 && npy.version[0] != 0x02) || (npy.version[1] != 0x00))
		return result::error_version_not_supported;

	// set the size byte count (which depends on the version)
	if (npy.version[0] == 0x01)
		npy.header_size_byte_count = 2;
	else
		npy.header_size_byte_count = 4;

	return result::ok;
}


inline result
validate_header_length(const u8_vector &buffer, ssize_t &bufpos, npyfile &npy)
{
	// TODO: more specific error message
	if (buffer.size() - bufpos < npy.header_size_byte_count)
		return result::error_file_truncated;

	npy.header_size = 0;
	for (size_t i = 0; i < npy.header_size_byte_count; i++)
		npy.header_size |= buffer[bufpos++] << (i * 8);

	// validate the length: len(magic string) + 2 + len(length) + HEADER_LEN must be divisible by 64
	npy.data_offset = npy.magic_byte_count + npy.version_byte_count + npy.header_size_byte_count + npy.header_size;
	if (npy.data_offset % 64 != 0)
		return result::error_header_invalid_length;

	return result::ok;
}


inline result
validate_header(const u8_vector &buffer, ssize_t &bufpos, npyfile &npy)
{
	if (buffer.size() - bufpos < npy.header_size)
		return result::error_header_truncated;

	// reserve and copy
	npy.header.reserve(npy.header_size);
	std::copy(buffer.begin() + bufpos, buffer.begin() + bufpos + npy.header_size, std::back_inserter(npy.header));
	bufpos += npy.header_size;

	// test the size
	if (npy.header.size() < npy.header_size)
		return result::error_header_truncated;

	return result::ok;
}


inline result
parse_descr_string(pyparser::parse_result *descr, dtype &dtype)
{
	// sanity check: test if the data type is actually a string or not
	if (descr->dtype != pyparser::type::string)
		return result::error_descr_invalid_string;

	auto range = descr->range();
	if (range.size() < 3)
		return result::error_descr_invalid_string;

	// first character is the byte order
	dtype.endianness = to_byte_order(range[0]);
	// second character is the type code
	dtype.type_code  = range[1];
	// remaining characters are the byte size of the data type
	std::string str(range.begin() + 2, range.end());

	// TODO: use something else than strtol
	char *end;
	dtype.size = std::strtol(str.c_str(), &end, 10);
	if (*end != '\0') {
		dtype.size = 0;
		return result::error_descr_invalid_data_size;
	}
	return result::ok;
}


inline result
parse_descr_list(pyparser::parse_result *descr, dtype &dtype)
{
	// the descr field of structured arrays is a list of tuples (n, t, s), where
	// n is the name of the field, t is the type, s is the shape. Note that the
	// shape is optional. Moreover, t could be a list of sub-types, which might
	// recursively contain further subtypes.

	if (descr->nodes.size() == 0)
		return result::error_descr_list_empty;

	for (auto &node: descr->nodes) {
		// check data type of the node
		if (node->dtype != pyparser::type::tuple)
			return result::error_descr_list_invalid_type;

		// needs at least 2 subnodes, i.e. tuple (n, t)
		if (node->nodes.size() < 2)
			return result::error_descr_list_incomplete_value;

		// can have at most 3 subnodes, i.e. tuple (n, t, s)
		if (node->nodes.size() > 3)
			return result::error_descr_list_invalid_value;

		// first field: name
		dtype.fields.push_back({.name = std::string(node->nodes[0]->begin, node->nodes[0]->end)});
		auto &field = dtype.fields.back();

		// second field: type description, which is either a string, or in case
		// of sub-structures it is again a list of tuples, which we can parse
		// recursively
		result res;
		switch (node->nodes[1]->dtype) {
			// string?
			case pyparser::type::string:
				if ((res = parse_descr_string(node->nodes[1].get(), field)) != result::ok)
					return res;
				break;

			// recursively go through the list
			case pyparser::type::list:
				if ((res = parse_descr_list(node->nodes[1].get(), field)) != result::ok)
					return res;
				break;

			// currently, other entries are not supported
			default:
				return result::error_descr_list_subtype_not_supported;
		}

		// third field (optional): shape
		if (node->nodes.size() > 2) {
			// test the type. must be a tuple
			if (node->nodes[2]->dtype != pyparser::type::tuple)
				return result::error_descr_list_invalid_shape;

			for (auto &n: node->nodes[2]->nodes) {
				// must be an integer value
				if (n->dtype != pyparser::type::integer)
					return result::error_descr_list_invalid_shape_value;
				field.shape.push_back(n->value.l);
			}
		}
	}

	return result::ok;
}


inline result
parse_descr(pyparser::parse_result *descr, dtype &dtype)
{
	if (!descr)
		return result::error_descr_invalid;

	switch (descr->dtype) {
		case pyparser::type::string: return parse_descr_string(descr, dtype);
		case pyparser::type::list:   return parse_descr_list(descr, dtype);
		default:                     return result::error_descr_invalid_type;
	}
}


inline result
parse_header(npyfile &finfo, dtype &dtype, storage_order &order, u64_vector &shape)
{
	// the header of a numpy file is an ASCII-string terminated by \n and padded
	// with 0x20 (whitespace), i.e. string\x20\x20\x20...\n, where string is a
	// literal expression of a Python dictionary.
	//
	// Generally, numpy files have a header dict with fields descr, fortran_order
	// and shape. For 'simple' arrays, descr contains a string with a
	// representation of the stored type. For structured arrays, 'descr'
	// consists of a list of tuples (n, t, s), where n is the name of the field,
	// t is the type, s is the [optional] shape.
	//
	// Example: {'descr': '<f8', 'fortran_order': False, 'shape': (3, 3), }
	//
	// see https://numpy.org/doc/stable/reference/arrays.interface.html#arrays-interface
	// for more information
	//
	// This function uses a (partial) python parser. on success, it will examine the
	// parse result and turn it into classical dtype information. While this
	// information is already within the parse result, there's no need to
	// transport all of the parsing details the the user. Also, it shouldn't
	// take much time to convert and thus have negligible impact on performance.

	// try to parse the header
	ncr::pyparser parser;
	auto pres = parser.parse(finfo.header);
	if (!pres)
		return result::error_header_parsing_error;

	// header must be one parse-node of type dict
	if (pres->nodes.size() != 1 || pres->nodes[0]->dtype != pyparser::type::dict)
		return result::error_header_invalid;

	// the dict itself must have child-nodes
	auto &root_dict = pres->nodes[0];
	if (root_dict->nodes.size() == 0)
		return result::error_header_empty;

	// the result code contains warnings for all fields. they will be disabled
	// during parsing below if they are discovered
	result res = result::warning_missing_descr | result::warning_missing_fortran_order | result::warning_missing_shape;

	// we are not to assume any order of the entries in the dict (albeit they
	// are normally ordered alphabetically). The parse result of the entires are
	// kvpairs, each having the key as node 0 and value as node 1
	for (auto &kv: root_dict->nodes) {
		// check the parsed type for consistency
		if (kv->dtype != pyparser::type::kvpair || kv->nodes.size() != 2)
			return result::error_header_invalid;

		// descr, might be a string or a list of tuples
		if (equals(kv->nodes[0]->range(), "descr")) {
			auto tmp = parse_descr(kv->nodes[1].get(), dtype);
			if (tmp != result::ok)
				return tmp;
			res &= ~result::warning_missing_descr;
		}

		// determine if the array data is in fortran order or not
		if (equals(kv->nodes[0]->range(), "fortran_order")) {
			if (kv->nodes[1]->dtype != pyparser::type::boolean)
				return result::error_fortran_order_invalid_value;
			order = kv->nodes[1]->value.b ? storage_order::col_major : storage_order::row_major;
			res &= ~result::warning_missing_fortran_order;
		}

		// read the shape of the array (NOTE: this is *not* the shape of a data
		// type, but the shape of the array)
		if (equals(kv->nodes[0]->range(), "shape")) {
			if (kv->nodes[1]->dtype != pyparser::type::tuple)
				return result::error_shape_invalid_value;

			// read each shape value
			shape.clear();
			for (auto &n: kv->nodes[1]->nodes) {
				// must be an integer value
				if (n->dtype != pyparser::type::integer)
					return result::error_shape_invalid_shape_value;
				shape.push_back(n->value.l);
			}
			res &= ~result::warning_missing_shape;
		}
	}

	return res;
}


inline result
compute_item_size(dtype &dtype)
{
	// for simple arrays, we simple report the item size
	if (!dtype.is_structured_array()) {
		// most types have their 'width' in bytes given directly in the
		// descr string, which is already stored in dtype.size. However, unicode
		// strings and objects differ in that the size that is given in the
		// dtype is not the size in bytes, but the number of 'elements'. In case
		// of unicude, U16 means a unicode string with 16 unicode characters,
		// each character taking up 4 bytes.
		u64 multiplier;
		switch (dtype.type_code) {
			case 'O': multiplier = 8; break;
			case 'U': multiplier = 4; break;
			default:  multiplier = 1;
		}
		dtype.item_size = multiplier * dtype.size;

		// if there's a shape attached, multiply it in
		for (auto s: dtype.shape)
			dtype.item_size *= s;
	}
	else {
		// the item_size for structured arrays is (often) 0, in which case we simply
		// update. If the item_size is not 0, double check to determine if there is
		// an item-size mismatch.
		u64 subsize = 0;
		for (auto &field: dtype.fields) {
			result res;
			if ((res = compute_item_size(field)) != result::ok)
				return res;
			subsize += field.item_size;
		}
		if (dtype.item_size != 0 && dtype.item_size != subsize)
			return result::error_item_size_mismatch;
		dtype.item_size = subsize;
	}
	return result::ok;
}


inline result
validate_data_size(const npyfile &finfo, const dtype &dtype, u64 &size)
{
	// detect if data is truncated
	if (finfo.data_size % dtype.item_size != 0)
		return result::error_data_size_mismatch;
	size = finfo.data_size / dtype.item_size;
	return result::ok;
}


inline result
compute_data_size(const u8_vector &buffer, ssize_t bufpos, npyfile &finfo)
{
	// TODO: for streaming objects, cannot compute the size (potentially missing
	// seekg, tellg)
	if (finfo.streaming)
		return result::ok;

	finfo.data_size = buffer.size() - bufpos;
	return result::ok;
}


inline result
from_stream(std::istream &)
{
	// TODO: for streaming data, we need read calls in between to fetch the
	// next amount of data from the streambuf_iterator.
	return result::error_unavailable;
}


// simple API interface -> rvalue reference to the buffer. Caller must make sure
// that this is correct or if a copy is required.
inline result
from_buffer(u8_vector &&buf, npyfile &npy, ndarray &array)
{
	result res = result::ok;

	// setup the finfo struct
	npy.streaming = false;

	// parts of the array description (will be moved into the array later)
	dtype         dtype;
	u64_vector    shape;
	storage_order order;
	u64           size;

	// go through each step (will update bufpos along the way)
	ssize_t bufpos = 0;
	if ((res |= validate_magic_string(buf, bufpos, npy) , is_error(res))) return res;
	if ((res |= validate_version(buf, bufpos, npy)      , is_error(res))) return res;
	if ((res |= validate_header_length(buf, bufpos, npy), is_error(res))) return res;
	if ((res |= validate_header(buf, bufpos, npy)       , is_error(res))) return res;
	if ((res |= compute_data_size(buf, bufpos, npy)     , is_error(res))) return res;
	if ((res |= parse_header(npy, dtype, order, shape)  , is_error(res))) return res;
	if ((res |= compute_item_size(dtype)                , is_error(res))) return res;
	if ((res |= validate_data_size(npy, dtype, size)    , is_error(res))) return res;

	// erase the entire header block. what's left is the raw data of the ndarray
	buf.erase(buf.begin(), buf.begin() + npy.data_offset);

	// build the ndarray from the data that we read by moving into it
	array.assign(std::move(dtype), std::move(shape), std::move(buf), order);

	return res;
}


inline bool
is_zip_file(std::istream &is)
{
	// each zip file starts with a local file header signature of 4 bytes and
	// the value 0x04034b50. For more information see the ZIP file format
	// specification: https://pkware.cachefly.net/webdocs/APPNOTE/APPNOTE-6.3.9.TXT
	u8 buffer[4];
	is.read((char*)buffer, 4);
	return buffer[0] == 0x50 &&
	       buffer[1] == 0x4b &&
	       buffer[2] == 0x03 &&
	       buffer[3] == 0x04;
}


inline u64
get_file_size(std::ifstream &is)
{
	auto ip = is.tellg();
	is.seekg(0, std::ios::end);
	auto res = is.tellg();
	is.seekg(ip);
	return static_cast<u64>(res);
}


inline result
from_zip_archive(std::filesystem::path filepath, npzfile &npz)
{
	// get a zip backend
	zip::backend_state *zip_state      = nullptr;
	zip::backend_interface zip_backend = zip::get_backend_interface();

	zip_backend.make(&zip_state);
	if (zip_backend.open(zip_state, filepath, zip::filemode::read) != zip::result::ok) {
		zip_backend.release(&zip_state);
		return result::error_file_open_failed;
	}

	std::vector<std::string> file_list;
	if (zip_backend.get_file_list(zip_state, file_list) != zip::result::ok) {
		zip_backend.close(zip_state);
		zip_backend.release(&zip_state);
		// TODO: better error return value
		return result::error_file_read_failed;
	}

	// for each archive file, decompress and parse the numpy array
	for (auto &fname: file_list) {
		u8_vector buffer;
		if (zip_backend.read(zip_state, fname, buffer) != zip::result::ok) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return result::error_file_read_failed;
		}

		// remove ".npy" from array name
		std::string array_name = fname.substr(0, fname.find_last_of("."));

		// get a npy file and array
		npyfile *npy = new npyfile;
		ndarray *array = new ndarray;
		result res;
		if ((res = from_buffer(std::move(buffer), *npy, *array)) != result::ok) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return res;
		}

		// store the information in an npz_file
		npz.names.push_back(array_name);
		npz.npys.insert(std::make_pair(array_name, std::make_unique<npyfile>(*npy)));
		npz.arrays.insert(std::make_pair(array_name, std::make_unique<ndarray>(*array)));
	}

	// close the zip backend and release it again
	zip_backend.close(zip_state);
	zip_backend.release(&zip_state);
	return result::ok;
}

inline result
open_fstream(std::filesystem::path filepath, std::ifstream &fstream)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (!fs::exists(filepath))
		return result::error_file_not_found;

	// attempt to read
	fstream.open(filepath, std::ios::binary);
	if (!fstream)
		return result::error_file_open_failed;

	return result::ok;
}


inline result
from_npz(std::filesystem::path filepath, npzfile &npz)
{
	// open the file for file type test
	result res;
	std::ifstream f;
	if ((res = open_fstream(filepath, f)) != result::ok)
		return res;

	// test if this is a PKzip file, also close it again
	bool test = is_zip_file(f);
	f.close();
	if (!test)
		return result::error_wrong_filetype;

	// let the zip backend handle this file from now on
	return from_zip_archive(filepath, npz);
}


inline result
from_npy(std::filesystem::path filepath, ndarray &array, npyfile *npy = nullptr)
{
	// open the file
	result res;
	std::ifstream f;
	if ((res = open_fstream(filepath, f)) != result::ok)
		return res;


	// test if this is a PKzip file, and if yes then we exit early. for loading
	// npz files, use from_npz
	if (is_zip_file(f))
		return result::error_wrong_filetype;

	// read the file into a vector. the c++ iostream interface is horrible to
	// work with and considered bad design by many developers. We'll load the
	// file into a vector (which is not the fastest), but then working with it
	// is reasonably simple
	f.seekg(0);
	auto filesize = get_file_size(f);
	u8_vector buf(filesize);
#if NCR_FSTREAM_UNSAFE_READ
	f.read(reinterpret_cast<char*>(buf.data()), filesize);
#else
	buf.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
#endif

	// if the caller didnt pass in a preallocated object, we'll allocate one
	// because it is needed during from_buffer
	bool tmp_npy = npy == nullptr;
	if (tmp_npy)
		npy = new npyfile;

	// Note the change in argument order!
	res = from_buffer(std::move(buf), *npy, array);

	// delete the temporary if necessary
	if (tmp_npy) {
		delete npy;
		npy = nullptr;
	}

	// done
	return res;
}


typedef std::variant<result, ndarray, npzfile> variant_result;

//
// helpers to extract the type from the variant. If you're sure about the file
// type (npz / npy), it's recommended to directly use the from_* functions
//
inline result  get_result(variant_result   &res) { return std::get<result>(std::forward<variant_result>(res));  }
inline result  get_result(variant_result  &&res) { return std::get<result>(std::forward<variant_result>(res));  }
inline ndarray get_ndarray(variant_result  &res) { return std::get<ndarray>(std::forward<variant_result>(res)); }
inline ndarray get_ndarray(variant_result &&res) { return std::get<ndarray>(std::forward<variant_result>(res)); }
inline npzfile get_npzfile(variant_result  &res) { return std::get<npzfile>(std::forward<variant_result>(res)); }
inline npzfile get_npzfile(variant_result &&res) { return std::get<npzfile>(std::forward<variant_result>(res)); }


/*
 * load - high level API which tries to load whatever file is given
 *
 * In case the file cannot be loaded, i.e. it's not an npz or npy file, the
 * variant will hold a corresponding error code
 */
inline variant_result
load(std::filesystem::path filepath)
{
	// open the file
	result res;
	std::ifstream f;
	if ((res = open_fstream(filepath, f)) != result::ok) {
		return res;
	}

	// return the full npz if this is an archive, so that users can work with
	// the result similar to what they would get from numpy.load
	if (is_zip_file(f)) {
		npzfile npz;
		if ((res = from_npz(filepath, npz)) != result::ok)
			return res;
		return npz;
	}

	// users are usually only interested in the array when loading from .npy
	ndarray arr;
	if ((res = from_npy(filepath, arr, nullptr)) != result::ok) {
		// in case the magic string is invalid, then this is not a numpy file
		if (res == result::error_magic_string_invalid)
			return result::error_unsupported_file_format;
		else
			// something else happened
			return res;
	}
	return arr;
}


/*
 * to_npy_buffer - construct a npy file compatible buffer from ndarray
 */
inline result
to_npy_buffer(const ndarray &arr, u8_vector &buffer)
{
	// initialize default header structure
	buffer = {
		// magic string
		0x93, 'N', 'U', 'M', 'P', 'Y',
		// version (2.0)
		0x02, 0x00,
		// space for header size (version 2.0 -> 4 bytes)
		0x00, 0x00, 0x00, 0x00
	};

	// write the header string
	std::string typedescr = arr.get_type_description();
	std::copy(typedescr.begin(), typedescr.end(), std::back_inserter(buffer));

	// the entire header must be divisible by 64 -> find next bigger. Common
	// formula is simply (((N + divisor - 1) / divisor) * divisor), where
	// divisor = 64.  However, we need to adapt for +1 for the trailing \n that
	// terminates the header
	size_t bufsize = buffer.size();
	size_t total_header_length = ((bufsize + 64) / 64) * 64;

	// fill white whitespace (0x20) and trailing \n
	buffer.resize(total_header_length);
	std::fill(buffer.begin() + bufsize, buffer.end() - 1, 0x20);
	buffer[buffer.size() - 1] = '\n';

	// write the header length (in little endian). Note that the eader length
	// itself is the length without magic string, size, and version bytes and
	// not the total header length!
	size_t header_length = total_header_length
		- ncr::numpy::npyfile::magic_byte_count
		- ncr::numpy::npyfile::version_byte_count
		- 4; // four bytes for the header size (version 2.0 file)

	// do we need to byteswap the header length?
	u8 *buf_hlen = buffer.data() + ncr::numpy::npyfile::magic_byte_count + ncr::numpy::npyfile::version_byte_count;
	if constexpr (std::endian::native == std::endian::big)
		*reinterpret_cast<u32*>(buf_hlen) = ncr::bswap<u32>(header_length);
	else
		*reinterpret_cast<u32*>(buf_hlen) = header_length;

	// copy the rest of the array
	const u8_vector payload = arr.data();
	buffer.insert(buffer.end(), payload.begin(), payload.end());

	return result::ok;
}


inline result
save(std::filesystem::path filepath, const ndarray &arr, bool overwrite=false)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return result::error_file_exists;

	std::ofstream fstream;
	fstream.open(filepath, std::ios::binary | std::ios::out);
	if (!fstream)
		return result::error_file_open_failed;

	// turn the array into a numpy buffer
	result res;
	u8_vector buffer;
	if ((res = to_npy_buffer(arr, buffer)) != result::ok)
		return res;

	// write to file
	fstream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	// size_t is most likely 64bit, but just to make sure cast it again. tellp()
	// can easily be more than 32bit...
	if (fstream.bad() || static_cast<u64>(fstream.tellp()) != static_cast<u64>(buffer.size()))
		return result::error_file_write_failed;

	return result::ok;
}


/*
 * savez_arg - helper object to capture arguments to savez* and save_npz
 */
struct savez_arg
{
	std::string name;
	ndarray     &array;
};


/*
 * save_npz - save arrays to an npz file
 */
inline result
to_zip_archive(std::filesystem::path filepath, std::vector<savez_arg> args, bool compress, bool overwrite=false)
{
	namespace fs = std::filesystem;

	// detect if there are any name clashes
	std::unordered_set<std::string> _set;
	for (auto &arg: args) {
		if (_set.contains(arg.name))
			return result::error_duplicate_array_name;
		_set.insert(arg.name);
	}

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return result::error_file_exists;

	zip::backend_state *zip_state        = nullptr;
	zip::backend_interface zip_interface = zip::get_backend_interface();

	zip_interface.make(&zip_state);
	if (zip_interface.open(zip_state, filepath, zip::filemode::write) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return result::error_file_open_failed;
	}

	// write all arrays. append .npy to each argument name
	for (auto &arg: args) {
		u8_vector buffer;
		to_npy_buffer(arg.array, buffer);
		std::string name = arg.name + ".npy";
		if (zip_interface.write(zip_state, name, std::move(buffer), compress, 0) != zip::result::ok) {
			zip_interface.release(&zip_state);
			return result::error_file_write_failed;
		}
	}

	if (zip_interface.close(zip_state) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return result::error_file_close;
	}

	zip_interface.release(&zip_state);
	return result::ok;
}


/*
 * savez - save name/array pairs to an uncompressed npz file
 */
inline result
savez(std::filesystem::path filepath, std::vector<savez_arg> args, bool overwrite=false)
{
	return to_zip_archive(filepath, std::forward<decltype(args)>(args), false, overwrite);
}


/*
 * savez_compressed - save to compressed npz file
 */
inline result
savez_compressed(std::filesystem::path filepath, std::vector<savez_arg> args, bool overwrite=false)
{
	return to_zip_archive(filepath, std::forward<decltype(args)>(args), true, overwrite);
}


/*
 * savez - save unamed arrays to an uncompressed npz file
 *
 * Note that the arrays will receive a name of the format arr_i, where i is the
 * position in the args vector
 */
inline result
savez(std::filesystem::path filepath, std::vector<std::reference_wrapper<ndarray>> args, bool overwrite=false)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), false, overwrite);
}


/*
 * savez_compressed - save unamed arrays to a compressed npz file
 *
 * Note that the arrays will receive a name of the format arr_i, where i is the
 * position in the args vector
 */
inline result
savez_compressed(std::filesystem::path filepath, std::vector<std::reference_wrapper<ndarray>> args, bool overwrite=false)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), true, overwrite);
}


}} // ncr::numpy

