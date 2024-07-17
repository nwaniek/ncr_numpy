/*
 * ncr_ndarray - n-dimensional array implementation
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * While the ndarray is tightly integrated with ncr_numpy, it is not within the
 * ncr::numpy namespace. The reason is that the ndarray implementation here is
 * generic enough that it can be used outside of numpy and the purpose of the
 * numpy namespace.
 *
 * ndarray is not a template. This was a design decision to avoid template creep
 * in ncr::numpy. That is, operator() does not return an explicit type such das
 * f32, because there is no such thing as ndarray<f32>. In contrast, operator()
 * returns an ndarray_item, which itself only encapsulates an u8 subrange. To
 * avoid the intermediary ndarray_item, use the ndarray's value() function.
 *
 *
 * TODO: determine if size-matching needs to be equal instead of smaller-equal
 *       (see e.g. ndarray.value)
 */

#pragma once

#include <cassert>
#include <functional>
#include <iomanip>
#include <iostream>
#include <ncr/ncr_bits.hpp>
#include <ncr/ncr_types.hpp>
#include <ncr/ncr_unicode.hpp>


namespace ncr {


/*
 * forward declarations
 */
struct dtype;
struct ndarray_item;
struct ndarray;

// explicit test to check if this is a structured array
template <typename T> bool is_structured_array(const T&);


/*
 * byte_order - byte order indicator
 */
enum class byte_order {
	little,
	big,
	not_relevant,
	invalid,
	// TODO: determine if native = little is correct
	native = little,
};


inline char
to_char(byte_order o) {
	switch (o) {
		case byte_order::little:       return '<';
		case byte_order::big:          return '>';
		case byte_order::not_relevant: return '|';

		// TODO: set a fail state for invalid
		case byte_order::invalid:      return '!';
	}
	return '!';
}


// operator<< usually used in std::cout
// TODO: remove or disable?
inline std::ostream&
operator<<(std::ostream &os, const byte_order bo)
{
	switch (bo) {
		case byte_order::little:       os << "little";       break;
		case byte_order::big:          os << "big";          break;
		case byte_order::not_relevant: os << "not_relevant"; break;
		case byte_order::invalid:      os << "invalid";      break;

		// this should never happen
		default: os.setstate(std::ios_base::failbit);
	}
	return os;
}


/*
 * storage_order - storage order of data in a dtype
 */
enum class storage_order {
	// linear storage in which consecutive elements form the columns, also
	// called 'fortran-order'
	col_major,

