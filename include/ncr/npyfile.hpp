#ifndef _9750a253a01642ea81d4721d4c92ad7c_
#define _9750a253a01642ea81d4721d4c92ad7c_

/*
 * npyfile - read/write numpy files
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

//@ncr-fusor-strip-start
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
//@ncr-fusor-strip-end

#include <cstring> // memcpy
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>

#include "ncr/types.hpp"
#include "ncr/bits.hpp"

#include "ncr/zip.hpp"
#include "ncr/pyparser.hpp"
#include "ncr/ndarray.hpp"
#include "ncr/npyerror.hpp"
#include "ncr/npybuffers.hpp"

// NOTE: npybuffers.hpp includes fcntl.h, unistd.h, and sys/mman.h in case MMAP
// is not disabled. no need to include this here as well


namespace ncr { namespace numpy {


/*
 * forward declarations and typedefs
 */
struct npyfile;
struct npzfile;
enum class source_type: u16;


/*
 * concepts
 *
 * The concepts here describe the *callback* shapes that the from_npy reader
 * family accepts. They drive `if constexpr` dispatch in from_npy_callback and
 * document what user-supplied lambdas must look like.
 *
 */
template <typename T>
concept NDArray = std::derived_from<T, ndarray>;

template<typename F>
concept GenericReaderCallback = requires(F f, const dtype& dt, const u64_vector &shape, const storage_order &order, u64 index, u8_vector item) {
    { f(dt, shape, order, index, std::move(item)) } -> std::same_as<bool>;
};

