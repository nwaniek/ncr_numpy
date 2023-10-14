/*
 * ncr_pyparser - a simple parser for python data
 *
 * SPDX-FileCopyrightText: 2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */
#pragma once

#include <memory>
#include <ncr/ncr_types.hpp>


namespace ncr {

#ifndef NCR_UTILS
/*
 * memory_guard - A simple memory scope guard
 *
 * This template allows to pass in several pointers which will be deleted when
 * the guard goes out of scope. This is the same as using a unique_ptr into
 * which a pointer is moved, but with slightly less verbose syntax. This is
 * particularly useful to scope members of structs / classes.
 *
 * Example:
 *
 *     struct MyStruct {
 *         SomeType *var;
 *
 *         void some_function
 *         {
 *             // for some reason, var should live only as long as some_function
 *             // is running. This can be useful in the case of recursive
 *             // functions to which sending all the context or state variables
 *             // is inconvenient, and save them in the surrounding struct. An
 *             // alternative, maybe even a preferred way, is to use PODs that
 *             // contain state and use free functions. Still, the memory guard
 *             // might be handy
 *             var = new SomeType();
 *             memory_guard<SomeType> guard(var);
 *
 *             ...
 *
 *             // the guard will call delete on `var' once it drops out of scope
 *         }
 *     };
 *
 * Example with unique_ptr:
 *
 *            // .. struct is same as above
 *            var = new SomeType();
 *            std::unique_ptr<SomeType>(std::move(*var));
 *
 * Yes, this only saves a few characters to type. However, memory_guard works
 * with an arbitrary number of arguments.
 */
template <typename... Ts>
struct memory_guard;

template <>
struct memory_guard<> {};

template <typename T, typename... Ts>
struct memory_guard<T, Ts...> : memory_guard<Ts...>
{
	T *ptr = nullptr;
	memory_guard(T *_ptr, Ts *...ptrs) : memory_guard<Ts...>(ptrs...), ptr(_ptr) {}
	~memory_guard() { if (ptr) delete ptr; }
};


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
#endif


inline bool
equals(const u8_const_subrange &subrange, const std::string_view &str)
{
	return std::ranges::equal(subrange, str);
}


inline bool
equals(u8_const_iterator first, u8_const_iterator last, const std::string_view &str)
{
	return equals(u8_const_subrange(first, last), str);
}


/*
 * token - representes a token read from an input vector
 */
struct token {

	// basic types
	enum class type : u8 {
		unknown,
		// punctuations / separators
		// dot,            // XXX: currently not handled explicitly
		// ellipsis,       // XXX: currently not handled explicitly
		left_brace,        // { begin set | dict
		right_brace,       // } end set | dict
		left_bracket,      // [ begin list
		right_bracket,     // ] end list
		left_paren,        // ( begin tuple
		right_paren,       // ) end tuple
		value_separator,   // ,
		kv_separator,      // : between key and value pairs
		// literals of known type
		string_literal,    // a string ...
		integer_literal,   // an integer number
		float_literal,     // a floating point number
		bool_literal,      // True or False
		none_literal,      // None
		// others
		// identifier,     // XXX: currently not supported
		// keyword,        // XXX: currently not supported
		// operator,       // XXX: currently not supported
	};

	// the type of this token
	type ttype = type::unknown;

	// explicitly store the iterators for this token relative to the input data.
	// this makes debugging and data extraction slightly easier
	u8_const_iterator begin;
	u8_const_iterator end;
	u8_const_subrange range() const { return u8_const_subrange(begin, end); }

	// in case of numbers or single symbols, we also directly store the
	// (converted) value. In case of numerical values, this is a byproduct of
	// testing for numerical values. In case of single symbols, it can make
	// access a bit faster than going through the iterator
	union {
		u8   sym;
		u64  l;
		f64  d;
		bool b;
	} value;

	/*
	 * mapping of a punctuation symbol to its type
	 */
	struct punctuation {
		const u8 sym            = 0;
		const token::type ptype = token::type::unknown;
	};

	/*
	 * list of all punctuations
	 */
	static constexpr punctuation punctuations[] = {
		// TODO: dot and ellipsis
		{'{', token::type::left_brace},
		{'}', token::type::right_brace},
		{'[', token::type::left_bracket},
		{']', token::type::right_bracket},
		{'(', token::type::left_paren},
		{')', token::type::right_paren},
		{':', token::type::kv_separator},
		{',', token::type::value_separator},
	};

