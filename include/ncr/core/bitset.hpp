/*
 * ncr_bitset - compile time and dynmic bitset implementations
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 */
#pragma once

#include <iterator>
#include <vector>
#include <climits>
#include <string>
#include <utility>
#include <ostream>
#include <locale>
#include <stdexcept>
#include <concepts>
#include <bit> // popcount

namespace ncr {

/*
 * struct bitset,dynamic_bitset - a simple bitset for arbitrary length bits
 *
 * This struct implements a basic bitset similar to std::bitset. However, the
 * goal of this bitset is to enable setting specific flags also on ranges of
 * bits (i.e. using a mask to set certain bits), and to export the bitset to
 * multiple integers. For instance, if there are 1024 bits in the bitset, this
 * could be written as 32 32bit integers (instead of 1024 characters, i.e. <= 128
 * byte instead of 1024)
 *
 * The struct dynamic_bitset implements variable length bitsets, whcih can be resized
 * during runtime. Note that resizing will erase previously stored data.
 *
 * Naturally, the implementation is currently lacking behind what std::bitset
 * provides. In particular, the structs below only implement what is needed in
 * other parts, while additional functions to make it 'complete' will be added
 * with time and contributions from others.
 *
 * Note: the TODO's are for both bitset and dynamic_bitset
 *
 * TODO: whole bit masks similar to the functions in ncr_utils.hpp, i.e. entire
 *       subsets of bits following some mask or patern
 *          [       ....       ]    <- mask / selection (offset + length)
 *          [       0110       ]    <- the pattern to set
 *
 * TODO: operator[] access to a "wrapped" item (i.e. a boolean, which reads and
 *       writes directly into the bitset)
 *
 * TODO: left- and right-shift operators
 *
 * TODO: documentation
 *
 * TODO: bitset, check which functions can be expressed as constexpr
 */


/*
 * base template for all functions that a compile-time and dynamic bitset share
 * in common using a Curiously Recurring Template Pattern (CRTP)
 */
template <typename StorageType, typename Derived>
	requires std::unsigned_integral<StorageType>
struct _bitset_base
{
	// define the word-type that'll be used
	typedef StorageType
		word_t;

	// computes the number of bits per word given the storage type
	constexpr static size_t
		_bits_per_word  = sizeof(word_t) * CHAR_BIT;

	// copy data from a regular string
	template <typename CharT, typename Traits, typename Alloc>
	void
	from_string(const std::basic_string<CharT, Traits, Alloc> &s,
			CharT zero, CharT one)
	{
		auto _this = static_cast<Derived*>(this);

		if (s.length() != _this->_Nbits)
			throw std::length_error("Length mismatch between ncr::_bitset_base and std::basic_string s (in ncr::bitset::from_string).");

		for (size_t i = 0; i < s.length(); ++i) {
			const CharT c = s[i];
			if (Traits::eq(c, zero)) {
				reset(_this->_Nbits - i - 1);
			}
			else if (Traits::eq(c, one)) {
				set(_this->_Nbits - i - 1);
			}
			else {
				// TODO: raise exception
			}
		}
	}

	// copy directly from an array. Note that this requires the array to be of
	// identical size as the data stored within this bitset
	void
	_copy_from_array(word_t *x)
	{
		auto _this = static_cast<Derived*>(this);
		for (size_t i = 0; i < _this->_word_count; ++i)
			_this->_bits[i] = x[i];
	}

	// Set the bitset from raw data.
	//
	// Note that this will automatically also set the size of the bitset so that
	// it fits to the data. This might be different from the actually used data,
	// e.g. in case only 12 bits are used for the data, but 2 8bit numbers are
	// passed in, then the bitset will assume a data size of 16bit.
	//
	// Also note that the 0th entry in the data vector will be the 0th entry in
	// the internally stored array.
	void
	from_vector(std::vector<word_t> data)
	{
		auto _this = static_cast<Derived*>(this);

		// if the sizes match, overwrite, if not clear first
		if (data.size() != _this->_word_count)
			throw std::length_error("Length mismatch between ncr::dynamic_bitset and std::vector data (in ncr::bitset::from_vector).");

		size_t i = 0;
		for (auto v: data)
			_this->_bits[i++] = v;
	}