	// linear storage in which consecutive elements form the rows of data,
	// also called c-order
	row_major,
};


// operator<< usually used in std::cout
// TODO: remove or disable?
inline std::ostream&
operator<<(std::ostream &os, const storage_order order)
{
	switch (order) {
		case storage_order::col_major: os << "col_major"; break;
		case storage_order::row_major: os << "row_major"; break;
	}
	return os;
}



template <typename T = size_t>
std::vector<T>
unravel_index(int index, const std::vector<T>& shape, storage_order order)
{
	int n = shape.size();
	std::vector<int> indices(n);

	switch (order) {
	case storage_order::row_major:
		for (int i = n - 1; i >= 0; --i) {
			indices[i] = index % shape[i];
			index /= shape[i];
		}
		break;

	case storage_order::col_major:
		for (int i = 0; i < n; ++i) {
			indices[i] = index % shape[i];
			index /= shape[i];
		}
		break;
	}

	return indices;
}


template <typename T = size_t>
std::vector<T>
unravel_index_strided(size_t offset, const std::vector<T> &strides, storage_order order)
{
	std::vector<T> indices(strides.size());

	switch (order) {
	case storage_order::row_major:
		for (size_t i = 0; i < strides.size(); ++i) {
			size_t currentStride = strides[i];
			size_t currentIndex = offset / currentStride;
			offset %= currentStride;
			indices[i] = currentIndex;
		}
		break;

	case storage_order::col_major:
		for (size_t i = strides.size(); i-- > 0;) {
			size_t currentStride = strides[i];
			size_t currentIndex = offset / currentStride;
			offset %= currentStride;
			indices[i] = currentIndex;
		}
		break;
	}

	return indices;
}

// array with dimensions N_1 x N_2 x ... x N_d and index tuple (n_1,
// n_2, ..., n_d), n_k ∈ [0, N_k - 1]:
//
// formula for row-major: sum_{k=1}^d (prod_{l=k+1}^d N_l) * n_k
// formula for col-major: sum_{k=1}^d (prod_{l=1}^{k-1} N_l) * n_k


template <typename T = size_t>
T
stride_row_major(const std::vector<T> &shape, ssize_t l)
{
	size_t s = 1;
	for (; ++l < (ssize_t)shape.size(); )
		s *= shape[l];
	return s;
}


template <typename T = size_t>
T
stride_col_major(const std::vector<T> &shape, ssize_t k)
{
	size_t s = 1;
	for (; --k >= 0; )
		s *= shape[k];
	return s;
}


template <typename T = size_t, bool single_loop = true>
void
compute_strides(const std::vector<T> &shape, std::vector<T> &strides, storage_order order)
{
	strides.resize(shape.size());

	if constexpr (single_loop) {
		T total = 1;
		switch (order) {
		case storage_order::row_major:
			for (size_t i = shape.size(); i-- > 0; ) {
				strides[i] = total;
				total *= shape[i];
			}
			break;

		case storage_order::col_major:
			for (size_t i = 0; i < shape.size(); ++i) {
				strides[i] = total;
				total *= shape[i];
			}
			break;
		}
	}
	else {
		T (*fptr)(const std::vector<T> &shape, ssize_t);
		switch (order) {
		case storage_order::row_major:
			fptr = &stride_row_major<T>;
			break;
		case storage_order::col_major:
			fptr = &stride_col_major<T>;
			break;
		}
		for (size_t k = 0; k < shape.size(); k++)
			strides[k] = (*fptr)(shape, k);
	}
}



/*
 * dtype - data type description of elements in the ndarray
 *
 * In case of structured arrays, only the values in the fields might be properly
 * filled in. Note also that structured arrays can have types with arbitrarily
 * deep nesting of sub-structures. To determine if a (sub-)dtype is a structured
 * array, you can query is_structured_array(), which simply tests if there are
 * fields within this dtype.
 *
 * Note that fields in a dtype which are leaves, i.e. are basic types, will
 * return false for is_structured_array.
 *
 * Note that structured arrays might contain types with mixed endianness.
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

	// structured arrays will contain fields, which are themselves dtypes
	std::vector<dtype>
		fields     = {};
};



template <>
inline bool
is_structured_array<dtype>(const dtype &dtype)
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


// operator<< usually used in std::cout
// TODO: remove or disable?
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


/*
 * ndarray_item - items returned from ndarray's operator()
 *
 * This is an indirection to make the syntax to convert values slightly more
 * elegant. Instead of this indirection, it is also possible to implement range
 * views on top of the data ranges returned from ndarray's get function, or to
 * use ndarray's .value() function.
 *
 * The indirection allows to write the following code:
 *		array(row, col) = 123.f;
 *	and
 *		f32 f = array(row, col).as<float>();
 *
 * While the second example is only marginally shorter than
 *		f32 f = array.value<float>(row, col);
 * the first clearly is.
 *
 * However, beware of temporaries!
 */
struct ndarray_item
{
	ndarray_item() = delete;
	ndarray_item(u8_subrange &&_ra, dtype &_dt) : _r(_ra), _dtype(_dt) {}
	ndarray_item(u8_vector::iterator begin, u8_vector::iterator end, dtype &dtype) : _r(u8_subrange(begin, end)), _dtype(dtype) {}


	template <typename T>
	T&
	as() const {
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_r.size() < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _r.size() << " bytes)";
			throw std::length_error(s.str());
		}
		return *reinterpret_cast<T*>(_r.data());
	}


