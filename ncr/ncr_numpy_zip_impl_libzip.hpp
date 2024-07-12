/*
 * ncr_numpy_zip_impl_libzip.hpp - ncr_numpy_zip backend based on libzip
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

// TODO: get rid of iostream
#include <iostream>

#include <zip.h>
#include <ncr/ncr_numpy_zip.hpp>

namespace ncr { namespace numpy { namespace zip {


/*
 * backend_state - libzip state
 */
struct backend_state
{
	// zip archive
	zip_t *zip {nullptr};

	// store all write buffers within the backend to make sure that the buffers
	// live long enough. zip_file_add does not directly read from the buffer,
	// and therefore a buffer might be invalid once writing actually happens
	std::vector<u8_vector> write_buffers;
};


/*
 * libzip_close - close a libzip backend sate
 */
inline result
libzip_close(backend_state *state)
{
	if (!state)
		return result::error_invalid_argument;
	if (state->zip != nullptr) {
		zip_close(state->zip);
		state->zip = nullptr;
	}
	return result::ok;
}


/*
 * libzip_get_file_list - get the list of files contained in an archive
 */
inline result
libzip_get_file_list(backend_state *state, std::vector<std::string> &list)
{
	if (!state)
		return result::error_invalid_state;
	if (!state->zip)
		return result::error_archive_not_open;

	zip_int64_t num_entries = zip_get_num_entries(state->zip, 0);
	for (zip_int64_t i = 0; i < num_entries; i++) {
		const char *fname = zip_get_name(state->zip, i, 0);
		if (fname == nullptr) {
			zip_error_t *error = zip_get_error(state->zip);
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


/*
 * libzip_make - make a (libzip) backend state
 */
inline result
libzip_make(backend_state **state)
{
	if (state == nullptr)
		return result::error_invalid_argument;

	result res = result::ok;
	if (*state != nullptr)
		res = result::warning_backend_ptr_not_null;

	*state = new backend_state{};
	return res;
}


/*
 * libzip_open - open a file from an archive
 */
inline result
libzip_open(backend_state *state, const std::filesystem::path filepath, filemode mode)
{
	if (!state)
		return result::error_invalid_argument;

	// TODO: currently, when opening for writing, the file will be truncated if
	//       it already exists. maybe a better approach would be to check the
	//       file. However, this is done already on the callsite (see savez and
	//       savez_compressed's overwrite argument). determine if this should be
	//       kept this way or not
	int flags = (mode == filemode::read) ? ZIP_RDONLY : (ZIP_CREATE | ZIP_TRUNCATE);

	int err = 0;
	if ((state->zip = zip_open(filepath.c_str(), flags, &err)) == nullptr) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);
		std::cerr << "cannot open zip archive " << filepath << ": " << zip_error_strerror(&error) << "\n";
		return result::error_invalid_filepath;
	}

	return result::ok;
}


/*
 * libzip_read - unzip a given filename (of an archive) into an u8 buffer
 */
inline result
libzip_read(backend_state *bptr, const std::string filename, u8_vector &buffer)
{
	if (!bptr)
		return result::error_invalid_state;
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


/*
 * libzip_release - release the libzip backend state
 */
inline result
libzip_release(backend_state **bptr)
{
	if (bptr == nullptr || *bptr == nullptr)
		return result::error_invalid_argument;
	(*bptr)->write_buffers.clear();
	delete *bptr;
	bptr = nullptr;
	return result::ok;
}


/*
 * libzip_write - write a buffer to a previously open zip archive
 */
inline result
libzip_write(backend_state *bptr, const std::string name, u8_vector &&buffer, bool compress, u32 compression_level = 0)
{
	if (!bptr)
		return result::error_invalid_state;
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


/*
 * get_backend_interface - get the (libzip) backend interface
 */
inline backend_interface&
get_backend_interface()
{
	static backend_interface interface = {
		libzip_make,
		libzip_release,
		libzip_open,
		libzip_close,
		libzip_get_file_list,
		libzip_read,
		libzip_write
	};
	return interface;
}


}}} // ncr::numpy::zip