	// return the size of the bitset
	constexpr size_t
	size() const
	{
		return static_cast<const Derived*>(this)->_Nbits;
	}

	// return the (signed) size of the bitset
	constexpr ptrdiff_t
	ssize() const
	{
		return std::ssize(this->size());
	}

	// return the size of the bitset in number of bits
	constexpr size_t
	length() const
	{
		return static_cast<const Derived*>(this)->_Nbits;
	}

	// return the number of bits per storage element of the underlying memory
	constexpr size_t
	bits_per_word() const
	{
		return _bits_per_word;
	}

	// return the number of elements in the underlying storage memory
	constexpr size_t
	word_count() const
	{
		return static_cast<const Derived*>(this)->_word_count;
	}

	// get the array location of the i-th bit
	constexpr size_t
	get_pos(size_t i) const
	{
		return  i / _bits_per_word;
	}

	// get the bitmask for the i-th bit (relative to its storage location in
	// _bits)
	constexpr word_t
	mask_bit(size_t i) const
	{
		return (static_cast<word_t>(1)) << i % _bits_per_word;
	}

	// return reference to the word that stores the i-th bit
	constexpr word_t&
	_get_word(size_t i)
	{
		return static_cast<Derived*>(this)->_bits[get_pos(i)];
	}

	// get copy of the word at position i
	word_t
	_get_word(size_t i) const
	{
		return static_cast<const Derived*>(this)->_bits[get_pos(i)];
	}

	// set the i-th bit to 1 (or 0 if argument val is false)
	void
	set(size_t i, bool val = true)
	{
		auto _this = static_cast<Derived*>(this);

		// TODO: move this into a check function that can be used at other
		//       locations
		if (get_pos(i) >= _this->_word_count)
			throw std::out_of_range("Bit-Index out of range in ncr::bitset::set.\n");

		if (val)
			_get_word(i) |= mask_bit(i);
		else
			_get_word(i) &= ~mask_bit(i);
	}

	// set the entire bitset to 1
	void
	set()
	{
		auto _this = static_cast<Derived*>(this);
		for(size_t i = 0; i < _this->_word_count; ++i) {
			// TODO: this only works if word_t is an unsigned data type
			// TODO: this also sets excess (i.e. padding) bits. Is this intended
			//       behavior?
			_this->_bits[i] = static_cast<word_t>(-1);
		}
	}

	// reset the i-th bit to 0
	void
	reset(size_t i)
	{
		_get_word(i) &= ~mask_bit(i);
	}

	// reset the entire bitset to 0
	void
	reset()
	{
		// TODO: this also modifies excess (i.e. padding) bits. Is this intended
		//       behavior?
		auto _this = static_cast<Derived*>(this);
		for(size_t i = 0; i < _this->_word_count; ++i) {
			_this->_bits[i] = static_cast<word_t>(0);
		}
	}

	// toggle - toggle the truth value of the i-th bit
	void
	toggle(size_t i)
	{
		_get_word(i) ^= mask_bit(i);
	}

	// flip - alias for toggle
	void
	flip(size_t i)
	{
		toggle(i);
	}

	// test if the i-th bit is set
	bool
	test(size_t i) const
	{
		return (_get_word(i) & mask_bit(i)) != static_cast<word_t>(0);
	}

	// copy the bitset to a basic string
	template <typename CharT, typename Traits, typename Alloc>
	void
	_copy_to_string(std::basic_string<CharT, Traits, Alloc> &s, CharT zero, CharT one) const
	{
		auto _this = static_cast<const Derived*>(this);
		s.assign(_this->_Nbits, zero);
		for (size_t i = _this->_Nbits; i > 0; --i)
			if (test(i - 1))
				Traits::assign(s[_this->_Nbits - i], one);
	}