// span-based generic callback. Same parameters as GenericReaderCallback but
// the item is a non-owning view into a reused chunk buffer, which avoids the
// per-item heap allocation of the vector-taking variant.
template<typename F>
concept GenericReaderCallbackSpan = requires(F f, const dtype& dt, const u64_vector &shape, const storage_order &order, u64 index, u8_const_span item) {
    { f(dt, shape, order, index, item) } -> std::same_as<bool>;
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

template <typename T>
concept Viewable = requires(T source, std::size_t size) {
	{ source.view(size) } -> std::same_as<std::span<uint8_t>>;
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
	u8
		magic[magic_byte_count]     = {};

	// storage for the version
	u8
		version[version_byte_count] = {};

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
release(npyfile &npy)
{
	npy.header_size_byte_count = 0;
	npy.header_size            = 0;
	npy.data_offset            = 0;
	npy.data_size              = 0;
	npy.file_size              = 0;
	std::memset(npy.magic,   0, npyfile::magic_byte_count * sizeof(u8));
	std::memset(npy.version, 0, npyfile::version_byte_count * sizeof(u8));
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
	// the names of all arrays in this file (insertion order)
	std::vector<std::string> names;

	// owning storage for the npy files and arrays. Both vectors are kept in
	// sync with `names` (same length, same order). A linear scan over names
	// is the lookup mechanism. For the small number of arrays in a typical
	// .npz file this is faster than std::map and avoids the dependency on
	// <map>.
	std::vector<std::unique_ptr<npyfile>> npys;
	std::vector<std::unique_ptr<ndarray>> arrays;

	// look up an array by name. Returns a pointer to the owned ndarray, or
	// nullptr when no such array exists.
	ndarray* find(const std::string &name)
	{
		for (size_t i = 0; i < names.size(); ++i)
			if (names[i] == name)
				return arrays[i].get();
		return nullptr;
	}

	// operator[] is mapped to the array, because this is what people expect
	// from using numpy
	ndarray& operator[](const std::string &name)
	{
		ndarray *a = find(name);
		if (!a)
			throw std::runtime_error(std::string("Key error: No array with name \"") + name + std::string("\""));
		return *a;
	}
};


// retain POD-like structure of npzfile and provide a free function to clear it
inline void
release(npzfile &npz)
{
	for (auto &arr: npz.arrays)
		if (arr)
			arr->release();

	npz.names.clear();
	npz.npys.clear();
	npz.arrays.clear();
}


/*
 * buffer_read - wrapper for vectors/buffers to make them a ReadableSource
 */
struct buffer_reader
{
	buffer_reader(u8_vector &data) : _data(data), _pos(0) {}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last  = std::end(dest);
		size_t dest_capacity = static_cast<size_t>(last - first);
		if (size > dest_capacity)
			size = dest_capacity;
		// note the subtraction is guarded by the comparison below. It never
		// underflows even when _pos > _data.size() (which can't happen
		// through the public API, but defensive readers shouldn't compute
		// huge sizes by accident)
		size_t available = (_pos < _data.size()) ? _data.size() - _pos : 0;
		if (size > available)
			size = available;
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

	u8_vector   &_data;
	std::size_t _pos;
};


#ifdef NCR_NUMPY_HAS_MMAP
/*
 * mmap_reader - wrapper for an mmap_buffer to make it a ReadableSource
 *
 * Mirrors buffer_reader but works against an existing mmap_buffer (so the
 * underlying storage stays mapped instead of being a std::vector). Used by
 * the mmap fast-path of from_npy to parse the header without copying any of
 * the file payload.
 */
struct mmap_reader
{
	mmap_reader(mmap_buffer *buf) : _buf(buf), _pos(0) {}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last  = std::end(dest);
		size_t dest_capacity = static_cast<size_t>(last - first);
		if (size > dest_capacity)
			size = dest_capacity;
		size_t available = (_pos < _buf->size) ? _buf->size - _pos : 0;
		if (size > available)
			size = available;
		std::copy_n(_buf->data + _pos, size, first);
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
	eof() noexcept { return _pos >= _buf->size; }

	mmap_buffer *_buf;
	std::size_t  _pos;
};
#endif // NCR_NUMPY_HAS_MMAP


/*
 * ifstream_reader - wrapper for ifstreams to make them a ReadableSource
 */
struct ifstream_reader
{
	ifstream_reader(std::ifstream &stream) : _stream(stream), _eof(false), _fail(false) {}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last = std::end(dest);
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
 *
 * The reader is expected to provide `read(buf, size) -> size_t`. Note that this
 * doesn't use a concept. It did before, but didn't catch any bug while adding
 * compile time noise.
 */
template <typename Reader>
result
read_magic_string(Reader &source, npyfile &npy)
{
	constexpr u8 magic[] = {0x93, 'N', 'U', 'M', 'P', 'Y'};
	if (source.read(npy.magic, npyfile::magic_byte_count) != npyfile::magic_byte_count)
		return {errors::magic_string_invalid};

	if (!std::equal(npy.magic, npy.magic + npyfile::magic_byte_count, magic))
		return {errors::magic_string_invalid};

	return {};
}


/*
 * read_version - read (and validate) the version from a ReadableSource
 */
template <typename Reader>
result
read_version(Reader &source, npyfile &npy)
{
	if (source.read(npy.version, npyfile::version_byte_count) != npyfile::version_byte_count)
		return {errors::file_truncated};

	// currently, only 1.0 and 2.0 are supported
	if ((npy.version[0] != 0x01 && npy.version[0] != 0x02) || (npy.version[1] != 0x00))
		return {errors::version_not_supported};

	// set the size byte count, which depends on the version
	if (npy.version[0] == 0x01)
		npy.header_size_byte_count = 2;
	else
		npy.header_size_byte_count = 4;

	return {};
}


/*
 * read_header_length - read (and validate) the header length from a ReadableSource
 */
template <typename Reader>
result
read_header_length(Reader &source, npyfile &npy)
{
	npy.header_size = 0;

	// read bytes and convert bytes to size_t
	u8 elem = 0;
	size_t i = 0;
	while (i < npy.header_size_byte_count) {
		if (source.read(&elem, 1) != 1)
			return {errors::file_truncated};
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
		return {errors::header_invalid_length};

	return {};
}


/*
 * read_header - read the header from a ReadableSource
 */
template <typename Reader>
result
read_header(Reader &source, npyfile &npy)
{
	npy.header.resize(npy.header_size);
	if (source.read(npy.header, npy.header_size) != npy.header_size)
		return {errors::file_truncated};
	if (npy.header.size() < npy.header_size)
		return {errors::header_truncated};
	return {};
}


/*
 * parse_descr_string - turn a string from the parser result for the descirption string into a dtype
 */
inline result
parse_descr_string(PyParser::ParseResult *descr, dtype &dt)
{
	// sanity check: test if the data type is actually a string or not
	if (descr->dtype != PyParser::Type::String)
		return {errors::descr_invalid_string};

	if (std::distance(descr->begin, descr->end) < 3)
		return {errors::descr_invalid_string};

	// first character is the byte order
	dt.endianness = to_byte_order(descr->begin[0]);
	// second character is the type code
	dt.type_code  = descr->begin[1];
	// remaining characters are the byte size of the data type. parse via
	// from_chars so we don't need a transient std::string
	const char *b = reinterpret_cast<const char*>(descr->begin + 2);
	const char *e = reinterpret_cast<const char*>(descr->end);
	auto [ptr, ec] = std::from_chars(b, e, dt.size);
	if (ec != std::errc{} || ptr != e) {
		dt.size = 0;
		return {errors::descr_invalid_data_size};
	}
	return {};
}


/*
 * parse_descr_list - turn a list (from a description string parse result) into a dtype
 */
inline result
parse_descr_list(PyParser::ParseResult *descr, dtype &dt)
{
	// the descr field of structured arrays is a list of tuples (n, t, s), where
	// n is the name of the field, t is the type, s is the shape. Note that the
	// shape is optional. Moreover, t could be a list of sub-types, which might
	// recursively contain further subtypes.

	if (descr->nodes.size() == 0)
		return {errors::descr_list_empty};

	for (auto &node: descr->nodes) {
		// check data type of the node
		if (node->dtype != PyParser::Type::Tuple)
			return {errors::descr_list_invalid_type};

		// needs at least 2 subnodes, i.e. tuple (n, t)
		if (node->nodes.size() < 2)
			return {errors::descr_list_incomplete_value};

		// can have at most 3 subnodes, i.e. tuple (n, t, s)
		if (node->nodes.size() > 3)
			return {errors::descr_list_invalid_value};

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
			case PyParser::Type::String:
				res = parse_descr_string(node->nodes[1].get(), field);
				if (!res.is_ok())
					return res;
				break;

			// recursively go through the list
			case PyParser::Type::List:
				res = parse_descr_list(node->nodes[1].get(), field);
				if (!res.is_ok())
					return res;
				break;

			// currently, other entries are not supported
			default:
				return {errors::descr_list_subtype_not_supported};
		}

		// third field (optional): shape
		if (node->nodes.size() > 2) {
			// test the type. must be a tuple
			if (node->nodes[2]->dtype != PyParser::Type::Tuple)
				return {errors::descr_list_invalid_shape};

			for (auto &n: node->nodes[2]->nodes) {
				// must be an integer value
				if (n->dtype != PyParser::Type::Integer)
					return {errors::descr_list_invalid_shape_value};
				field.shape.push_back(n->value.l);
			}
		}
	}

	return {};
}


/*
 * parse_descr - turn a parser result into a dtype
 */
inline result
parse_descr(PyParser::ParseResult *descr, dtype &dt)
{
	if (!descr)
		return {errors::descr_invalid};

	switch (descr->dtype) {
		case PyParser::Type::String: return parse_descr_string(descr, dt);
		case PyParser::Type::List:   return parse_descr_list(descr, dt);
		default:                     return {errors::descr_invalid_type};
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
	PyParser parser;
	auto pres = parser.parse(npy.header);
	if (!pres)
		return {errors::header_parsing_error};

	// header must be one parse-node of type dict
	if (pres->nodes.size() != 1 || pres->nodes[0]->dtype != PyParser::Type::Dict)
		return {errors::header_invalid};

	// the dict itself must have child-nodes
	auto &root_dict = pres->nodes[0];
	if (root_dict->nodes.size() == 0)
		return {errors::header_empty};

	// the result code contains warnings for all fields. they will be disabled
	// during parsing below if they are discovered
	result res = {warnings::missing_descr | warnings::missing_fortran_order | warnings::missing_shape};

	// we are not to assume any order of the entries in the dict (albeit they
	// are normally ordered alphabetically). The parse result of the entires are
	// kvpairs, each having the key as node 0 and value as node 1
	for (auto &kv: root_dict->nodes) {
		// check the parsed type for consistency
		if (kv->dtype != PyParser::Type::KVPair || kv->nodes.size() != 2)
			return {errors::header_invalid};

		// descr, might be a string or a list of tuples
		if (kv->nodes[0]->equals("descr")) {
			auto tmp = parse_descr(kv->nodes[1].get(), dt);
			if (!tmp.is_ok())
				return tmp;
			res.warn &= ~warnings::missing_descr;
		}

		// determine if the array data is in fortran order or not
		if (kv->nodes[0]->equals("fortran_order")) {
			if (kv->nodes[1]->dtype != PyParser::Type::Boolean)
				return {errors::fortran_order_invalid_value};
			order = kv->nodes[1]->value.b ? storage_order::col_major : storage_order::row_major;
			res.warn &= ~warnings::missing_fortran_order;
		}

		// read the shape of the array (NOTE: this is *not* the shape of a data
		// type, but the shape of the array)
		if (kv->nodes[0]->equals("shape")) {
			if (kv->nodes[1]->dtype != PyParser::Type::Tuple)
				return {errors::shape_invalid_value};

			// read each shape value
			shape.clear();
			for (auto &n: kv->nodes[1]->nodes) {
				// must be an integer value
				if (n->dtype != PyParser::Type::Integer)
					return {errors::shape_invalid_shape_value};
				shape.push_back(n->value.l);
			}
			res.warn &= ~warnings::missing_shape;
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
			result res = compute_item_size(field, dt.offset + subsize);
			if (!res.is_ok())
				return res;

			u64 tmp = 0;
			if (add_overflow(subsize, field.item_size, tmp))
				return {errors::size_overflow};
			subsize = tmp;
		}
		if (dt.item_size != 0 && dt.item_size != subsize)
			return {errors::item_size_mismatch};
		dt.item_size = subsize;
	}
	return {};
}


inline result
validate_data_size(const npyfile &npy, const dtype &dt)
{
	// TODO: for streaming data, we cannot decide this (we don't know yet how
	// much data there will be)
	if (npy.streaming)
		return {};

	// detect if data is truncated
	if (npy.data_size % dt.item_size != 0)
		return {errors::data_size_mismatch};

	// size = npy.data_size / dt.item_size;
	return {};
}


/*
 * compute_data_size - compute the size of the data in a ReadableSource (if possible)
 *
 * TODO: we could
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
	else if constexpr (std::is_same_v<Reader, ifstream_reader>) {
		auto cur = source._stream.tellg();
		source._stream.seekg(0, std::ios::end);
		auto end = source._stream.tellg();
		source._stream.seekg(cur);
		npy.data_size = (end >= cur) ? static_cast<u64>(end - cur) : 0;
	}
#ifdef NCR_NUMPY_HAS_MMAP
	else if constexpr (std::is_same_v<Reader, mmap_reader>) {
		npy.data_size = (source._buf && source._buf->size > source._pos)
			? source._buf->size - source._pos
			: 0;
	}
#endif
	else {
		npy.data_size = 0;
	}
	return {};
}


inline result
from_stream(std::istream &)
{
	// TODO: for streaming data, we need read calls in between to fetch the
	// next amount of data from the streambuf_iterator.
	return {errors::unavailable};
}


template <typename Reader>
// requires Readable<Reader, OutputRange>
inline result
process_file_header(Reader &source, npyfile &npy, dtype &dt, u64_vector &shape, storage_order &order)
{
	result res = {};

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
	result res = {};

	// setup the npyfile struct as non-streaming
	npy.streaming = false;

	// parts of the array description (will be moved into the array later)
	dtype         dt;
	u64_vector    shape;
	storage_order order;

	// wrap the buffer so that it becomes a ReadableSource
	auto source = buffer_reader(buffer);
	if ((res = process_file_header(source, npy, dt, shape, order), is_error(res))) return res;

	// create a new npybuffer with a vector backend. we can move the data right
	// into it. the npy header bytes still sit at the start of the buffer,
	// marking them with data_offset so the array data starts after them.
	// this avoids an O(N) erase of the entire payload
	npybuffer* npybuf = new npybuffer(npybuffer::type::vector);
	npybuf->vector    = make_vector_buffer(std::move(buffer));
	npybuf->vector->data_offset = npy.data_offset;

	// build the ndarray from the data that we read by moving into it. we also
	// transfer ownership of the npybuf to the array. the user is responsible to
	// call release on the array
	dest.assign(std::move(dt),
				std::move(shape),
				npybuf,
				order);

	return res;
}


#ifdef NCR_NUMPY_HAS_MMAP
/*
 * from_mmap_buffer - build an ndarray on top of an existing mmap_buffer.
 *
 * Takes ownership of `mbuf`. The npyfile's data_offset is parsed from the
 * mapped header bytes; the array's data pointer ends up pointing directly
 * into the mapped region (no payload copy). On error the mapping is
 * released. On success the resulting ndarray owns the mapping and unmap
 * happens during ndarray::release().
 */
inline result
from_mmap_buffer(mmap_buffer *mbuf, npyfile &npy, ndarray &dest)
{
	result res = {};

	npy.streaming = false;

	dtype         dt;
	u64_vector    shape;
	storage_order order;

	auto source = mmap_reader(mbuf);
	if ((res = process_file_header(source, npy, dt, shape, order), is_error(res))) {
		// header parse failed - clean up the mapping ourselves since
		// nothing further will adopt it
		(void) ncr::numpy::release(mbuf);
		return res;
	}

	npybuffer* npybuf = new npybuffer(npybuffer::type::mmap);
	npybuf->mmap     = mbuf;
	npybuf->mmap->data_offset = npy.data_offset;

	dest.assign(std::move(dt),
				std::move(shape),
				npybuf,
				order);
	return res;
}
#endif // NCR_NUMPY_HAS_MMAP


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
		return {errors::file_open_failed};
	}

	std::vector<std::string> file_list;
	if (zip_backend.get_file_list(zip_state, file_list) != zip::result::ok) {
		zip_backend.close(zip_state);
		zip_backend.release(&zip_state);
		// TODO: better error return value
		return {errors::file_read_failed};
	}

	// for each archive file, decompress and parse the numpy array
	for (auto &fname: file_list) {
		u8_vector buffer;
		if (zip_backend.read(zip_state, fname, buffer) != zip::result::ok) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return {errors::file_read_failed};
		}

		// remove ".npy" from array name
		std::string array_name = fname.substr(0, fname.find_last_of("."));

		// get a npy file and array. construct owning pointers up-front so that
		// any subsequent error path automatically releases them.
		// TODO: rethink if we want to have a unique pointer here or not. For
		// now it's the least effort variant in resource managing things in the
		// npz struct
		auto npy   = std::make_unique<npyfile>();
		auto array = std::make_unique<ndarray>();
		result res = from_buffer(std::move(buffer), *npy, *array);
		if (!res.is_ok()) {
			zip_backend.close(zip_state);
			zip_backend.release(&zip_state);
			return res;
		}

		// store the information in an npz_file. names/npys/arrays are kept
		// index-aligned so a single name lookup picks both the npy and the
		// array.
		npz.names.push_back(std::move(array_name));
		npz.npys.push_back(std::move(npy));
		npz.arrays.push_back(std::move(array));
	}

	// close the zip backend and release it again
	zip_backend.close(zip_state);
	zip_backend.release(&zip_state);
	return {};
}

inline result
open_fstream(std::filesystem::path filepath, std::ifstream &fstream)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (!fs::exists(filepath))
		return {errors::file_not_found};

	// attempt to read
	fstream.open(filepath, std::ios::binary);
	if (!fstream)
		return {errors::file_open_failed};

	return {};
}


