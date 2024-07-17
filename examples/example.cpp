/*
 * example.cpp - examples for the usage of ncr_numpy
 *
 * SPDX-FileCopyrightText: 2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#include <ncr/ncr_numpy.hpp>
#include <ncr/ncr_ndarray.hpp>
#include <ncr/ncr_zip_impl_libzip.hpp>
#include <ncr/ncr_unicode.hpp>

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif

#ifndef VERSION
#define VERSION "0.0"
#endif


/*
 * operator<< - utility operator to dump an u8_const_subrange
 */
inline std::ostream&
operator<<(std::ostream &os, const u8_const_subrange &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		std::cout << *it;
	return os;
}


/*
 * strpad - pad a string with whitespace to make it at least length chars long
 */
inline std::string
strpad(const std::string& str, size_t length)
{
    return str + std::string(std::max(length - str.size(), size_t(0)), ' ');
}


/*
 * hexdump - print an u8_vector similar to hex editor displays
 */
void
hexdump(std::ostream& os, const u8_vector &data)
{
	// record current formatting
	std::ios old_state(nullptr);
	old_state.copyfmt(std::cout);

	const size_t bytes_per_line = 16;
	for (size_t offset = 0; offset < data.size(); offset += bytes_per_line) {
		os << std::setw(8) << std::setfill('0') << std::hex << offset << ": ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			// missing bytes will be replaced with whitespace
			if (offset + i < data.size())
				os << std::setw(2) << std::setfill('0') << std::hex << static_cast<i32>(data[offset + i]) << ' ';
			else
				os << "   ";
		}
		os << " | ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			if (offset + i >= data.size())
				break;

			// non-printable characters will be replaced with '.'
			char c = data[offset + i];
			if (c < 32 || c > 126)
				c = '.';
			os << c;
		}
		os << "\n";
	}
	// reset to previous state
	std::cout.copyfmt(old_state);
}


/*
 * buffer_from_file - fill an u8_vector by (binary) reading a file
 */
bool
buffer_from_file(std::filesystem::path filepath, u8_vector &buffer)
{
	std::ifstream fstream(filepath, std::ios::binary | std::ios::in);
	if (!fstream) {
		std::cerr << "Error opening file: " << filepath << std::endl;
		return false;
	}

	// resize buffer and read
	auto filesize = ncr::numpy::get_file_size(fstream);
	buffer.resize(filesize);
	fstream.read(reinterpret_cast<char*>(buffer.data()), filesize);

	// check for errors
	if (!fstream) {
		std::cerr << "Error reading file: " << filepath << std::endl;
		return false;
	}
	return true;
}


/*
 * example_ndarray - simple ndarray examples
 */
void
example_ndarray()
{
	std::cout << "ndarray example\n";
	std::cout << "---------------";

	ncr::ndarray array({2, 2}, ncr::dtype_float32());
	std::cout << "\nshape: ";
	ncr::serialize_shape(std::cout, array.shape());
	std::cout << "\ndtype: ";
	ncr::serialize_dtype(std::cout, array.type());
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
}


/*
 * example_simple_api - examples for the simple/high level ncr_numpy API
 */