	// convert the bitset to a basic_string
	template <typename CharT, typename Traits, typename Alloc>
	std::basic_string<CharT, Traits, Alloc>
	to_string(CharT zero = CharT('0'), CharT one = CharT('1')) const
	{
		std::basic_string<CharT, Traits, Alloc> result;
		_copy_to_string(result, zero, one);
		return result;
	}

	// convert the bitset to a char-string
	std::basic_string<char, std::char_traits<char>, std::allocator<char>>
	to_string(char zero = '0', char one = '1') const
	{
		return to_string<char, std::char_traits<char>, std::allocator<char>>(zero, one);
	}

	// raw data copy to vector
	std::vector<word_t>
	to_vector() const
	{
		auto _this = static_cast<const Derived*>(this);
		std::vector<word_t> result(_this->_word_count);
		for (size_t i = 0; i < _this->_word_count; ++i)
			result[i] = _this->_bits[i];
		return result;
	}

	// turn the bitset into a bool vector
	std::vector<bool>
	to_bool_vector() const
	{
		auto _this = static_cast<const Derived*>(this);
		std::vector<bool> result(_this->_Nbits);
		for (size_t i = 0; i < _this->_Nbits; ++i)
			result[i] = test(i);
		return result;
	}

	// compute the number of bits in a word indexed by p (note this is different
	// from the word indexed by bit a i in the functions above)
	inline size_t
	_bits_in_word(size_t p) const
	{
		// don't use _get_word here, because _get_word
		// assumes a bit-index, not a word index!
		auto _this = static_cast<const Derived*>(this);
		word_t w = _this->_bits[p];
		// need to cast popcount to size_t. For whatever reason, popcount is
		// defined to return an int instead of unsigned
		return static_cast<size_t>(std::popcount(w));
	}

	// count the number of bits in the bitset
	size_t
	count() const
	{
		size_t result = 0;
		auto _this = static_cast<const Derived*>(this);
		for (size_t i = 0; i < _this->_word_count; i++)
			result += _bits_in_word(i);
		return result;
	}

	bool all() const  { return count() == static_cast<const Derived*>(this)->_Nbits; }

	bool any() const  { return count() > 0; }

	bool none() const { return count() == 0; }

};


/*
 * struct bitset - compile time fixed-size bitset implementation
 */
template <size_t Nbits, typename StorageType = size_t>
struct bitset : _bitset_base<StorageType, bitset<Nbits, StorageType>>
{
	// define a shorthand for the base type
	typedef _bitset_base<StorageType, bitset<Nbits, StorageType>>
		base_type;

	// define the word-type that'll be used
	typedef StorageType
		word_t;

	constexpr static size_t _Nbits = Nbits;

	// computes the number of words required to store _Nbits many bits
	constexpr static size_t
		_word_count = _Nbits / base_type::_bits_per_word + (_Nbits % base_type::_bits_per_word == 0 ? 0 : 1);

	// the raw (consecutive) memory for all bits in the bitset
	word_t
		_bits[_word_count] = {0};

	// constructors
	bitset() {}

	// copy constructor
	bitset(const bitset &o) = default;

	// constructor from initializer list
	bitset(std::initializer_list<bool> l)
	{
		if (l.size() != _Nbits)
			throw std::length_error("Length mismatch between ncr::bitset and std::initializer_list.\n");

		size_t i = l.size() - 1;
		for (auto v : l) {
			if (v)
				this->set(i);
			--i;
		}
	}

	// initialize from a string
	template <typename CharT, typename Traits, typename Alloc>
	bitset(const std::basic_string<CharT, Traits, Alloc> &s, CharT zero, CharT one)
	{
		this->template from_string<CharT, Traits, Alloc>(s, zero, one);
	}

	// initialize from a string
	bitset(const std::basic_string<char, std::char_traits<char>, std::allocator<char>> &s,
			char zero = '0', char one = '1')
	{
		this->template from_string<char, std::char_traits<char>, std::allocator<char>>(s, zero, one);
	}