inline result
from_npz(std::filesystem::path filepath, npzfile &npz)
{
	// open the file for file type test
	result res;
	std::ifstream f;
	if ((res = open_fstream(filepath, f), is_error(res)))
		return res;

	// test if this is a PKzip file, also close it again
	bool test = is_zip_file(f);
	f.close();
	if (!test)
		return {errors::wrong_filetype};

	// let the zip backend handle this file from now on
	return from_zip_archive(filepath, npz);
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
	if ((res = open_fstream(filepath, file), is_error(res)))
		return res;

	// test if this is a PKzip file, and if yes then we exit early. for loading
	// npz files, use from_npz
	if (is_zip_file(file))
		return {errors::wrong_filetype};

	file.seekg(0);
	return res;
}


#ifndef NCR_HAS_GET_FILE_SIZE
#define NCR_HAS_GET_FILE_SIZE
inline u64
get_file_size(std::ifstream &is)
{
	auto ip = is.tellg();
	is.seekg(0, std::ios::end);
	auto res = is.tellg();
	is.seekg(ip);
	return static_cast<u64>(res);
}
#endif


/*
 * from_npy_ifstream - read an already opened ifstream into an ndarray
 */
template <typename NDArrayType, bool bulk_read = true>
result
from_npy_ifstream(std::ifstream &file, NDArrayType &array, npyfile *npy = nullptr)
{
	result res = {};

	// read the file into a vector. the c++ iostream interface is horrible to
	// work with and considered bad design by many developers. We'll load the
	// file into a vector (which is not the fastest), but then working with it
	// is reasonably simple
	auto filesize = get_file_size(file);
	u8_vector buf(filesize);
	if constexpr (bulk_read) {
		file.read(reinterpret_cast<char*>(buf.data()), filesize);
		if (file.bad() || static_cast<u64>(file.gcount()) != filesize)
			return {errors::file_read_failed};
	}
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
}