void
example_simple_api(size_t padwidth = 30)
{
	std::cout << "Simple API\n";
	std::cout << "----------";

	auto val = ncr::numpy::load("assets/in/simple.npy");
	std::cout << std::boolalpha << "\n";
	std::cout << strpad("simple.npy:", padwidth) << std::holds_alternative<ncr::ndarray>(val) << "\n";
	print_tensor<i64>(std::get<ncr::ndarray>(val), "  ");
	std::cout << "\n\n";


	val = ncr::numpy::load("assets/in/simpletensor1.npy");
	std::cout << strpad("simpletensor1.npy:", padwidth) << std::holds_alternative<ncr::ndarray>(val) << "\n";
	print_tensor<f64>(std::get<ncr::ndarray>(val), "  ");
	std::cout << "\n\n";


	val = ncr::numpy::load("assets/in/simpletensor2.npy");
	std::cout << strpad("simpletensor2.npy:", padwidth) << std::holds_alternative<ncr::ndarray>(val) << "\n";
	print_tensor<i64>(std::get<ncr::ndarray>(val), "  ");
	std::cout << "\n\n";


	val = ncr::numpy::load("assets/in/complex.npy");
	std::cout << strpad("complex.npy:", padwidth) << std::holds_alternative<ncr::ndarray>(val) << "\n";
	// the data in this tensor needs a byteswap because it is stored in
	// big-endian, while most systems actually are little-endian. We can apply
	// the transform in the print_tensor function
	std::cout << "big-endian complex valued array transformed to little-endian on-the-fly:\n";
	print_tensor<c64>(std::get<ncr::ndarray>(val), "  ", [](c64 val){ return ncr::bswap<c64>(val); });
	std::cout << "\n\n";

	// another way to transform values is with the 'transform' method, which
	// transforms them given a function during the call. The example above used
	// a lambda to wrap ncr::bswap. However, ncr::bswap itself is a function
	// that fits the required signature. We can directly pass it to transform
	// instead of using a lambda. The numbers after the function are the indices
	// of the value which we want to transform
	auto arr = std::get<ncr::ndarray>(val);
	std::cout << "endianness transform during call to .transform(): ";
	std::cout << arr.transform<c64>(ncr::bswap<c64>, 1, 1) << "\n";

	// can also call apply() and transform each value in the array. Note that
	// there are different variants of apply, which might be useful when working
	// with structured arrays
	arr.apply<c64>(ncr::bswap<c64>);
	// after the previous line, all values are byteswapped within the array. we
	// can now use it regularly without having to transform it again
	std::cout << "array after endianness was changed in-place during call to .apply():\n";
	print_tensor<c64>(arr, "  ");
	std::cout << "\n\n";


	val = ncr::numpy::load("assets/in/structured.npy");
	std::cout << strpad("structured.npy:", padwidth) << std::holds_alternative<ncr::ndarray>(val) << "\n";


	val = ncr::numpy::load("assets/in/multiple_named.npz");
	std::cout << strpad("multiple_named.npz:", padwidth) << std::holds_alternative<ncr::numpy::npzfile>(val) << "\n";

	// try to load a file that does not exist. the variant will contain an
	// ncr::numpy::result with the error code describing what happened.
	val = ncr::numpy::load("assets/in/does_not_exist.npy");
	if (std::holds_alternative<ncr::numpy::result>(val))
		std::cout << strpad("does_not_exist.npy:", padwidth) << std::get<ncr::numpy::result>(val) << "\n";
	else
		std::cout << strpad("does_not_exist.npy:", padwidth) << "surprisingly, file was found o_O\n";
	std::cout << "\n";
}


/*
 * example_simple_api - examples for the slightly more explicit ncr_numpy API
 */
