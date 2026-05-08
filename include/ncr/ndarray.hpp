/*
 * ndarray.hpp - n-dimensional array implementation
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
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
 * TODO: combine ndarray_item with ndarray_t::proxy if possible (and sensible)
 * TODO: broadcasting / ellipsis
 */
#ifndef _719685da6c474222b60a9d28795719db_
#define _719685da6c474222b60a9d28795719db_

#include <cstring> // for memcpy
#include <cassert>
#include <type_traits>
#include <vector>
#include <iostream>
#include <iomanip>
#include <span>

#include "ncr/types.hpp"
#include "ncr/unicode.hpp"
#include "ncr/ndindex.hpp"
#include "ncr/dtype.hpp"
#include "ncr/npybuffers.hpp"


namespace ncr { namespace numpy {



/*
 * forward declarations
 */
struct ndarray_item;
struct ndarray;


// using u8_span = std::span<const u8>;


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

	explicit
	ndarray_item(u8* data, size_t size, struct dtype &_dt)
		: _data(data)
		, _size(size)
		, _dtype(_dt)
		{}

	explicit
	ndarray_item(u8_span&& span, struct dtype& _dt)
		: _data(span.data())
		, _size(span.size())
		, _dtype(_dt)
		{}


	template <typename T>
	T
	as() const {
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_size != sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) mismatch with item size (" << _size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T val;
		std::memcpy(&val, _data, sizeof(T));
		return val;
	}


	template <typename T>
	void
	operator=(T value)
	{
		if (_size != sizeof(T)) {
			std::ostringstream s;
			s << "Value size (" << sizeof(T) << " bytes) mismatch with item size (" << _size << " bytes)";
			throw std::length_error(s.str());
		}
		std::memcpy(_data, &value, sizeof(T));
	}


	inline
	u8_const_span
	span() const {
		return u8_const_span(_data, _size);
	}


	inline
	const u8*
	data() const {
		return _data;
	}


	inline
	size_t
	bytesize() const {
		return _size;
	}


	inline
	const struct dtype&
	dtype() const {
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
	u8*
		_data;

	// the size of the subrange. this might be required frequently, so we store
	// it once
	const size_t
		_size;

	// the data type of the item (equal to the data type of its ndarray)
	const struct dtype &
		_dtype;
};


template <typename T, typename = void>
struct field_extractor
{
	static const T
	get_field(const ndarray_item &item, const dtype& dt)
	{
		auto range_size = item.bytesize();
		if ((dt.offset + sizeof(T)) > range_size) {
			std::ostringstream s;
			s << "Target type size (" << sizeof(T) << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, item.data() + dt.offset, sizeof(T));
		return value;
	}
};


template <typename T>
struct field_extractor<T, std::enable_if_t<is_ucs4string<T>::value>>
{
	static const T
	get_field(const ndarray_item &item, const dtype& dt)
	{
		constexpr auto N = ucs4string_size<T>::value;
		constexpr auto B = ucs4string_bytesize<T>::value;
		auto range_size = item.bytesize();
		if ((dt.offset + B) > range_size) {
			std::ostringstream s;
			s << "Target string size (" << B << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		std::array<u32, N> arr;
		std::memcpy(arr.data(), item.data() + dt.offset, sizeof(arr));
		return to_ucs4<N>(arr);
	}
};


template <typename T, typename... Args>
const T
ndarray_item::field(const ndarray_item &item, Args&&... args)
{
	const struct dtype *dt = find_field_recursive(item.dtype(), args...);
	if (!dt)
		throw std::runtime_error("Field not found: " + (... + ('/' + std::string(args))));
	return field_extractor<T>::get_field(item, *dt);
}


/*
 * ndarray - basic ndarray without a lot of functionality
 *
 * TODO: documentation
 * TODO: add option to tell the ndarray that it is not owning the buffer
 */
struct ndarray
{
	enum class result {
		ok,
		value_error
	};

	ndarray() {}
	~ndarray() { _release_buffer(); }

	// non-copyable
	ndarray(const ndarray&) = delete;
	ndarray& operator=(const ndarray&) = delete;

	// move constructor
	ndarray(ndarray&& other) noexcept
	: _dtype(std::move(other._dtype))
	, _shape(std::move(other._shape))
	, _size(other._size)
	, _order(other._order)
	, _strides(std::move(other._strides))
	, _data_ptr(other._data_ptr)
	, _data_size(other._data_size)
	, _buffer(other._buffer)
	{
		// leave the moved-from instance in a defensible state: no buffer
		// ownership and no dangling pointers
		other._buffer    = nullptr;
		other._data_ptr  = nullptr;
		other._data_size = 0;
		other._size      = 0;
	}

