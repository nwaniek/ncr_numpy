/*
 * ncr/npybuffers.hpp - some buffer backends for npyfile and ndarray
 *
 * SPDX-FileCopyrightText: 2024-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */
#ifndef _69a274a94acf465aaa21a9e5046fa6ed_
#define _69a274a94acf465aaa21a9e5046fa6ed_

#include <cstddef>
#include <cstdint>
#include <vector>

// mmap-based source is opt-out via NCR_NUMPY_DISABLE_MMAP. The default is to
// have it enabled on POSIX-like systems (linux, *BSD, macOS). Disabling skips
// the POSIX headers entirely, which is useful for freestanding/Windows builds
// where these headers don't exist or aren't desired.
//@ncr-fusor-keep-includes-start
#ifndef NCR_NUMPY_DISABLE_MMAP
#define NCR_NUMPY_HAS_MMAP
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif
//@ncr-fusor-keep-includes-end

#include "ncr/npyerror.hpp"


namespace ncr { namespace numpy {

#ifdef NCR_NUMPY_HAS_MMAP

/*
 * mmap'ed file based buffer
 *
 * TODO: also store the offset (in bytes) for the actual data
 */
struct mmap_buffer
{
	uint8_t* data        = nullptr;
	size_t   size        = 0;
	size_t   position    = 0;
	size_t   data_offset = 0;
};


inline result
open(const char *filepath, mmap_buffer* buf)
{
	if (!buf)
		return {errors::invalid_data_pointer};

	int fd = ::open(filepath, O_RDONLY);
	if (fd == -1) {
		return {errors::file_open_failed};
	}

	buf->size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	// MAP_PRIVATE + PROT_READ|PROT_WRITE: pages are shared-clean until the
	// process writes, at which point the kernel COWs to a private page.
	// This matches numpy.load semantics (the user gets a writable copy)
	// while still being zero-copy for read-only workflows. Disk file is
	// never touched.
	buf->data = (uint8_t*)mmap(NULL, buf->size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	::close(fd);
	if (buf->data == MAP_FAILED) {
		buf->size = 0;
		buf->data = nullptr;
		return {errors::mmap_failed};
	}
	buf->position = 0;
	return {};
}


inline result
close(mmap_buffer* buf)
{
	if (!buf)
		return {errors::invalid_data_pointer};

	if (munmap(buf->data, buf->size) == -1)
		return {errors::munmap_failed};

	buf->size = 0;
	buf->data = nullptr;
	buf->position = 0;
	return {};
}


inline result
release(mmap_buffer* buf)
{
	if (!buf)
		return {errors::invalid_data_pointer};

	result res = close(buf);
	if (is_error(res))
		return res;

	delete buf;
	return {};
}

#endif // NCR_NUMPY_HAS_MMAP


/*
 * raw array based buffer
 */
struct raw_buffer
{
	uint8_t* data        = nullptr;
	size_t   size        = 0;
	size_t   data_offset = 0;
};


inline raw_buffer*
make_raw_buffer(size_t N)
{
	auto *buffer = new raw_buffer();
	buffer->data = new uint8_t[N]{};
	buffer->size = N;
	return buffer;
}


inline result
release(raw_buffer* buf)
{
	if (!buf)
		return {errors::invalid_data_pointer};

	delete[] buf->data;
	delete buf;
	return {};
}


/*
 * vector based buffer
 */
struct vector_buffer
{
	std::vector<uint8_t> data;
	size_t               data_offset = 0;
};


inline vector_buffer*
make_vector_buffer(size_t N)
{
	auto* buf = new vector_buffer();
	buf->data.resize(N);
	return buf;
}

inline vector_buffer*
make_vector_buffer(u8_vector&& other)
{
	auto* buf = new vector_buffer();
	buf->data = std::move(other);
	return buf;
}



inline result
release(vector_buffer* buf)
{
	if (!buf)
		return {errors::invalid_data_pointer};

	delete buf;
	return {};
}



/*
 * npybuffer - simple frontend for different buffers
 *
 * In case of a regular file I/O or no file I/O at all, an ndarray will simply
 * store its data within a vector. however, as soon as a file is opened in mmap
 * mode, we want the actual data to be accessed via the memory mapped file. to
 * make this flexibly possible within ndarray, internally it only sets the data
 * pointer corresponding to what the data source actually is (i.e. a vector,
 * mmap, etc.). all of this could be implanted directly in ndarray, but there's
 * almost zero cost to separate npybuffer from ndarray and keep the interface
 * small and tidy.
 *
 * Why not std::variant? because the types are *very* simple, and there's no
 * need for type checking beyond testing the enum flag in any part of the code.
 * Also, this provides a few custom functions
 *
 * TODO: maybe rename
 * TODO: maybe provide constructors to move stuff in
 */
struct npybuffer
{
	enum class type : uint8_t {
		raw,
		vector,
#ifdef NCR_NUMPY_HAS_MMAP
		mmap
#endif
	};

	// tagged union
	type type;
	union {
		raw_buffer*    raw;
		vector_buffer* vector;
		mmap_buffer*   mmap;
	};

	npybuffer(enum type _t) : type(_t) {}

	// pointer to the actual array data (with the data_offset applied, so this
	// skips any header bytes that the backend buffer might still hold)
	inline u8*
	get_data_ptr()
	{
		switch (type) {
		case type::raw:
			return raw->data + raw->data_offset;
		case type::vector:
			return vector->data.data() + vector->data_offset;
#ifdef NCR_NUMPY_HAS_MMAP
		case type::mmap:
			return mmap->data + mmap->data_offset;
#endif
		}
		return nullptr;
	}

	// pointer to the start of the underlying buffer, ignoring data_offset.
	// Useful for backends that need access to the raw, unsliced storage
	inline u8*
	get_raw_data_ptr()
	{
		switch (type) {
		case type::raw:
			return raw->data;
		case type::vector:
			return vector->data.data();
#ifdef NCR_NUMPY_HAS_MMAP
		case type::mmap:
			return mmap->data;
#endif
		}
		return nullptr;
	}

	// number of bytes available starting from get_data_ptr() (i.e. the size
	// of the array payload, not the size of the underlying buffer)
	size_t
	get_data_size()
	{
		switch (type) {
		case type::raw:
			return raw->size - raw->data_offset;
		case type::vector:
			return vector->data.size() - vector->data_offset;
#ifdef NCR_NUMPY_HAS_MMAP
		case type::mmap:
			return mmap->size - mmap->data_offset;
#endif
		}
		return 0;
	}

	// number of bytes in total of this buffer, including header information
	size_t
	get_size()
	{
		switch (type) {
		case type::raw:
			return raw->size;
		case type::vector:
			return vector->data.size();
#ifdef NCR_NUMPY_HAS_MMAP
		case type::mmap:
			return mmap->size;
#endif
		}
		return 0;
	}

	// Returns the result of the underlying release() call. release() can
	// fail (e.g. munmap returning EINVAL on a corrupted state); callers
	// that don't care can drop the return value with `(void)buf.release()`,
	// but the value is exposed so failure is observable.
	result
	release() {
		switch (type) {
		case type::raw:
			return ncr::numpy::release(raw);
		case type::vector:
			return ncr::numpy::release(vector);
#ifdef NCR_NUMPY_HAS_MMAP
		case type::mmap:
			return ncr::numpy::release(mmap);
#endif
		}
		// TODO: maybe return something else?
		return {};
	}
};



}} // ncr::numpy

#endif /* _69a274a94acf465aaa21a9e5046fa6ed_ */