/*
 * threshold above which from_npy prefers a memory-mapped load over a
 * vector-backed read. Below this size mmap setup overhead (open/mmap/munmap
 * plus a TLB pollution) outweighs the saved copy, and a quick read+vector
 * is faster.
 */
#ifndef NCR_NUMPY_MMAP_THRESHOLD_BYTES
#define NCR_NUMPY_MMAP_THRESHOLD_BYTES (64ull * 1024ull)
#endif

/*
 * from_nyp - read a file into a container
 *
 * For regular files larger than NCR_NUMPY_MMAP_THRESHOLD_BYTES this attempts
 * a zero-copy memory-mapped load: the array's data pointer ends up pointing
 * directly into the mapped region. Smaller files and non-mmap builds fall
 * back to the existing read-into-vector path. Define NCR_NUMPY_DISABLE_MMAP
 * (compile-time) or NCR_NUMPY_NO_MMAP_LOAD (compile-time) to force the
 * vector path.
 */
template <typename NDArrayType, bool unsafe_read = true>
result
from_npy(std::filesystem::path filepath, NDArrayType &array, npyfile *npy = nullptr)
{
	static_assert(std::is_base_of_v<ndarray, NDArrayType>,
				  "NDArrayType must derive from ncr::numpy::ndarray");

	// try to open the file
	result res = {};
	std::ifstream file;
	if ((res = open_npy(filepath, file), is_error(res))) return res;

#if defined(NCR_NUMPY_HAS_MMAP) && !defined(NCR_NUMPY_NO_MMAP_LOAD)
	const u64 filesize = get_file_size(file);
	if (filesize >= NCR_NUMPY_MMAP_THRESHOLD_BYTES) {
		// fast path: mmap the file and build the array on top of the mapping.
		// the ifstream is no longer needed after the zip-magic check.
		file.close();

		mmap_buffer *mbuf = new mmap_buffer();
		result mres = ncr::numpy::open(filepath.c_str(), mbuf);
		if (!is_error(mres)) {
			npyfile _tmp;
			npyfile *npy_ptr = npy ? npy : &_tmp;
			return from_mmap_buffer(mbuf, *npy_ptr, array);
		}
		// mmap failed; fall through to the vector path. delete the empty
		// mmap_buffer first since open() didn't bind anything to it.
		delete mbuf;

		// reopen the ifstream for the fallback
		std::ifstream file2;
		if ((res = open_npy(filepath, file2), is_error(res))) return res;
		return from_npy_ifstream<NDArrayType, unsafe_read>(file2, array, npy);
	}
#endif
	return from_npy_ifstream<NDArrayType, unsafe_read>(file, array, npy);
}