	// move assignment
	ndarray& operator=(ndarray&& other) noexcept
	{
		if (this == &other) return *this;

		// release the buffer this instance currently owns, otherwise the
		// pointer overwrite below would leak it
		_release_buffer();

		_dtype     = std::move(other._dtype);
		_shape     = std::move(other._shape);
		_size      = other._size;
		_order     = other._order;
		_strides   = std::move(other._strides);
		_data_ptr  = other._data_ptr;
		_data_size = other._data_size;
		_buffer    = other._buffer;

		other._buffer    = nullptr;
		other._data_ptr  = nullptr;
		other._data_size = 0;
		other._size      = 0;

		return *this;

	}

	// TODO: copy, which needs to explicitly be called and copy resources

	// TODO: default data type
	ndarray(std::initializer_list<u64> shape,
	        struct dtype dt = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dt), _shape{shape}, _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	// TODO: default data type
	ndarray(u64_vector shape,
	        struct dtype dt = dtype_float64(),
	        storage_order o = storage_order::row_major)
	: _dtype(dt), _shape(shape), _size(0), _order(o)
	{
		_compute_size();
		_resize();
		_compute_strides();
	}


	ndarray(struct dtype &&dt,
	        u64_vector &&shape,
	        u8_vector &&buffer,
	        storage_order o = storage_order::row_major)
	: _dtype(std::move(dt)) , _shape(std::move(shape)) , _order(o)
	{
		_from_vector_rvalue(std::move(buffer));
		_compute_size();
		_compute_strides();
	}


	/*
	 * assign - assign new data to this array
	 *
	 * Note that this will clear all existing data beforehand
	 */
	void
	assign(dtype &&dt,
	       u64_vector &&shape,
	       npybuffer *buffer,
	       storage_order o = storage_order::row_major)
	{
		// tidy up first
		_shape.clear();
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// assign values
		_dtype = std::move(dt);
		_shape = std::move(shape);
		_order = o;
		_from_npybuffer(buffer);

		// recompute size and strides
		_compute_size();
		_compute_strides();
	}


	/*
	 * unravel - unravel a given index for this particular array
	 */
	template <typename T = size_t>
	u64_vector
	unravel(size_t index)
	{
		return unravel_index<u64>(index, _shape, _order);
	}


	/*
	 * get - get the u8 span in the data buffer for an element
	 */
	template <typename ...Indexes>
	u8_span
	get(Indexes... index)
	{
		// Number of indices must match number of dimensions.
		if (_shape.size() != sizeof...(Indexes))
			throw std::out_of_range("ndarray::get: number of indices does not match array shape");

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

			return u8_span(_data_ptr + _dtype.item_size * offset, _dtype.item_size);
		}
		else
			// TODO: evaluate if this is the correct response here
			return u8_span();
	}


	/*
	 * get - get the u8 subrange in the data buffer for an element
	 */
	inline u8_span
	get(u64_vector indexes)
	{
		if (indexes.size() != _shape.size())
			throw std::out_of_range("ndarray::get: number of indices does not match array shape");

		if (indexes.size() > 0) {
			size_t offset = 0;
			for (size_t i = 0; i < indexes.size(); i++) {
				if (indexes[i] >= _shape[i])
					throw std::out_of_range("Index out of bounds\n");

				// update offset
				offset += indexes[i] * _strides[i];
			}
			return u8_span(_data_ptr + _dtype.item_size * offset, _dtype.item_size);
		}
		else
			// TODO: like above, evaluate if this is the correct response
			return u8_span();
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
	inline T
	value(Indexes... index)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, this->get(index...).data(), sizeof(T));
		return value;
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
	inline T
	value(u64_vector indexes)
	{
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}
		T value;
		std::memcpy(&value, this->get(indexes).data(), sizeof(T));
		return value;
	}


	/*
	 * transform - transform a value
	 *
	 * This is useful, for instance, when the data stored in the array is not in
	 * the same storage_order as the system that is using the data.
	 */
	template <typename T, typename Func, typename... Indexes>
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
	template <typename Func>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = _dtype.item_size;
		while (offset < _data_size) {
			auto span = u8_span(_data_ptr + offset, stride);
			auto new_value = func(span);
			if (new_value.size() != span.size())
				throw std::length_error("Invalid size of result");
			std::copy(new_value.begin(), new_value.end(), _data_ptr + offset);
			offset += stride;
		}
	}


