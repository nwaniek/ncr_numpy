/*
 * pyparser.hpp - a simple parser for python data
 *
 * SPDX-FileCopyrightText: 2023-2026 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */


#ifndef _f03a19a69cac46f38404d117df9d9c37_
#define _f03a19a69cac46f38404d117df9d9c37_

#include <cassert>
#include <memory>
#include <charconv>
#include "ncr/types.hpp"
#include "ncr/strconv.hpp"


namespace ncr { namespace numpy {


inline bool
equals(const u8_const_span &subspan, const std::string_view &str)
{
	return (subspan.size() == str.size()) &&
	       std::equal(subspan.begin(), subspan.end(), str.begin());
}


inline bool
equals(u8_const_iterator first, u8_const_iterator last, const std::string_view &str)
{
	return equals(u8_const_span(&*first, std::distance(first, last)), str);
}

inline bool
equals(const u8* first, const u8* last, const std::string_view &str)
{
	if (!first || !last)
		return false;
	assert(last >= first);

	return equals(u8_const_span(first, static_cast<size_t>(last - first)), str);
}

// basic types
enum class TokenType : u8 {
	Unknown,
	// punctuations / separators
	// Dot,            // XXX: currently not handled explicitly
	// Ellipsis,       // XXX: currently not handled explicitly
	LeftBrace,        // { begin set | dict
	RightBrace,       // } end set | dict
	LeftBracket,      // [ begin list
	RightBracket,     // ] end list
	LeftParen,        // ( begin tuple
	RightParen,       // ) end tuple
	ValueSeparator,   // ,
	KVSeparator,      // : between key and value pairs
	// literals of known type
	StringLiteral,    // a string ...
	IntegerLiteral,   // an integer number
	FloatLiteral,     // a floating point number
	BoolLiteral,      // True or False
	NoneLiteral,      // None
	// others
	// Identifier,     // XXX: currently not supported
	// Keyword,        // XXX: currently not supported
	// Operator,       // XXX: currently not supported
};


/*
 * mapping of a punctuation symbol to its type
 */
struct Punctuation {
	const u8 sym          = 0;
	const TokenType ttype = TokenType::Unknown;
};


/*
 * list of all punctuations
 */
inline constexpr Punctuation punctuations[] = {
	// TODO: dot and ellipsis
	{'{', TokenType::LeftBrace},
	{'}', TokenType::RightBrace},
	{'[', TokenType::LeftBracket},
	{']', TokenType::RightBracket},
	{'(', TokenType::LeftParen},
	{')', TokenType::RightParen},
	{':', TokenType::KVSeparator},
	{',', TokenType::ValueSeparator},
};


/*
 * list of all literals
 */
inline constexpr TokenType literals[] = {
	TokenType::StringLiteral,
	TokenType::IntegerLiteral,
	TokenType::FloatLiteral,
	TokenType::BoolLiteral,
	TokenType::NoneLiteral,
};


/*
 * is_literal - evaluate if the token type is literal
 */
inline constexpr bool
is_literal(const TokenType &t) {
	for (auto &l: literals)
		if (l == t) return true;
	return false;
}


// determine the punctuation type of the symbol under the cursor
inline bool
get_punctuation_type(u8 sym, TokenType &t)
{
	for (auto &p: punctuations)
		if (p.sym == sym) {
			t = p.ttype;
			return true;
		}
	return false;
}


// determine if [first, last) parses as an integer literal. Uses std::from_chars
// instead of a byte range to avoid the per-token std::string allocation
// that std::strtol/std::strtod would require.
inline bool
is_integer_literal(const u8* first, const u8* last, u64 &value)
{
	const char *b = reinterpret_cast<const char*>(first);
	const char *e = reinterpret_cast<const char*>(last);
	auto [ptr, ec] = std::from_chars(b, e, value);
	return ec == std::errc{} && ptr == e;
}


// determine if [first, last) parses as a floating-point literal. As above,
// avoids constructing a transient std::string for std::strtod by using
// std::from_chars.
inline bool
is_float_literal(const u8* first, const u8* last, double &value)
{
	const char *b = reinterpret_cast<const char*>(first);
	const char *e = reinterpret_cast<const char*>(last);
	auto [ptr, ec] = std::from_chars(b, e, value);
	return ec == std::errc{} && ptr == e;
}


// determine if the range given by [first,last) is a literal true or literal
// false
inline bool
is_bool_literal(u8_const_iterator first, u8_const_iterator last, bool &value)
{
	if (equals(first, last, "False")) { value = false; return true; }
	if (equals(first, last, "True"))  { value = true;  return true; }
	return false;
}

inline bool
is_bool_literal(const u8* first , const u8* last, bool &value)
{
	auto subspan = u8_const_span(first, std::distance(first, last));

	if (equals(subspan, "False")) { value = false; return true; }
	if (equals(subspan, "True"))  { value = true;  return true; }
	return false;
}


inline bool
is_whitespace(u8 sym)
{
	return sym == ' ' || sym == '\n' || sym == '\t';
}



/*
 * token - representes a token read from an input vector
 */
struct Token
{
	// the type of this token
	TokenType ttype = TokenType::Unknown;