	template <typename T>
	void
	operator=(T value)
	{
		if (_r.size() < sizeof(T)) {
			std::ostringstream s;
			s << "Value size (" << sizeof(T) << " bytes) exceeds location size (" << _r.size() << " bytes)";
			throw std::length_error(s.str());
		}
		*reinterpret_cast<T*>(_r.data()) = value;
	}


	inline
	const u8*
	data() const {
		return _r.data();
	}


	inline
	const dtype&
	type() const {
		return _dtype;
	}


	template <typename T, typename... Args>
	static
	const T
	field(const ndarray_item &item, Args&&... args);


	template<typename T, typename... Args>
	const T
	get_field(Args&&... args) const {
		return field<T>(*this, std::forward<Args>(args)...);
	}


private:
	// the data subrange within the ndarray
	const u8_subrange _r;

	// the data type of the item (equal to the data type of its ndarray)
	const dtype &_dtype;
};


template <typename T, typename = void>
struct field_extractor
{
	static const T
	get_field(const ndarray_item &item, const dtype& dtype)
	{
		// TODO: bounds checking
		return *reinterpret_cast<const T*>(item.data() + dtype.offset);
	}
};


template <typename T>
struct field_extractor<T, std::enable_if_t<is_ucs4string<T>::value>>
{
	static const T
	get_field(const ndarray_item &item, const dtype& dtype)
	{
		// TODO: bounds checking
		constexpr auto N = ucs4string_size<T>::value;
		return to_ucs4<N>(*reinterpret_cast<const std::array<u32, N>*>(item.data() + dtype.offset));
	}
};


template <typename T, typename... Args>
const T
ndarray_item::field(const ndarray_item &item, Args&&... args)
{
	const dtype *dtype = get_nested_dtype(item.type(), args...);
	if (!dtype)
		throw std::runtime_error("Field not found: " + (... + ('/' + std::string(args))));
	return field_extractor<T>::get_field(item, *dtype);
}


/*
 * ndarray - basic ndarray without a lot of functionality
 *
 * TODO: documentation
 */
struct ndarray
{
	enum class result {
		ok,
		value_error
	};

	ndarray() {}

	// TODO: default data type
	ndarray(std::initializer_list<u64> shape,
	        dtype dtype = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dtype), _shape{shape}, _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	// TODO: default data type
	ndarray(u64_vector shape,
	        dtype dtype = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dtype), _shape(shape), _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	ndarray(dtype &&dtype,
	        u64_vector &&shape,
	        u8_vector &&buffer,
	        storage_order o = storage_order::row_major)
	: _dtype(std::move(dtype)) , _shape(std::move(shape)) , _order(o), _data(std::move(buffer))
	{
		_compute_size();
		_compute_strides();
	}


	/*
	 * assign - assign new data to this array
	 *
	 * Note that this will clear all available data beforehand
	 */
	void
	assign(dtype &&dtype,
	       u64_vector &&shape,
	       u8_vector &&buffer,
	       storage_order o = storage_order::row_major)
	{
		// tidy up first
		_shape.clear();
		_data.clear();

		// assign values
		_dtype = std::move(dtype);
		_shape = std::move(shape);
		_order = o;
		_data  = std::move(buffer);

		// recompute size and strides
		_compute_size();
		_compute_strides();
	}


	/*
	 * unravel - unravel a given index for this particular array
	 */
	template <typename T>
	std::vector<T>
	unravel(int index)
	{
		return unravel_index(index, _shape, _order);
	}


