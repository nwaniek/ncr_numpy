/*
 * example.cpp - examples for the usage of ncr_numpy
 *
 * SPDX-FileCopyrightText: 2023-2025 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#define NCR_NUMPY_STANDALONE
#define NCR_ENABLE_STREAM_OPERATORS
#include "../ncr_numpy.hpp"
// #ifdef NCR_NUMPY_STANDALONE
// #include "../../meta/staging/ncr_numpy.hpp"
// #else
// #define NCR_ENABLE_STREAM_OPERATORS
// #include <ncr/strutil.hpp>
// #include <ncr/filesystem.hpp>
// #include <ncr/numpy.hpp>
// #include <ncr/impl/zip_libzip.hpp>
// #endif


#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif

#ifndef VERSION_REVISION
#define VERSION_REVISION 0
#endif

#ifndef VERSION
#define VERSION "0.0.0"
#endif


using namespace ncr;


#ifdef NCR_NUMPY_STANDALONE
/*
 * strpad - pad a string with whitespace to make it at least length chars long
 */
inline std::string
strpad(const std::string& str, size_t length)
{
    return str + std::string(std::max(length - str.size(), size_t(0)), ' ');
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


inline bool
read_file(std::filesystem::path filepath, u8_vector &buffer)
{
	std::ifstream fstream(filepath, std::ios::binary | std::ios::in);
	if (!fstream) {
		// std::cerr << "Error opening file: " << filepath << std::endl;
		return false;
	}

	// resize buffer and read
	auto filesize = get_file_size(fstream);
	buffer.resize(filesize);
	fstream.read(reinterpret_cast<char*>(buffer.data()), filesize);

	// check for errors
	if (!fstream) {
		// std::cerr << "Error reading file: " << filepath << std::endl;
		return false;
	}
	return true;
}
#endif


/*
 * example_ndarray - simple ndarray examples
 */
void
example_ndarray()
{
	std::cout << "ndarray example\n";
	std::cout << "---------------";

	numpy::ndarray array({2, 2}, numpy::dtype_float32());
	std::cout << "\nshape: ";
	numpy::serialize_shape(std::cout, array.shape());
	std::cout << "\ndtype: ";
	numpy::serialize_dtype(std::cout, array.dtype());
	std::cout << "\n";
	std::cout << array.get_type_description() << "\n";

	// read (and write)
	std::cout << "array before modification\n";
	for (size_t row = 0; row < 2; row++)
		for (size_t col = 0; col < 2; col++) {
			f32 f = array(row, col).as<f32>();
			std::cout << "  array(" << row << "," << col << ") = " << f << "\n";

			// set value to something else using the ndarray_item interface
			array(row, col) = (f32)(row + 1) + (f32)col * 0.1f;
		}

	// read the values again and display them
	std::cout << "\narray after modification\n";
	for (size_t row = 0; row < 2; row++)
		for (size_t col = 0; col < 2; col++) {
			f32 f = array.value<float>(row, col);
			std::cout << "  array(" << row << "," << col << ") = " << f << "\n";
		}

	std::cout << "\n";

	// need to explicitly release memory!
	numpy::release(array);
}


/*
 * example_simple_api - examples for the simple/high level ncr_numpy API
 */
void
example_simple_api(size_t padwidth = 30)
{
	std::cout << "Simple API\n";
	std::cout << "----------";


	numpy::ndarray arr;

	auto res = numpy::load("assets/in/simple.npy", arr);
	std::cout << std::boolalpha << "\n";
	std::cout << strpad("simple.npy:", padwidth) << (res == numpy::result::ok) << "\n";
	print_tensor<i64>(arr, "  ");
	std::cout << "\n\n";
	numpy::release(arr);


	res = numpy::load("assets/in/simpletensor1.npy", arr);
	std::cout << strpad("simpletensor1.npy:", padwidth) << (res == numpy::result::ok) << "\n";
	print_tensor<f64>(arr, "  ");
	std::cout << "\n\n";
	numpy::release(arr);


	res = numpy::load("assets/in/simpletensor2.npy", arr);
	std::cout << strpad("simpletensor2.npy:", padwidth) << (res == numpy::result::ok) << "\n";
	print_tensor<i64>(arr, "  ");
	std::cout << "\n\n";
	numpy::release(arr);


	res = numpy::load("assets/in/complex.npy", arr);
	std::cout << strpad("complex.npy:", padwidth) << (res == numpy::result::ok) << "\n";
	// the data in this tensor needs a byteswap because it is stored in
	// big-endian, while most systems actually are little-endian. We can apply
	// the transform in the print_tensor function
	std::cout << "big-endian complex valued array transformed to little-endian on-the-fly:\n";
	print_tensor<c64>(arr, "  ", [](c64 val){ return bswap<c64>(val); });
	std::cout << "\n\n";
	// another way to transform values is with the 'transform' method, which
	// transforms them given a function during the call. The example above used
	// a lambda to wrap bswap. However, bswap itself is a function
	// that fits the required signature. We can directly pass it to transform
	// instead of using a lambda. The numbers after the function are the indices
	// of the value which we want to transform
	std::cout << "endianness transform during call to .transform(): ";
	std::cout << arr.transform<c64>(bswap<c64>, 1, 1) << "\n";
	// can also call apply() and transform each value in the array. Note that
	// there are different variants of apply, which might be useful when working
	// with structured arrays
	arr.apply<c64>(bswap<c64>);
	// after the previous line, all values are byteswapped within the array. we
	// can now use it regularly without having to transform it again
	std::cout << "array after endianness was changed in-place during call to .apply():\n";
	print_tensor<c64>(arr, "  ");
	std::cout << "\n\n";
	numpy::release(arr);

	res = numpy::load("assets/in/structured.npy", arr);
	std::cout << strpad("structured.npy:", padwidth) << (res == numpy::result::ok) << "\n";
	numpy::release(arr);

	numpy::npzfile npz;
	res = numpy::loadz("assets/in/multiple_named.npz", npz);
	std::cout << strpad("multiple_named.npz:", padwidth) << (res == numpy::result::ok) << "\n";

	// try to load a file that does not exist. the variant will contain an
	// numpy::result with the error code describing what happened.
	res = numpy::load("assets/in/does_not_exist.npy", arr);
	if (res != numpy::result::ok) {
		std::cout << strpad("does_not_exist.npy:", padwidth) << numpy::to_string(res) << "\n";
	}
	else
		std::cout << strpad("does_not_exist.npy:", padwidth) << "surprisingly, file was found o_O\n";
	numpy::release(arr);

	std::cout << "\n";
}


/*
 * example_simple_api - examples for the slightly more explicit ncr_numpy API
 */
void
example_advanced_api(size_t padwidth = 30)
{
	numpy::npyfile npy;
	numpy::npzfile npz;
	numpy::ndarray arr;

	std::cout << "Advanced API\n";
	std::cout << "------------\n";

	// in this example we try to load numpy files, and then print the result
	// code from the call to numpy::from_npy/from_npz. Will use a lambda here to
	// avoid some boilerplate code which is the same for all example. also clear
	// the numpy file info "npy" within the lambda, as this should be done when
	// re-using a npyfile
	auto print_result = [padwidth, &npy](numpy::result res, std::string descr){
		std::cout << strpad(descr, padwidth) << numpy::to_string(res) << "\n";
		numpy::release(npy);
	};

	print_result(numpy::from_npy("assets/in/simple.npy", arr, &npy), "simpletensor1.npy");
	numpy::release(arr);
	numpy::release(npy);

	print_result(numpy::from_npy("assets/in/simpletensor2.npy", arr, &npy), "simpletensor2.npy");
	numpy::release(arr);
	numpy::release(npy);

	print_result(numpy::from_npy("assets/in/complex.npy", arr, &npy), "complex.npy");
	numpy::release(arr);
	numpy::release(npy);

	print_result(numpy::from_npy("assets/in/structured.npy", arr, &npy), "structured.npy");
	numpy::release(arr);
	numpy::release(npy);

	print_result(numpy::from_npz("assets/in/multiple_named.npz", npz), "multiple_named.npy");
	/// accessing existing arrays
	for (auto const& name: npz.names) {
		auto shape = npz[name].shape();
		std::cout << "    " << name << ".shape = ";
		numpy::serialize_shape(std::cout, shape);
		std::cout << "\n";
	}
	// trying to access an array which does not exist will throw an
	// std::runtime_error
	try {
		std::cout << npz["does_not_exist"].shape()[0];
	}
	catch (std::runtime_error &err) {
		std::cerr << err.what();
	}
	numpy::release(npz);

	// attempt to open a file that does not exist. should produce
	// "error_file_not_found"
	std::cout << "\n";
	print_result(numpy::from_npz("assets/in/invalid.npz", npz), "invalid.npz");
	numpy::release(npz);
	std::cout << "\n";
}


/*
 * example_serialization - examples for writing numpy arrays
 */
void
example_serialization(size_t padwidth = 30)
{
	std::cout << "Serialization examples: npy files\n";
	std::cout << "---------------------------------\n";

	// as in the previous example, use a lambda to reduce some of the code
	// verbosity in the example
	auto print_result = [padwidth](numpy::result res, std::string descr){
		std::cout << strpad(descr, padwidth) << numpy::to_string(res) << "\n";
	};

	numpy::ndarray arr;
	numpy::npyfile npy;
	numpy::from_npy("assets/in/structured.npy", arr, &npy);
	print_result(numpy::save("assets/out/structured.npy", arr, true), "structured.npy");
	numpy::release(arr, npy);

	std::cout << "\n";
	std::cout << "Serialization examples: npz files\n";
	std::cout << "---------------------------------\n";

	// test npz -> load some of the files, and write them as npz.
	numpy::ndarray arr0;
	numpy::load("assets/in/simple.npy", arr0);
	print_result(numpy::savez("assets/out/simple.npz", {{"simple_array", arr0}}, true), "simple.npz");
	numpy::release(arr0);

	// load some data that is then written to npz files
	numpy::ndarray arr1, arr2;
	numpy::load("assets/in/simpletensor1.npy", arr1);
	numpy::load("assets/in/complex.npy", arr2);
	// save the arrays with names
	print_result(numpy::savez("assets/out/savez_named.npz", {{"arr1", arr1}, {"arr2", arr2}}, true), "savez_named.npy:");
	print_result(numpy::savez_compressed("assets/out/savez_named_compressed.npz", {{"arr1", arr1}, {"arr2", arr2}}, true), "savez_named_compressed.npz:");
	// save the arrays without names (creates arr_0, arr_1, ...)
	print_result(numpy::savez("assets/out/savez_unnamed.npz", {arr1, arr2}, true), "save savez_unnamed.npz");
	print_result(numpy::savez_compressed("assets/out/savez_unnamed_compressed.npz", {arr1, arr2}, true), "savez_unnamed_compressed.npz");
	numpy::release(arr1, arr2);

	std::cout << "\n";
	std::cout << "hexdump comparison\n";
	std::cout << "------------------\n";
	// Note: file assets/in/structured.npy was generated using python+numpy and
	// has file version 1.0. In contrast ncr_numpy writes files using version
	// 2.0. The difference is that 2.0 uses 4 bytes for the header length
	// instead of 2. This can be verified visually for instance by looking at
	// the hex dump of the files:
	std::cout << "assets/in/structured.npy:\n";
	u8_vector buf_in;
	read_file("assets/in/structured.npy", buf_in);
	hexdump(std::cout, buf_in);

	std::cout << "assets/out/structured.npy: \n";
	u8_vector buf_out;
	read_file("assets/out/structured.npy", buf_out);
	hexdump(std::cout, buf_out);
}


/*
 * example_facade - examples for using the ndarray face `ndarray_t`
 */
void
example_facade()
{
	std::cout << "facade example\n";
	std::cout << "--------------\n";

	{
		std::cout << "dtype_selector\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<i16>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<i32>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<i64>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<u16>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<u32>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<u64>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<f16>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<f32>::get()); std::cout << "\n";
		numpy::serialize_dtype(std::cout, ncr::numpy::dtype_selector<f64>::get()); std::cout << "\n";
	}

	std::cout << "\narray and from_npy\n";

	// we can create facades for arrays, which wrap operator(). This makes
	// working with ndarrays even easier than with the basic ndarray itself if
	// you know the underlying type of your data.
	numpy::ndarray_t<f64> arr;
	numpy::serialize_dtype(std::cout, arr.dtype()); std::cout << "\n";

	numpy::from_npy("assets/in/simpletensor1.npy", arr);
	numpy::serialize_dtype(std::cout, arr.dtype()); std::cout << "\n";
	std::cout << "shape: "; numpy::serialize_shape(std::cout, arr.shape()); std::cout << "\n";
	std::cout << "\narray before changes\n"	;
	print_tensor(arr, "  ");
	std::cout << "\n";

	// change some random values and print again
	arr(0, 0, 0) = 7.0;
	arr(1, 1, 1) = 17.0;
	arr(1, 2, 3) = 23.1234;
	std::cout << "\narray after changes\n";
	print_tensor(arr, "  ");
	std::cout << "\n";

	// using arr in an expression
	f64 value = 5.0;
	value = value + arr(1, 2, 3);
	std::cout << "\nvalue = " << value << "\n";
	// numpy::release(arr);
}


/*
 * student_t - example struct for the structured.npy file
 *
 * The numpy file 'structured.npy' contains
 *     array([('Sarah', [8., 7.]), ('John', [6., 7.])],
 *           dtype=[('name', '<U16'), ('grades', '<f8', (2,))])
 *
 * which means the array contains structured arrays of the format
 *
 *		name  : unicode string of at most 16 characters
 *		grades: 2 64bit float values
 *
 * Because numpy uses C memory layout for structured arrays, this can be mapped
 * directly to a POD struct.
 */
struct student_t
{
	// each student has a name, stored as a unicode string with UCS-4 encoding
	// per character (see https://numpy.org/doc/stable/reference/arrays.dtypes.html
	// for more details)
	ucs4string<16>
		name;

	// each student has two grades, stored as a 64bit float
	f64
		grades[2];
};


/*
 * example_structured - examples for working with structured arrays
 */
void
example_structured()
{

	std::cout << "Basic tests for utf8 and ucs4 strings\n";
	std::cout << "-------------------------------------\n";
	{
		// variable width, internally stored as std::vector
		ucs4string str0 = to_ucs4("Hello, World");
		utf8string str1 = to_utf8(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		// variable width, internally stored as std::vector
		utf8string str0 = to_utf8("Hello, World");
		ucs4string str1 = to_ucs4(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		ucs4string<20> str0 = to_ucs4<20>("Hello, World");
		utf8string<20> str1 = to_utf8(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		utf8string<20> str0 = to_utf8<20>("Hello, World");
		// Note: for fixed-size ucs4 strings, to_ucs4 requires at least one
		//       template argument.
		ucs4string<20> str1 = to_ucs4<20>(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	std::cout << "\n";

	std::cout << "Examples for structured arrays\n";
	std::cout << "------------------------------\n";

	numpy::ndarray arr;
	numpy::npyfile npy;
	numpy::from_npy("assets/in/structured.npy", arr, &npy);

	std::cout << arr.dtype() << "\n";
	std::cout << "sizeof(student_t):  " << sizeof(student_t) << "\n";
	std::cout << "arr.item_size:      " << arr.dtype().item_size << "\n";
	std::cout << "student_t is a POD: " << (std::is_standard_layout_v<student_t> && std::is_trivial_v<student_t>) << "\n";


	// numpy uses C's memory layout for structured arrays. The array's data can
	// therefore be read directly into a suitable variable such as a POD struct
	std::cout << "Explicitly accessing data:\n";
	student_t student = arr.value<student_t>(0);
	std::cout << "  " << student.name << " has grades " << student.grades[0] << " and " << student.grades[1] << "\n";

	// we can also use the apply function and a lambda to do this for all
	// students
	std::cout << "Walking over all items in the array:\n";
	arr.apply<student_t>(
		[](student_t student) {
			std::cout << "  " << student.name << " has grades " << student.grades[0] << " and " << student.grades[1] << "\n";
			// don't forget to return (see definition of apply for details)
			return student;
		});

	numpy::release(arr, npy);
}


/*
 * country_gdp_record_packed_t - example struct for nested structured arrays
 *
 * This struct is packed, i.e. the compiler is supposed to remove any padding
 */
#pragma pack(push, 1)
struct country_gdp_record_packed_t
{
	ucs4string<16>
		country_name;

	u64
		gdp;
};
#pragma pack(pop)


/*
 * country_gdp_record_t - example struct for nested structured arrays
 *
 * This struct is *not* packed, i.e. the compiler can add padding
 */
struct country_gdp_record_t
{
	ucs4string<16>
		country_name;

	u64
		gdp;
};



/*
 * year_gdp_record_packed_t - example struct for nested structured arrays
 *
 * This struct is packed, i.e. the compiler is supposed to remove any padding
 */
#pragma pack(push, 1)
struct year_gdp_record_packed_t
{
	u32
		year;

	country_gdp_record_packed_t
		c1, c2, c3;
};
#pragma pack(pop)


/*
 * year_gdp_record_t - example struct for nested structured arrays
 *
 * This struct is *not* packed, i.e. the compiler can add padding
 */
struct year_gdp_record_t
{
	u32
		year;

	country_gdp_record_t
		c1, c2, c3;
};


/*
 * operator<< - pretty print a year_gdp_record_t
 */
inline std::ostream&
operator<< (std::ostream &os, const year_gdp_record_t &record)
{
	os << "  " << record.year << "\n";
	os << "    " << strpad(to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
	os << "    " << strpad(to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
	os << "    " << strpad(to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
	return os;
}



/*
 * inspect_dtype - print some information about the bytes of a dtype
 *
 * TODO: move to a better place
 */
void
inspect_dtype(const numpy::dtype &dtype, std::string indent = "")
{
	for_each(dtype.fields, [=](const numpy::dtype &field) {
		std::cout << indent << field.name
			      << ": offset = " << field.offset
			      << ", item_size = " << field.item_size
			      << ", end = " << (field.offset + field.item_size)
			      << "\n";
		if (is_structured_array(field))
			inspect_dtype(field, indent + "  ");
	});
}


/*
 * example_nested - read a nested structured array
 */
void
example_nested()
{
	std::cout << "Examples for working with nested structured arrays\n";
	std::cout << "--------------------------------------------------\n";

	numpy::ndarray arr;
	numpy::npyfile npy;
	numpy::from_npy("assets/in/nested.npy", arr, &npy);

	// make sure that the sizes correspond when using methods that cast (e.g.
	// apply, value)! To achieve this, it might not be sufficient to simply have
	// POD data types, but sometimes also padding needs to be removed. See the
	// #pragma pack around struct year_gdp_record_t for an example how to avoid
	// padding.

	// the hexdump can be useful to compare the type description create by
	// numpy::ndarray and the one stored in the file
	u8_vector buf_in;
	read_file("assets/in/nested.npy", buf_in);
	hexdump(std::cout, buf_in);

	std::cout << "\n";
	std::cout << "dtype information\n";
	std::cout << arr.dtype() << "\n";
	std::cout << "type description string: " << arr.get_type_description() << "\n";
	inspect_dtype(arr.dtype());

	std::cout << "\n";
	std::cout << "sizeof(year_gdp_record_t):            " << sizeof(year_gdp_record_packed_t) << "\n";
	std::cout << "arr.item_size:                        " << arr.dtype().item_size << "\n";
	std::cout << "country_gdp_record_packed_t is a POD: " << (std::is_standard_layout_v<year_gdp_record_packed_t> && std::is_trivial_v<year_gdp_record_packed_t>) << "\n";
	std::cout << "year_gdp_record_packed_t is a POD:    " << (std::is_standard_layout_v<year_gdp_record_packed_t> && std::is_trivial_v<year_gdp_record_packed_t>) << "\n";


	// one
	std::ios old_state(nullptr);
	old_state.copyfmt(std::cout);

	// one way to get the content of a structured array is using ndarray::apply.
	// This is particularly useful if the values inside the array should change,
	// because apply expects the callback to return a new value that will be
	// written in-place.
	std::cout << "Top 3 countries w.r.t GDP (via ndarray::apply):\n";
	arr.apply<year_gdp_record_packed_t>(
		[](year_gdp_record_packed_t &record) {
			std::cout << "  " << record.year << "\n";
			std::cout << "    " << strpad(to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
			std::cout << "    " << strpad(to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
			std::cout << "    " << strpad(to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
			// don't forget to return (see definition of apply for details)
			return record;
		});

	// however, apply above might not be ideal, because it takes the return
	// value and copies it back into the array. This is often not what
	// is wanted or required, and comes at the cost of copy operations.
	// Instead of apply, it's also possible to use map. map gives the callback
	// a reference to an ndarray_item instance, which can be cast to the
	// required type via ndarray_item::as.
	std::cout << "\n";
	std::cout << "Top 3 countries w.r.t GDP (via ndarray::map):\n";
	arr.map([&](const numpy::ndarray_item &item, size_t flat_index) {
			auto record = item.as<year_gdp_record_packed_t>();
			std::cout << "  " << record.year << " (item index: " << ncr::to_string(arr.unravel(flat_index)) << ")\n";
			std::cout << "    " << strpad(to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
			std::cout << "    " << strpad(to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
			std::cout << "    " << strpad(to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
		});
	// note that, in principle, it's also possible to use ndarray_t for packed
	// PODs.

	std::cout.copyfmt(old_state);

	// padded structs, print some further information and a hexdump
	std::cout << "\n";
	std::cout << "Example of nested structured array when working with potentially padded structs\n";
	std::cout << "sizeof(year_gdp_record_t):     " << sizeof(year_gdp_record_t) << "\n";
	std::cout << "arr.item_size:                 " << arr.dtype().item_size << "\n";
	std::cout << "country_gdp_record_t is a POD: " << (std::is_standard_layout_v<country_gdp_record_t> && std::is_trivial_v<country_gdp_record_t>) << "\n";
	std::cout << "year_gdp_record_t is a POD:    " << (std::is_standard_layout_v<year_gdp_record_t> && std::is_trivial_v<year_gdp_record_t>) << "\n";

	// map the data into our custom structs using the array's map function and a
	// suitable lambda/callback
	old_state.copyfmt(std::cout);
	arr.map([](const numpy::ndarray_item &item, size_t) {
		// Manually map each field into a struct member.
		//
		// The example shows how to use either the static ::field method of
		// ndarray_item, or the non-static membre function get_field (which in
		// turn calls the static method).
		//
		// For particular non-standard types that need special treatment of the
		// data underlying the item, please implement a custom field_extractor.
		// An example for this is provided for ucs4strings, see struct
		// field_extractor in ncr_ndarray.hpp

		year_gdp_record_t record;
		record.year = numpy::ndarray_item::field<u32>(item, "year");

		record.c1.country_name = numpy::ndarray_item::field<ucs4string<16>>(item, "countries", "c1", "country");
		record.c1.gdp  = numpy::ndarray_item::field<u64>(item, "countries", "c1", "gdp");

		record.c2.country_name = numpy::ndarray_item::field<ucs4string<16>>(item, "countries", "c2", "country");
		record.c2.gdp  = item.get_field<u64>("countries", "c2", "gdp");

		record.c3.country_name = item.get_field<ucs4string<16>>("countries", "c3", "country");
		record.c3.gdp  = item.get_field<u64>("countries", "c3", "gdp");

		std::cout << record;
	});
	std::cout.copyfmt(old_state);

	numpy::release(arr, npy);
}


/*
 * example_callback - how to use a callback when reading data
 */
void
example_callbacks()
{
	// sometimes data is too big to fit into memory, or one wants to pass the
	// data to an iterator, or it's not required to hold all data in memory, but
	// go through each item in a file once, process it, and then close the file
	// again. if that's the case, then the numpy::from_npy with a callback
	// function can be used.

	// Internally, this variant opens the file, parses the header, and reads one
	// item at a time and reports it back to you. Note that ncr::numpy does not
	// know how to handle the dtype, meaning it is unaware what to do with the
	// actual data, which is why you get a vector with the data (in addition to
	// all the dtype, shape, order information as well as a flat index to the
	// item). Casting it to the appropriate type (while checking byte ordering,
	// endianness, etc.) is up to you, also unravelling the flat index to a
	// multi-index if you need it.

	// the tensor contains i64 values, from which we want to sum up the first 30
	// elements
	i64 sum = 0;
	constexpr u64 max_count = 30;
	numpy::result res;

	//
	// In this first example, we'll use the most detailed version of the
	// callback, which will give you, besides the flat item index and the actual
	// raw item data, access to the dtype, the shape, as well as the
	// storage_order. It's up to you to cast the data into the appropriate
	// format.
	//
	if ((res = numpy::from_npy("assets/in/simpletensor2.npy",
		[&](const numpy::dtype &, const u64_vector& shape, const storage_order& order, u64 index, u8_vector item){
			// To exit early, simply return false from within the callback.
			// for instance when we read enough data
			if (index >= max_count)
				return false;

			// here we cast the data into the format that we want/expect. We
			// could also use dtype to determine if the data is actually in the
			// format that we expect, and if not, exit early.
			i64 value = *reinterpret_cast<i64*>(item.data());
			auto multi_index = unravel_index(index, shape, order);
			// use to_string's beg and end values to add space and :
			std::cout << "Item " << index << ncr::to_string(multi_index, {.end="]: "}) << value << "\n";
			sum += value;

			// we return true to let the backend know that we want to have more
			// data
			return true;
		})) != numpy::result::ok)
	{
		std::cout << "Callback Example 1, Error reading file: " << numpy::to_string(res) << "\n";
	}
	else {
		std::cout << "Callback Example 1, Computed sum = " << sum << " (expected sum = 435)\n";
	}

	//
	// In this second example, we'll use a more direct approach and hand over a
	// callback that expects a certain explicit type, and a flat index.
	//
	sum = 0;
	if ((res = numpy::from_npy<u64>("assets/in/simpletensor2.npy",
		[&](u64 index, u64 value){
			if (index >= max_count)
				return false;
			sum += value;
			return true;
		})) != numpy::result::ok)
	{
		std::cout << "Callback Example 2, Error reading file: " << numpy::to_string(res) << "\n";
	}
	else {
		std::cout << "Callback Example 2, Computed sum = " << sum << " (expected sum = 435)\n";
	}

	//
	// In this third example, we'll tell from_numpy which type the item should
	// have, and that we'd like to have a multi_index instead of a flat index.
	// it'll internally unravel the index for us, so we don't need to care about
	// dtype, shape, or order.
	//
	sum = 0;
	size_t i = 0;
	if ((res = numpy::from_npy<u64>("assets/in/simpletensor2.npy",
		[&](u64_vector index, u64 value){
			if (i++ >= max_count)
				return false;
			std::cout << "Item" << ncr::to_string(index, {.end = "]: "}) << value << "\n";
			sum += value;
			return true;
		})) != numpy::result::ok)
	{
		std::cout << "Callback Example 3, Error reading file: " << numpy::to_string(res) << "\n";
	}
	else {
		std::cout << "Callback Example 3, Computed sum = " << sum << " (expected sum = 435)\n";
	}


	//
	// In this fourth example, we'll use yet another form of from_npy, which
	// allows to pass in two separate callbacks. the first will be passed in
	// array properties, meaning its dtype, shape, and order, while the second
	// callback is one from the previous two examples, meaning either a callback
	// for a flat index, or for a multi-index.
	//
	sum = 0;
	i = 0;
	if ((res = numpy::from_npy<u64>("assets/in/simpletensor2.npy",
		[&](const numpy::dtype &dt, const u64_vector& shape, const storage_order& order){
			// This callback will be invoked first, so it is possible to use it
			// to setup other data, or emit information, or exit early if the
			// shape or contained data type is not what was expected.
			std::cout << "Array example 4, Array Properties: item size = " << dt.item_size << ", shape = " << ncr::to_string(shape) << ", storage order = " << order << "\n";

			// as with the other callbacks, we indicate by return value if
			// processing shall continue or not with a boolean return value
			return true;
		},
		[&](u64_vector index, u64 value){
			if (i++ >= max_count)
				return false;
			std::cout << "Item" << ncr::to_string(index, {.end = "]: "}) << value << "\n";
			sum += value;
			return true;
		})) != numpy::result::ok)
	{
		std::cout << "Callback Example 4, Error reading file: " << numpy::to_string(res) << "\n";
	}
	else {
		std::cout << "Callback Example 4, Computed sum = " << sum << " (expected sum = 435)\n";
	}
}


void
example_readerng()
{
	{ // mmap stuff
		numpy::npyreader<numpy::source_type::mmap> reader;
		auto res = numpy::open("assets/in/simple.npy", reader);
		std::cout << "open = " << to_string(res);
		std::cout << ", eof = " << reader.source.eof();
		std::cout << ", shape = " << to_string(reader.shape);

		size_t i = 0;
		std::cout << " ";
		for (auto item: reader) {
			i64 val;
			std::memcpy(&val, item.data(), sizeof(i64));
			if (i > 0) std::cout << ", ";
			std::cout << val;
			i += 1;
		}
		std::cout << ", count = " << i;

		reader.seek(4);
		i64 foo = reader.view<i64>();
		std::cout << ", view-value = " << foo;

		std::cout << "\n";

		numpy::close(reader);
	}

	{ // fstream stuff
		numpy::npyreader<numpy::source_type::fstream> reader;
		auto res = numpy::open("assets/in/simple.npy", reader);
		std::cout << "open = " << to_string(res);
		std::cout << ", eof = " << reader.source.eof();
		std::cout << ", shape = " << to_string(reader.shape);

		size_t i = 0;
		std::cout << " ";
		// for (auto val: reader.as<i64>()) {
		for (auto item: reader) {
			i64 val;
			std::memcpy(&val, item.data(), sizeof(i64));

			if (i > 0) std::cout << ", ";
			std::cout << val;
			i += 1;
		}
		std::cout << ", count = " << i;
		std::cout << "\n";

		numpy::close(reader);
	}

	{ // buffered stuff
		numpy::npyreader<numpy::source_type::buffered> reader;
		auto res = numpy::open("assets/in/simple.npy", reader);
		std::cout << "open = " << to_string(res);
		std::cout << ", eof = " << reader.source.eof();
		std::cout << ", shape = " << to_string(reader.shape);

		size_t i = 0;
		std::cout << " ";
		for (auto val: reader.as<i64>()) {
		//for (auto item: reader) {
		//	i64 val;
		//	std::memcpy(&val, item.data(), sizeof(i64));

			if (i > 0) std::cout << ", ";
			std::cout << val;
			i += 1;
		}
		std::cout << ", count = " << i;

		reader.seek(4);
		i64 foo = reader.view<i64>();
		std::cout << ", view-value = " << foo;

		std::cout << "\n";

		numpy::close(reader);
	}
}


int
main()
{
	// setlocale(LC_ALL, "");
	// std::cout << "Examples for ncr_numpy " << VERSION << "\n\n";

	example_ndarray();       std::cout << "\n";
	example_simple_api();    std::cout << "\n";
	example_advanced_api();  std::cout << "\n";
	example_serialization(); std::cout << "\n";
	example_facade();        std::cout << "\n";
	example_structured();    std::cout << "\n";
	example_nested();        std::cout << "\n";
	example_callbacks();     std::cout << "\n";
	example_readerng();

	return 0;
}
