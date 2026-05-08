/*
 * ncr/bswapdefs.hpp - definitions for bswap16, bswap32, and bswap64
 *
 * TODO: Note that this is particularly ugly, and a better way might be to
 * define the functions for each system / compiler in a particular header and
 * let the build system or user decide which to pull in. For the time being,
 * have everything in here.
 *
 * XXX: extend to other systems, see e.g. https://github.com/google/cityhash/blob/8af9b8c2b889d80c22d6bc26ba0df1afb79a30db/src/city.cc#L50
 *
 * @ncr-fusor-copy-file-as-is
 */
#ifndef _ba6cf59bec5b4f2b92694c85e64f44cb_
#define _ba6cf59bec5b4f2b92694c85e64f44cb_


// figure out if there are builtins or system functions available for byte
// swapping, and if yes, take the compiler built in bswaps
#if __has_builtin(__builtin_bswap16)
	#define ncr_bswap_16(x) __builtin_bswap16(x)
	#define NCR_HAS_BSWAP16
#endif
#if __has_builtin(__builtin_bswap32)
	#define ncr_bswap_32(x) __builtin_bswap32(x)
	#define NCR_HAS_BSWAP32
#endif
#if __has_builtin(__builtin_bswap64)
	#define ncr_bswap_64(x) __builtin_bswap64(x)
	#define NCR_HAS_BSWAP64
#endif

// only pull in headers when really needed, and only those appropriate for the
// system this will be compiled on
#if defined(_MSC_VER) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <stdlib.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   _byteswap_short(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   _byteswap_long(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   _byteswap_uint64(x)
		#define NCR_HAS_BSWAP64
	#endif
#elif defined(__APPLE__) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <libkern/OSByteOrder.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   OSSwapInt16(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   OSSwapInt32(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   OSSwapInt64(x)
		#define NCR_HAS_BSWAP64
	#endif
#elif defined(__linux__) && (!defined(NCR_HAS_BSWAP16) || !defined(NCR_HAS_BSWAP32) || !defined(NCR_HAS_BSWAP64))
	#include <byteswap.h>
	#if !defined(NCR_HAS_BSWAP16)
		#define ncr_bswap_16(x)   bswap_16(x)
		#define NCR_HAS_BSWAP16
	#endif
	#if !defined(NCR_HAS_BSWAP32)
		#define ncr_bswap_32(x)   bswap_32(x)
		#define NCR_HAS_BSWAP32
	#endif
	#if !defined(NCR_HAS_BSWAP64)
		#define ncr_bswap_64(x)   bswap_64(x)
		#define NCR_HAS_BSWAP64
	#endif
#endif

#endif /* _ba6cf59bec5b4f2b92694c85e64f44cb_ */

