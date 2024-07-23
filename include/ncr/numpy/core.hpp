/*
 * core - core algorithms for ncr_numpy
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <iostream>
#include <vector>

namespace ncr { namespace numpy {


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


/*
 * operator<< - pretty print the storage order as text
 */
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
unravel_index(T index, const std::vector<T>& shape, storage_order order)
{
	size_t n = shape.size();
	std::vector<T> indices(n);

	switch (order) {
	case storage_order::row_major:
		{
			size_t i = n;
			while (i > 0) {
				--i;
				indices[i] = static_cast<T>(index % shape[i]);
				index /= shape[i];
			}
		}
		break;

	case storage_order::col_major:
		for (size_t i = 0; i < n; ++i) {
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
			indices[i] = static_cast<T>(currentIndex);
		}
		break;

	case storage_order::col_major:
		for (size_t i = strides.size(); i-- > 0;) {
			size_t currentStride = strides[i];
			size_t currentIndex = offset / currentStride;
			offset %= currentStride;
			indices[i] = static_cast<T>(currentIndex);
		}
		break;
	}

	return indices;
}


/*
 * the strides for array with dimensions N_1 x N_2 x ... x N_d and
 * index tuple (n_1, n_2, ..., n_d), n_k ∈ [0, N_k - 1] can be computed as
 * follows:

 * formula for row-major: sum_{k=1}^d (prod_{l=k+1}^d N_l) * n_k
 * formula for col-major: sum_{k=1}^d (prod_{l=1}^{k-1} N_l) * n_k
 */


/*
 * stride_row_major - get the l-th stride of an array of given shape
 */
template <typename T = size_t>
T
stride_row_major(const std::vector<T> &shape, ssize_t l)
{
	size_t s = 1;
	for (; ++l < (ssize_t)shape.size(); )
		s *= shape[l];
	return s;
}


/*
 * stride_col_major - get the k-th stride of an array of given shape
 */
template <typename T = size_t>
T
stride_col_major(const std::vector<T> &shape, ssize_t k)
{
	size_t s = 1;
	for (; --k >= 0; )
		s *= shape[k];
	return s;
}


/*
 * compute_strides - compute the stires of an array of given shape and storage order
 */
template <typename T = size_t, bool single_loop = true>
void
compute_strides(const std::vector<T> &shape, std::vector<T> &strides, storage_order order = storage_order::row_major)
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
		T (*fptr)(const std::vector<T> &shape, ssize_t) =
			order == storage_order::row_major ?
				&stride_row_major<T> :
				&stride_col_major<T> ;
		for (size_t k = 0; k < shape.size(); k++)
			strides[k] = (*fptr)(shape, k);
	}
}


}} // ncr::numpy