	/*
	 * get - get the u8 subrange in the data buffer for an element
	 */
	template <typename ...Indexes>
	u8_subrange
	get(Indexes... index)
	{
		// Number of indices must match number of dimensions
		assert(_shape.size() == sizeof...(Indexes));

		// test if indexes are out of bounds. we don't handle negative indexes
		if (sizeof...(Indexes) > 0) {
			{
				size_t i = 0;
				bool valid_index = ((index >= 0 && (size_t)index < _shape[i++]) && ...);
				if (!valid_index)
					throw std::out_of_range("Index out of bounds\n");
			}

			// this ravels the index, i.e. turns it into a flat index. note that
			// in contrast to numpy.ndarray.strides, _strides contains only
			// number of elements, not bytes. the bytes will be multiplied in
			// below when extracting u8_subrange
			size_t i = 0;
			size_t offset = 0;
			((offset += index * _strides[i], i++), ...);

			return u8_subrange(_data.begin() + _dtype.item_size * offset,
			                   _data.begin() + _dtype.item_size * (offset + 1));
		}
		else
			// TODO: evaluate if this is the correct response here
			return u8_subrange();
	}


	/*
	 * get - get the u8 subrange in the data buffer for an element
	 */
	u8_subrange
	get(u64_vector indexes)
	{
		// TODO: don't assert, throw exception
		assert(indexes.size() == _shape.size());
		if (indexes.size() > 0) {
			size_t offset = 0;
			for (size_t i = 0; i < indexes.size(); i++) {
				if (indexes[i] >= _shape[i])
					throw std::out_of_range("Index out of bounds\n");

				// update offset
				offset += indexes[i] * _strides[i];
			}
			return u8_subrange(_data.begin() + _dtype.item_size * offset,
			                   _data.begin() + _dtype.item_size * (offset + 1));
		}
		else
			// TODO: like above, evaluate if this is the correct response
			return u8_subrange();
	}


	/*
	 * operator() - convenience function to avoid template creep in ncr::numpy
	 *
	 * when only reading values, use .value() instead to avoid an intermediary
	 * ndarray_item. when writing values
	 */
	template <typename... Indexes>
	inline ndarray_item
	operator()(Indexes... index)
	{
		return ndarray_item(this->get(index...), _dtype);
	}


	/*
	 * operator() - convenience function to avoid template creep in ncr::numpy
	 *
	 * this function accepts a vector of indexes to access an array element at
	 * the specified location
	 */
	inline ndarray_item
	operator()(u64_vector indexes)
	{
		return ndarray_item(this->get(indexes), _dtype);
	}