	/*
	 * list of all literals
	 */
	static constexpr token::type literals[] = {
		token::type::string_literal,
		token::type::integer_literal,
		token::type::float_literal,
		token::type::bool_literal,
		token::type::none_literal,
	};

	/*
	 * is_literal - evaluate if the token type is literal
	 */
	static constexpr bool
	is_literal(const token::type &t) {
		for (auto &l: literals)
			if (l == t) return true;
		return false;
	}
};


// TODO: only for debug purposes
inline std::ostream&
operator<<(std::ostream &os, const u8_const_subrange &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		os << (*it);
	return os;
}

/*
 * TODO: for debugging purposes only
 */
inline std::ostream&
operator<<(std::ostream &os, const ncr::token::type &type)
{
	using namespace ncr;

	switch (type) {
	case token::type::string_literal:  os << "string";         return os;
	//case token::type::dot:             os << "dot";            return os;
	//case token::type::ellipsis:        os << "ellipsis";       return os;
	case token::type::value_separator: os << "delimiter";      return os;
	case token::type::left_brace:      os << "braces_left";    return os;
	case token::type::right_brace:     os << "braces_right";   return os;
	case token::type::left_bracket:    os << "brackets_left";  return os;
	case token::type::right_bracket:   os << "brackets_right"; return os;
	case token::type::left_paren:      os << "parens_left";    return os;
	case token::type::right_paren:     os << "parens_right";   return os;
	case token::type::kv_separator:    os << "colon";          return os;
	case token::type::integer_literal: os << "integer";        return os;
	case token::type::float_literal:   os << "floating_point"; return os;
	case token::type::bool_literal:    os << "boolean";        return os;
	case token::type::none_literal:    os << "none";           return os;
	case token::type::unknown:         os << "unknown";        return os;
	default:                           os.setstate(std::ios_base::failbit);
	}
	return os;
}

inline std::ostream&
operator<<(std::ostream &os, const ncr::token &token)
{
	using namespace ncr;
	os << "token type: " << token.ttype << ", value: " << token.range();
	return os;
}


/*
 * tokenizer - a backtracking tokenizer to lex tokens from an input vector
 *
 * This tokenizer uses a very basic implementation by stepping through the input
 * vector and extracting valid tokens, i.e. those sequences of characters mostly
 * surrounded by whitespace, punctutations, or other delimiters, and putting
 * them into a token struct. Backtracking is implemented using a std::vector as
 * buffer, which simply stores each token that was extracted from the input.
 * This makes backtracking effective and simple to implement, with the drawback
 * of having a growing vector (currently, no .consume() is implemented because
 * that would require some more housekeeping).
 *
 * Could this parser be implemented any other way? Sure! For instance, we could
 * roll a template-based tokenizer, but I'm too lazy to do that for real right
 * now. If anyone is interested and has too much time on their hand, here's a
 * starting point for the interested coder:
 *
 *
 *     template <char C>
 *     struct Char {
 *         enum { value = C };
 *         static constexpr bool match(char c) { return c == C; }
 *     };
 *
 *
 *     template <typename... Ts>
 *     struct Or {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if any values match
 *             return (Ts::match(arg) || ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct And {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if all values match
 *             return (Ts::match(arg) && ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct Not {
 *         template <typename Arg>
 *         static constexpr bool match(Arg arg) {
 *             // use fold expression to test if all values match
 *             return !(Ts::match(arg) || ...);
 *         }
 *     };
 *
 *     template <typename... Ts>
 *     struct Seq {
 *         template <typename... Args>
 *         static bool match(Args... args) {
 *             // use fold expression to test if all values match
 *             return (Ts::match(args) && ...);
 *         }
 *
 *         template <typename Arg>
 *         static constexpr bool match(const std::vector<Arg> &arg) {
 *             // determine if size matches
 *             if (arg.size() != sizeof...(Ts)) return false;
 *             // use fold expression to test if all values match
 *             size_t i = 0;
 *             return (Ts::match(arg[i++]) && ...);
 *         }
 *
 *         template <typename I>
 *         static constexpr bool match(const std::ranges::subrange<I> &arg) {
 *             // determine if size matches
 *             if (arg.size() != sizeof...(Ts)) return false;
 *             // use fold expression to test if all values match
 *             size_t i = 0;
 *             return (Ts::match(arg[i++]) && ...);
 *         }
 *     };
 *
 *
 *     // build parse table
 *     using Whitespace = Or<Char<'\n'>, Char<' '>, Char<'\t'>>;
 *     using Letter     = Or<Char<'a'>, Char<'b'>>; // this is of course entirely incomplete
 *     using If         = Seq<Char<'i'>, Char<'f'>>;
 *     using Else       = Seq<Char<'e'>, Char<'l'>, Char<'s'>, Char<'e'>>;
 *     using IfElse     = Or<If, Else>;
 *     // a ton more declarations
 *
 *     // This can be then used as follows:
 *     std::vector<unsigned> v{'i', 'f', 'g'};
 *     std::cout << "expected false: " << If::match(v) << "\n";
 *     std::ranges::subrange range(v.begin(), std::prev(v.end()));
 *     std::cout << "expected true:  " << If::match(range) << "\n";
 *     std::cout << "expected true:  " << IfElse::match(range) << "\n";
 *
 * I welcome any patches that replace the tokenizer with the template version.
 * Such a patch must (!) include a performance comparison that clearly shows an
 * advantage of the template version to convince me that it's worth applying,
 * though.
 */
struct tokenizer
{
	// reference to the input data
	const u8_const_subrange &data;