void
example_advanced_api(size_t padwidth = 30)
{
	ncr::numpy::npyfile npy;
	ncr::numpy::npzfile npz;
	ncr::ndarray arr;

	std::cout << "Advanced API\n";
	std::cout << "------------\n";

	std::cout << strpad("simple.npy:", padwidth) << ncr::numpy::from_npy("assets/in/simple.npy", arr, &npy) << "\n";

	ncr::numpy::clear(npy);
	std::cout << strpad("simpletensor1.npy:", padwidth) << ncr::numpy::from_npy("assets/in/simpletensor1.npy", arr, &npy) << "\n";

	ncr::numpy::clear(npy);
	std::cout << strpad("simpletensor2.npy:", padwidth) << ncr::numpy::from_npy("assets/in/simpletensor2.npy", arr, &npy) << "\n";

	ncr::numpy::clear(npy);
	std::cout << strpad("complex.npy:", padwidth) << ncr::numpy::from_npy("assets/in/complex.npy", arr, &npy) << "\n";

	ncr::numpy::clear(npy);
	std::cout << strpad("structured.npy:", padwidth) << ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy) << "\n";

	std::cout << strpad("multiple_named.npz:", padwidth) << ncr::numpy::from_npz("assets/in/multiple_named.npz", npz) << "\n";
	/// accessing existing arrays
	for (auto const& name: npz.names) {
		auto shape = npz[name].shape();
		std::cout << "    " << name << ".shape = ";
		ncr::serialize_shape(std::cout, shape);
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

	// attempt to open a file that does not exist. should produce
	// "error_file_not_found"
	std::cout << "\n";
	std::cout << strpad("invalid.npz:", padwidth) << ncr::numpy::from_npz("assets/in/invalid.npz", npz) << "\n";

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

	ncr::ndarray arr;
	ncr::numpy::npyfile npy;
	ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy);
	std::cout << strpad("write test:", padwidth) << ncr::numpy::save("assets/out/structured.npy", arr, true) << "\n";

	std::cout << "\n";
	std::cout << "Serialization examples: npz files\n";
	std::cout << "---------------------------------\n";

	// test npz -> load some of the files, and write them as npz.
	ncr::ndarray arr0 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/simple.npy"));
	std::cout << strpad("save simple.npz:", padwidth) << ncr::numpy::savez("assets/out/simple.npz", {{"simple_array", arr0}}, true) << "\n";

	// load some data that is then written to npz files
	ncr::numpy::variant_result val;
	ncr::ndarray arr1 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/simpletensor1.npy"));
	ncr::ndarray arr2 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/complex.npy"));

	// save the arrays with names
	std::cout << strpad("savez_named:", padwidth) << ncr::numpy::savez("assets/out/savez_named.npz", {{"arr1", arr1}, {"arr2", arr2}}, true) << "\n";
	std::cout << strpad("savez_named_compressed:", padwidth) << ncr::numpy::savez_compressed("assets/out/savez_named_compressed.npz", {{"arr1", arr1}, {"arr2", arr2}}, true) << "\n";

	// save the arrays without names (creates arr_0, arr_1, ...)
	std::cout << strpad("savez_unnamed:", padwidth) << ncr::numpy::savez("assets/out/savez_unnamed.npz", {arr1, arr2}, true) << "\n";
	std::cout << strpad("savez_unnamed_compressed:", padwidth) << ncr::numpy::savez_compressed("assets/out/savez_unnamed_compressed.npz", {arr1, arr2}, true) << "\n";


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
	buffer_from_file("assets/in/structured.npy", buf_in);
	hexdump(std::cout, buf_in);

	std::cout << "assets/out/structured.npy: \n";
	u8_vector buf_out;
	buffer_from_file("assets/out/structured.npy", buf_out);
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

	// we can create facades for arrays, which wrap operator(). This makes
	// working with ndarrays even easier than with the basic ndarray itself if
	// you know the underlying type of your data.
	ncr::ndarray_t<f64> arr;
	ncr::numpy::from_npy("assets/in/simpletensor1.npy", arr);
	std::cout << "shape: "; ncr::serialize_shape(std::cout, arr.shape()); std::cout << "\n";
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
	ncr::ucs4string<16>
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
		ncr::ucs4string str0 = ncr::to_ucs4("Hello, World");
		ncr::utf8string str1 = ncr::to_utf8(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		// variable width, internally stored as std::vector
		ncr::utf8string str0 = ncr::to_utf8("Hello, World");
		ncr::ucs4string str1 = ncr::to_ucs4(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		ncr::ucs4string<20> str0 = ncr::to_ucs4<20>("Hello, World");
		ncr::utf8string<20> str1 = ncr::to_utf8(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	{
		ncr::utf8string<20> str0 = ncr::to_utf8<20>("Hello, World");
		// Note: for fixed-size ucs4 strings, to_ucs4 requires at least one
		//       template argument.
		ncr::ucs4string<20> str1 = ncr::to_ucs4<20>(str0);
		std::cout << str0 << " :: " << str1 << "\n";
	}
	std::cout << "\n";

	std::cout << "Examples for structured arrays\n";
	std::cout << "------------------------------\n";

	ncr::ndarray arr;
	ncr::numpy::npyfile npy;
	ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy);

	std::cout << arr.type() << "\n";
	std::cout << "sizeof(student_t):  " << sizeof(student_t) << "\n";
	std::cout << "arr.item_size:      " << arr.type().item_size << "\n";
	std::cout << "student_t is a POD: " << (std::is_standard_layout_v<student_t> && std::is_trivial_v<student_t>) << "\n";


	// numpy uses C's memory layout for structured arrays. The array's data can
	// therefore be read directly into a suitable variable such as a POD struct
	std::cout << "Explicitly accessing data:\n";
	student_t &student = arr.value<student_t>(0);
	std::cout << "  " << student.name << " has grades " << student.grades[0] << " and " << student.grades[1] << "\n";

	// we can also use the apply function and a lambda to do this for all
	// students
	std::cout << "Walking over all items in the array:\n";
	arr.apply<student_t>(
		[](student_t &student) {
			std::cout << "  " << student.name << " has grades " << student.grades[0] << " and " << student.grades[1] << "\n";
			// don't forget to return (see definition of apply for details)
			return student;
		});
}


/*
 * country_gdp_record_packed_t - example struct for nested structured arrays
 *
 * This struct is packed, i.e. the compiler is supposed to remove any padding
 */
#pragma pack(push, 1)
struct country_gdp_record_packed_t
{
	ncr::ucs4string<16>
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
	ncr::ucs4string<16>
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
	os << "    " << strpad(ncr::to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
	os << "    " << strpad(ncr::to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
	os << "    " << strpad(ncr::to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
	return os;
}



/*
 * inspect_dtype - print some information about the bytes of a dtype
 *
 * TODO: move to a better place
 */
void
inspect_dtype(const ncr::dtype &dtype, std::string indent = "")
{
	for (const auto &field_dtype: dtype.fields) {
		std::cout << indent << field_dtype.name
			      << ": offset = " << field_dtype.offset
			      << ", item_size = " << field_dtype.item_size
			      << ", end = " << (field_dtype.offset + field_dtype.item_size)
			      << "\n";
		if (is_structured_array(field_dtype))
			inspect_dtype(field_dtype, indent + "  ");
	}
}


/*
 * example_nested - read a nested structured array
 */
void
example_nested()
{
	std::cout << "Examples for working with nested structured arrays\n";
	std::cout << "--------------------------------------------------\n";

	ncr::ndarray arr;
	ncr::numpy::npyfile npy;
	ncr::numpy::from_npy("assets/in/nested.npy", arr, &npy);

	// make sure that the sizes correspond when using methods that cast (e.g.
	// apply, value)! To achieve this, it might not be sufficient to simply have
	// POD data types, but sometimes also padding needs to be removed. See the
	// #pragma pack around struct year_gdp_record_t for an example how to avoid
	// padding.

	// the hexdump can be useful to compare the type description create by
	// ncr::ndarray and the one stored in the file
	u8_vector buf_in;
	buffer_from_file("assets/in/nested.npy", buf_in);
	hexdump(std::cout, buf_in);

	std::cout << "\n";
	std::cout << "dtype information\n";
	std::cout << arr.type() << "\n";
	std::cout << "type description string: " << arr.get_type_description() << "\n";
	inspect_dtype(arr.type());

	std::cout << "\n";
	std::cout << "sizeof(year_gdp_record_t):            " << sizeof(year_gdp_record_packed_t) << "\n";
	std::cout << "arr.item_size:                        " << arr.type().item_size << "\n";
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
			std::cout << "    " << strpad(ncr::to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
			std::cout << "    " << strpad(ncr::to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
			std::cout << "    " << strpad(ncr::to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
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
	arr.map([](const ncr::ndarray_item &item) {
			auto &record = item.as<year_gdp_record_packed_t>();
			std::cout << "  " << record.year << "\n";
			std::cout << "    " << strpad(ncr::to_string(record.c1.country_name) + ":", 10) << std::setw(10) << record.c1.gdp << " USD\n";
			std::cout << "    " << strpad(ncr::to_string(record.c2.country_name) + ":", 10) << std::setw(10) << record.c2.gdp << " USD\n";
			std::cout << "    " << strpad(ncr::to_string(record.c3.country_name) + ":", 10) << std::setw(10) << record.c3.gdp << " USD\n";
		});
	// note that, in principle, it's also possible to use ndarray_t for packed
	// PODs.

	std::cout.copyfmt(old_state);

	// padded structs, print some further information and a hexdump
	std::cout << "\n";
	std::cout << "Example of nested structured array when working with potentially padded structs\n";
	std::cout << "sizeof(year_gdp_record_t):     " << sizeof(year_gdp_record_t) << "\n";
	std::cout << "arr.item_size:                 " << arr.type().item_size << "\n";
	std::cout << "country_gdp_record_t is a POD: " << (std::is_standard_layout_v<country_gdp_record_t> && std::is_trivial_v<country_gdp_record_t>) << "\n";
	std::cout << "year_gdp_record_t is a POD:    " << (std::is_standard_layout_v<year_gdp_record_t> && std::is_trivial_v<year_gdp_record_t>) << "\n";

	// map the data into our custom structs using the array's map function and a
	// suitable lambda/callback
	old_state.copyfmt(std::cout);
	arr.map([](const ncr::ndarray_item &item) {
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
		record.year = ncr::ndarray_item::field<u32>(item, "year");

		record.c1.country_name = ncr::ndarray_item::field<ncr::ucs4string<16>>(item, "countries", "c1", "country");
		record.c1.gdp  = ncr::ndarray_item::field<u64>(item, "countries", "c1", "gdp");

		record.c2.country_name = ncr::ndarray_item::field<ncr::ucs4string<16>>(item, "countries", "c2", "country");
		record.c2.gdp  = item.get_field<u64>("countries", "c2", "gdp");

		record.c3.country_name = item.get_field<ncr::ucs4string<16>>("countries", "c3", "country");
		record.c3.gdp  = item.get_field<u64>("countries", "c3", "gdp");

		std::cout << record;
	});
	std::cout.copyfmt(old_state);


}


int
main()
{
	setlocale(LC_ALL, "");
	std::cout << "Examples for ncr_numpy " << VERSION << "\n\n";

	example_ndarray();       std::cout << "\n";
	example_simple_api();    std::cout << "\n";
	example_advanced_api();  std::cout << "\n";
	example_serialization(); std::cout << "\n";
	example_facade();        std::cout << "\n";
	example_structured();    std::cout << "\n";
	example_nested();


	return 0;
}
