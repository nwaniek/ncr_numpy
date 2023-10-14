/*
 * ncr_numpy_impl_libzip - ncr_numpy zip backend based on libzip
 *
 * SPDX-FileCopyrightText: 2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#include <zip.h>
#include <ncr/ncr_numpy.hpp>

namespace ncr { namespace numpy { namespace zip {

struct backend
{
	// zip archive
	zip_t *zip = nullptr;

	// store all write buffers within the backend to make sure that the buffers
	// live long enough. zip_file_add does not directly read from the buffer,
	// and therefore a buffer might be invalid once writing actually happens
	std::vector<u8_vector> write_buffers;
};


result
close(backend *bptr)
{
	if (!bptr)
		return result::error_invalid_argument;
	zip_close(bptr->zip);
	return result::ok;
}


result
get_file_list(backend *bptr, std::vector<std::string> &list)
{
	if (!bptr)
		return result::error_invalid_bptr;
	if (!bptr->zip)
		return result::error_archive_not_open;

	zip_int64_t num_entries = zip_get_num_entries(bptr->zip, 0);
	for (zip_int64_t i = 0; i < num_entries; i++) {
		const char *fname = zip_get_name(bptr->zip, i, 0);
		if (fname == nullptr) {
			zip_error_t *error = zip_get_error(bptr->zip);
			// translate the error code
			switch (error->zip_err) {
				case ZIP_ER_MEMORY:  return result::error_memory;
				case ZIP_ER_DELETED: return result::error_file_deleted;
				case ZIP_ER_INVAL:   return result::error_invalid_file_index;
				default:             return result::internal_error;
			}
		}
		list.push_back(fname);
	}
	return result::ok;
}


result
make(backend **bptr)
{
	if (bptr == nullptr)
		return result::error_invalid_argument;

	result res = result::ok;
	if (*bptr != nullptr)
		res = result::warning_backend_ptr_not_null;

	*bptr = new backend;
	return res;
}


result
open(backend *bptr, const std::filesystem::path filepath, filemode mode)
{
	if (!bptr)
		return result::error_invalid_argument;

	// TODO: currently, when opening for writing, the file will be truncated if
	//       it already exists. maybe a better approach would be to check the
	//       file. However, this is done already on the callsite (see savez and
	//       savez_compressed's overwrite argument). determine if this should be
	//       kept this way or not
	int flags = (mode == filemode::read) ? ZIP_RDONLY : (ZIP_CREATE | ZIP_TRUNCATE);

	int err = 0;
	if ((bptr->zip = zip_open(filepath.c_str(), flags, &err)) == nullptr) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);
		std::cerr << "cannot open zip archive " << filepath << ": " << zip_error_strerror(&error) << "\n";
		return result::error_invalid_filepath;
	}

	return result::ok;
}


// unzip a given filename into a buffer
result
read(backend *bptr, const std::string filename, u8_vector &buffer)
{
	if (!bptr)
		return result::error_invalid_bptr;
	if (!bptr->zip)
		return result::error_archive_not_open;

	// get the file index
	zip_int64_t fid;
	if ((fid = zip_name_locate(bptr->zip, filename.c_str(), 0)) < 0) {
		zip_error_t *error = zip_get_error(bptr->zip);
		// translate the error code
		switch (error->zip_err) {
			case ZIP_ER_MEMORY:  return result::error_memory;
			case ZIP_ER_INVAL:   return result::error_invalid_file_index;
			case ZIP_ER_NOENT:   return result::error_file_not_found;
			default:             return result::internal_error;
		}
	}

	// open the file pointer
	zip_file_t *fp = zip_fopen_index(bptr->zip, fid, 0);
	if (fp == nullptr) {
		zip_error_t *error = zip_get_error(bptr->zip);
		switch (error->zip_err) {
			case ZIP_ER_MEMORY: return result::error_memory;
			case ZIP_ER_READ:   return result::error_read;
			case ZIP_ER_WRITE:  return result::error_write;
			// TODO: translate more errors
			default:            return result::internal_error;
		}
	}

	// get stats to know how many bytes to read
	zip_stat_t stat;
	if (zip_stat_index(bptr->zip, fid, 0, &stat) < 0) {
		zip_error_t *error = zip_get_error(bptr->zip);
		// translate the error code
		switch (error->zip_err) {
			case ZIP_ER_INVAL:   return result::error_invalid_file_index;
			default:             return result::internal_error;
		}
	}

	// finally, read the file into the buffer
	buffer.resize(stat.size);
	zip_int64_t nread = zip_fread(fp, buffer.data(), stat.size);
	if (nread < 0) {
		// TODO: better error reporting
		return result::internal_error;
	}
	else if (nread == 0) {
		return result::error_end_of_file;
	}

	int err_code;
	if ((err_code = zip_fclose(fp)) != 0) {
		return result::error_file_close;
	}

	return result::ok;
}


result
release(backend **bptr)
{
	if (bptr == nullptr || *bptr == nullptr)
		return result::error_invalid_argument;
	(*bptr)->write_buffers.clear();
	delete *bptr;
	bptr = nullptr;
	return result::ok;
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



/*
 * write - write a buffer to a previously open zip archive
 */
result
write(backend *bptr, const std::string name, u8_vector &&buffer, bool compress, u32 compression_level)
{
	if (!bptr)
		return result::error_invalid_bptr;
	if (!bptr->zip)
		return result::error_archive_not_open;

	// move the buffer into the local list of write buffers. this will retain
	// the livetime of the buffer as long as required (cleared only during
	// release and after zip_close was called, or on error)
	bptr->write_buffers.push_back(std::move(buffer));
	void *data_ptr = bptr->write_buffers.back().data();
	zip_uint64_t size = bptr->write_buffers.back().size();

	// create a source from the buffer
	zip_source_t *source = zip_source_buffer(bptr->zip, data_ptr, size, 0);
	if (!source) {
		return result::error_write;
	}

	zip_int64_t fid;
	if ((fid = zip_file_add(bptr->zip, name.c_str(), source, ZIP_FL_ENC_UTF_8)) < 0) {
		zip_source_free(source);
		return result::error_write;
	}

	if (compress) {
		// Note: ZIP_CM_DEFLATE accepts compression levels 1 to 9, with 0
		// indicating "default". the values origin from zlib. python's zlib
		// backend until python 3.7 used zlib's "default" value, meaning numpy
		// arrays were compressed most likely with the default value
		if (zip_set_file_compression(bptr->zip, fid, ZIP_CM_DEFLATE, compression_level) < 0) {
			return result::error_compression_failed;
		}
	}

	return result::ok;
}


}}} // ncr::numpy::zip
