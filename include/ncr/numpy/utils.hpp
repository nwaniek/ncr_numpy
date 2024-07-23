/*
 * utils.hpp - utility functions for n-dimensional array implementation
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <iomanip>
#include <iostream>
#include "ndarray.hpp"

namespace ncr { namespace numpy {

/*
 * print_tensor - print an ndarray to an ostream
 *
 * Explicit interface, commonly a user does not need to use this function
 * directly
 */
template <typename T, typename Func = std::function<T (T)>>
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
template <typename T, typename Func = std::function<T (T)>>
void
print_tensor(ndarray &arr, std::string indent="", Func transform = [](T v){ return v; }, std::ostream &os = std::cout)
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


}} // ncr::numpy