// target size for the chunk buffer used when iterating items via a callback.
// Picking a value in the high tens of KiB amortises ifstream::read overhead
// over many items without growing the working set noticeably. The actual
// chunk size used is rounded down to a whole number of items but at least
// one item.
constexpr size_t npy_callback_chunk_target_bytes = 64 * 1024;

template <typename T, typename F, typename G>
result
from_npy_callback(std::filesystem::path filepath,
                  G array_properties_callback,
                  F data_callback,
                  npyfile *npy = nullptr)
{
	// try to open the file
	result res = {};
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
	if ((res = process_file_header(source, *npy_ptr, dt, shape, order), is_error(res)))
		return res;
	if constexpr (ArrayPropertiesCallback<G>) {
		bool cb_result = array_properties_callback(dt, shape, order);
		if (!cb_result)
			return res;
	}

	// chunked read: pull a multiple of item_size bytes per ifstream::read so
	// the per-item dispatch is just pointer arithmetic + a memcpy. For the
	// typed and span-generic callbacks this avoids any per-item allocation.
	const size_t item_size  = dt.item_size;
	if (item_size == 0) {
		// nothing to iterate; treat as empty file
		return res;
	}
	size_t items_per_chunk = npy_callback_chunk_target_bytes / item_size;
	if (items_per_chunk == 0)
		items_per_chunk = 1;
	const size_t chunk_bytes = items_per_chunk * item_size;
	u8_vector chunk(chunk_bytes, 0);

	u64 i = 0;
	bool stop = false;
	while (!stop) {
		size_t bytes_read = source.read(chunk, chunk_bytes);

		// short read means EOF, truncation, or stream failure
		if (bytes_read < chunk_bytes) {
			if (source.fail() && !source.eof()) {
				res = {errors::file_read_failed};
				break;
			}
			// any leftover bytes after the last full item indicate truncation
			if (bytes_read % item_size != 0) {
				res = {errors::file_truncated};
				break;
			}
			if (bytes_read == 0)
				break;
		}

		const size_t items_in_chunk = bytes_read / item_size;
		for (size_t k = 0; k < items_in_chunk; ++k, ++i) {
			const u8* item_ptr = chunk.data() + k * item_size;

			if constexpr (GenericReaderCallbackSpan<F>) {
				u8_const_span item_view(item_ptr, item_size);
				if (!data_callback(dt, shape, order, i, item_view)) {
					stop = true;
					break;
				}
			}
			else if constexpr (GenericReaderCallback<F>) {
				// preserve original semantics: hand the user an owning vector.
				// One allocation per item is unavoidable for this contract;
				// callers wanting zero-alloc should use the span overload.
				u8_vector item(item_ptr, item_ptr + item_size);
				if (!data_callback(dt, shape, order, i, std::move(item))) {
					stop = true;
					break;
				}
			}
			else if constexpr (TypedReaderCallbackFlat<T, F>) {
				T value;
				std::memcpy(&value, item_ptr, sizeof(T));
				if (!data_callback(i, value)) {
					stop = true;
					break;
				}
			}
			else if constexpr (TypedReaderCallbackMulti<T, F>) {
				T value;
				std::memcpy(&value, item_ptr, sizeof(T));
				u64_vector multi_index = unravel_index(i, shape, order);
				if (!data_callback(std::move(multi_index), value)) {
					stop = true;
					break;
				}
			}
			else {
				static_assert(GenericReaderCallback<F> || GenericReaderCallbackSpan<F> || TypedReaderCallback<T, F>,
							  "The provided function does not satisfy any of the required concepts.");
			}
		}

		// short read with all complete items consumed -> we've reached EOF
		if (bytes_read < chunk_bytes)
			break;
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


// span-based generic-callback overload. Same API as the vector-taking
// overload but the item is delivered as a u8_const_span view into a reused
// internal chunk buffer. Eliminates the per-item heap allocation.
template <typename F>
requires GenericReaderCallbackSpan<F> && (!GenericReaderCallback<F>)
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


/*
 * load - high level API which tries to load whatever file is given
 *
 * In case the file cannot be loaded, i.e. it's not an npz or npy file, the
 * variant will hold a corresponding error code
 *
 * TODO: memory-mapped variant, which then moves the file descriptor *into* the
 *       array (this way the array will be backed by the memory mapped data,
 *       similar to numpy.memmap)
 */
inline result
load(std::filesystem::path filepath, ndarray &arr)
{
	return from_npy(filepath, arr);
}


inline result
loadz(std::filesystem::path filepath, npzfile &npz)
{
	return from_npz(filepath, npz);
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
	if constexpr (std::endian::native == std::endian::big) {
		u32 swapped_length = ncr::bswap<u32>(header_length);
		std::memcpy(buf_hlen, &swapped_length, sizeof(u32));
	}
	else
		std::memcpy(buf_hlen, &header_length, sizeof(u32));

	// copy the rest of the array
	const u8* ptr = arr.data();
	const size_t size = arr.bytesize();
	buffer.insert(buffer.end(), ptr, ptr + size);

	return {};
}


inline result
save(std::filesystem::path filepath, const ndarray &arr, bool overwrite=false)
{
	namespace fs = std::filesystem;

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return {errors::file_exists};

	std::ofstream fstream;
	fstream.open(filepath, std::ios::binary | std::ios::out);
	if (!fstream)
		return {errors::file_open_failed};

	// turn the array into a numpy buffer
	u8_vector buffer;
	result res = to_npy_buffer(arr, buffer);
	if (!res.is_ok())
		return res;

	// write to file
	fstream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	// size_t is most likely 64bit, but just to make sure cast it again. tellp()
	// can easily be more than 32bit...
	if (fstream.bad() || static_cast<u64>(fstream.tellp()) != static_cast<u64>(buffer.size()))
		return {errors::file_write_failed};

	return {};
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

	// detect if there are any name clashes. argument lists for savez are
	// short (typically a handful of arrays), so a quadratic scan is faster
	// than building an unordered_set and avoids a header dependency.
	for (size_t i = 0; i < args.size(); ++i) {
		for (size_t j = i + 1; j < args.size(); ++j) {
			if (args[i].name == args[j].name)
				return {errors::duplicate_array_name};
		}
	}

	// test if the file exists
	if (fs::exists(filepath) && !overwrite)
		return {errors::file_exists};

	zip::backend_state *zip_state        = nullptr;
	zip::backend_interface zip_interface = zip::get_backend_interface();

	zip_interface.make(&zip_state);
	if (zip_interface.open(zip_state, filepath, zip::filemode::write) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return {errors::file_open_failed};
	}

	// write all arrays. append .npy to each argument name
	for (auto &arg: args) {
		u8_vector buffer;
		result enc_res = to_npy_buffer(arg.array, buffer);
		if (is_error(enc_res)) {
			zip_interface.close(zip_state);
			zip_interface.release(&zip_state);
			return enc_res;
		}
		std::string name = arg.name + ".npy";
		if (zip_interface.write(zip_state, name, std::move(buffer), compress, compression_level) != zip::result::ok) {
			zip_interface.close(zip_state);
			zip_interface.release(&zip_state);
			return {errors::file_write_failed};
		}
	}

	if (zip_interface.close(zip_state) != zip::result::ok) {
		zip_interface.release(&zip_state);
		return {errors::file_close};
	}

	zip_interface.release(&zip_state);
	return {};
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
 * ndarray_ref - tiny ndarray reference wrapper used by the unnamed-savez
 * overloads so that callers can write `savez("...", {arr1, arr2})` without
 * having to pull in std::reference_wrapper (and the rest of <functional>).
 */
struct ndarray_ref
{
	ndarray *ptr;
	ndarray_ref(ndarray &a) : ptr(&a) {}
	operator ndarray&() const { return *ptr; }
};


/*
 * savez - save unamed arrays to an uncompressed npz file
 *
 * Note that the arrays will receive a name of the format arr_i, where i is the
 * position in the args vector
 */
inline result
savez(std::filesystem::path filepath, std::vector<ndarray_ref> args, bool overwrite=false)
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
savez_compressed(std::filesystem::path filepath, std::vector<ndarray_ref> args, bool overwrite=false, u32 compression_level=0)
{
	std::vector<savez_arg> _args;
	size_t i = 0;
	for (auto &arg: args)
		_args.push_back({std::string("arr_") + std::to_string(i++), arg});
	return to_zip_archive(filepath, std::move(_args), true, overwrite, compression_level);
}



template <typename... Args>
void
release(Args&&... args)
{
	(release(args), ...);
}


//@ncr-fusor-strip-start
/*
 * TODO:
 * reading and writing is currently done with the help of ifstream_reader and
 * buffer_reader, and happens in from_npy* functions as well as in load. It
 * would be nice to unify all of this code into one npyreader with which all
 * reading happens, regardless the source of the data. Then we could also
 * provide an iterator-interface for the reader so that users can iterate over
 * individual items in a for loop in addition to the 'load' interface that loads
 * into an array, or to the callback interface that calls a function per item.
 * although we could get rid of the callback stuff, because with the for(...)
 * loop, users could very easily implement this in their code instead of us
 * calling back into user code.
 *
 * This ideally puts in the code from buffer_reader and from ifstream_reader
 *
 * Q: should the user decide at compile time if the file should be memory mapped
 * or not?
 */
//@ncr-fusor-strip-end

enum class source_type : u16 {
#ifdef NCR_NUMPY_HAS_MMAP
	mmap,
#endif
	fstream,
	buffered,
};


template <source_type>
struct npysource;


#ifdef NCR_NUMPY_HAS_MMAP
template<>
struct npysource<source_type::mmap>
{
	mmap_buffer
		*buf = nullptr;

	inline result
	open(std::filesystem::path filepath)
	{
		if (buf) {
			// re-open: best-effort close of the previous mapping; if that
			// fails the new mapping below will fail anyway and we propagate
			// that error.
			(void) close();
		}
		buf = new mmap_buffer();

		result res = ncr::numpy::open(filepath.c_str(), buf);
		if (is_error(res)) {
			delete buf;
			buf = nullptr;
			return res;
		}
		return {};
	}

	inline size_t
	size()
	{
		return buf->size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		switch (way) {
		case std::ios::beg:
			buf->position = offset;
			break;
		case std::ios::cur:
			buf->position = buf->position + offset;
			break;
		case std::ios::end:
			buf->position = buf->size - offset;
			break;
		}
		return {};
	}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
        auto last = std::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (buf->position + size > buf->size) ? buf->size - buf->position : size;
		std::copy_n(buf->data + buf->position, size, first);
		buf->position += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	std::span<uint8_t>
	view(std::size_t size)
	{
		return std::span<uint8_t>(buf->data + buf->position,
						          buf->data + buf->position + size);
	}

	inline result
	close()
	{
		result res = numpy::close(buf);
		if (is_error(res))
			return res;

		delete buf;
		buf = nullptr;
		return res;
	}

	inline bool
	eof() noexcept {
		return buf->position >= buf->size;
	}
};
#endif // NCR_NUMPY_HAS_MMAP


template<>
struct npysource<source_type::fstream>
{
	std::ifstream
		fstream;

	size_t
		total_size = 0;

	inline result
	open(std::filesystem::path filepath)
	{
		namespace fs = std::filesystem;

		// test if the file exists
		if (!fs::exists(filepath))
			return {errors::file_not_found};

		// attempt to open
		fstream.open(filepath, std::ios::binary);
		if (!(fstream))
			return {errors::file_open_failed};

		total_size = get_file_size(fstream);
		return {};
	}

	inline size_t
	size()
	{
		return total_size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		// clear eof/fail before seeking
		fstream.clear();
		fstream.seekg(offset, way);
		if (fstream.fail() or fstream.bad())
			return {errors::seek_failed};
		return {};
	}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
		auto last = std::end(dest);
		size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));

		fstream.read(reinterpret_cast<char *>(&(*first)), size);
		return static_cast<std::size_t>(fstream.gcount());
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span(dest, size), size);
	}

	inline result
	close()
	{
		fstream.close();
		return {};
	}

	inline bool
	eof() noexcept {
		// peek one character to check if this is eof in the stream
		fstream.peek();
		return fstream.eof();
	}

	inline bool
	fail() noexcept {
		return fstream.fail();
	}
};


