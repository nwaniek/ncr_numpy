/*
 * zip - zip backend interface declaration
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#ifndef _8d2a79e5218b40e3807880febfa294a0_
#define _8d2a79e5218b40e3807880febfa294a0_

//@ncr-fusor-keep-includes-start
#ifndef NCR_TYPES
#include <cstdint>
using u8 = std::uint8_t;
using u32 = std::uint32_t;
#endif

#ifndef NCR_TYPES_HAS_VECTORS
#include <vector>
using u8_vector = std::vector<u8>;
#endif
//@ncr-fusor-keep-includes-end

#include <string>
#include <filesystem>

namespace ncr {

/*
 * zip - namespace for arbitrary zip backend implementations
 *
 * Everyone and their grandma has their favourite zip backend. Let it be some
 * custom rolled zlib backend, zlib-ng, minizip, libzip, etc. To provide some
 * flexibility in the backend and allow projects to avoid pulling in too many
 * dependencies (if they already use a backend), specify only an interface here.
 * It is up to the user to select which backend to use. For an example backend
 * implementation, see ncr_zip_impl_libzip.hpp, which implements a libzip
 * backend. Follow the implementation there to implement alternative backends.
 * Make sure to include the appropriate one you want.
 */
namespace zip {
	// (partially) translated errors from within a zip backend. they are mostly
	// inspired from working with libzip
	enum class result {
		ok,

		warning_backend_ptr_not_null,

		error_invalid_filepath,
		error_invalid_argument,
		error_invalid_state,

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
	//       clear if this is the best way to treat this. Also, when opening for
	//       writing, the file will be truncated at least in the libzip backend
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
	//
	// Lifecycle contract for implementors:
	//
	//   make(&state)          -> allocates a fresh backend_state. The
	//                            pointer the caller passes in must be
	//                            null; callers should set it to nullptr
	//                            before calling. On success *state is
	//                            non-null and owns its resources.
	//   open(state, p, mode)  -> binds the state to the file at `p`. May
	//                            be called only on an opened-but-empty
	//                            state (i.e. after make() and before any
	//                            other call). The backend_state must
	//                            survive until close().
	//   close(state)          -> closes the file but leaves the state
	//                            usable for another open(). Idempotent.
	//   release(&state)       -> frees the backend_state (deletes the
	//                            object, sets *state = nullptr). After
	//                            release the pointer the caller still
	//                            holds is invalid.
	//   write(...)            -> the backend takes *ownership* of the
	//                            buffer (it is moved in). Some backends
	//                            (libzip in particular) defer the actual
	//                            compression to close(), so the buffer
	//                            *must* live until the backend itself is
	//                            closed; callers must therefore not
	//                            reuse, read, or otherwise touch the
	//                            moved-from buffer after the call.
	//   read(...)             -> the backend resizes `buffer` and writes
	//                            the decompressed file content into it.
	//                            Ownership of the buffer stays with the
	//                            caller.
	struct backend_interface {
		// create a backend state. *state must be null on entry.
		result (*make)(backend_state **);

		// free a backend state. *state is set to nullptr on success.
		result (*release)(backend_state **);

		// open an archive on disk. The state must have been created via
		// make() and not currently be associated with another file.
		result (*open)(backend_state *, const std::filesystem::path filepath, filemode mode);

		// close an archive. Idempotent. The state remains usable.
		result (*close)(backend_state *);

		// return the list of files contained within an archive
		result (*get_file_list)(backend_state *, std::vector<std::string> &list);

		// read a given filename from an archive. Note that the filename relates to
		// a file within the archive, not on the local filesystem. The
		// decompressed/read file is written into `buffer`, which the
		// backend resizes as needed. Ownership of the buffer stays with
		// the caller.
		result (*read)(backend_state *, const std::string filename, u8_vector &buffer);

		// write a buffer to an already opened zip archive. the compression level
		// depends on the backend. for zlib and many zlib based libraries, 0 is
		// most the default compression level, and other compression levels range
		// from 1 to 9, with 1 being the fastest (but weakest) compression and 9 the
		// slowest (but strongest).
		//
		// Ownership: the backend implementation takes ownership of the
		// buffer. Some backends (e.g. libzip via zip_source_buffer) only
		// read the data when the archive is actually closed, so the
		// buffer's storage *must* outlive the call to write(). Concretely,
		// it must remain valid until close() and release() have run. The
		// reference libzip backend handles this by stashing the moved-in
		// buffer inside backend_state::write_buffers; alternative backends
		// must provide an equivalent guarantee.
		result (*write)(backend_state *, const std::string filename, u8_vector &&buffer, bool compress, u32 compression_level);
	};

	// get an interface for the backend
	extern backend_interface& get_backend_interface();

} // zip::

} // ncr::

#endif /* _8d2a79e5218b40e3807880febfa294a0_ */