	/*
	 * apply - apply a function to each value in the array given a type T
	 *
	 * Note: no size checking is performed. As a consequence, it is possible to
	 * call transform<i32>(...) on an array that stores values of types with
	 * different size, e.g. i16 or i64.
	 */
	template <typename T, typename Func>
	inline void
	apply(Func func)
	{
		size_t offset = 0;
		auto stride = sizeof(T);
		while (offset < _data_size) {
			T tmp;
			std::memcpy(&tmp, _data_ptr + offset, sizeof(T));
			tmp = func(tmp);
			std::memcpy(_data_ptr + offset, &tmp, sizeof(T));
			offset += stride;
		}
	}


	/*
	 * map - call a function for each element
	 *
	 * map calls a function for each item of the array, passing the item to the
	 * function that is given to map. The provided function will also receive
	 * the flat-index of the item, which can be used on the caller-side to get
	 * the multi-index (via ndarray::unravel).
	 */
	template <typename Func>
	inline void
	map(Func func)
	{
		for (size_t i = 0; i < _size; i++) {
			func(ndarray_item(
					_data_ptr + _dtype.item_size * i,
					_dtype.item_size,
					_dtype), i);
		}
	}


	template <typename T>
	T
	max()
	{
		if (_dtype.item_size < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _dtype.item_size << " bytes)";
			throw std::out_of_range(s.str());
		}

		auto stride = sizeof(T);
		auto nelems = _data_size / stride;

		T _max;
		std::memcpy(&_max, &_data_ptr[0], sizeof(T));
		for (size_t i = 1; i < nelems; i++) {
			T val;
			std::memcpy(&val, &_data_ptr[i * stride], sizeof(T));
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
		serialize_dtype_descr(s, dtype());
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

	void
	release()
	{
		_release_buffer();
	}

	//
	// property getters
	//
	const struct dtype& dtype()    const { return _dtype;     }
	storage_order       order()    const { return _order;     }
	const u64_vector&   shape()    const { return _shape;     }
	const u8*           data()     const { return _data_ptr;  }
	size_t              size()     const { return _size;      }
	size_t              bytesize() const { return _data_size; }

private:
	// _data stores the type information of the array
	struct dtype
		_dtype;

	// _shape contains the shape of the array, meaning the size of each
	// dimension. Example: a shape of [2,3] would mean an array of size 2x3,
	// i.e. with 2 rows and 3 columns.
	u64_vector
		_shape;

	// _size contains the number of elements in the array
	size_t
		_size  = 0;

	// storage order used in this array. by default this corresponds to
	// row_major (or 'C' order). Alternatively, this could be col_major (or
	// 'Fortran' order).
	storage_order
		_order = storage_order::row_major;

	// _strides is the tuple (or vector) of elements in each dimension when
	// traversing the array. Note that this differs from numpy's ndarray.strides
	// in that _strides contains number of elements and *not* number of bytes.
	// the bytes depend on _dtype.item_size, and will be usually multiplied in
	// only after the number of elements to skip are determined. See get<> for
	// an example
	u64_vector
		_strides;

	u8*
		_data_ptr = nullptr;

	size_t
		_data_size = 0;

	npybuffer*
		_buffer = nullptr;


	/*
	 * _compute_strides - compute the strides for this particular ndarray
	 */
	void
	_compute_strides()
	{
		compute_strides(_shape, _strides, _order);
	}


	/*
	 * _compute_size - compute the number of elements in the array
	 */
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
				_size = _data_size / _dtype.item_size;
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
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		if (!_size)
			return;
		_alloc_buffer(_size * _dtype.item_size);
	}


	void
	_from_npybuffer(npybuffer *buffer)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		_buffer = buffer;
		if (_buffer) {
			_data_ptr  = _buffer->get_data_ptr();
			_data_size = _buffer->get_data_size();
		}
	}