template<>
struct npysource<source_type::buffered>
{
	vector_buffer
		*buffer = nullptr;

	size_t
		total_size  = 0;

	size_t
		position    = 0;

	inline result
	open(std::filesystem::path filepath)
	{
		constexpr bool unsafe_read = false;

		if (buffer)
			(void) close();

		std::ifstream fstream;
		fstream.open(filepath, std::ios::binary);
		if (!fstream)
			return {errors::file_open_failed};

		total_size = get_file_size(fstream);
		buffer = make_vector_buffer(total_size);

		if constexpr (unsafe_read)
			fstream.read(reinterpret_cast<char*>(buffer->data.data()), total_size);
		else
			buffer->data.assign(std::istreambuf_iterator<char>(fstream), std::istreambuf_iterator<char>());

		fstream.close();
		position = 0;
		return {};
	};

	inline size_t
	size()
	{
		return total_size;
	}

	inline result
	seek(size_t offset, std::ios_base::seekdir way = std::ios::beg)
	{
		switch (way) {
		case std::ios::beg:
			position = offset;
			break;
		case std::ios::cur:
			position = position + offset;
			break;
		case std::ios::end:
			position = total_size - offset;
			break;
		}
		if (position > total_size)
			return {errors::seek_failed};
		return {};
	}