	// iterators to track start and end of a token. also called 'cursor'
	u8_const_iterator  tok_start;
	u8_const_iterator  tok_end;

	// buffer and position within the buffer for all tokens that were read. Note
	// that the buffer is not pruned at the moment and thus lives as long as the
	// tokenizer. For most inputs that this tokenizer will see, this currently
	// does not pose any issue. In the future, a .parse() function might be
	// implemented which takes care of the buffer growing out-of-bounds.
	using restore_point = size_t;
	std::vector<token> buffer;
	size_t             buffer_pos = 0;

	// different tokenizer result
	enum class result : u8 {
		ok,
		end_of_input,
		incomplete_token,
		invalid_token,
	};

	tokenizer(u8_const_subrange &_data) : data(_data), tok_start(data.begin()), tok_end(data.begin()) {}

	// determine the punctuation type of the symbol under the cursor
	inline bool
	get_punctuation_type(u8 sym, token::type &t) const
	{
		for (auto &p: token::punctuations)
			if (p.sym == sym) {
				t = p.ptype;
				return true;
			}
		return false;
	}

	// determine if a string is an integer number or not.
	// TODO: maybe adapt std::from_chars, as this might circumvent using an
	// std::string, or maybe parse the numbers manually.
	// TODO: also allow users to have the ability to use
	// boost::lexical_cast. But I don't see why we should pull in anything
	// from boost for one or two lines of code.
	inline bool
	is_integer_literal(std::string str, u64 &value) const
	{
		char *end;
		value = std::strtol(str.c_str(), &end, 10);
		return *end == '\0';
	}

	// determine if a string is a floating point number or not.
	// TODO: maybe adapt std::from_chars, as this might circumvent using an
	// std::string, or maybe parse the numbers manually
	// TODO: also allow users to have the ability to use
	// boost::lexical_cast. But I don't see why we should pull in anything
	// from boost for one or two lines of code.
	inline bool
	is_float_literal(std::string str, double &value) const
	{
		// TODO: maybe adapt std::from_chars, as this might circumvent using an
		// std::string. However, from_chars for float will be only in C++23
		char *end;
		value = std::strtod(str.c_str(), &end);
		return *end == '\0';
	}


	// determine if the range given by [first,last) is a literal true or literal
	// false
	inline bool
	is_bool_literal(u8_const_iterator first, u8_const_iterator last, bool &value) const
	{
		if (equals(first, last, "False")) { value = false; return true; }
		if (equals(first, last, "True"))  { value = true;  return true; }
		return false;
	}

	inline bool
	is_whitespace(u8 sym) const
	{
		return sym == ' ' || sym == '\n' || sym == '\t';
	}