	void
	_from_vector_rvalue(u8_vector&& vec)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// move the vec into a new npybuffer of suitable type
		_buffer = new npybuffer(npybuffer::type::vector);
		_buffer->vector = make_vector_buffer(std::move(vec));
		_data_ptr  = _buffer->get_data_ptr();
		_data_size = _buffer->get_data_size();
	}


	void
	_alloc_buffer(size_t N)
	{
		// TODO: determine if _release_buffer should be called or not
		_release_buffer();

		// by default, we allocate a vector buffer
		_buffer = new npybuffer(npybuffer::type::vector);
		_buffer->vector = make_vector_buffer(N);
		_data_ptr  = _buffer->get_data_ptr();
		_data_size = _buffer->get_data_size();
	}


	void
	_release_buffer()
	{
		if (_buffer) {
			// best-effort tear-down: the buffer may have been swapped out
			// (move) and the underlying release() can technically fail (e.g.
			// munmap on a damaged mapping), but we cannot do anything useful
			// in a destructor path -> Cast to void to make the intent explicit.
			(void) _buffer->release();
			delete _buffer;
			_buffer = nullptr;
		}
		_data_size = 0;
		_data_ptr = nullptr;
	}
};


inline void
release(ndarray &arr)
{
	arr.release();
}


inline bool
is_structured_array(const ndarray &arr)
{
	return !arr.dtype().fields.empty();
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
struct ndarray_t : ndarray
{
	struct proxy {
		ndarray_t<T>& array;
		std::vector<size_t> indices;

		proxy(ndarray_t<T>& arr, std::vector<size_t> idx) : array(arr), indices(idx) {}

		// assignment operator so that an item can be used as lvalue
		proxy& operator=(const T& value)
		{
			auto range = array.get(indices);
			std::memcpy(range.data(), &value, sizeof(T));
			return *this;
		}

		// conversion operator to type T to allow using an item in an expression
		operator T() const
		{
			T val;
			auto range = array.get(indices);
			std::memcpy(&val, range.data(), sizeof(T));
			return val;
		}
	};

	// default constructor (without arguments), select dtype based on T
	ndarray_t()
	: ndarray(u64_vector{}, dtype_selector<T>::get(), storage_order::row_major) {}

	// constructor for shape and storage order, setting dtype based on T
	template <typename... Shape, typename = std::enable_if_t<sizeof...(Shape) != 0>>
	ndarray_t(Shape... shape, storage_order so = storage_order::row_major)
	: ndarray({static_cast<u64>(shape)...}, dtype_selector<T>::get(), so) {}

	// constructor for shape as an initializer list and storage order, setting dtype based on T
	ndarray_t(std::initializer_list<u64> shape, storage_order so = storage_order::row_major)
	: ndarray(shape, dtype_selector<T>::get(), so) {}

	// constructor for shape vector and storage order, setting dtype based on T
	ndarray_t(u64_vector shape, storage_order so = storage_order::row_major)
	: ndarray(shape, dtype_selector<T>::get(), so) {}

	// constructor for pre-allocated buffer and storage order, setting dtype based on T
	ndarray_t(u64_vector shape, u8_vector buffer, storage_order so = storage_order::row_major)
	: ndarray(dtype_selector<T>::get(), std::move(shape), std::move(buffer), so) {}

	template <typename... Indexes>
	inline proxy
	operator()(Indexes... index)
	{
		return proxy(*this, {static_cast<size_t>(index)...});
	}

	inline proxy
	operator()(u64_vector indexes)
	{
		return proxy(*this, indexes);
	}
};

template <typename T>
inline void
release(ndarray_t<T> &arr)
{
	arr.release();
}


/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T, typename Func>
void
print_tensor(std::ostream &os, ndarray &arr, std::string indent, u64_vector &indexes, size_t dim, Func transform)
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
// default identity-transform overload
template <typename T>
void
print_tensor(ndarray &arr, std::string indent="", std::ostream &os = std::cout)
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor<T>(os, arr, indent, indexes, 0, [](T v){ return v; });
}

// caller-supplied transform overload
template <typename T, typename Func>
void
print_tensor(ndarray &arr, std::string indent, Func transform, std::ostream &os = std::cout)
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
print_tensor(std::ostream &os, ndarray_t<T> &arr, std::string indent, u64_vector &indexes, size_t dim)
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
void print_tensor(ndarray_t<T> &arr, std::string indent="")
{
	auto shape = arr.shape();
	auto dims  = shape.size();
	u64_vector indexes(dims);
	print_tensor(std::cout, arr, indent, indexes, 0);
}



}} // ncr


#endif /* _719685da6c474222b60a9d28795719db_ */
