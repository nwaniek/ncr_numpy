/*
 * ncr/utility.hpp - utility definitions and functions
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE for more details
 */
#ifndef _65fc1481d8d149029547d3932c93f2e0_
#define _65fc1481d8d149029547d3932c93f2e0_

#include <cstddef>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "ncr/types.hpp"

/*
 * NCR_DEFINE_ENUM_FLAG_OPERATORS - define all binary operators used for flags
 *
 * This macro expands into functions for bit-wise and binary operations on
 * enums, e.g. given two enum values a and b, one might want to write `a |= b;`.
 * With the macro below, this will be possible.
 */
#define NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_T) \
	inline ENUM_T operator~(ENUM_T a)              { return static_cast<ENUM_T>(~ncr::to_underlying(a)); } \
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


/*
 * defaults for DECL_ENTRY and COUNT_ENTRY used in non-EX variants to declare
 * enums.
 */
#define NCR_ENUM_DEFAULT_DECL_ENTRY(NAME, VALUE)  \
	NAME = VALUE,

#define NCR_ENUM_DEFAULT_COUNT_ENTRY(NAME, VALUE) \
	+1

/*
 * macro to define an enum class of a specific underlying type, and also
 * generating a template specialization that returns the number of values in the
 * enum class.
 */
template <typename T> constexpr size_t enum_count();

#define NCR_ENUM_CLASS(EnumName, UnderlyingType, LIST_MACRO) \
	NCR_ENUM_CLASS_EX(EnumName, UnderlyingType, LIST_MACRO, \
		NCR_ENUM_DEFAULT_DECL_ENTRY, NCR_ENUM_DEFAULT_COUNT_ENTRY)

#define NCR_ENUM_CLASS_EX(EnumName, UnderlyingType, LIST_MACRO, DECL_ENTRY, COUNT_ENTRY) \
	enum class [[nodiscard]] EnumName : UnderlyingType { \
		LIST_MACRO(DECL_ENTRY) \
	}; \
	template<> constexpr size_t enum_count<EnumName>() { return 0 LIST_MACRO(COUNT_ENTRY); }


/*
 * like NCR_ENUM_CLASS, but tags the enum with [[nodiscard]] so the compiler
 * warns when a return value is dropped. Use for error/result enums.
 */
#define NCR_NODISCARD_ENUM_CLASS(EnumName, UnderlyingType, LIST_MACRO) \
	NCR_NODISCARD_ENUM_CLASS_EX(EnumName, UnderlyingType, LIST_MACRO, \
		NCR_ENUM_DEFAULT_DECL_ENTRY, NCR_ENUM_DEFAULT_COUNT_ENTRY)

#define NCR_NODISCARD_ENUM_CLASS_EX(EnumName, UnderlyingType, LIST_MACRO, DECL_ENTRY, COUNT_ENTRY) \
	enum class [[nodiscard]] EnumName : UnderlyingType { \
		LIST_MACRO(DECL_ENTRY) \
	}; \
	template<> constexpr size_t enum_count<EnumName>() { return 0 LIST_MACRO(COUNT_ENTRY); }


/*
 * A simple define to reduce the verbosity to declare a tuple. This is
 * particularly useful, for instance, in calls to random.hpp:random_coord.
 * In this example, the template accepts a variadic number of tuples, e.g.
 *
 *     auto xlim = std::tuple{0, 1};
 *     auto ylim = std::tuple{0, 10};
 *     auto coord = random_coord(rng, xlim, ylim);
 *
 * It would be better to avoid the temporary variables. However,
 * brace-initializers wont work, as there is no clear (read: acceptably sane)
 * way to turn an initializer_list into a tuple. With the following macro, it is
 * actually possible to succinctly write
 *
 *     auto coord = random_coord(_T(0, 1), _T(0, 10));
 *
 * without any local declaration of temporaries, or overly long calls that
 * include the specific tuple type.
 */
#ifndef _tup
	#define _tup(...) std::tuple{__VA_ARGS__}
#endif



