/*
 * filesystem.hpp - ncr filesystem related functions and utilities
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <fstream>
#include <filesystem>

#include "types.hpp"
#include "common.hpp"


namespace ncr {


enum struct filesystem_status : unsigned {
	Success           = 0x00,
	ErrorFileNotFound = 0x01
};
NCR_DEFINE_ENUM_FLAG_OPERATORS(filesystem_status)



/*
 * get_file_size - get the file size of an ifstream in bytes
 */
inline u64
get_file_size(std::ifstream &is)
{
	auto ip = is.tellg();
	is.seekg(0, std::ios::end);
	auto res = is.tellg();
	is.seekg(ip);
	return static_cast<u64>(res);
}


/*
 * buffer_from_file - fill an u8_vector by (binary) reading a file
 */
inline bool
read_file(std::filesystem::path filepath, u8_vector &buffer)
{
	std::ifstream fstream(filepath, std::ios::binary | std::ios::in);
	if (!fstream) {
		std::cerr << "Error opening file: " << filepath << std::endl;
		return false;
	}

	// resize buffer and read
	auto filesize = get_file_size(fstream);
	buffer.resize(filesize);
	fstream.read(reinterpret_cast<char*>(buffer.data()), filesize);

	// check for errors
	if (!fstream) {
		std::cerr << "Error reading file: " << filepath << std::endl;
		return false;
	}
	return true;
}


} // ncr::
