/*
 * ndarray.hpp - n-dimensional array implementation
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

#include <ncr/core/types.hpp>
#include <ncr/core/bits.hpp>
#include <ncr/core/unicode.hpp>
#include <ncr/core/type_operators.hpp>

#include "core.hpp"
#include "dtype.hpp"

namespace ncr { namespace numpy {


/*
 * forward declarations
 */
struct ndarray_item;
struct ndarray;


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

	ndarray_item(u8_subrange &&_ra, struct dtype &_dt)
		: _r(_ra)
		, _size(sizeof(_r))
		, _dtype(_dt)
		{}

	ndarray_item(u8_vector::iterator begin, u8_vector::iterator end, dtype &dt)
		: _r(u8_subrange(begin, end))
		, _size(sizeof(_r))
		, _dtype(dt)
		{}


	template <typename T>
	T&
	as() const {
		// avoid reintrepreting to types which are too large and thus exceed
		// memory bounds
		if (_r.size() < sizeof(T)) {
			std::ostringstream s;
			s << "Template argument type size (" << sizeof(T) << " bytes) exceeds location size (" << _r.size() << " bytes)";
			throw std::out_of_range(s.str());
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
	const u8_subrange
	range() const {
		return _r;
	}


	inline
	const u8*
	data() const {
		return _r.data();
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
	const u8_subrange
		_r;

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
		auto range_size = item.range().size();
		if ((dt.offset + sizeof(T)) > range_size) {
			std::ostringstream s;
			s << "Target type size (" << sizeof(T) << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		return *reinterpret_cast<const T*>(item.data() + dt.offset);
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
		auto range_size = item.range().size();
		if ((dt.offset + B) > range_size) {
			std::ostringstream s;
			s << "Target string size (" << B << " bytes) out of range (" << range_size << " bytes, offset " << dt.offset << " bytes)";
			throw std::out_of_range(s.str());
		}
		return to_ucs4<N>(*reinterpret_cast<const std::array<u32, N>*>(item.data() + dt.offset));
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
	: _dtype(std::move(dt)) , _shape(std::move(shape)) , _order(o), _data(std::move(buffer))
	{
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
	       u8_vector &&buffer,
	       storage_order o = storage_order::row_major)
	{
		// tidy up first
		_shape.clear();
		_data.clear();

		// assign values
		_dtype = std::move(dt);
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
	template <typename T = size_t>
	u64_vector
	unravel(size_t index)
	{
		return unravel_index<u64>(index, _shape, _order);
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
			throw std::out_of_range(s.str());
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
			throw std::out_of_range(s.str());
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


	/*
	 * map - call a function for each element
	 *
	 * map calls a function for each item of the array, passing the item to the
	 * function that is given to map. The provided function will also receive
	 * the flat-index of the item, which can be used on the caller-side to get
	 * the multi-index (via ndarray::unravel).
	 */
	template <typename Func = std::function<void (const ndarray_item&, size_t)>>
	inline void
	map(Func func)
	{
		for (size_t i = 0; i < _size; i++) {
			func(ndarray_item(
					u8_subrange(_data.begin() + _dtype.item_size * i,
								_data.begin() + _dtype.item_size * (i + 1)),
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


	//
	// property getters
	//
	const struct dtype& dtype()    const { return _dtype; }
	storage_order       order()    const { return _order; }
	const u64_vector&   shape()    const { return _shape; }
	const u8_vector&    data()     const { return _data;  }
	size_t              size()     const { return _size;  }
	size_t              bytesize() const { return _data.size(); }

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

	// _data contains the 'raw' data of the array
	u8_vector
		_data;


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


}} // ncr