namespace ncr {

// safe comparison functions --------------------------------------------------

/*
 * the following avoid having to pull in std's <utility>.
 */
template<class T, class U>
constexpr bool
cmp_less(T t, U u) noexcept
{
	if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
		return t < u;
	else if constexpr (std::is_signed_v<T>)
		return t < 0 || std::make_unsigned_t<T>(t) < u;
	else
		return u >= 0 && t < std::make_unsigned_t<U>(u);
}


template <typename T, typename U>
constexpr bool
cmp_greater(T t, U u) noexcept
{
	return cmp_less(u, t);
}


template <typename T, typename U>
constexpr bool
cmp_greater_equal(T t, U u) noexcept
{
	return !cmp_less(t, u);
}


// overflow related functions -------------------------------------------------

/*
 * mul_overflow - multiply two numbers, returns true if an overflow would happen
 */
template <typename T>
constexpr bool
mul_overflow(T a, T b, T& result)
{
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_mul_overflow(a, b, &result);
#else
	// Fallback using std::numeric_limits
	if (a > 0) {
		if (b > 0)
			if (a > (std::numeric_limits<T>::max() / b)) return true;
		else
			if (b < (std::numeric_limits<T>::min() / a)) return true;
	}
	else {
		if (b > 0)
			if (a < (std::numeric_limits<T>::min() / b)) return true;
		else
			if (a != 0 && b < (std::numeric_limits<T>::max() / a)) return true;
	}
	result = a * b;
	return false;
#endif
}


template <typename T>
constexpr bool
add_overflow(T a, T b, T& result) {
#if defined(__GNUC__) || defined(__clang__)
	// Works for both signed and unsigned
	return __builtin_add_overflow(a, b, &result);
#else
	if constexpr (std::is_unsigned_v<T>) {
		// Unsigned check: if the sum is smaller than either operand, it wrapped.
		result = a + b;
		return result < a;
	}
	else {
		// Signed check: Overflow only happens if signs are the same
		if ((b > 0 && a > (std::numeric_limits<T>::max() - b)) ||
			(b < 0 && a < (std::numeric_limits<T>::min() - b))) {
			return true;
		}
		result = a + b;
		return false;
	}
#endif
}


/*
 * ensure at compile time that one or more types are PODs
 */
template <typename T>
constexpr void ensure_pod1() {
	static_assert(std::is_trivial_v<T>,         "Type is not trivial!");
	static_assert(std::is_standard_layout_v<T>, "Type does not have a standard layout!");
}
template <typename... Types>
constexpr void ensure_pod() { (ensure_pod1<Types>(), ...); }


/*
 * compile time count of elements in an array. If standard library is used,
 * could also use std::size instead.
 */
template <std::size_t N, class T>
constexpr std::size_t len(T(&)[N]) { return N; }


/*
 * to_underlying - Get the underlying type of some type
 *
 * This is an implementation of C++23's to_underlying function, which is not yet
 * available in C++20 but handy for casting enum-structs to their underlying
 * type (see NCR_DEFINE_ENUM_FLAG_OPERATORS for an example).
 */
template <typename E>
constexpr typename std::underlying_type<E>::type
to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}


/*
 * get_index_of - get the index of a pointer to T in a vector of T
 *
 * This function returns an optional to indicate if the pointer to T was found
 * or not.
 */
template <typename T>
inline bool
get_index_of(std::vector<T*> vec, T *needle, size_t &out_index)
{
	for (size_t i = 0; i < vec.size(); i++)
		if (vec[i] == needle) {
			out_index = i;
			return true;
		}
	return false;
}


/*
 * determine if a container contains a certain element or not
 */
template <typename ContainerT, typename U>
inline bool
contains(const ContainerT &container, const U &needle)
{
	auto it = std::find(container.begin(), container.end(), needle);
	return it != container.end();
}


/*
 * hexdump - generate a hexdump for a buffer similar to hex editors
 */
inline void
hexdump(std::ostream& os, const std::vector<uint8_t> &data)
{
	std::ios old_state(nullptr);
	old_state.copyfmt(os);

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
	// reset to default
	os << std::setfill(os.widen(' '));
	os.copyfmt(old_state);
}


} // namespace ncr

#endif /* _65fc1481d8d149029547d3932c93f2e0_ */