	template <typename D>
	std::size_t
	read(D &&dest, std::size_t size)
	{
		auto first = std::begin(dest);
        auto last = std::end(dest);
        size = std::min(size, static_cast<std::size_t>(std::distance(first, last)));
		size = (position + size > buffer->data.size()) ? buffer->data.size() - position : size;
		std::copy_n(buffer->data.begin() + position, size, first);
		position += size;
		return size;
	}

	template <typename T>
	requires std::same_as<T, u8>
	std::size_t
	read(T* dest, std::size_t size)
	{
		return read(std::span<T>(dest, size), size);
	}

	inline std::span<uint8_t>
	view(std::size_t size)
	{
		return std::span<uint8_t>(buffer->data.begin() + position,
						          buffer->data.begin() + position + size);
	}

	inline result
	close()
	{
		delete buffer;
		buffer = nullptr;
		return {};
	}

	inline bool
	eof() noexcept {
		return position >= total_size;
	}
};


template <typename Source>
struct npyreader_iterator_base
{
	using difference_type = std::ptrdiff_t;

	npyreader_iterator_base(Source& src, size_t item_sz, bool end = false)
		: source(src), item_size(item_sz), buffer(item_sz), is_end(end), dirty_buffer(true) {}

	npyreader_iterator_base& operator++() {
		seek_next();
		return *this;
	}

	npyreader_iterator_base operator++(int) {
		auto tmp = *this;
		++(*this);
		return tmp;
	}

	bool operator==(const npyreader_iterator_base& other) const { return is_end == other.is_end; }
	bool operator!=(const npyreader_iterator_base& other) const { return !(*this == other); }

protected:
	Source& source;
	size_t item_size;
	u8_vector buffer;
	bool is_end;
	bool dirty_buffer;