	// bitwise operators: XOR
	bitset<_Nbits, StorageType>&
	operator^=(const bitset<_Nbits, StorageType> &b)
	{
		for (size_t i = 0; i < _word_count; i++)
			_bits[i] ^= b._bits[i];
		return *this;
	}

	// bitwise operators: OR
	bitset<_Nbits, StorageType>&
	operator|=(const bitset<_Nbits, StorageType> &b)
	{
		for (size_t i = 0; i < _word_count; i++)
			_bits[i] |= b._bits[i];
		return *this;
	}

	// bitwise operators: AND
	bitset<_Nbits, StorageType>&
	operator&=(const bitset<_Nbits, StorageType> &b)
	{
		for (size_t i = 0; i < _word_count; i++)
			_bits[i] &= b._bits[i];
		return *this;
	}

	// assignment operator
	// The compile-time fixed size bitset does not need to check, as the size is
	// checked by the compiler
	bitset<_Nbits, StorageType>&
	operator=(const bitset<_Nbits, StorageType> &b)
	{
		for (size_t i = 0; i < _word_count; i++)
			_bits[i] = b._bits[i];
		return *this;
	}

	// const forward iterator
	struct const_iterator
	{
		using iterator_category = std::bidirectional_iterator_tag;
		using difference_type   = std::ptrdiff_t;

		using value_type       = bool;
		using pointer          = value_type *;
		using reference        = value_type &;
		using const_pointer    = const value_type *;
		using const_reference  = const value_type &;

		// constructors
		// const_iterator(const const_iterator &o) : _bitset(o._bitset), _i(o._i) {}
		const_iterator(const bitset<_Nbits, StorageType> *bitset, size_t i) : _bitset(bitset), _i(i) {}

		// dereference operator
		value_type operator* () { return _bitset->test(this->_i) == true; }

		// pointer operator->() { return nullptr; }

		// prefix increment, decrement
		const_iterator& operator++ ()    { _i++; return *this; }
		const_iterator& operator-- ()    { _i--; return *this; }

		// postfix increment, decrement
		const_iterator  operator++ (int) { const_iterator tmp(*this); ++*this; return tmp; }
		const_iterator  operator-- (int) { const_iterator tmp(*this); --*this; return tmp; }

		// comparison operators
		friend bool operator==(const const_iterator &a, const const_iterator &b)
		{
			return (a._bitset == b._bitset) && (a._i == b._i);
		}

	private:
		const bitset<_Nbits, StorageType> *_bitset;
		size_t _i;
	};

	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	const_iterator begin() const { return const_iterator(this, 0); }
	const_iterator end()   const { return const_iterator(this, _Nbits); }

	const_reverse_iterator rbegin() const { return std::reverse_iterator<const_iterator>(end()); }
	const_reverse_iterator rend()   const { return std::reverse_iterator<const_iterator>(begin()); }
};


// global bitwise operator& on compile-time bitsets
template <size_t Nbits, typename StorageType>
inline bitset<Nbits, StorageType>
operator&(const bitset<Nbits, StorageType> &a, const bitset<Nbits, StorageType> &b)
{
	bitset<Nbits, StorageType> result(a);
	result &= b;
	return result;
}

// global bitwise operator| on compile-time bitsets
template <size_t Nbits, typename StorageType>
inline bitset<Nbits, StorageType>
operator|(const bitset<Nbits, StorageType> &a, const bitset<Nbits, StorageType> &b)
{
	bitset<Nbits, StorageType> result(a);
	result |= b;
	return result;
}

// global bitwise operator^ on compile-time bitsets
template <size_t Nbits, typename StorageType>
inline bitset<Nbits, StorageType>
operator^(const bitset<Nbits, StorageType> &a, const bitset<Nbits, StorageType> &b)
{
	bitset<Nbits, StorageType> result(a);
	result ^= b;
	return result;
}

// hamming distance between two bitsets based on XOR-ing the two sets
template <size_t Nbits, typename StorageType>
inline size_t
hamming(const bitset<Nbits, StorageType> &a, const bitset<Nbits, StorageType> &b)
{
	auto tmp = a ^ b;
	return tmp.count();
}


template <size_t Nbits, typename StorageType>
inline size_t
levensthein(const bitset<Nbits, StorageType> &a, const bitset<Nbits, StorageType> &b)
{
	return ncr::levensthein(a.rbegin(), a.rend(), b.rbegin(), b.rend());
}


/*
 * global input and output stuff, based on standard bitset implementations
 */
template <typename CharT, typename Traits, size_t Nbits, typename StorageType>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> &os, const bitset<Nbits, StorageType> &bits)
{
	// use the current locale to select the proper characters for 0 and 1
	std::basic_string<CharT, Traits> tmp;
	const std::ctype<CharT> &ct = std::use_facet<std::ctype<CharT>>(os.getloc());
	bits._copy_to_string(tmp, ct.widen('0'), ct.widen('1'));
	return os << tmp;
}




