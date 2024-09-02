/*
 * npy - read/write numpy files
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
 * include hell
 */
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <map>
#include <variant>
#include <unordered_set>

#include <ncr/core/types.hpp>
#include <ncr/core/bits.hpp>
#include <ncr/core/zip.hpp>
#include <ncr/core/filesystem.hpp>
#include "pyparser.hpp"
#include "ndarray.hpp"


namespace ncr { namespace numpy {

/*
 * forward declarations and typedefs
 */
struct npyfile;
struct npzfile;
enum class result : u64;

typedef std::variant<result, ndarray, npzfile> variant_result;


/*
 * concepts
 */
template <typename T>
concept NDArray = std::derived_from<T, ndarray>;

template<typename F>
concept GenericReaderCallback = requires(F f, const dtype& dt, const u64_vector &shape, const storage_order &order, u64 index, u8_vector item) {
    { f(dt, shape, order, index, std::move(item)) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallbackFlat = requires(F f, u64 index, T value) {
	{ f(index, value) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallbackMulti = requires(F f, u64_vector index, T value) {
	{ f(std::move(index), value) } -> std::same_as<bool>;
};

template <typename T, typename F>
concept TypedReaderCallback = TypedReaderCallbackFlat<T, F> || TypedReaderCallbackMulti<T, F>;

template <typename F>
concept ArrayPropertiesCallback = requires(F f, const dtype &dt, const u64_vector &shape, const storage_order &order) {
	{ f(dt, shape, order) } -> std::same_as<bool>;
};

template <typename Range, typename Tp>
concept Writable = std::ranges::output_range<Range, Tp>;

template <typename T, typename OutputRange>
concept Readable = requires(T source, OutputRange &&dest, std::size_t size) {
	{ source.read(dest, size) } -> std::same_as<std::size_t>;
	{ source.eof() } -> std::same_as<bool>;
	// TODO: maybe also fail()
};


/*
 * npyfile - file information of a numpy file
 *
 */
struct npyfile
{
	// numpy files begin with a magic string of 6 bytes, followed by two bytes
	// bytes that identify the version of the file.
	static constexpr u8
		magic_byte_count            {6};

	static constexpr u8
		version_byte_count          {2};

	// the header size field is either 2 or 4 bytes, depending on the version.
	u8
		header_size_byte_count      {0};

	// header size in bytes
	u32
		header_size                 {0};

	// data offset relative to the original file
	u64
		data_offset                 {0};

	// data (i.e. payload) size. Note that the data size is the size of the raw
	// numpy array data which follows the header. Not to be confused with the
	// data size in dtype
	u64
		data_size                   {0};

	// file size of the entire file. Note that this is only known when reading
	// from buffers or streams which support seekg (not necessarily the case for
	// named pipes or tcp streams).
	u64
		file_size                   {0};

	// storage for the magic string.
	std::array<u8, magic_byte_count>
		magic                       {};

	// storage for the version
	std::array<u8, version_byte_count>
		version                     {};

	// the numpy header which describes which data type is stored in this numpy
	// array and how it is stored. Essentially this is a string representation
	// of a python dictionary. Note that the dict can be nested.
	u8_vector
		header;

	// prepare for streaming support via non-seekable streams
	bool
		streaming                   {false};
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
	npy.magic.fill(0);
	npy.version.fill(0);
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

// map from error code to string for pretty printing the error code. This is a
// bit more involved than just listing the strings, because result codes can be
// OR-ed together, i.e. a result code might have several codes that are set.
constexpr std::array<std::pair<result, const char*>, 38> result_strings = {{
	{result::ok,                                    "ok"},

	{result::warning_missing_descr,                 "warning_missing_descr"},
	{result::warning_missing_fortran_order,         "warning_missing_fortran_order"},
	{result::warning_missing_shape,                 "warning_missing_shape"},

	{result::error_wrong_filetype,                  "error_wrong_filetype"},
	{result::error_file_not_found,                  "error_file_not_found"},
	{result::error_file_exists,                     "error_file_exists"},
	{result::error_file_open_failed,                "error_file_open_failed"},
	{result::error_file_truncated,                  "error_file_truncated"},
	{result::error_file_write_failed,               "error_file_write_failed"},
	{result::error_file_read_failed,                "error_file_read_failed"},
	{result::error_file_close,                      "error_file_close"},
	{result::error_unsupported_file_format,         "error_unsupported_file_format"},
	{result::error_duplicate_array_name,            "error_duplicate_array_name"},

	{result::error_magic_string_invalid,            "error_magic_string_invalid"},
	{result::error_version_not_supported,           "error_version_not_supported"},
	{result::error_header_invalid_length,           "error_header_invalid_length"},
	{result::error_header_truncated,                "error_header_truncated"},
	{result::error_header_parsing_error,            "error_header_parsing_error"},
	{result::error_header_invalid,                  "error_header_invalid"},
	{result::error_header_empty,                    "error_header_empty"},

	{result::error_descr_invalid,                   "error_descr_invalid"},
	{result::error_descr_invalid_type,              "error_descr_invalid_type"},
	{result::error_descr_invalid_string,            "error_descr_invalid_string"},
	{result::error_descr_invalid_data_size,         "error_descr_invalid_data_size"},
	{result::error_descr_list_empty,                "error_descr_list_empty"},
	{result::error_descr_list_invalid_type,         "error_descr_list_invalid_type"},
	{result::error_descr_list_incomplete_value,     "error_descr_list_incomplete_value"},
	{result::error_descr_list_invalid_value,        "error_descr_list_invalid_value"},
	{result::error_descr_list_invalid_shape,        "error_descr_list_invalid_shape"},
	{result::error_descr_list_invalid_shape_value,  "error_descr_list_invalid_shape_value"},
	{result::error_descr_list_subtype_not_supported,"error_descr_list_subtype_not_supported"},

	{result::error_fortran_order_invalid_value,     "error_fortran_order_invalid_value"},
	{result::error_shape_invalid_value,             "error_shape_invalid_value"},
	{result::error_shape_invalid_shape_value,       "error_shape_invalid_shape_value"},
	{result::error_item_size_mismatch,              "error_item_size_mismatch"},
	{result::error_data_size_mismatch,              "error_data_size_mismatch"},
	{result::error_unavailable,                     "error_unavailable"}
}};


inline bool
is_error(result r)
{
	return
		r != result::ok &&
		r != result::warning_missing_descr &&
		r != result::warning_missing_shape &&
		r != result::warning_missing_fortran_order;
}


/*
 * to_string - returns a string representation of a result code
 *
 * Note that a result might contain not only a single code, but several codes
 * that are set (technically by OR-ing them). As such, this function returns a
 * string which will contain all string representations for all codes,
 * concatenated by " | ".
 */
inline
std::string
to_string(result res)
{
	if (res == result::ok)
		return result_strings[0].second;

	std::ostringstream oss;
	bool first = true;
	for (size_t i = 1; i < result_strings.size(); ++i) {
		const auto& [enum_val, str] = result_strings[i];
		if ((res & enum_val) != enum_val)
			continue;
		if (!first)
			oss << " | ";
		oss << str;
		first = false;
	}
	return oss.str();
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


/*
 * buffer_read - wrapper for vectors/buffers to make them a ReadableSource
 */
struct buffer_reader
{
	buffer_reader(u8_vector &data) : _data(data), _pos(0) {}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::ranges::begin(dest);
        auto last = std::ranges::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (_pos + size > _data.size()) ? _data.size() - _pos : size;
		std::copy_n(_data.begin() + _pos, size, first);
		_pos += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	inline bool
	eof() noexcept {
		return _pos >= _data.size();
	}

	u8_vector   _data;
	std::size_t _pos;
};


/*
 * ifstream_reader - wrapper for ifstreams to make them a ReadableSource
 */
struct ifstream_reader
{
	ifstream_reader(std::ifstream &stream) : _stream(stream), _eof(false), _fail(false) {}

	template <Writable<u8> D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::ranges::begin(dest);
		auto last = std::ranges::end(dest);
		size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));

		_stream.read(reinterpret_cast<char *>(&(*first)), size);
		_fail = _stream.fail();
		_eof  = _stream.eof();
		return static_cast<std::size_t>(_stream.gcount());
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span(dest, size), size);
	}

	inline bool
	eof() noexcept {
		return _eof;
	}

	inline bool
	fail() noexcept {
		return _fail;
	}

	std::ifstream &_stream;
	bool _eof;
	bool _fail;
};


/*
 * read_magic_string - read (and validate) the magic string from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::magic)>
result
read_magic_string(Reader &source, npyfile &npy)
{
	constexpr std::array<uint8_t, 6> magic = {0x93, 'N', 'U', 'M', 'P', 'Y'};
	if (source.read(npy.magic, npyfile::magic_byte_count) != npyfile::magic_byte_count)
		return result::error_magic_string_invalid;
	if (!std::equal(npy.magic.begin(), npy.magic.end(), magic.begin()))
		return result::error_magic_string_invalid;

	return result::ok;
}


/*
 * read_version - read (and validate) the version from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::version)>
result
read_version(Reader &source, npyfile &npy)
{
	if (source.read(npy.version, npyfile::version_byte_count) != npyfile::version_byte_count)
		return result::error_file_truncated;

	// currently, only 1.0 and 2.0 are supported
	if ((npy.version[0] != 0x01 && npy.version[0] != 0x02) || (npy.version[1] != 0x00))
		return result::error_version_not_supported;

	// set the size byte count, which depends on the version
	if (npy.version[0] == 0x01)
		npy.header_size_byte_count = 2;
	else
		npy.header_size_byte_count = 4;

	return result::ok;
}


/*
 * read_header_length - read (and validate) the header length from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, u8*>
result
read_header_length(Reader &source, npyfile &npy)
{
	npy.header_size = 0;

	// read bytes and convert bytes to size_t
	u8 elem = 0;
	size_t i = 0;
	while (i < npy.header_size_byte_count) {
		if (source.read(&elem, 1) != 1)
			return result::error_file_truncated;
		npy.header_size |= static_cast<size_t>(elem) << (i * 8);
		++i;
	}

	/*
	 * Note: the above could be replaced with the following that reads all
	 * header bytes into an std::array<u8, 4>. Still undecided which is better
	 */
	// if (source.read(npy.header_size_bytes, npy.header_size_byte_count) != npy.header_size_byte_count)
	// 	return result::error_file_truncated;
	// for (size_t i = 0; i < npy.header_size_byte_count; i++)
	// 	npy.header_size |= static_cast<size_t>(npy.header_size_bytes[i]) << (i * 8);


	// validate the length: len(magic string) + 2 + len(length) + HEADER_LEN must be divisible by 64
	npy.data_offset = npyfile::magic_byte_count + npy.version_byte_count + npy.header_size_byte_count + npy.header_size;
	if (npy.data_offset % 64 != 0)
		return result::error_header_invalid_length;

	return result::ok;
}


/*
 * read_header - read the header from a ReadableSource
 */
template <typename Reader>
requires Readable<Reader, decltype(npyfile::header)>
result
read_header(Reader &source, npyfile &npy)
{
	npy.header.resize(npy.header_size);
	if (source.read(npy.header, npy.header_size) != npy.header_size)
		return result::error_file_truncated;
	if (npy.header.size() < npy.header_size)
		return result::error_header_truncated;
	return result::ok;
}


/*
 * parse_descr_string - turn a string from the parser result for the descirption string into a dtype
 */
inline result
parse_descr_string(pyparser::parse_result *descr, dtype &dt)
{
	// sanity check: test if the data type is actually a string or not
	if (descr->dtype != pyparser::type::string)
		return result::error_descr_invalid_string;

	auto range = descr->range();
	if (range.size() < 3)
		return result::error_descr_invalid_string;

	// first character is the byte order
	dt.endianness = to_byte_order(range[0]);
	// second character is the type code
	dt.type_code  = range[1];
	// remaining characters are the byte size of the data type
	std::string str(range.begin() + 2, range.end());

	// TODO: use something else than strtol
	char *end;
	dt.size = std::strtol(str.c_str(), &end, 10);
	if (*end != '\0') {
		dt.size = 0;
		return result::error_descr_invalid_data_size;
	}
	return result::ok;
}


/*
 * parse_descr_list - turn a list (from a description string parse result) into a dtype
 */
inline result
parse_descr_list(pyparser::parse_result *descr, dtype &dt)
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
		auto &field = add_field(dt, dtype{.name = std::string(node->nodes[0]->begin, node->nodes[0]->end)});
		// dt.fields.push_back({.name = std::string(node->nodes[0]->begin, node->nodes[0]->end)});
		// auto &field = dt.fields.back();

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


/*
 * parse_descr - turn a parser result into a dtype
 */
inline result
parse_descr(pyparser::parse_result *descr, dtype &dt)
{
	if (!descr)
		return result::error_descr_invalid;

	switch (descr->dtype) {
		case pyparser::type::string: return parse_descr_string(descr, dt);
		case pyparser::type::list:   return parse_descr_list(descr, dt);
		default:                     return result::error_descr_invalid_type;
	}
}


/*
 * parse_header - parse the header string of a .npy file
 */
inline result
parse_header(npyfile &npy, dtype &dt, storage_order &order, u64_vector &shape)
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
	pyparser parser;
	auto pres = parser.parse(npy.header);
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
			auto tmp = parse_descr(kv->nodes[1].get(), dt);
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


/*
 * compute_item_size - compute the item size of a (possibly nested) dtype
 */
inline result
compute_item_size(dtype &dt, u64 offset = 0)
{
	dt.offset = offset;

	// for simple arrays, we simple report the item size
	if (!is_structured_array(dt)) {
		// most types have their 'width' in bytes given directly in the
		// descr string, which is already stored in dtype.size. However, unicode
		// strings and objects differ in that the size that is given in the
		// dtype is not the size in bytes, but the number of 'elements'. In case
		// of unicude, U16 means a unicode string with 16 unicode characters,
		// each character taking up 4 bytes.
		u64 multiplier;
		switch (dt.type_code) {
			case 'O': multiplier = 8; break;
			case 'U': multiplier = 4; break;
			default:  multiplier = 1;
		}
		dt.item_size = multiplier * dt.size;

		// if there's a shape attached, multiply it in
		for (auto s: dt.shape)
			dt.item_size *= s;
	}
	else {
		// the item_size for structured arrays is (often) 0, in which case we simply
		// update. If the item_size is not 0, double check to determine if there is
		// an item-size mismatch.
		u64 subsize = 0;
		for (auto &field: dt.fields) {
			result res;
			if ((res = compute_item_size(field, dt.offset + subsize)) != result::ok)
				return res;
			subsize += field.item_size;
		}
		if (dt.item_size != 0 && dt.item_size != subsize)
			return result::error_item_size_mismatch;
		dt.item_size = subsize;
	}
	return result::ok;
}


inline result
validate_data_size(const npyfile &npy, const dtype &dt)
{
	// TODO: for streaming data, we cannot decide this (we don't know yet how
	// much data there will be)
	if (npy.streaming)
		return result::ok;

	// detect if data is truncated
	if (npy.data_size % dt.item_size != 0)
		return result::error_data_size_mismatch;

	// size = npy.data_size / dt.item_size;
	return result::ok;
}


/*
 * compute_data_size - compute the size of the data in a ReadableSource (if possible)
 */
template <typename Reader>
inline result
compute_data_size(Reader &source, npyfile &npy)
{
	// TODO: implement for other things or use another approach to externalize
	//       type detection
	if constexpr (std::is_same_v<Reader, buffer_reader>) {
		npy.data_size = source._data.size() - source._pos;
	}
	else {
		npy.data_size = 0;
	}
	return result::ok;
}


inline result
from_stream(std::istream &)
{
	// TODO: for streaming data, we need read calls in between to fetch the
	// next amount of data from the streambuf_iterator.
	return result::error_unavailable;
}


template <typename Reader>
// requires Readable<Reader, OutputRange>
inline result
process_file_header(Reader &source, npyfile &npy, dtype &dt, u64_vector &shape, storage_order &order)
{
	auto res = result::ok;

	// read stuff
	if ((res |= read_magic_string(source,  npy)    , is_error(res))) return res;
	if ((res |= read_version(source, npy)          , is_error(res))) return res;
	if ((res |= read_header_length(source, npy)    , is_error(res))) return res;
	if ((res |= read_header(source, npy)           , is_error(res))) return res;

	// parse + compute stuff
	if ((res |= parse_header(npy, dt, order, shape), is_error(res))) return res;
	if ((res |= compute_item_size(dt)              , is_error(res))) return res;
	if ((res |= compute_data_size(source, npy)     , is_error(res))) return res;
	if ((res |= validate_data_size(npy, dt)        , is_error(res))) return res;

	return res;
}


inline result
from_buffer(u8_vector &&buffer, npyfile &npy, ndarray &dest)
{
	auto res = result::ok;

	// setup the npyfile struct as non-streaming
	npy.streaming = false;

	// parts of the array description (will be moved into the array later)
	dtype         dt;
	u64_vector    shape;
	storage_order order;

	// wrap the buffer so that it becomes a ReadableSource
	auto source = buffer_reader(buffer);
	if ((res = process_file_header(source, npy, dt, shape, order), is_error(res))) return res;

	// erase the entire header block. what's left is the raw data of the ndarray
	buffer.erase(buffer.begin(), buffer.begin() + npy.data_offset);

	// build the ndarray from the data that we read by moving into it
	dest.assign(std::move(dt), std::move(shape), std::move(buffer), order);

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
		npyfile *npy = new npyfile{};
		ndarray *array = new ndarray{};
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



inline void
read_bytes(std::ifstream& file, std::size_t num_bytes, const std::function<void(u8_vector)>& callback)
{
	u8_vector buffer(num_bytes);
	if (file.read(reinterpret_cast<char*>(buffer.data()), num_bytes)) {
		callback(std::move(buffer));
	} else {
		// Handle read error
	}
}


/*
 * open_npy - attempt to open an npy file.
 *
 * This function opens a file and examines the first few bytes to test if it is
 * a zip file (e.g. used in .npz). If that's the case, it returns an error,
 * because handling .npz files is different regarding how data is read and where
 * it will be written to. If the file is not a zip file, this function resets
 * the read cursor to before the first byte, and returns OK. If it later turns
 * out that this is, in fact, not a .npy file, other functions will return
 * errors because parsing the .npy header will (most likely) fail.
 */
inline result
open_npy(std::filesystem::path filepath, std::ifstream &file)
{
	result res;
	if ((res = open_fstream(filepath, file)) != result::ok)
		return res;

	// test if this is a PKzip file, and if yes then we exit early. for loading
	// npz files, use from_npz
	if (is_zip_file(file))
		return result::error_wrong_filetype;

	file.seekg(0);
	return res;
}


/*
 * from_nyp - read a file into a container
 *
 * When reading a file into an ndarray, we read the file in one go into a buffer
 * and then process it.
 */
template <NDArray NDArrayType, bool unsafe_read = true>
result
from_npy(std::filesystem::path filepath, NDArrayType &array, npyfile *npy = nullptr)
{
	// try to open the file
	result res = result::ok;
	std::ifstream file;
	if ((res = open_npy(filepath, file), is_error(res))) return res;

	// read the file into a vector. the c++ iostream interface is horrible to
	// work with and considered bad design by many developers. We'll load the
	// file into a vector (which is not the fastest), but then working with it
	// is reasonably simple
	auto filesize = ncr::get_file_size(file);
	u8_vector buf(filesize);
	if constexpr (unsafe_read)
		file.read(reinterpret_cast<char*>(buf.data()), filesize);
	else
		buf.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	// if the caller didnt pass in a preallocated object, we'll use a local one.
	// this way avoids allocating an object, as _tmp is already present on the
	// stack. it also doesn't tamper with the npy pointer
	npyfile _tmp;
	npyfile *npy_ptr = npy ? npy : &_tmp;

	// Note the change in argument order!
	res = from_buffer(std::move(buf), *npy_ptr, array);

	// done
	return res;

	return result::ok;
}


template <typename T, typename F, typename G>
result
from_npy_callback(std::filesystem::path filepath, G array_properties_callback, F data_callback, npyfile *npy = nullptr)
{
	// try to open the file
	result res = result::ok;
	std::ifstream file;
	if ((res = open_npy(filepath, file), is_error(res))) return res;

	// see comment in from_npy for NDArrayType
	npyfile _tmp;
	npyfile *npy_ptr = npy ? npy : &_tmp;

	// process the file header and extract properties of the array
	dtype         dt;
	u64_vector    shape;
	storage_order order;
	auto source = ifstream_reader(file);
	if ((res = process_file_header(source, *npy_ptr, dt, shape, order), is_error(res))) return res;
	if constexpr (ArrayPropertiesCallback<G>) {
		bool cb_result = array_properties_callback(dt, shape, order);
		if (!cb_result)
			return res;
	}

	// at this point we know the item size, and can read items from the file
	// until we hit eof
	for (u64 i = 0;; ++i) {
		u8_vector buffer(dt.item_size, 0);
		size_t bytes_read = source.read(buffer, dt.item_size);
		if (bytes_read != dt.item_size) {
			// EOF -> nothing more to read, w'ere in a good state
			if (bytes_read == 0 && source.eof())
				break;
			else {
				// there was some failure while reading. this might also be set
				// when trying to read more bytes than available
				// TODO: determine when this might happen
				if (source.fail())
					res = result::error_file_read_failed;

				// the file is truncated, there were not enough bytes for
				// another item.
				else
					res = result::error_file_truncated;

				break;
			}
		}

		// select the right callback variant. if the callback returns false, the
		// user wants an early exit
		if constexpr (GenericReaderCallback<F>) {
			if (!data_callback(dt, shape, order, i, std::move(buffer)))
				break;
		}
		else if constexpr (TypedReaderCallbackFlat<T, F>) {
			// when the callback returns false, the user wants an early exit
			if (!data_callback(i, *reinterpret_cast<T*>(buffer.data())))
				break;
		}
		else if constexpr (TypedReaderCallbackMulti<T, F>) {
			// when the callback returns false, the user wants an early exit
			u64_vector multi_index = unravel_index(i, shape, order);
			if (!data_callback(multi_index, *reinterpret_cast<T*>(buffer.data())))
				break;
		}
		else {
			static_assert(GenericReaderCallback<F> || TypedReaderCallback<T, F>,
						  "The provided function does not satisfy any of the required concepts.");
		}
	}
	return res;
}

template <typename F> requires GenericReaderCallback<F>
result
from_npy(std::filesystem::path filepath, F callback, npyfile *npy = nullptr)
{
	return from_npy_callback<void>(
		std::move(filepath),
		nullptr,
		std::forward<F>(callback),
		npy);
}


template <typename T, typename F> requires TypedReaderCallback<T, F>
result
from_npy(std::filesystem::path filepath, F callback, npyfile *npy = nullptr)
{
	return from_npy_callback<T, F>(
		std::move(filepath),
		nullptr,
		std::forward<F>(callback),
		npy);
}


template <typename T, typename G, typename F>
requires ArrayPropertiesCallback<G> && TypedReaderCallback<T, F>
result
from_npy(std::filesystem::path filepath, G array_properties_callback, F data_callback, npyfile *npy = nullptr)
{
	return from_npy_callback<T, F, G>(
		std::move(filepath),
		std::forward<G>(array_properties_callback),
		std::forward<F>(data_callback),
		npy);
}




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
	ndarray&    array;
};


/*
 * save_npz - save arrays to an npz file
 */
inline result
to_zip_archive(std::filesystem::path filepath, std::vector<savez_arg> args, bool compress, bool overwrite=false, u32 compression_level=0)
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
		if (zip_interface.write(zip_state, name, std::move(buffer), compress, compression_level) != zip::result::ok) {
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
savez_compressed(std::filesystem::path filepath, std::vector<savez_arg> args, bool overwrite=false, u32 compression_level = 0)
{
	return to_zip_archive(filepath, std::forward<decltype(args)>(args), true, overwrite, compression_level);
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
savez_compressed(std::filesystem::path filepath, std::vector<std::reference_wrapper<ndarray>> args, bool overwrite=false, u32 compression_level=0)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), true, overwrite, compression_level);
}


}} // ncr::numpy

