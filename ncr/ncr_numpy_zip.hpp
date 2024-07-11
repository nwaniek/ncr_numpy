/*
 * ncr_numpy_zip - zip backend interface declaration
 *
 * SPDX-FileCopyrightText: 2023-2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <ncr/ncr_types.hpp>
#include <filesystem>

namespace ncr { namespace numpy {

/*
 * zip - namespace for arbitrary zip backend implementations
 *
 * Everyone and their grandma has their favourite zip backend. Let it be some
 * custom rolled zlib backend, zlib-ng, minizip, libzip, etc. To provide some
 * flexibility in the backend and allow projects to avoid pulling in too many
 * dependencies (if they already use a backend), specify only an interface here.
 * It is up to the user to select which backend to use. With ncr_numpy ships
 * ncr_numpy_zip_impl_libzip.hpp, which implements a libzip backend.  Follow the
 * implementation there to implement alternative backends. Make sure to include
 * the appropriate one you want.
 */
namespace zip {
	// TODO: improve reporting of backend errors and pass-through of errors to
	// numpy's API

	// (partially) translated errors from within a zip backend. they are mostly
	// inspired from working with libzip
	enum class result {
		ok,

		warning_backend_ptr_not_null,

		error_invalid_filepath,
		error_invalid_argument,
		error_invalid_bptr,

		error_archive_not_open,
		error_invalid_file_index,
		error_file_not_found,
		error_file_deleted,
		error_memory,
		error_write,
		error_read,
		error_compression_failed,

		error_end_of_file,
		error_file_close,

		internal_error,
	};

	// mode for file opening.
	// TODO: currently, reading and writing are treated mutually exclusive. not
	//       clear if this is the best way to treat this
	enum class filemode : unsigned {
		read   = 1 << 0,
		write  = 1 << 1
	};

	// a zip backend might require to store state between calls, e.g. when
	// opening a file to store an (internal) file pointer. this needs to be
	// opaque to the interface and is handled here in a separate (implementation
	// specific) struct.
	struct backend_state;

	// common interface for any zip backend
	struct backend_interface {
		// create a backend state
		result (*make)(backend_state **);

		// release a backend pointer.
		result (*release)(backend_state **);

		// open an archive
		result (*open)(backend_state *, const std::filesystem::path filepath, filemode mode);

		// close an archive
		result (*close)(backend_state *);

		// return the list of files contained within an archive
		result (*get_file_list)(backend_state *, std::vector<std::string> &list);

		// read a given filename from an archive. Note that the filename relates to
		// a file within the archive, not on the local filesystem. The
		// decompressed/read file should be stored in `buffer'.
		result (*read)(backend_state *, const std::string filename, u8_vector &buffer);

		// write a buffer to an already opened zip archive. the compression level
		// depends on the backend. for zlib and many zlib based libraries, 0 is
		// most the default compression level, and other compression levels range
		// from 1 to 9, with 1 being the fastest (but weakest) compression and 9 the
		// slowest (but strongest).
		//
		// Note: the backend implementation will take ownership of the buffer. that
		// is, the buffer will be moved to the function. The reason is that
		// (currently) the buffer is created locally and its livetime might be
		// shorter than what the backend requires. With transfer of ownership, the
		// backend can make sure that the buffer survives as long as required. For
		// an example of this behavior, see ncr_numpy_zip_impl_libzip.hpp
		result (*write)(backend_state *, const std::string filename, u8_vector &&buffer, bool compress, u32 compression_level);
	};

	// get an interface for the backend
	extern backend_interface& get_backend_interface();

} // zip::

}}
