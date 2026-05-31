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

#include <stdexcept>
#include "ncr/utility.hpp"

namespace ncr { namespace numpy {


#define WARNING_CODE_LIST(_)                                                  \
	_(none                             , 0)                                   \
	/* warnings about missing fields. Note that not all fields are required   \
	 * and it might not be a problem for an application if they are not       \
	 * present. However, it might be nice to inform the user if this is the   \
	 * case. Warnings are OR-able, because there might be multiple warnings   \
	 * active at the same time */                                             \
	_(missing_descr                    , 1ul << 0)                            \
	_(missing_fortran_order            , 1ul << 1)                            \
	_(missing_shape                    , 1ul << 2)                            \


#define ERROR_CODE_LIST(_)                                                    \
	_(none                             ,   0)                                 \
	/* error codes. in particular for nested/structured arrays, it might be   \
	 * helpful to know precisely what went wrong. */                          \
	_(wrong_filetype                   , 101)                                 \
	_(file_not_found                   , 102)                                 \
	_(file_exists                      , 103)                                 \
	_(file_open_failed                 , 104)                                 \
	_(file_truncated                   , 105)                                 \
	_(file_write_failed                , 106)                                 \
	_(file_read_failed                 , 107)                                 \
	_(file_close                       , 108)                                 \
	_(unsupported_file_format          , 109)                                 \
	_(duplicate_array_name             , 110)                                 \
	/* */                                                                     \
	_(magic_string_invalid             , 201)                                 \
	_(version_not_supported            , 202)                                 \
	_(header_invalid_length            , 203)                                 \
	_(header_truncated                 , 204)                                 \
	_(header_parsing_error             , 205)                                 \
	_(header_invalid                   , 206)                                 \
	_(header_empty                     , 207)                                 \
	/* */                                                                     \
	_(descr_invalid                    , 301)                                 \
	_(descr_invalid_type               , 302)                                 \
	_(descr_invalid_string             , 303)                                 \
	_(descr_invalid_data_size          , 304)                                 \
	_(descr_list_empty                 , 305)                                 \
	_(descr_list_invalid_type          , 306)                                 \
	_(descr_list_incomplete_value      , 307)                                 \
	_(descr_list_invalid_value         , 308)                                 \
	_(descr_list_invalid_shape         , 309)                                 \
	_(descr_list_invalid_shape_value   , 310)                                 \
	_(descr_list_subtype_not_supported , 311)                                 \
	/* */                                                                     \
	_(fortran_order_invalid_value      , 401)                                 \
	_(shape_invalid_value              , 402)                                 \
	_(shape_invalid_shape_value        , 403)                                 \
	_(item_size_mismatch               , 404)                                 \
	_(data_size_mismatch               , 405)                                 \
	_(unavailable                      , 406)                                 \
	/* */                                                                     \
	_(mmap_failed                      , 501)                                 \
	_(seek_failed                      , 502)                                 \
	_(reader_not_open                  , 503)                                 \
	_(invalid_item_offset              , 504)                                 \
	_(invalid_data_pointer             , 505)                                 \
	_(munmap_failed                    , 506)                                 \
	/* used in ndarray */                                                     \
	_(invalid_value                    , 601)                                 \
	_(index_out_of_bounds              , 602)                                 \
	_(index_shape_mismatch             , 603)                                 \
	_(size_overflow                    , 604)                                 \


#define WARNING_CODE_STRINGIFY(NAME, VALUE)  \
	{warnings::NAME, #NAME},

#define ERROR_CODE_STRINGIFY(NAME, VALUE)  \
	{errors::NAME, #NAME},


// need to bring enum_count into this namespace for the MACRO to work (TODO: fix this)
template <typename T> constexpr size_t enum_count();

/*
 * declare enums `warnings` and `errors`, and enable boolean operatores on
 * `warnings`.
 */
NCR_NODISCARD_ENUM_CLASS(warnings, u16, WARNING_CODE_LIST)
NCR_NODISCARD_ENUM_CLASS(errors,   u16, ERROR_CODE_LIST)
NCR_DEFINE_ENUM_FLAG_OPERATORS(warnings)


constexpr inline std::array<std::pair<warnings, const char*>, enum_count<warnings>()>
warning_strings = {{
	WARNING_CODE_LIST(WARNING_CODE_STRINGIFY)
}};


constexpr inline std::array<std::pair<errors, const char*>, enum_count<errors>()>
error_strings = {{
	ERROR_CODE_LIST(ERROR_CODE_STRINGIFY)
}};


struct result
{
	warnings warn = warnings::none;
	errors   err  = errors::none;

	// Default constructor (uses the default member initializers above)
	constexpr result() = default;

	// Allow direct initialization from just an error
	constexpr result(errors e) : warn(warnings::none), err(e) {}

	// Allow direct initialization from just a warning
	constexpr result(warnings w) : warn(w), err(errors::none) {}

	// Allow initialization from both
	constexpr result(warnings w, errors e) : warn(w), err(e) {}

	inline bool is_ok()       { return ncr::to_underlying(this->err) == 0; }
	inline bool has_error()   { return !is_ok(); }
	inline bool has_warning() { return ncr::to_underlying(this->warn) > 0; }

	/*
 	 * to_string - returns a string representation of a result.
 	 *
 	 * Note that a result might contain not only a single warning code, but several codes
 	 * that are set (technically by OR-ing them). As such, this function returns a
 	 * string which will contain all string representations for all warning codes,
 	 * concatenated by " | ", and if there's an error code, the error code as well.
 	 * An example might look like:
 	 *     missing_descr | missing_shape, file_truncated
 	 * which indicates two warnings and one error. A pristine result, i.e. no error
 	 * and no warning is set, the function returns with a simple "none, none" string,
 	 * while if there is one or the other set (i.e. some warning or some error),
 	 * then the good part contains 'none', e.g.
 	 *     missing_descr | missing_shape, none
 	 * or
 	 *     none, file_not_found
 	 */
	inline std::string
	to_string()
	{
		std::ostringstream oss;

		// build warning part
		if (!this->has_warning())
			oss << "none";
		else {
			bool first = true;
			for (size_t i = 0; i < warning_strings.size(); ++i) {
				const auto& [enum_val, str] = warning_strings[i];
				if (str == nullptr || enum_val == warnings::none) continue;
				if ((this->warn & enum_val) != enum_val)
					continue;
				if (!first)
					oss << " | ";
				oss << str;
				first = false;
			}
		}

		// build error part
		oss << ", ";
		if (!this->has_error())
			oss << "none";
		else {
			for (size_t i = 0; i < error_strings.size(); ++i) {
				const auto& [enum_val, str] = error_strings[i];
				if (str == nullptr || enum_val == errors::none) continue;
				if (this->err == enum_val) {
					oss << str;
					break;
				}
			}
		}

		return oss.str();
	}

};


/*
 * TODO: for now, have an operator| for result so that we can OR warning
 * flags. this isn't so great and should probably be removed in the future
 */
inline result&
operator|=(result& lhs, const result& rhs)
{
	// Accumulate warnings using their underlying type values
	lhs.warn = static_cast<warnings>(
		ncr::to_underlying(lhs.warn) | ncr::to_underlying(rhs.warn)
	);

	// Propagate the error if the incoming result has one
	if (rhs.err != errors::none)
		lhs.err = rhs.err;

	return lhs;
}


inline bool        is_error(result r)    { return r.has_error(); }
inline bool        has_warning(result r) { return r.has_warning(); }
inline std::string to_string(result res) { return res.to_string(); }


/*
* Helper to handle the "throw or set" logic used in get
*/
inline void
report_error(result code, result* out_err, const char* msg)
{
	if (out_err)
		*out_err = code;
	else
		throw std::out_of_range(msg);
}


struct ErrorContext
{
	result      res;
	const char *failed_function;
};


}} // ncr::numpy::

#endif /* _8c9e4fd8e3de4665b327b3e0a6481c9f_ */