/*
 * struct dynamic_bitset - A Run-Time Variable Size bitset similar to std::bitset
 *
 *  variable size bitset where the user does not know the number of bits during compile time
 */
template <typename StorageType = size_t>
	requires std::unsigned_integral<StorageType>
struct dynamic_bitset : _bitset_base<StorageType, dynamic_bitset<StorageType>>
{
	// define a shorthand for the base type
	typedef _bitset_base<StorageType, dynamic_bitset<StorageType>>
		base_type;

	// define the word-type that'll be used
	typedef StorageType
		word_t;

	// computes the number of bits per word given the storage type
	constexpr static size_t
		_bits_per_word  = sizeof(word_t) * __CHAR_BIT__;

	// number of bits stored in this bitset
	size_t
		_Nbits = 0;

	// number of words required to store _Nbits many bits
	size_t
		_word_count = 0;

	// the raw (consecutive) memory for all bits in the bitset
	word_t *
		_bits = nullptr;

	// default constructor
	dynamic_bitset() {}

	// copy constructor
	dynamic_bitset(const dynamic_bitset &o)
	{
		// set the size and copy data
		_set_size(o.size());
		this->_copy_from_array(o._bits);
	}

	// move constructor
	dynamic_bitset(dynamic_bitset &&o)
	: _Nbits(std::exchange(o._Nbits, 0))
	, _word_count(std::exchange(o._word_count, 0))
	, _bits(std::exchange(o._bits, nullptr))
	{}

	// constructor with explicit word count
	dynamic_bitset(size_t word_count)
	{
		resize(word_count * _bits_per_word);
	}

	// this constructor assumes that 0s and 1s are passed in.
	dynamic_bitset(std::initializer_list<bool> l, size_t padding = 0)
	{
		resize(l.size() + padding);
		size_t i = l.size() - 1;
		for (auto v : l) {
			if (v)
				this->set(i);
			--i;
		}
	}

	// initialize from a string
	template <typename CharT, typename Traits, typename Alloc>
	dynamic_bitset(const std::basic_string<CharT, Traits, Alloc> &s, CharT zero, CharT one)
	{
		resize(s.length());
		this->template from_string<CharT, Traits, Alloc>(s, zero, one);
	}

	// initialize from a string
	dynamic_bitset(const std::basic_string<char, std::char_traits<char>, std::allocator<char>> &s,
			char zero = '0', char one = '1')
	{
		resize(s.length());
		this->template from_string<char, std::char_traits<char>, std::allocator<char>>(s, zero, one);
	}

	~dynamic_bitset()
	{
		clear();
	}

	// clear the entire variable bitset
	void
	clear()
	{
		_word_count = 0;
		_Nbits = 0;
		delete[] _bits;
		_bits = nullptr;
	}