	/*
	 * value - access the value at a given index
	 *
	 * This function returns a reference, which makes it possible to change the
	 * value.
	 */
	template <typename T, typename... Indexes>
	inline T&
	value(Indexes... index)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::length_error(s.str());
		}

		return *reinterpret_cast<T*>(this->get(index...).data());
	}


	/*
	 * value - access the value at a given index
	 *
	 * Given a vector of indexes, this function returns a reference to the value
	 * at this index. This makes it possible to change the value within the
	 * array's data buffer.
	 *
	 * Note: This function throws if the size of T is larger than elements
	 *       stored in the array, of if the indexes are out of bounds.
	 */
	template <typename T>
	inline T&
	value(u64_vector indexes)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::length_error(s.str());
		}
		return *reinterpret_cast<T*>(this->get(indexes).data());
	}


	/*
	 * transform - transform a value
	 *
	 * This is useful, for instance, when the data stored in the array is not in
	 * the same storage_order as the system that is using the data.
	 */
	template <typename T, typename Func = std::function<T (T)>, typename... Indexes>
	inline T
	transform(Func func, Indexes... index)
	{
		T val = value<T>(index...);
		return func(val);
	}


	/*
	 * apply - apply a function to each value in the array
	 *
	 * This function applies a user-specified function to each element of the
	 * array. The user-specified function will receive a constant subrange of u8
	 * containing the array element, and is expected to return a vector
	 * containing u8 of the same size as the range. If there is a size-mismatch,
	 * this function will throw an std::length_error.
	 *
	 * TODO: provide an apply function which also passes the element index back
	 * to the transformation function
	 */
	template <typename Func = std::function<u8_vector (u8_const_subrange)>>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = _dtype.item_size;
		while (offset < _data.size()) {
			auto range = u8_const_subrange(_data.begin() + offset, _data.begin() + offset + stride);
			auto new_value = func(range);
			if (new_value.size() != range.size())
				throw std::length_error("Invalid size of result");
			std::copy(new_value.begin(), new_value.end(), _data.begin() + offset);
			offset += stride;
		}
	}


	/*
	 * apply - apply a function to each value in the array
	 *
	 * This variant of apply takes a template argument T to internally
	 * reinterpret_cast the array elements to type T.
	 *
	 * Note: no size checking is performed. As a consequence, it is possible to
	 * call transform<i32>(...) on an array that stores values of types with
	 * different size, e.g. i16 or i64.
	 */
	template <typename T, typename Func = std::function<T (T)>>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = sizeof(T);
		while (offset < _data.size()) {
			T &val = *reinterpret_cast<T*>(_data.data() + offset);
			val = func(val);
			offset += stride;
		}
	}


	// TODO: give each ndarray_item its index
	template <typename Func = std::function<void (size_t, const ndarray_item&, const dtype)>>
	inline void
	map(Func func)
	{
		for (size_t i = 0; i < _size; i++) {
			func(ndarray_item(
					u8_subrange(_data.begin() + _dtype.item_size * i,
								_data.begin() + _dtype.item_size * (i + 1)),
					_dtype));
		}
	}


	template <typename T>
	T
	max()
	{
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::length_error(s.str());
		}

		auto stride = sizeof(T);
		auto nelems = _data.size() / stride;

		T _max = *reinterpret_cast<const T*>(&_data[0]);
		for (size_t i = 1; i < nelems; i++) {
			T val = *reinterpret_cast<const T*>(&_data[i * stride]);
			if (val > _max)
				_max = val;
		}
		return _max;
	}


	// TODO: provide variant with vector argument
	template <typename... Lengths>
	result
	reshape(Lengths... length)
	{
		size_t n_elems = (length * ...);
		if (n_elems != _size)
			return result::value_error;

		// set the shape
		_shape.resize(sizeof...(Lengths));
		size_t i = 0;
		((_shape[i++] = length), ...);

		// re-compute strides
		_compute_strides();
		return result::ok;
	}


	inline std::string
	get_type_description() const
	{
		std::ostringstream s;
		s << "{";
		serialize_dtype_descr(s, type());
		s << ", ";
		serialize_fortran_order(s, _order);
		if (_shape.size() > 0) {
			s << ", 'shape': ";
			serialize_shape(s, _shape);
		}
		s << ", ";
		// TODO: optional fields of the array interface
		s << "}";
		return s.str();
	}


	//
	// property getters
	//
	const dtype&        type()  const { return _dtype; }
	storage_order       order() const { return _order; }
	const u64_vector&   shape() const { return _shape; }
	const u8_vector&    data()  const { return _data;  }
	size_t              size()  const { return _size;  }

private:
	// _data stores the type information of the array
	dtype         _dtype;

	// _shape contains the shape of the array, meaning the size of each
	// dimension. Example: a shape of [2,3] would mean an array of size 2x3,
	// i.e. with 2 rows and 3 columns.
	u64_vector    _shape;

	// _size contains the number of elements in the array
	size_t        _size  = 0;

	// storage order used in this array. by default this corresponds to
	// row_major (or 'C' order). Alternatively, this could be col_major (or
	// 'Fortran' order).
	storage_order _order = storage_order::row_major;

	// _strides is the tuple (or vector) of elements in each dimension when
	// traversing the array. Note that this differs from numpy's ndarray.strides
	// in that _strides contains number of elements and *not* number of bytes.
	// the bytes depend on _dtype.item_size, and will be usually multiplied in
	// only after the number of elements to skip are determined. See get<> for
	// an example
	u64_vector    _strides;

	// _data contains the 'raw' data of the array
	u8_vector     _data;



	void
	_compute_strides()
	{
		compute_strides(_shape, _strides, _order);
	}


	void
	_compute_size()
	{
		// TODO: verify that dtype.item_size and computed _size match
		if (_shape.size() > 0) {
			auto prod = 1;
			for (auto &s: _shape)
				prod *= s;
			_size = prod;
		}
		else {
			if (_dtype.item_size > 0) {
				// infer from data and itemsize if possible
				_size = _data.size() / _dtype.item_size;
				// set shape to 1-dimensional of _size count
				if (_size > 0) {
					_shape.clear();
					_shape.push_back(_size);
				}
			}
			else {
				// can't do anything with dtype.item_size 0
				_size = 0;
			}
		}
	}


	// _resize - resize _data for _size many items
	//
	// Note that this should only be called in the constructor after setting
	// _dtype and _shape and after a call of _compute_size
	void
	_resize()
	{
		_data.clear();
		if (!_size)
			return;
		_data.resize(_size * _dtype.item_size);
	}
};