	bool
	eof() {
		// try to read a token. if it's all whitespace at the end, then we'll
		// get a corresponding result. if not, then backtrack. This prevents
		// parsing fails when there's no more input.
		// TODO: better to propagate EOF from within the parse_* methods (see
		//       also comment in parse()
		restore_point rp;
		token tok;
		if (tok_start == data.end() || get_next_token(tok, &rp) == result::end_of_input)
			return true;
		restore(rp);
		return false;
	}

	result
	__fetch_token(token &tok)
	{
		if (tok_start == data.end())
			return result::end_of_input;

		// ignore whitespace
		if (is_whitespace(*tok_start)) {
			do {
				++tok_start;
				if (tok_start == data.end())
					break;
			} while (is_whitespace(*tok_start));
			tok_end = tok_start;
			if (tok_start == data.end())
				return result::end_of_input;
		}

		// TODO: number start, -,+,number,. -> would need to parse the entire
		//       number manually, though

		/*
		 * TODO: dot and ellipsis
		 */
		/*
		// determine if this is an ellipsis, because python interprets '...'
		// as a single token!
			tok_end = tok_start;
			u16 accum = 1;
			while (++tok_end < dlen) {
				if (data[tok_end] != '.')
					break;
				++accum;
			}
			if (accum != 1 && accum != 3) {
				return result::invalid_token;
			}
		*/

		// punctuations
		token::type ttype;
		if (get_punctuation_type(*tok_start, ttype)) {
			tok.ttype     = ttype;
			tok.begin     = tok_start;
			tok.end       = tok_start + 1;
			// TODO: this is wrong in the case of an ellipsis
			tok.value.sym = *tok_start;
			++tok_start;
			tok_end = tok_start;
			return result::ok;
		}

		// string token
		if (*tok_start == '\'' || *tok_start == '\"') {
			u8 str_delim = *tok_start;
			tok_end = tok_start;
			while (++tok_end != data.end()) {
				if (*tok_end == str_delim)
					break;
			}
			tok.ttype = token::type::string_literal;
			// range excludes the surrounding ''
			tok.begin = tok_start + 1;
			tok.end   = tok_end;

			// test if string is finished
			if (tok_end == data.end() || *tok_end != str_delim)
				return result::incomplete_token;

			tok_start = ++tok_end;
			return result::ok;
		}

		// read everything until a punctuation or whitespace
		while (tok_end != data.end()) {
			token::type ttype;
			if (*tok_end == ' ' || get_punctuation_type(*tok_end, ttype))
				break;
			++tok_end;
		}
		if (tok_end > tok_start) {
			tok.begin = tok_start;
			tok.end   = tok_end;

			// TODO: avoid using a temporary string, and use something that
			// actually uses system locales to determine numbers based on a
			// locale's decimal point settings
			std::string tmp(tok.begin, tok.end);
			if (is_integer_literal(tmp, tok.value.l))
				tok.ttype = token::type::integer_literal;
			else if (is_float_literal(tmp, tok.value.d))
				tok.ttype = token::type::float_literal;
			else if (is_bool_literal(tok.begin, tok.end, tok.value.b))
				tok.ttype = token::type::bool_literal;
			else if (equals(tok.begin, tok.end, "None"))
				tok.ttype = token::type::none_literal;
			else
				// we could not determine this type.
				// TODO: maybe return an error code
				tok.ttype = token::type::unknown;
		}
		tok_start = tok_end;
		return result::ok;
	};


	/*
	 * get_next_token - get the next token
	 *
	 * This will return a tokenizer result and store the next token in the
	 * passed in reference tok. In case a restore_point is passed in to bpoint,
	 * then bpoint will be set to the token *before* the one returned in tok.
	 * This makes it possible to reset the tokenizer to before the read symbol
	 * in case the symbol is unexpected during parsing.
	 *
	 * Note: Internally, the tokenizer stores all read tokens in a buffer which
	 * grows over time. While this is not ideal, and a mechanism that parses
	 * the tokens and prevents unbounded growth of the buffer, it's currently
	 * not required. On systems with significantly limited memory, this might
	 * become an issue, though.
	 */
	result
	get_next_token(token &tok, restore_point *bpoint =nullptr)
	{
		if (bpoint != nullptr)
			*bpoint = buffer_pos;

		if (buffer_pos < buffer.size()) {
			tok = buffer[buffer_pos++];
			return tokenizer::result::ok;
		}

		if (buffer.empty() || buffer_pos >= buffer.size()) {
			token _tok;
			if (__fetch_token(_tok) == tokenizer::result::ok) {
				buffer.push_back(_tok);
				tok = buffer.back();
				buffer_pos = buffer.size();
				return tokenizer::result::ok;
			}
		}

		return tokenizer::result::end_of_input;
	}

