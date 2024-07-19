/*
 * common.hpp - ncr comon definitinos, functions, and operators
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <type_traits>

namespace ncr {


/*
 * to_underlying - convert a type to its underlying type
 *
 * This will be available in C++23 as std::to_underlying. Until then, use the
 * implementation here.
 */
template <typename E>
constexpr typename std::underlying_type<E>::type
to_underlying(E e) noexcept {
	return static_cast<typename std::underlying_type<E>::type>(e);
}


/*
 * NCR_DEFINE_ENUM_FLAG_OPERATORS - define all binary operators used for flags
 *
 * This macro expands into functions for bit-wise and binary operations on
 * enums, e.g. given two enum values a and b, one might want to write `a |= b;`.
 * With the macro below, this will be possible.
 */
#define NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_T) \
	inline ENUM_T operator~(ENUM_T a) { return static_cast<ENUM_T>(~ncr::to_underlying(a)); } \
	inline ENUM_T operator|(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T operator&(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T operator^(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); } \
	inline ENUM_T& operator|=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T& operator&=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T& operator^=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); }


/*
 * NCR_DEFINE_FUNCTION_ALIAS - define a function alias for another function
 *
 * Using perfect forwarding, this creates a function with a novel name that
 * forwards all the arguments to the original function
 *
 * Note: In case there are multiple overloaded functions, this macro can be used
 *       _after_ the last overloaded function itself.
 */
#define NCR_DEFINE_FUNCTION_ALIAS(ALIAS_NAME, ORIGINAL_NAME)           \
	template <typename... Args>                                        \
	inline auto ALIAS_NAME(Args &&... args)                            \
		noexcept(noexcept(ORIGINAL_NAME(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME(std::forward<Args>(args)...))        \
	{                                                                  \
		return ORIGINAL_NAME(std::forward<Args>(args)...);             \
	}


/*
 * NCR_DEFINE_FUNCTION_ALIAS_EXT - similar as above, but with additional
 * template arguments that are not captured in the case above.
 *
 * For instance, if one implements a function that gets specialized on its
 * return type, then the this could be used.
 *
 * Example:
 *
 *     // some template which has a template arguemnt for the return type
 *     template <typename T, typename U> T ncr_some_fun(int x);
 *
 *     // specialization
 *     template <typename U>
 *     float ncr_some_fun(int x)
 *     {
 *     	return (float)x;
 *     }
 *
 *     NCR_DEFINE_SHORT_NAME_EXT(some_fun, ncr_some_fun)
 */
#define NCR_DEFINE_FUNCTION_ALIAS_EXT(ALIAS_NAME, ORIGINAL_NAME)                 \
	template <typename... Args2, typename... Args>                               \
	inline auto ALIAS_NAME(Args &&... args)                                      \
		noexcept(noexcept(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))        \
	{                                                                            \
		return ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...);             \
	}

#define NCR_DEFINE_TYPE_ALIAS(ALIAS_NAME, ORIGINAL_NAME) \
	using ALIAS_NAME = ORIGINAL_NAME


/*
 * NCR_DEFINE_SHORT_NAME - define a short name for a longer one
 *
 * This allows to easily define short function names, e.g. without the ncr_
 * prefix, for a given function. Not the the alias definition will only take
 * place if NCR_ENABLE_SHORT_NAMES is defined.
 */
#ifdef NCR_ENABLE_SHORT_NAMES
	#define NCR_DEFINE_SHORT_FN_NAME(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS(SHORT_NAME, LONG_NAME)

	#define NCR_DEFINE_SHORT_FN_NAME_EXT(SHORT_FN_NAME, LONG_FN_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS_EXT(SHORT_FN_NAME, LONG_FN_NAME)

	#define NCR_DEFINE_SHORT_TYPE_ALIAS(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_TYPE_ALIAS(SHORT_NAME, LONG_NAME)
#else
	#define NCR_DEFINE_SHORT_FN_NAME(_0, _1)
	#define NCR_DEFINE_SHORT_FN_NAME_EXT(_0, _1)
	#define NCR_DEFINE_SHORT_TYPE_ALIAS(_0, _1)
#endif


/*
 * compile time count of elements in an array. If standard library is used,
 * could also use std::size instead.
 */
template <std::size_t N, class T>
constexpr std::size_t len(T(&)[N]) { return N; }


/*
 * Count the number of arguments to a variadic macro. Up to 64 arguments are
 * supported
 */
#define NCR_COUNT_ARGS2(X,_64,_63,_62,_61,_60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41,_40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21,_20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) N
#define NCR_COUNT_ARGS(...) NCR_COUNT_ARGS2(0, __VA_ARGS__ ,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)


/*
 * Suppress warnings for unused arguments. Up to 10 arguments are supported in
 * the variadic version NCR_UNUSED
 */
#define NCR_UNUSED_1(X)        (void)X;
#define NCR_UNUSED_2(X0, X1)   NCR_UNUSED_1(X0); NCR_UNUSED_1(X1)
#define NCR_UNUSED_3(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_2(__VA_ARGS__)
#define NCR_UNUSED_4(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_3(__VA_ARGS__)
#define NCR_UNUSED_5(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_4(__VA_ARGS__)
#define NCR_UNUSED_6(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_5(__VA_ARGS__)
#define NCR_UNUSED_7(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_6(__VA_ARGS__)
#define NCR_UNUSED_8(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_7(__VA_ARGS__)
#define NCR_UNUSED_9(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_8(__VA_ARGS__)
#define NCR_UNUSED_10(X0, ...) NCR_UNUSED_1(X0); NCR_UNUSED_9(__VA_ARGS__)

#define NCR_UNUSED_INDIRECT3(N, ...)  NCR_UNUSED_ ## N(__VA_ARGS__)
#define NCR_UNUSED_INDIRECT2(N, ...)  NCR_UNUSED_INDIRECT3(N, __VA_ARGS__)
#define NCR_UNUSED(...)               NCR_UNUSED_INDIRECT2(NCR_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)

} // ncr::