	// _set_size - set the size of the bitset to Nbits.
	//
	// This will allocate memory in case Nbits can't be stored in _word_count
	// words. In this case, it will be re-allocating. i.e. copy the old data
	// into the new memory.
	void
	_set_size(size_t Nbits)
	{
		// nothing to do?
		if (Nbits == _Nbits)
			return;

		// figure out if new space needs to be allocated
		const size_t new_wordcount = Nbits / _bits_per_word + (Nbits % _bits_per_word == 0 ? 0 : 1);
		const size_t min_wordcount = new_wordcount > _word_count ? _word_count : new_wordcount;

		if (new_wordcount != _word_count) {
			// reallocate
			word_t *buf = new word_t[new_wordcount];
			for (size_t i = 0; i < min_wordcount; i++)
				buf[i] = _bits[i];
			delete[] _bits;
			_bits = buf;
		}

		// default initialize all new bits. For this, get the "end of word" bit
		// of the old memory
		const size_t end_of_word = min_wordcount * _bits_per_word;
		for (size_t i = _Nbits; i < end_of_word; i++)
			this->reset(i);

		// default initialize all new words
		for (size_t i = _word_count; i < new_wordcount; i++)
			this->_bits[i] = static_cast<StorageType>(0);

		// store new size
		_Nbits = Nbits;
		_word_count = new_wordcount;
	}

	// resize - resize the bitset *to* Nbits
	//
	// Important: resizing *to* not *by*, i.e. resize(32) resizes *to* a size of
	// 32 bits, not by 32 bits.
	void
	resize(size_t Nbits)
	{
		_set_size(Nbits);
	}

	// bitwise operators: XOR
	dynamic_bitset<StorageType>&
	operator^=(const dynamic_bitset<StorageType> &b)
	{
		if (b._Nbits != _Nbits)
			throw std::length_error("Length mismatch in operator^= of ncr::dynamic_bitset.");

		for (size_t i = 0; i < _word_count; i++)
			_bits[i] ^= b._bits[i];
		return *this;
	}

	// bitwise operators: OR
	dynamic_bitset<StorageType>&
	operator|=(const dynamic_bitset<StorageType> &b)
	{
		if (b._Nbits != _Nbits)
			throw std::length_error("Length mismatch in operator^= of ncr::dynamic_bitset.");

		for (size_t i = 0; i < _word_count; i++)
			_bits[i] |= b._bits[i];
		return *this;
	}

	// bitwise operators: AND
	dynamic_bitset<StorageType>&
	operator&=(const dynamic_bitset<StorageType> &b)
	{
		if (b._Nbits != _Nbits)
			throw std::length_error("Length mismatch in operator^= of ncr::dynamic_bitset.");

		for (size_t i = 0; i < _word_count; i++)
			_bits[i] &= b._bits[i];
		return *this;
	}

	// assignment operator
	// The dynamic bitset will check and throw an exception in case there's a
	// size mismatch. Note that the size mismatch will be checked on the number
	// of bits, not words
	dynamic_bitset<StorageType>&
	operator=(const dynamic_bitset<StorageType> &b)
	{
		if (b._Nbits != _Nbits)
			throw std::length_error("Length mismatch in operator= of ncr::dynamic_bitset.");

		// assign the entire set of words
		for (size_t i = 0; i < _word_count; i++)
			_bits[i] = b._bits[i];
		return *this;
	}

	// assign - assign bits from another bitset
	//
	// Note that this assigns as many bits as possible, i.e. min(Na, Nb) many
	// bits, where Na and Nb are the bits stored in this bitset (a) and the one
	// assigning from (b).
	dynamic_bitset<StorageType>&
	assign(const dynamic_bitset<StorageType> &b)
	{
		for (size_t i = 0; i < std::min(_Nbits, b._Nbits); i++)
			this->set(i, b.test(i));
		return *this;
	}

	// const forward iterator
	struct const_iterator
	{
		using iterator_category = std::bidirectional_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type        = bool;
		using pointer           = value_type *;
		using reference         = value_type &;
		using const_pointer     = const value_type *;
		using const_reference   = const value_type &;