	void seek_next() {
		// in case of a buffered source, the call to read() moves the read
		// cursor forward already. to avoid moving over (seeking foward over) an
		// item, we check if the buffer is dirty or not. if the buffer is not
		// dirty, then we "just read" (as in past tense), and the source's read
		// cursor is already at the next item. if the buffer is dirty, then it's
		// not clear when the last actual read from the source happened.
		// In case of a Viewable source, we don't read into the buffer in the
		// first place to avoid copying
		bool do_seek = Viewable<Source> || dirty_buffer;
		if (do_seek)
			(void) source.seek(item_size, std::ios::cur);

		if (source.eof())
			is_end = true;

		dirty_buffer = true;
	}

	void buffer_next_item() {
		// we only need to buffer the next item if the buffer_position is equals
		// the position (reading will move the source character pointer)
		if (!dirty_buffer)
			return;

		if (source.eof()) {
			is_end = true;
		} else {
			// read extracts the characters from the underlying source. need to
			// keep track of this
			auto bytes_read = source.read(buffer.data(), item_size);
			dirty_buffer = false;

			if (bytes_read != item_size) {
				is_end = true;
			}
		}
	}
};


template <typename Source>
struct npyreader_iterator : public npyreader_iterator_base<Source>
{
	using iterator_type     = npyreader_iterator<Source>;
	using iterator_category = std::input_iterator_tag;
	using value_type        = std::span<u8>;
	using pointer           = std::span<u8>*;
	using reference         = std::span<u8>;

	npyreader_iterator(Source& src, size_t item_sz, bool end = false)
		: npyreader_iterator_base<Source>(src, item_sz, end) {}

	reference operator*() {
		if constexpr (Viewable<Source>) {
			return this->source.view(this->item_size);
		} else {
			this->buffer_next_item();
			return this->buffer;
		}
	}
};


template <typename Source, typename T>
struct typed_npyreader_iterator : public npyreader_iterator_base<Source>
{
	using iterator_type     = typed_npyreader_iterator<Source, T>;
	using iterator_category = std::input_iterator_tag;
	using value_type        = T;
	using pointer           = T*;
	using reference         = T;

	typed_npyreader_iterator(Source& src, size_t item_sz, bool end = false)
		: npyreader_iterator_base<Source>(src, item_sz, end)
	{
		if (sizeof(T) != item_sz)
			throw std::runtime_error("Type size mismatch with item_size");
	}

	reference operator*() {
		T item;
		if constexpr (Viewable<Source>) {
			auto view = this->source.view(this->item_size);
			std::memcpy(&item, view.data(), sizeof(T));
		} else {
			this->buffer_next_item();
			std::memcpy(&item, this->buffer.data(), sizeof(T));
		}
		return item;
	}
};


// default to mmap when available, otherwise fall back to a buffered reader
#ifdef NCR_NUMPY_HAS_MMAP
template <source_type E = source_type::mmap>
#else
template <source_type E = source_type::buffered>
#endif
struct npyreader
{
	using type        = npyreader<E>;
	using source_type = npysource<E>;
	using iterator    = npyreader_iterator<source_type>;

	dtype           dt;
	u64_vector      shape;
	storage_order   order;
	npyfile         npy;
	source_type     source;
	bool            is_open = false;

	iterator begin() { return iterator(source, dt.item_size); }
	iterator end()   { return iterator(source, dt.item_size, true); }

	result
	seek(size_t item_index)
	{
		if (!is_open)
			return {errors::reader_not_open};

		size_t offset = npy.data_offset + dt.item_size * item_index;
		if (offset > source.size())
			return {errors::invalid_item_offset};

		return source.seek(offset);
	}

	template <typename T>
	struct typed_view {
		using source_type = npysource<E>;
		using iterator    = typed_npyreader_iterator<source_type, T>;

		typed_view(source_type &src, dtype& dtp)
		: source(src), dt(dtp) {}

		iterator begin() { return iterator(source, dt.item_size); }
		iterator end()   { return iterator(source, dt.item_size, true); }

		source_type& source;
		dtype&       dt;
	};

	template <typename T>
	auto as() {
		return typed_view<T>(source, dt);
	}

	template <typename T = std::span<uint8_t>> requires Viewable<source_type>
	T view()
	{
		if constexpr (std::is_same_v<T, std::span<uint8_t>>) {
			// forward the subrange from the source
			return source.view(dt.item_size);
		}
		else {
			if (sizeof(T) != dt.item_size)
				throw std::runtime_error("Type size mismatch with item_size");

			T value;
			auto buf = source.view(dt.item_size);
			std::memcpy(&value, buf.data(), sizeof(T));
			return value;
		}
	}
};


template <source_type E>
inline result
open(std::filesystem::path filepath, npyreader<E>& reader)
{
	result res;

	if ((res = reader.source.open(filepath),
		 is_error(res))) return res;
	if ((res = process_file_header(reader.source, reader.npy, reader.dt, reader.shape, reader.order),
		 is_error(res))) return res;

	reader.is_open = true;
	return res;
}


template <source_type E>
inline result
close(npyreader<E>& reader)
{
	auto res = reader.source.close();
	reader.is_open = false;
	return res;
}


//@ncr-fusor-strip-start
#if 0
template <Writable<u8> T>
inline result
read_next(npyreader& reader, T& buffer)
{
	NCR_UNUSED(reader, buffer);
	/*
	size_t bytes_read = reader.source.read(buffer, reader.dt.item_size);
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
	*/
	return result::ok;
}
#endif
//@ncr-fusor-strip-end


}} // ncr::numpy


#endif /* _9750a253a01642ea81d4721d4c92ad7c_ */

