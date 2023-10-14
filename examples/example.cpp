/*
 * example.cpp - examples for the usage of ncr_numpy
 *
 * SPDX-FileCopyrightText: 2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#include <ncr/ncr_numpy.hpp>
#include <ncr/ncr_ndarray.hpp>


inline std::ostream&
operator<<(std::ostream &os, const u8_const_subrange &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		std::cout << *it;
	return os;
}


void
hexdump(std::ostream& os, const u8_vector &data)
{
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
}


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


void
test_ndarray()
{
	std::cout << "Test ndarray\n";
	std::cout << "------------";

	ncr::ndarray array({2, 2}, ncr::dtype_float32());
	std::cout << "\nshape: ";

	// use ncr::numpy's function for pretty printing
	ncr::serialize_shape(std::cout, array.shape());
	std::cout << "\n\n";

	// read (and write)
	std::cout << "array before modification\n";
	for (size_t row = 0; row < 2; row++)
		for (size_t col = 0; col < 2; col++) {
			f32 f = array(row, col).as<f32>();
			std::cout << "  array(" << row << "," << col << ") = " << f << "\n";

			// set value to something else
			array.value<float>(row, col) = (f32)(row + 1) + (f32)col * 0.1f;
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


void
test_simple_api()
{
	std::cout << "Simple API\n";
	std::cout << "----------";

	auto val = ncr::numpy::load("assets/in/simple.npy");
	std::cout << std::boolalpha;
	std::cout << "\nsimple.npy:               " << std::holds_alternative<ncr::ndarray>(val);

	val = ncr::numpy::load("assets/in/simpletensor1.npy");
	std::cout << "\nsimpletensor1.npy:        " << std::holds_alternative<ncr::ndarray>(val);

	val = ncr::numpy::load("assets/in/simpletensor2.npy");
	std::cout << "\nsimpletensor2.npy:        " << std::holds_alternative<ncr::ndarray>(val);

	val = ncr::numpy::load("assets/in/complex.npy");
	std::cout << "\ncomplex.npy:              " << std::holds_alternative<ncr::ndarray>(val);

	val = ncr::numpy::load("assets/in/structured.npy");
	std::cout << "\nstructured.npy:           " << std::holds_alternative<ncr::ndarray>(val);

	val = ncr::numpy::load("assets/in/multiple_named.npz");
	std::cout << "\nmultiple_named.npz:       " << std::holds_alternative<ncr::numpy::npzfile>(val);

	// try to load a file that does not exist. the variant will contain an
	// ncr::numpy::result with the error code describing what happened.
	val = ncr::numpy::load("assets/in/does_not_exist.npy");
	if (std::holds_alternative<ncr::numpy::result>(val))
		std::cout << "\ndoes_not_exist.npy:       " << std::get<ncr::numpy::result>(val);
	else
		std::cout << "\ndoes_not_exist.npy:       surprisingly, file was found o_O";

	std::cout << "\n";
}


void
test_advanced_api()
{
	ncr::numpy::npyfile npy;
	ncr::numpy::npzfile npz;
	ncr::ndarray arr;

	std::cout << "Advanced API\n";
	std::cout << "------------";

	std::cout << "\nsimple.npy:               " << ncr::numpy::from_npy("assets/in/simple.npy", arr, &npy);

	ncr::numpy::clear(npy);
	std::cout << "\nsimpletensor1.npy:        " << ncr::numpy::from_npy("assets/in/simpletensor1.npy", arr, &npy);

	ncr::numpy::clear(npy);
	std::cout << "\nsimpletensor2.npy:        " << ncr::numpy::from_npy("assets/in/simpletensor2.npy", arr, &npy);

	ncr::numpy::clear(npy);
	std::cout << "\ncomplex.npy:              " << ncr::numpy::from_npy("assets/in/complex.npy", arr, &npy);

	ncr::numpy::clear(npy);
	std::cout << "\nstructured.npy:           " << ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy);

	std::cout << "\nmultiple_named.npz:       " << ncr::numpy::from_npz("assets/in/multiple_named.npz", npz);

	std::cout << "\n";
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

	std::cout << "\n";
}


void
test_serialization()
{
	std::cout << "Serialization examples: npy files\n";
	std::cout << "---------------------------------";

	ncr::ndarray arr;
	ncr::numpy::npyfile npy;
	ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy);
	std::cout << "\nwrite test:               " << ncr::numpy::save("assets/out/structured.npy", arr, true);

	std::cout << "\n\n";
	std::cout << "Serialization examples: npz files\n";
	std::cout << "---------------------------------";

	// test npz -> load some of the files, and write them as npz.
	ncr::ndarray arr0 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/simple.npy"));
	std::cout << "\nsave simple.npz:          " << ncr::numpy::savez("assets/out/simple.npz", {{"simple_array", arr0}}, true);

	// load some data that is then written to npz files
	ncr::numpy::variant_result val;
	ncr::ndarray arr1 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/simpletensor1.npy"));
	ncr::ndarray arr2 = ncr::numpy::get_ndarray(ncr::numpy::load("assets/in/complex.npy"));

	// save the arrays with names
	std::cout << "\nsavez_named:              " << ncr::numpy::savez("assets/out/savez_named.npz", {{"arr1", arr1}, {"arr2", arr2}}, true);
	std::cout << "\nsavez_named_compressed:   " << ncr::numpy::savez("assets/out/savez_named_compressed.npz", {{"arr1", arr1}, {"arr2", arr2}}, true);

	// save the arrays without names (creates arr_0, arr_1, ...)
	std::cout << "\nsavez_unnamed:            " << ncr::numpy::savez("assets/out/savez_unnamed.npz", {arr1, arr2}, true);
	std::cout << "\nsavez_unnamed_compressed: " << ncr::numpy::savez_compressed("assets/out/savez_unnamed_compressed.npz", {arr1, arr2}, true);


	std::cout << "\n\n";
	std::cout << "hexdump comparison\n";
	std::cout << "------------------";
	// Note: file assets/in/structured.npy was generated using python+numpy and
	// has file version 1.0. In contrast ncr_numpy writes files using version
	// 2.0. The difference is that 2.0 uses 4 bytes for the header length
	// instead of 2. This can be verified visually for instance by looking at
	// the hex dump of the files:
	std::cout << "\nassets/in/structured.npy:  \n";
	u8_vector buf_in;
	buffer_from_file("assets/in/structured.npy", buf_in);
	hexdump(std::cout, buf_in);

	std::cout << "\nassets/out/structured.npy: \n";
	u8_vector buf_out;
	buffer_from_file("assets/out/structured.npy", buf_out);
	hexdump(std::cout, buf_out);

	std::cout << "\n";
}


int
main()
{
	test_ndarray();      std::cout << "\n";
	test_simple_api();   std::cout << "\n";
	test_advanced_api(); std::cout << "\n";

	test_serialization();

	return 0;
}