	// backup points to memoize where the
	restore_point backup()             { return buffer_pos;   }
	void restore(restore_point bpoint) { buffer_pos = bpoint; }

};



/*
 * parser - A simple recursive descent parser (RDP)
 *
 * The parser uses a backtracking tokenizer to be able to -- in principle --
 * parse LL(k) languages. However, it is reduced to the purpose of what is
 * required.
 *
 * Note: In theory, an RDP for LL(k) languages with backtracking can suffer from
 * an exponential runtime due to backtracking. However, this is rarely an issue
 * besides theoretical considerations and nasty language grammars of obscure and
 * mostly theoretical programming languages. If ever this becomes a problem, the
 * parser type can be changed, of course. Until then, I'll stick to RDP and
 * ignore discussions around this issue.
 */
struct pyparser
{
	enum class result : u8 {
		ok,             // parsing succeeded
		failure,        // failure parsing a context / type / object
		syntax_error,   // syntax error while parsing
		incomplete      // parsing encountered an incomplete context / type / object
	};

	/*
	 * the type of object that was parsed
	 */
	enum class type : u8 {
		uninitialized,  // something we don't yet know (default value)

		none,           // the none-type for the keyword "None"
		string,
		integer,
		floating_point,
		boolean,
		kvpair,         // a key:value pair
		tuple,          // tuples of the form (value0, value1, ...)
		list,           // lists of the form [value0, value1, ...]
		set,            // sets of the form {value0, value1, ...}
		dict,           // dict of the form {key0:value0, key1:value1, ...}

		symbol,         // anything like {}[], etc. we return parse_results for
						// symbols even though they don't specify a particular
						// type, because we might want to extract the beginning
						// and end of certain groups. This keeps the interface
						// somewhat small. A parse result of type symbol should
						// never end up directly or indirectly in the
						// root_context. This allows to store everything the
						// parser returns in a parse result, i.e. it will not
						// require a different data type. Could have used an
						// std::vector or similar container, though.

		root_context,   // root context, contains everything that was parsed
						// successfully
	};


	/*
	 * a parse result
	 *
	 * A parse result is treated as a context. A context can be a value, or
	 * contain other parse results as nodes. For instance, a list contains the
	 * elements of the list in the nodes vector. Generally, the parse result
	 * contains information about the type, the range of the result, as well as
	 * embedded nodes. In the case of 'basic types' (integer, double, bool), the
	 * value is accessible directly via the value union.
	 *
	 * XXX: maybe use an std::variant to get more type checking into the parser
	 */
	struct parse_result {
		using parse_result_nodes = std::vector<std::unique_ptr<parse_result>>;

		// parse status of this result / context
		result             status = result::failure;

		// data type of this result / context
		type               dtype = type::uninitialized;

		// where this type / context starts in the input range
		u8_const_iterator  begin;

		// where this type / context ends
		u8_const_iterator  end;

		// in case of a group (kvpairs, lists, etc.), this contains the group's
		// children. In case of a key-value pair, there will be 2 children:
		// first the key, then the value
		parse_result_nodes nodes;

		// for 'basic types', this contains the actual value
		union {
			i64    l;
			f64    d;
			bool   b;
		} value;

		// access to the range within the input which this parse_result captures
		u8_const_subrange range() { return u8_const_subrange(begin, end); }
	};


	// state variables of the parse
	tokenizer *tokens = nullptr;      // the tokenizer used during parsing. Note
									  // that this member will live only during
									  // a call to parse()

	inline static constexpr bool
	is_number(token &tok)
	{
		return
			tok.ttype == token::type::float_literal ||
			tok.ttype == token::type::integer_literal;
	}


	inline static constexpr bool
	is_string(token &tok)
	{
		return
			tok.ttype == token::type::string_literal;
	}


	inline static constexpr bool
	is_delimiter(token &tok)
	{
		return
			tok.ttype == token::type::value_separator;
	}