	// explicitly store the iterators for this token relative to the input data.
	// this makes debugging and data extraction slightly easier
	const u8* begin;
	const u8* end;
	u8_const_span span() const { return u8_const_span(&*begin, std::distance(begin, end)); }

	// in case of numbers or single symbols, we also directly store the
	// (converted) value. In case of numerical values, this is a byproduct of
	// testing for numerical values. In case of single symbols, it can make
	// access a bit faster than going through the iterator
	//
	// Default-initialize to 0 so tokens whose ttype does not carry a value
	// (e.g. brackets, the value separator) don't leave indeterminate bytes
	// behind in the buffered tokens vector.
	union {
		u8   sym;
		u64  l;
		f64  d;
		bool b;
	} value = {};
};


inline bool
equals(const Token &tok, const std::string_view &str)
{
	return equals(u8_const_span(&*tok.begin, std::distance(tok.begin, tok.end)), str);
}


/*
 * TODO: for debugging purposes only
 */
inline
const char*
to_string(const TokenType &type)
{
	using namespace ncr;

	switch (type) {
	case TokenType::StringLiteral:  return "string";
	//case token_type::dot:             return "dot";
	//case token_type::ellipsis:        return "ellipsis";
	case TokenType::ValueSeparator: return "delimiter";
	case TokenType::LeftBrace:      return "braces_left";
	case TokenType::RightBrace:     return "braces_right";
	case TokenType::LeftBracket:    return "brackets_left";
	case TokenType::RightBracket:   return "brackets_right";
	case TokenType::LeftParen:      return "parens_left";
	case TokenType::RightParen:     return "parens_right";
	case TokenType::KVSeparator:    return "colon";
	case TokenType::IntegerLiteral: return "integer";
	case TokenType::FloatLiteral:   return "floating_point";
	case TokenType::BoolLiteral:    return "boolean";
	case TokenType::NoneLiteral:    return "none";
	case TokenType::Unknown:        return "unknown";
	}

	return "";
}


/*
 * TODO: for debugging purposes only
 */
inline
std::string
to_string(const Token &token)
{
	std::ostringstream oss;
	oss << "token type: " << to_string(token.ttype) << ", value: " << ncr::to_string(token.span(), {.sep=" ", .beg="", .end=""});
	return oss.str();
}


inline constexpr bool
is_number(Token &tok)
{
	return
		tok.ttype == TokenType::FloatLiteral ||
		tok.ttype == TokenType::IntegerLiteral;
}


inline constexpr bool
is_string(Token &tok)
{
	return
		tok.ttype == TokenType::StringLiteral;
}


inline constexpr bool
is_delimiter(Token &tok)
{
	return
		tok.ttype == TokenType::ValueSeparator;
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
struct Tokenizer
{
	// reference to the input data
	const u8* data_start;
	const u8* data_end;

	// iterators to track start and end of a token. also called 'cursor'
	const u8 * tok_start;
	const u8 * tok_end;

	// buffer and position within the buffer for all tokens that were read. Note
	// that the buffer is not pruned at the moment and thus lives as long as the
	// tokenizer. For most inputs that this tokenizer will see, this currently
	// does not pose any issue. In the future, a .parse() function might be
	// implemented which takes care of the buffer growing out-of-bounds.
	using RestorePoint = size_t;
	std::vector<Token> buffer;
	size_t             buffer_pos {0};

	// different tokenizer result
	enum class result : u8 {
		ok,
		end_of_input,
		incomplete_token,
		invalid_token,
	};


	Tokenizer(const u8* begin, const u8* end)
	 	 : data_start(begin), data_end(end)
	 	 , tok_start(begin), tok_end(end) {}

	// tokenizer(u8_span &_data) : data(_data), tok_start(data.begin()), tok_end(data.begin()) {}


	bool
	eof() {
		// try to read a token. if it's all whitespace at the end, then we'll
		// get a corresponding result. if not, then backtrack. This prevents
		// parsing fails when there's no more input.
		// TODO: better to propagate EOF from within the parse_* methods (see
		//       also comment in parse()
		RestorePoint rp;
		Token tok;
		if (tok_start == data_end || get_next_token(tok, &rp) == result::end_of_input)
			return true;
		restore(rp);
		return false;
	}


	result
	__fetch_token(Token &tok)
	{
		if (tok_start == data_end)
			return result::end_of_input;

		// ignore whitespace
		if (is_whitespace(*tok_start)) {
			do {
				++tok_start;
				if (tok_start == data_end)
					break;
			} while (is_whitespace(*tok_start));
			tok_end = tok_start;
			if (tok_start == data_end)
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
		TokenType ttype;
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
			while (++tok_end != data_end) {
				// skip escaped character
				if (*tok_end == '\\') {
					if (tok_end+1 != data_end)
						++tok_end;
					continue;
				}
				if (*tok_end == str_delim)
					break;
			}
			tok.ttype = TokenType::StringLiteral;
			// range excludes the surrounding ''
			tok.begin = tok_start + 1;
			tok.end   = tok_end;

			// test if string is finished
			if (tok_end == data_end || *tok_end != str_delim)
				return result::incomplete_token;

			tok_start = ++tok_end;
			return result::ok;
		}

		// read everything until a punctuation or whitespace
		while (tok_end != data_end) {
			TokenType ttype;
			if (*tok_end == ' ' || get_punctuation_type(*tok_end, ttype))
				break;
			++tok_end;
		}
		if (tok_end > tok_start) {
			tok.begin = tok_start;
			tok.end   = tok_end;

			if (is_integer_literal(tok.begin, tok.end, tok.value.l))
				tok.ttype = TokenType::IntegerLiteral;
			else if (is_float_literal(tok.begin, tok.end, tok.value.d))
				tok.ttype = TokenType::FloatLiteral;
			else if (is_bool_literal(tok.begin, tok.end, tok.value.b))
				tok.ttype = TokenType::BoolLiteral;
			else if (equals(u8_const_span(tok.begin, static_cast<size_t>(tok.end - tok.begin)), "None"))
				tok.ttype = TokenType::NoneLiteral;
			else
				// we could not determine this type.
				// TODO: maybe return an error code
				tok.ttype = TokenType::Unknown;
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
	get_next_token(Token &tok, RestorePoint *bpoint =nullptr)
	{
		if (bpoint != nullptr)
			*bpoint = buffer_pos;

		if (buffer_pos < buffer.size()) {
			tok = buffer[buffer_pos++];
			return Tokenizer::result::ok;
		}

		if (buffer.empty() || buffer_pos >= buffer.size()) {
			Token _tok;
			if (__fetch_token(_tok) == Tokenizer::result::ok) {
				buffer.push_back(_tok);
				tok = buffer.back();
				buffer_pos = buffer.size();
				return Tokenizer::result::ok;
			}
		}

		return Tokenizer::result::end_of_input;
	}


	/*
	 * peek at the next token without actually moving the buffer position
	 */
	bool
	peek_token(Token &tok)
	{
		// buffer already filled with the next token?
		if (buffer_pos < buffer.size()) {
			tok = buffer[buffer_pos];
			return true;
		}

		// not the case, so we need to fetch and immediately reset buffer
		// position.
		// NOTE: could also have used the restore point mechanism instead
		size_t saved = buffer_pos;
		Token tmp;
		if (get_next_token(tmp) == result::ok) {
			tok = tmp;
			buffer_pos = saved;
			return true;
		}

		// could not read, so go back
		buffer_pos = saved;
		return false;
	}


	/*
	 * peek the type of the next token
	 */
	TokenType
	peek_type()
	{
		Token tok;
		if (peek_token(tok))
			return tok.ttype;
		return TokenType::Unknown;
	}

	// backup points to memoize where the
	RestorePoint backup()             { return buffer_pos;   }
	void restore(RestorePoint bpoint) { buffer_pos = bpoint; }
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
struct PyParser
{
	enum class result : u8 {
		ok,             // parsing succeeded
		failure,        // failure parsing a context / type / object
		syntax_error,    // syntax error while parsing
		incomplete      // parsing encountered an incomplete context / type / object
	};

	/*
	 * the type of object that was parsed
	 */
	enum class Type : u8 {
		Uninitialized,  // something we don't yet know (default value)

		None,           // the none-type for the keyword "None"
		String,
		Integer,
		FloatingPoint,
		Boolean,
		KVPair,         // a key:value pair
		Tuple,          // tuples of the form (value0, value1, ...)
		List,           // lists of the form [value0, value1, ...]
		Set,            // sets of the form {value0, value1, ...}
		Dict,           // dict of the form {key0:value0, key1:value1, ...}

		Symbol,         // anything like {}[], etc. we return parse_results for
		                // symbols even though they don't specify a particular
		                // type, because we might want to extract the beginning
		                // and end of certain groups. This keeps the interface
		                // somewhat small. A parse result of type symbol should
		                // never end up directly or indirectly in the
		                // root_context. This allows to store everything the
		                // parser returns in a parse result, i.e. it will not
		                // require a different data type. Could have used an
		                // std::vector or similar container, though.

		RootContext,    // root context, contains everything that was parsed
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
	struct ParseResult {
		using ParseResultNodes = std::vector<std::unique_ptr<ParseResult>>;

		// parse status of this result / context
		result
			status {result::failure};

		// data type of this result / context
		Type
			dtype  {Type::Uninitialized};

		// where this type / context starts in the input range
		const u8*
			begin;

		// where this type / context ends
		const u8*
			end;

		// in case of a group (kvpairs, lists, etc.), this contains the group's
		// children. In case of a key-value pair, there will be 2 children:
		// first the key, then the value
		ParseResultNodes nodes;

		// for 'basic types', this contains the actual value
		union {
			i64  l;
			f64  d;
			bool b;
		} value;

		// access to the range within the input which this parse_result captures
		u8_const_span span() { return u8_const_span(begin, std::distance(begin, end)); }


		inline bool
		equals(const std::string_view &str)
		{
			return ::ncr::numpy::equals(this->begin, this->end, str);
		}
	};


	/*
	 * parse a token of a particular type Type
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <TokenType TokenType, PyParser::Type ParserType>
	std::unique_ptr<ParseResult>
	parse_token_type(Tokenizer &tokens)
	{
		Token tok;
		Tokenizer::RestorePoint rp;
		if (tokens.get_next_token(tok, &rp) == Tokenizer::result::ok && tok.ttype == TokenType) {
			auto ptr = std::make_unique<ParseResult>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == Type::Boolean)
				ptr->value.b = tok.value.b;
			if constexpr (ParserType == Type::Integer)
				ptr->value.l = tok.value.l;
			if constexpr (ParserType == Type::FloatingPoint)
				ptr->value.d = tok.value.d;

			return ptr;
		}

		tokens.restore(rp);
		return {};
	}


	/*
	 * parse a token that evalutes Fn to true.
	 *
	 * If parsing fails, the tokenizer will be reset to the token that was just
	 * read.
	 */
	template <PyParser::Type ParserType, typename F>
	std::unique_ptr<ParseResult>
	parse_token_fn(Tokenizer &tokens, F fn)
	{
		Token tok;
		Tokenizer::RestorePoint rp;
		if (tokens.get_next_token(tok, &rp) == Tokenizer::result::ok && fn(tok)) {
			auto ptr = std::make_unique<ParseResult>();
			ptr->status = result::ok;
			ptr->dtype  = ParserType;
			ptr->begin  = tok.begin;
			ptr->end    = tok.end;

			// direct value assignments
			if constexpr (ParserType == Type::Integer)
				ptr->value.l = tok.value.l;

			return ptr;
		}

		tokens.restore(rp);
		return {};
	}


	// symbols. result of these parse instructions will be ignored, but for the
	// sake of completeness, we still specify a parser type
	inline std::unique_ptr<ParseResult> parse_delimiter(Tokenizer &tokens) { return parse_token_fn<Type::Symbol>(tokens, is_delimiter);                  }
	inline std::unique_ptr<ParseResult> parse_colon(Tokenizer &tokens)     { return parse_token_type<TokenType::KVSeparator,   Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lbracket(Tokenizer &tokens)  { return parse_token_type<TokenType::LeftBracket,   Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rbracket(Tokenizer &tokens)  { return parse_token_type<TokenType::RightBracket,  Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lbrace(Tokenizer &tokens)    { return parse_token_type<TokenType::LeftBrace,     Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rbrace(Tokenizer &tokens)    { return parse_token_type<TokenType::RightBrace,    Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_lparen(Tokenizer &tokens)    { return parse_token_type<TokenType::LeftParen,     Type::Symbol>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_rparen(Tokenizer &tokens)    { return parse_token_type<TokenType::RightParen,    Type::Symbol>(tokens);  }

	// types / literals
	inline std::unique_ptr<ParseResult> parse_number(Tokenizer &tokens)    { return parse_token_fn<Type::Integer>(tokens, is_number);                    }
	inline std::unique_ptr<ParseResult> parse_string(Tokenizer &tokens)    { return parse_token_type<TokenType::StringLiteral, Type::String>(tokens);  }
	inline std::unique_ptr<ParseResult> parse_bool(Tokenizer &tokens)      { return parse_token_type<TokenType::BoolLiteral,   Type::Boolean>(tokens); }
	inline std::unique_ptr<ParseResult> parse_none(Tokenizer &tokens)      { return parse_token_type<TokenType::NoneLiteral,   Type::None>(tokens);    }


	std::unique_ptr<ParseResult>
	parse_kvpair(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		// parse the key
		auto      key = parse_string(tokens);
		if (!key) key = parse_number(tokens);
		if (!key) key = parse_tuple(tokens);
		if (!key) {
			tokens.restore(rp);
			return {};
		}

		// parse :
		if (!parse_colon(tokens)) {
			tokens.restore(rp);
			return {};
		}

		// parse the value
		auto        value = parse_none(tokens);
		if (!value) value = parse_bool(tokens);
		if (!value) value = parse_number(tokens);
		if (!value) value = parse_string(tokens);
		if (!value) value = parse_tuple(tokens);
		if (!value) value = parse_list(tokens);
		if (!value) value = parse_set(tokens);
		if (!value) value = parse_dict(tokens);
		if (!value) {
			// failed to parse kv pair, backtrack out
			tokens.restore(rp);
			return {};
		}

		// package up the result
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::ok;
		ptr->dtype = Type::KVPair;
		ptr->begin = key->begin;
		ptr->end   = value->end;
		ptr->nodes.push_back(std::move(key));
		ptr->nodes.push_back(std::move(value));
		return ptr;
	}


	std::unique_ptr<ParseResult>
	parse_tuple(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lparen = parse_lparen(tokens);
		if (!lparen) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Tuple;
		ptr->begin  = lparen->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rparen = parse_rparen(tokens);
			if (rparen) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rparen->end;
				return ptr;
			}

			// almost everything is allowed in tuples. we only care about those
			// types that we have implemented (so far), though.
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_list(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) elem = parse_dict(tokens);
			if (!elem) {
				// failed to parse tuple, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_list(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbracket = parse_lbracket(tokens);
		if (!lbracket) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::List;
		ptr->begin  = lbracket->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbracket = parse_rbracket(tokens);
			if (rbracket) {
				// finalize the package
				ptr->status = result::ok;
				ptr->end = rbracket->end;
				return ptr;
			}

			// almost everything is allowed in a list
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_list(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) elem = parse_dict(tokens);
			if (!elem) {
				// failed to parse list, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_set(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbrace = parse_lbrace(tokens);
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Set;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace(tokens);
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// allowed types in a set are all immutable and hashable types. we
			// don't support arbitrary hashable objects, so we only need to
			// check for immutable things that we know
			auto       elem = parse_none(tokens);
			if (!elem) elem = parse_bool(tokens);
			if (!elem) elem = parse_number(tokens);
			if (!elem) elem = parse_string(tokens);
			if (!elem) elem = parse_tuple(tokens);
			if (!elem) elem = parse_set(tokens);
			if (!elem) {
				// failed to parse set, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(elem));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_dict(Tokenizer &tokens)
	{
		auto rp = tokens.backup();

		auto lbrace = parse_lbrace(tokens);
		if (!lbrace) return {};

		// prepare package
		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::Dict;
		ptr->begin  = lbrace->begin;

		bool expect_delim = false;
		while (!tokens.eof()) {
			if (parse_delimiter(tokens)) {
				if (!expect_delim)
					return {};
				expect_delim = false;
				continue;
			}

			auto rbrace = parse_rbrace(tokens);
			if (rbrace) {
				// finalize package
				ptr->status = result::ok;
				ptr->end = rbrace->end;
				return ptr;
			}

			// sets are surprisingly simple beasts to tame, because they only
			// want to eat kv pairs
			auto kv_pair = parse_kvpair(tokens);
			if (!kv_pair) {
				// failed to parse dict, backtrack out
				tokens.restore(rp);
				return {};
			}
			ptr->nodes.push_back(std::move(kv_pair));
			expect_delim = true;
		}
		return {};
	}


	std::unique_ptr<ParseResult>
	parse_expression(Tokenizer &tokens)
	{
		// parse the things we know (and care about) in order they are specified
		// in python's formal grammar
		auto         result = parse_tuple(tokens);
		if (!result) result = parse_list(tokens);
		if (!result) result = parse_set(tokens);
		if (!result) result = parse_dict(tokens);

		return result;
	}


	std::unique_ptr<ParseResult>
	parse(u8_const_span input)
	{
		Tokenizer tokens{input.data(), input.data() + input.size()};

		auto ptr = std::make_unique<ParseResult>();
		ptr->status = result::incomplete;
		ptr->dtype  = Type::RootContext;
		// TODO: currently we don't store begin nor end for the root context.
		// maybe this should change

		// properly initialize the parser state
		while (!tokens.eof()) {
			auto expr = parse_expression(tokens);
			if (!expr)
				return {};
			ptr->nodes.push_back(std::move(expr));
		}
		ptr->status = result::ok;
		return ptr;
	}

	std::unique_ptr<ParseResult>
	parse(const u8_vector &input)
	{
		return parse(u8_const_span(input.data(), input.size()));
	}

};


}} // ncr::numpy

#endif /* _f03a19a69cac46f38404d117df9d9c37_ */