template <>
inline bool
is_structured_array<ndarray>(const ndarray &arr)
{
	return !arr.type().fields.empty();
}



/*
 * ndarray_t - simple typed facade for ndarray
 *
 * This template wraps an ndarray and provides new operator() which return
 * direct references to the underlying data. These facades are helpful when the
 * data type of an ndarray is properly known in advance and can be easily
 * converted to T. This is the case for all basic types.
 */
template <typename T>
struct ndarray_t : ncr::ndarray
{
	template <typename... Indexes>
	inline T&
	operator()(Indexes... index)
	{
		auto range = get(index...);
		return *reinterpret_cast<T*>(range.data());
	}

	inline T&
	operator()(u64_vector indexes)
	{
		auto range = get(indexes);
		return *reinterpret_cast<T*>(range.data());
	}
};


/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T, typename Func = std::function<T (T)>>
void
print_tensor(std::ostream &os, ncr::ndarray &arr, std::string indent, u64_vector &indexes, size_t dim, Func transform)
{
	auto shape = arr.shape();
	auto len   = shape.size();

	if (len == 0) {
		os << "[]";
		return;
	}

	if (dim == len - 1) {
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			if (i > 0)
				os << ", ";
			os << std::setw(2) << transform(arr.value<T>(indexes));
		}
		os << "]";
	}
	else {
		if (dim == 0)
			os << indent;
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			// indent
			if (i > 0)
				os << indent << std::setw(dim+1) << "";
			print_tensor<T>(os, arr, indent, indexes, dim+1, transform);
			if (shape[dim] > 1) {
				if (i < shape[dim] - 1)
					os << ",\n";
			}
		}
		os << "]";
	}
}


/*
 * print_tensor - print an ndarray to an ostream
 */
template <typename T, typename Func = std::function<T (T)>>
void
print_tensor(ncr::ndarray &arr, std::string indent="", Func transform = [](T v){ return v; }, std::ostream &os = std::cout)
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor<T>(os, arr, indent, indexes, 0, transform);
}


/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T>
void
print_tensor(std::ostream &os, ncr::ndarray_t<T> &arr, std::string indent, u64_vector &indexes, size_t dim)
{
	auto shape = arr.shape();
	auto len   = shape.size();

	if (len == 0) {
		os << "[]";
		return;
	}

	if (dim == len - 1) {
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			if (i > 0)
				os << ", ";
			os << std::setw(2) << arr(indexes);
		}
		os << "]";
	}
	else {
		if (dim == 0)
			os << indent;
		os << "[";
		for (size_t i = 0; i < shape[dim]; i++) {
			indexes[dim] = i;
			// indent
			if (i > 0)
				os << indent << std::setw(dim+1) << "";
			print_tensor(os, arr, indent, indexes, dim+1);
			if (shape[dim] > 1) {
				if (i < shape[dim] - 1)
					os << ",\n";
			}
		}
		os << "]";
	}
}


/*
 * print_tensor - print an ndarray to an ostream
 */
template <typename T>
void print_tensor(ncr::ndarray_t<T> &arr, std::string indent="")
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor(std::cout, arr, indent, indexes, 0);
}


} // ncr
