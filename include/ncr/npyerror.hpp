/*
 * ncr/npyerror.hpp - return codes used throughout ncr::numpy
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#ifndef _8c9e4fd8e3de4665b327b3e0a6481c9f_
#define _8c9e4fd8e3de4665b327b3e0a6481c9f_

#include "ncr/utility.hpp"

namespace ncr { namespace numpy {

#define NCR_NUMPY_ERROR_CODE_LIST(_)                                          \
	_(ok                                     , 0)                             \
	/* warnings about missing fields. Note that not all fields are required   \
	 * and it might not be a problem for an application if they are not       \
	 * present. However, inform the user about this state */                  \
	_(warning_missing_descr                  , 1ul << 0)                      \
	_(warning_missing_fortran_order          , 1ul << 1)                      \
	_(warning_missing_shape                  , 1ul << 2)                      \
	/* error codes. in particular for nested/structured arrays, it might be   \
	 * helpful to know precisely what went wrong. */                          \
	_(error_wrong_filetype                   , 1ul << 3)                      \
	_(error_file_not_found                   , 1ul << 4)                      \
	_(error_file_exists                      , 1ul << 5)                      \
	_(error_file_open_failed                 , 1ul << 6)                      \
	_(error_file_truncated                   , 1ul << 7)                      \
	_(error_file_write_failed                , 1ul << 8)                      \
	_(error_file_read_failed                 , 1ul << 9)                      \
	_(error_file_close                       , 1ul << 10)                     \
	_(error_unsupported_file_format          , 1ul << 11)                     \
	_(error_duplicate_array_name             , 1ul << 12)                     \
	/* */                                                                     \
	_(error_magic_string_invalid             , 1ul << 13)                     \
	_(error_version_not_supported            , 1ul << 14)                     \
	_(error_header_invalid_length            , 1ul << 15)                     \
	_(error_header_truncated                 , 1ul << 16)                     \
	_(error_header_parsing_error             , 1ul << 17)                     \
	_(error_header_invalid                   , 1ul << 18)                     \
	_(error_header_empty                     , 1ul << 19)                     \
	/* */                                                                     \
	_(error_descr_invalid                    , 1ul << 20)                     \
	_(error_descr_invalid_type               , 1ul << 21)                     \
	_(error_descr_invalid_string             , 1ul << 22)                     \
	_(error_descr_invalid_data_size          , 1ul << 23)                     \
	_(error_descr_list_empty                 , 1ul << 24)                     \
	_(error_descr_list_invalid_type          , 1ul << 25)                     \
	_(error_descr_list_incomplete_value      , 1ul << 26)                     \
	_(error_descr_list_invalid_value         , 1ul << 27)                     \
	_(error_descr_list_invalid_shape         , 1ul << 28)                     \
	_(error_descr_list_invalid_shape_value   , 1ul << 29)                     \
	_(error_descr_list_subtype_not_supported , 1ul << 30)                     \
	/* */                                                                     \
	_(error_fortran_order_invalid_value      , 1ul << 31)                     \
	_(error_shape_invalid_value              , 1ul << 32)                     \
	_(error_shape_invalid_shape_value        , 1ul << 33)                     \
	_(error_item_size_mismatch               , 1ul << 34)                     \
	_(error_data_size_mismatch               , 1ul << 35)                     \
	_(error_unavailable                      , 1ul << 36)                     \
    /* */                                                                     \
	_(error_mmap_failed                      , 1ul << 37)                     \
	_(error_seek_failed                      , 1ul << 38)                     \
	_(error_reader_not_open                  , 1ul << 39)                     \
	_(error_invalid_item_offset              , 1ul << 40)                     \
	_(error_invalid_data_pointer             , 1ul << 41)                     \
	_(error_munmap_failed                    , 1ul << 42)                     \

#define NCR_NUMPY_ERROR_CODE_ENUM_ENTRY(NAME, VALUE) \
	NAME = VALUE,

#define NCR_NUMPY_ERROR_CODE_STRINGIFY(NAME, VALUE) \
	{result::NAME, #NAME},

// need to bring enum_count into this namespace for the MACRO to work (TODO:
// fix this)
template <typename T> constexpr size_t enum_count();
NCR_NODISCARD_ENUM_CLASS(result, u64, NCR_NUMPY_ERROR_CODE_LIST(NCR_NUMPY_ERROR_CODE_ENUM_ENTRY))

NCR_DEFINE_ENUM_FLAG_OPERATORS(result);

// map from error code to string for pretty printing the error code. This is a
// bit more involved than just listing the strings, because result codes can be
// OR-ed together, i.e. a result code might have several codes that are set.
constexpr inline std::array<std::pair<result, const char*>, enum_count<result>()>
result_strings = {{
	NCR_NUMPY_ERROR_CODE_LIST(NCR_NUMPY_ERROR_CODE_STRINGIFY)
}};

// mask of all warning bits. New warnings should be added to bits below this
// constant; the corresponding NCR_NUMPY_ERROR_CODE_LIST entries already
// place the warnings at the lowest bits, so a single masking step
// distinguishes warnings from real errors.
inline constexpr u64 warning_mask =
	to_underlying(result::warning_missing_descr)         |
	to_underlying(result::warning_missing_fortran_order) |
	to_underlying(result::warning_missing_shape);


inline bool
is_error(result r)
{
	return (to_underlying(r) & ~warning_mask) != 0;
}


struct ErrorContext
{
	result      res;
	const char *failed_function;
};


}} // ncr::numpy::

#endif /* _8c9e4fd8e3de4665b327b3e0a6481c9f_ */