		// constructors
		// const_iterator(const const_iterator &o) : _bitset(o._bitset), _i(o._i) {}
		const_iterator(const dynamic_bitset<StorageType> *bitset, size_t i) : _bitset(bitset), _i(i) {}

		// dereference operator
		value_type operator* () { return _bitset->test(this->_i) == true; }

		// pointer operator->() { return nullptr; }

		// prefix increment, decrement
		const_iterator& operator++ ()    { _i++; return *this; }
		const_iterator& operator-- ()    { _i--; return *this; }

		// postfix increment, decrement
		const_iterator  operator++ (int) { const_iterator tmp(*this); ++*this; return tmp; }
		const_iterator  operator-- (int) { const_iterator tmp(*this); --*this; return tmp; }

		// comparison operators
		friend bool operator==(const const_iterator &a, const const_iterator &b)
		{
			return (a._bitset == b._bitset) && (a._i == b._i);
		}

	private:
		const dynamic_bitset<StorageType> *_bitset;
		size_t _i;
	};

	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	const_iterator begin() const { return const_iterator(this, 0); }
	const_iterator end()   const { return const_iterator(this, _Nbits); }

	const_reverse_iterator rbegin() const { return std::reverse_iterator<const_iterator>(end()); }
	const_reverse_iterator rend()   const { return std::reverse_iterator<const_iterator>(begin()); }
};

// global bitwise operators on bitsets
template <typename StorageType>
inline dynamic_bitset<StorageType>
operator&(const dynamic_bitset<StorageType> &a, const dynamic_bitset<StorageType> &b)
{
	dynamic_bitset<StorageType> result(a);
	result &= b;
	return result;
}

template <typename StorageType>
inline dynamic_bitset<StorageType>
operator|(const dynamic_bitset<StorageType> &a, const dynamic_bitset<StorageType> &b)
{
	dynamic_bitset<StorageType> result(a);
	result |= b;
	return result;
}

template <typename StorageType>
inline dynamic_bitset<StorageType>
operator^(const dynamic_bitset<StorageType> &a, const dynamic_bitset<StorageType> &b)
{
	dynamic_bitset<StorageType> result(a);
	result ^= b;
	return result;
}

// hamming distance between two bitsets is rather simple to compute
template <typename StorageType>
inline size_t
hamming(const dynamic_bitset<StorageType> &a, const dynamic_bitset<StorageType> &b)
{
	auto tmp = a ^ b;
	return tmp.count();
}

// Levensthein for this type of bitset
// TODO: this requires ncr_algorithms.hpp -> maybe move levensthein and hamming
//       into a ncr_bitset_algorithms, which includes both?
template <typename StorageType>
inline size_t
levensthein(const dynamic_bitset<StorageType> &a, const dynamic_bitset<StorageType> &b)
{
	// XXX: Originally, the implementation used the reverse iterator! This was
	//      changed because of an issue with g++ <= 10.3.0, in which the reverse
	//      iterator changed the reference type (or more precisely the type of
	//      the dereference function) to a reference. This is problematic in the
	//      case of booleans.
	//      Given that we 1) don't actively use Levensthein right now, and 2)
	//      also don't see a significant difference in using the forward or
	//      reverse version, this was changed to use the forward iterator.
	// TODO: Find solution for reverse iterator on old GCC
	return ncr::levensthein(a.begin(), a.end(), b.begin(), b.end());
}

/*
 * global input and output stuff, based on standard bitset implementations
 */
template <typename CharT, typename Traits, typename StorageType>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> &os, const dynamic_bitset<StorageType> &bits)
{
	// use the current locale to select the proper characters for 0 and 1
	std::basic_string<CharT, Traits> tmp;
	const std::ctype<CharT> &ct = std::use_facet<std::ctype<CharT>>(os.getloc());
	bits._copy_to_string(tmp, ct.widen('0'), ct.widen('1'));
	return os << tmp;
}


} // ncr::