	/*
	 * parse a token of a particular type Type
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <token::type TokenType, pyparser::type ParserType>
	std::unique_ptr<parse_result>
	parse_token_type() {
		if (!tokens) return {};

		token tok;
		tokenizer::restore_point rp;
		if (tokens->get_next_token(tok, &rp) == tokenizer::result::ok && tok.ttype == TokenType) {
			auto ptr = std::make_unique<parse_result>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == type::boolean)
				ptr->value.b = tok.value.b;
			if constexpr (ParserType == type::integer)
				ptr->value.l = tok.value.l;
			if constexpr (ParserType == type::floating_point)
				ptr->value.d = tok.value.d;

			return ptr;
		}

		tokens->restore(rp);
		return {};
	}

	/*
	 * parse a token that evalutes Fn to true.
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <pyparser::type ParserType, typename F>
	std::unique_ptr<parse_result>
	parse_token_fn(F fn) {
		if (!tokens) return {};

		token tok;
		tokenizer::restore_point rp;
		if (tokens->get_next_token(tok, &rp) == tokenizer::result::ok && fn(tok)) {
			auto ptr = std::make_unique<parse_result>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == type::integer)
				ptr->value.l = tok.value.l;

			return ptr;
		}

		tokens->restore(rp);
		return {};
	}

	// symbols. result of these parse instructions will be ignored, but for the
	// sake of completeness, we still specify a parser type
	inline std::unique_ptr<parse_result> parse_delimiter() { return parse_token_fn<type::symbol>(is_delimiter);                     }
	inline std::unique_ptr<parse_result> parse_colon()     { return parse_token_type<token::type::kv_separator,   type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_lbracket()  { return parse_token_type<token::type::left_bracket,   type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_rbracket()  { return parse_token_type<token::type::right_bracket,  type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_lbrace()    { return parse_token_type<token::type::left_brace,     type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_rbrace()    { return parse_token_type<token::type::right_brace,    type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_lparen()    { return parse_token_type<token::type::left_paren,     type::symbol>();  }
	inline std::unique_ptr<parse_result> parse_rparen()    { return parse_token_type<token::type::right_paren,    type::symbol>();  }

	// types / literals
	inline std::unique_ptr<parse_result> parse_number()    { return parse_token_fn<type::integer>(is_number);                       }
	inline std::unique_ptr<parse_result> parse_string()    { return parse_token_type<token::type::string_literal, type::string>();  }
	inline std::unique_ptr<parse_result> parse_bool()      { return parse_token_type<token::type::bool_literal,   type::boolean>(); }
	inline std::unique_ptr<parse_result> parse_none()      { return parse_token_type<token::type::none_literal,   type::floating_point>();    }


	std::unique_ptr<parse_result>
	parse_kvpair()
	{
		if (!tokens) return {};
		auto rp = tokens->backup();

		// parse the key
		auto      key = parse_string();
		if (!key) key = parse_number();
		if (!key) key = parse_tuple();
		if (!key) {
			tokens->restore(rp);
			return {};
		}

		// parse :
		if (!parse_colon()) {
			tokens->restore(rp);
			return {};
		}

		// parse the value
		auto        value = parse_none();
		if (!value) value = parse_bool();
		if (!value) value = parse_number();
		if (!value) value = parse_string();
		if (!value) value = parse_tuple();
		if (!value) value = parse_list();
		if (!value) value = parse_set();
		if (!value) value = parse_dict();
		if (!value) {
			// failed to parse kv pair, backtrack out
			tokens->restore(rp);
			return {};
		}

		// package up the result
		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::ok;
		ptr->dtype = type::kvpair;
		ptr->begin = key->begin;
		ptr->end   = value->end;
		ptr->nodes.push_back(std::move(key));
		ptr->nodes.push_back(std::move(value));
		return ptr;
	}


	std::unique_ptr<parse_result>
	parse_tuple()
	{
		if (!tokens) return {};
		auto rp = tokens->backup();

		auto lparen = parse_lparen();
		if (!lparen) return {};

		// prepare package
		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::incomplete;
		ptr->dtype  = type::tuple;
		ptr->begin  = lparen->begin;

		bool expect_delim = false;
		while (true) {
			if (parse_delimiter()) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rparen = parse_rparen();
			if (rparen) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rparen->end;
				return ptr;
			}

			// almost everything is allowed in tuples. we only care about those
			// types that we have implemented (so far), though.
			auto       elem = parse_none();
			if (!elem) elem = parse_bool();
			if (!elem) elem = parse_number();
			if (!elem) elem = parse_string();
			if (!elem) elem = parse_tuple();
			if (!elem) elem = parse_list();
			if (!elem) elem = parse_set();
			if (!elem) elem = parse_dict();
			if (!elem) {
				// failed to parse tuple, backtrack out
				tokens->restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
	}


	std::unique_ptr<parse_result>
	parse_list()
	{
		if (!tokens) return {};
		auto rp = tokens->backup();

		auto lbracket = parse_lbracket();
		if (!lbracket) return {};

		// prepare package
		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::incomplete;
		ptr->dtype  = type::list;
		ptr->begin  = lbracket->begin;

		bool expect_delim = false;
		while (true) {
			if (parse_delimiter()) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbracket = parse_rbracket();
			if (rbracket) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rbracket->end;
				return ptr;
			}

			// almost everything is allowed in a list
			auto       elem = parse_none();
			if (!elem) elem = parse_bool();
			if (!elem) elem = parse_number();
			if (!elem) elem = parse_string();
			if (!elem) elem = parse_tuple();
			if (!elem) elem = parse_list();
			if (!elem) elem = parse_set();
			if (!elem) elem = parse_dict();
			if (!elem) {
				// failed to parse list, backtrack out
				tokens->restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
	}


	std::unique_ptr<parse_result>
	parse_set()
	{
		if (!tokens) return {};
		auto rp = tokens->backup();

		auto lbrace = parse_lbrace();
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::incomplete;
		ptr->dtype  = type::set;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (true) {
			if (parse_delimiter()) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace();
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// allowed types in a set are all immutable and hashable types. we
			// don't support arbitrary hashable objects, so we only need to
			// check for immutable things that we know
			auto       elem = parse_none();
			if (!elem) elem = parse_bool();
			if (!elem) elem = parse_number();
			if (!elem) elem = parse_string();
			if (!elem) elem = parse_tuple();
			if (!elem) elem = parse_set();
			if (!elem) {
				// failed to parse set, backtrack out
				tokens->restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
	}


	std::unique_ptr<parse_result>
	parse_dict()
	{
		if (!tokens) return {};
		auto rp = tokens->backup();

		auto lbrace = parse_lbrace();
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::incomplete;
		ptr->dtype  = type::dict;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (true) {
			if (parse_delimiter()) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace();
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// sets are surprisingly simple beasts to tame, because they only
			// want to eat kv pairs
			auto kv_pair = parse_kvpair();
			if (!kv_pair) {
				// failed to parse dict, backtrack out
				tokens->restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(kv_pair));
			expect_delim = true;
		}


	}


	std::unique_ptr<parse_result>
	parse_expression()
	{
		if (!tokens)       return {};

		// parse the things we know (and care about) in order they are specified
		// in python's formal grammar
		auto         result = parse_tuple();
		if (!result) result = parse_list();
		if (!result) result = parse_set();
		if (!result) result = parse_dict();

		return result;
	}


	std::unique_ptr<parse_result>
	parse(u8_const_subrange &input)
	{
		// tokenizer is a member, but lives only within this scope. we could, of
		// course call delete before each return, but that'd be too easy. We
		// could also use another local unique_ptr and move tokens into it, i.e.
		//    auto ptr = std::make_unique<Foo>(std::move(*tokens));
		// but that looks really ugly.
		tokens = new tokenizer(input);
		memory_guard<tokenizer> guard(tokens);

		auto ptr = std::make_unique<parse_result>();
		ptr->status = result::incomplete;
		ptr->dtype  = type::root_context;
		// TODO: currently we don't store begin nor end for the root context.
		// maybe this should change

		// properly initialize the parser state
		while (!tokens->eof()) {
			auto expr = parse_expression();
			if (!expr)
				return {};
			ptr->nodes.push_back(std::move(expr));
		}
		ptr->status = result::ok;
		return ptr;
	}

	std::unique_ptr<parse_result>
	parse(const u8_vector &input)
	{
		auto sr = u8_const_subrange(input.cbegin(), input.cend());
		return parse(sr);
	}

};


} // ncr
