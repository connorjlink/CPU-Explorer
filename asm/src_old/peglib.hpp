﻿//
//  peglib.h
//
//  Copyright (c) 2020 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <algorithm>
#include <any>
#include <cassert>
#include <cctype>
#if __has_include(<charconv>)
#include <charconv>
#endif
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace peg
{

	/*-----------------------------------------------------------------------------
	 *  scope_exit
	 *---------------------------------------------------------------------------*/

	 // This is based on
	 // "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189".

	template <typename EF> struct scope_exit {
		explicit scope_exit(EF&& f)
			: exit_function(std::move(f)), execute_on_destruction{ true } {}

		scope_exit(scope_exit&& rhs)
			: exit_function(std::move(rhs.exit_function)),
			execute_on_destruction{ rhs.execute_on_destruction } {
			rhs.release();
		}

		~scope_exit() {
			if (execute_on_destruction) { this->exit_function(); }
		}

		void release() { this->execute_on_destruction = false; }

	private:
		scope_exit(const scope_exit&) = delete;
		void operator=(const scope_exit&) = delete;
		scope_exit& operator=(scope_exit&&) = delete;

		EF exit_function;
		bool execute_on_destruction;
	};

	/*-----------------------------------------------------------------------------
	 *  UTF8 functions
	 *---------------------------------------------------------------------------*/

	inline size_t codepoint_length(const char* s8, size_t l) {
		if (l) {
			auto b = static_cast<uint8_t>(s8[0]);
			if ((b & 0x80) == 0) {
				return 1;
			}
			else if ((b & 0xE0) == 0xC0 && l >= 2) {
				return 2;
			}
			else if ((b & 0xF0) == 0xE0 && l >= 3) {
				return 3;
			}
			else if ((b & 0xF8) == 0xF0 && l >= 4) {
				return 4;
			}
		}
		return 0;
	}

	inline size_t codepoint_count(const char* s8, size_t l) {
		size_t count = 0;
		for (size_t i = 0; i < l; i += codepoint_length(s8 + i, l - i)) {
			count++;
		}
		return count;
	}

	inline size_t encode_codepoint(char32_t cp, char* buff) {
		if (cp < 0x0080) {
			buff[0] = static_cast<char>(cp & 0x7F);
			return 1;
		}
		else if (cp < 0x0800) {
			buff[0] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
			buff[1] = static_cast<char>(0x80 | (cp & 0x3F));
			return 2;
		}
		else if (cp < 0xD800) {
			buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
			buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
			return 3;
		}
		else if (cp < 0xE000) {
			// D800 - DFFF is invalid...
			return 0;
		}
		else if (cp < 0x10000) {
			buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
			buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
			return 3;
		}
		else if (cp < 0x110000) {
			buff[0] = static_cast<char>(0xF0 | ((cp >> 18) & 0x7));
			buff[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			buff[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			buff[3] = static_cast<char>(0x80 | (cp & 0x3F));
			return 4;
		}
		return 0;
	}

	inline std::string encode_codepoint(char32_t cp) {
		char buff[4];
		auto l = encode_codepoint(cp, buff);
		return std::string(buff, l);
	}

	inline bool decode_codepoint(const char* s8, size_t l, size_t& bytes,
		char32_t& cp) {
		if (l) {
			auto b = static_cast<uint8_t>(s8[0]);
			if ((b & 0x80) == 0) {
				bytes = 1;
				cp = b;
				return true;
			}
			else if ((b & 0xE0) == 0xC0) {
				if (l >= 2) {
					bytes = 2;
					cp = ((static_cast<char32_t>(s8[0] & 0x1F)) << 6) |
						(static_cast<char32_t>(s8[1] & 0x3F));
					return true;
				}
			}
			else if ((b & 0xF0) == 0xE0) {
				if (l >= 3) {
					bytes = 3;
					cp = ((static_cast<char32_t>(s8[0] & 0x0F)) << 12) |
						((static_cast<char32_t>(s8[1] & 0x3F)) << 6) |
						(static_cast<char32_t>(s8[2] & 0x3F));
					return true;
				}
			}
			else if ((b & 0xF8) == 0xF0) {
				if (l >= 4) {
					bytes = 4;
					cp = ((static_cast<char32_t>(s8[0] & 0x07)) << 18) |
						((static_cast<char32_t>(s8[1] & 0x3F)) << 12) |
						((static_cast<char32_t>(s8[2] & 0x3F)) << 6) |
						(static_cast<char32_t>(s8[3] & 0x3F));
					return true;
				}
			}
		}
		return false;
	}

	inline size_t decode_codepoint(const char* s8, size_t l, char32_t& cp) {
		size_t bytes;
		if (decode_codepoint(s8, l, bytes, cp)) { return bytes; }
		return 0;
	}

	inline char32_t decode_codepoint(const char* s8, size_t l) {
		char32_t cp = 0;
		decode_codepoint(s8, l, cp);
		return cp;
	}

	inline std::u32string decode(const char* s8, size_t l) {
		std::u32string out;
		size_t i = 0;
		while (i < l) {
			auto beg = i++;
			while (i < l && (s8[i] & 0xc0) == 0x80) {
				i++;
			}
			out += decode_codepoint(&s8[beg], (i - beg));
		}
		return out;
	}

	template <typename T> const char* u8(const T* s) {
		return reinterpret_cast<const char*>(s);
	}

	/*-----------------------------------------------------------------------------
	 *  escape_characters
	 *---------------------------------------------------------------------------*/

	inline std::string escape_characters(const char* s, size_t n) {
		std::string str;
		for (size_t i = 0; i < n; i++) {
			auto c = s[i];
			switch (c) {
			case '\n': str += "\\n"; break;
			case '\r': str += "\\r"; break;
			case '\t': str += "\\t"; break;
			default: str += c; break;
			}
		}
		return str;
	}

	inline std::string escape_characters(std::string_view sv) {
		return escape_characters(sv.data(), sv.size());
	}

	/*-----------------------------------------------------------------------------
	 *  resolve_escape_sequence
	 *---------------------------------------------------------------------------*/

	inline bool is_hex(char c, int& v) {
		if ('0' <= c && c <= '9') {
			v = c - '0';
			return true;
		}
		else if ('a' <= c && c <= 'f') {
			v = c - 'a' + 10;
			return true;
		}
		else if ('A' <= c && c <= 'F') {
			v = c - 'A' + 10;
			return true;
		}
		return false;
	}

	inline bool is_digit(char c, int& v) {
		if ('0' <= c && c <= '9') {
			v = c - '0';
			return true;
		}
		return false;
	}

	inline std::pair<int, size_t> parse_hex_number(const char* s, size_t n,
		size_t i) {
		int ret = 0;
		int val;
		while (i < n && is_hex(s[i], val)) {
			ret = static_cast<int>(ret * 16 + val);
			i++;
		}
		return std::pair(ret, i);
	}

	inline std::pair<int, size_t> parse_octal_number(const char* s, size_t n,
		size_t i) {
		int ret = 0;
		int val;
		while (i < n && is_digit(s[i], val)) {
			ret = static_cast<int>(ret * 8 + val);
			i++;
		}
		return std::pair(ret, i);
	}

	inline std::string resolve_escape_sequence(const char* s, size_t n) {
		std::string r;
		r.reserve(n);

		size_t i = 0;
		while (i < n) {
			auto ch = s[i];
			if (ch == '\\') {
				i++;
				if (i == n) { throw std::runtime_error("Invalid escape sequence..."); }
				switch (s[i]) {
				case 'n':
					r += '\n';
					i++;
					break;
				case 'r':
					r += '\r';
					i++;
					break;
				case 't':
					r += '\t';
					i++;
					break;
				case '\'':
					r += '\'';
					i++;
					break;
				case '"':
					r += '"';
					i++;
					break;
				case '[':
					r += '[';
					i++;
					break;
				case ']':
					r += ']';
					i++;
					break;
				case '\\':
					r += '\\';
					i++;
					break;
				case 'x':
				case 'u': {
					char32_t cp;
					std::tie(cp, i) = parse_hex_number(s, n, i + 1);
					r += encode_codepoint(cp);
					break;
				}
				default: {
					char32_t cp;
					std::tie(cp, i) = parse_octal_number(s, n, i);
					r += encode_codepoint(cp);
					break;
				}
				}
			}
			else {
				r += ch;
				i++;
			}
		}
		return r;
	}

	/*-----------------------------------------------------------------------------
	 *  token_to_number_ - This function should be removed eventually
	 *---------------------------------------------------------------------------*/

	template <typename T> T token_to_number_(std::string_view sv) {
		T n = 0;
#if __has_include(<charconv>)
		if constexpr (!std::is_floating_point<T>::value) {
			std::from_chars(sv.data(), sv.data() + sv.size(), n);
#else
		if constexpr (false) {
#endif
		}
		else {
			auto s = std::string(sv);
			std::istringstream ss(s);
			ss >> n;
		}
		return n;
		}

	/*-----------------------------------------------------------------------------
	 *  Trie
	 *---------------------------------------------------------------------------*/

	class Trie {
	public:
		Trie() = default;
		Trie(const Trie&) = default;

		Trie(const std::vector<std::string>& items) {
			for (const auto& item : items) {
				for (size_t len = 1; len <= item.size(); len++) {
					auto last = len == item.size();
					std::string_view sv(item.data(), len);
					auto it = dic_.find(sv);
					if (it == dic_.end()) {
						dic_.emplace(sv, Info{ last, last });
					}
					else if (last) {
						it->second.match = true;
					}
					else {
						it->second.done = false;
					}
				}
			}
		}

		size_t match(const char* text, size_t text_len) const {
			size_t match_len = 0;
			auto done = false;
			size_t len = 1;
			while (!done && len <= text_len) {
				std::string_view sv(text, len);
				auto it = dic_.find(sv);
				if (it == dic_.end()) {
					done = true;
				}
				else {
					if (it->second.match) { match_len = len; }
					if (it->second.done) { done = true; }
				}
				len += 1;
			}
			return match_len;
		}

	private:
		struct Info {
			bool done;
			bool match;
		};

		// TODO: Use unordered_map when heterogeneous lookup is supported in C++20
		// std::unordered_map<std::string, Info> dic_;
		std::map<std::string, Info, std::less<>> dic_;
	};

	/*-----------------------------------------------------------------------------
	 *  PEG
	 *---------------------------------------------------------------------------*/

	 /*
	  * Line information utility function
	  */
	inline std::pair<size_t, size_t> line_info(const char* start, const char* cur) {
		auto p = start;
		auto col_ptr = p;
		auto no = 1;

		while (p < cur) {
			if (*p == '\n') {
				no++;
				col_ptr = p + 1;
			}
			p++;
		}

		auto col = codepoint_count(col_ptr, p - col_ptr) + 1;

		return std::pair(no, col);
	}

	/*
	 * String tag
	 */
	inline constexpr unsigned int str2tag_core(const char* s, size_t l,
		unsigned int h) {
		return (l == 0) ? h
			: str2tag_core(s + 1, l - 1,
				(h * 33) ^ static_cast<unsigned char>(*s));
	}

	inline constexpr unsigned int str2tag(std::string_view sv) {
		return str2tag_core(sv.data(), sv.size(), 0);
	}

	namespace udl {

		inline constexpr unsigned int operator"" _(const char* s, size_t l) {
			return str2tag_core(s, l, 0);
		}

	} // namespace udl

	/*
	 * Semantic values
	 */
	struct SemanticValues : protected std::vector<std::any> {
		// Input text
		const char* path = nullptr;
		const char* ss = nullptr;
		std::function<const std::vector<size_t>& ()> source_line_index;

		// Matched string
		std::string_view sv() const { return sv_; }

		// Definition name
		const std::string& name() const { return name_; }

		std::vector<unsigned int> tags;

		// Line number and column at which the matched string is
		std::pair<size_t, size_t> line_info() const {
			auto& idx = source_line_index();

			auto cur = static_cast<size_t>(std::distance(ss, sv_.data()));
			auto it = std::lower_bound(
				idx.begin(), idx.end(), cur,
				[](size_t element, size_t value) { return element < value; });

			auto id = static_cast<size_t>(std::distance(idx.begin(), it));
			auto off = cur - (id == 0 ? 0 : idx[id - 1] + 1);
			return std::pair(id + 1, off + 1);
		}

		// Choice count
		size_t choice_count() const { return choice_count_; }

		// Choice number (0 based index)
		size_t choice() const { return choice_; }

		// Tokens
		std::vector<std::string_view> tokens;

		std::string_view token(size_t id = 0) const {
			if (tokens.empty()) { return sv_; }
			assert(id < tokens.size());
			return tokens[id];
		}

		// Token conversion
		std::string token_to_string(size_t id = 0) const {
			return std::string(token(id));
		}

		template <typename T> T token_to_number() const {
			return token_to_number_<T>(token());
		}

		// Transform the semantic value vector to another vector
		template <typename T>
		std::vector<T> transform(size_t beg = 0,
			size_t end = static_cast<size_t>(-1)) const {
			std::vector<T> r;
			end = (std::min)(end, size());
			for (size_t i = beg; i < end; i++) {
				r.emplace_back(std::any_cast<T>((*this)[i]));
			}
			return r;
		}

		using std::vector<std::any>::iterator;
		using std::vector<std::any>::const_iterator;
		using std::vector<std::any>::size;
		using std::vector<std::any>::empty;
		using std::vector<std::any>::assign;
		using std::vector<std::any>::begin;
		using std::vector<std::any>::end;
		using std::vector<std::any>::rbegin;
		using std::vector<std::any>::rend;
		using std::vector<std::any>::operator[];
		using std::vector<std::any>::at;
		using std::vector<std::any>::resize;
		using std::vector<std::any>::front;
		using std::vector<std::any>::back;
		using std::vector<std::any>::push_back;
		using std::vector<std::any>::pop_back;
		using std::vector<std::any>::insert;
		using std::vector<std::any>::erase;
		using std::vector<std::any>::clear;
		using std::vector<std::any>::swap;
		using std::vector<std::any>::emplace;
		using std::vector<std::any>::emplace_back;

	private:
		friend class Context;
		friend class Sequence;
		friend class PrioritizedChoice;
		friend class Holder;
		friend class PrecedenceClimbing;

		std::string_view sv_;
		size_t choice_count_ = 0;
		size_t choice_ = 0;
		std::string name_;
	};

	/*
	 * Semantic action
	 */
	template <typename F, typename... Args> std::any call(F fn, Args &&... args) {
		using R = decltype(fn(std::forward<Args>(args)...));
		if constexpr (std::is_void<R>::value) {
			fn(std::forward<Args>(args)...);
			return std::any();
		}
		else if constexpr (std::is_same<typename std::remove_cv<R>::type,
			std::any>::value) {
			return fn(std::forward<Args>(args)...);
		}
		else {
			return std::any(fn(std::forward<Args>(args)...));
		}
	}

	template <typename T>
	struct argument_count : argument_count<decltype(&T::operator())> {};
	template <typename R, typename... Args>
	struct argument_count<R(*)(Args...)>
		: std::integral_constant<unsigned, sizeof...(Args)> {};
	template <typename R, typename C, typename... Args>
	struct argument_count<R(C::*)(Args...)>
		: std::integral_constant<unsigned, sizeof...(Args)> {};
	template <typename R, typename C, typename... Args>
	struct argument_count<R(C::*)(Args...) const>
		: std::integral_constant<unsigned, sizeof...(Args)> {};

	class Action {
	public:
		Action() = default;
		Action(Action&& rhs) = default;
		template <typename F> Action(F fn) : fn_(make_adaptor(fn)) {}
		template <typename F> void operator=(F fn) { fn_ = make_adaptor(fn); }
		Action& operator=(const Action& rhs) = default;

		operator bool() const { return bool(fn_); }

		std::any operator()(SemanticValues& vs, std::any& dt) const {
			return fn_(vs, dt);
		}

	private:
		using Fty = std::function<std::any(SemanticValues& vs, std::any& dt)>;

		template <typename F> Fty make_adaptor(F fn) {
			if constexpr (argument_count<F>::value == 1) {
				return [fn](auto& vs, auto& /*dt*/) { return call(fn, vs); };
			}
			else {
				return [fn](auto& vs, auto& dt) { return call(fn, vs, dt); };
			}
		}

		Fty fn_;
	};

	/*
	 * Semantic predicate
	 */
	 // Note: 'parse_error' exception class should be be used in sematic action
	 // handlers to reject the rule.
	struct parse_error {
		parse_error() = default;
		parse_error(const char* s) : s_(s) {}
		const char* what() const { return s_.empty() ? nullptr : s_.data(); }

	private:
		std::string s_;
	};

	/*
	 * Parse result helper
	 */
	inline bool success(size_t len) { return len != static_cast<size_t>(-1); }

	inline bool fail(size_t len) { return len == static_cast<size_t>(-1); }

	/*
	 * Log
	 */
	using Log = std::function<void(size_t, size_t, const std::string&)>;

	/*
	 * ErrorInfo
	 */
	struct ErrorInfo {
		const char* error_pos = nullptr;
		std::vector<std::pair<const char*, bool>> expected_tokens;
		const char* message_pos = nullptr;
		std::string message;
		mutable const char* last_output_pos = nullptr;

		void clear() {
			error_pos = nullptr;
			expected_tokens.clear();
			message_pos = nullptr;
			message.clear();
		}

		void add(const char* token, bool is_literal) {
			for (const auto& [t, l] : expected_tokens) {
				if (t == token && l == is_literal) { return; }
			}
			expected_tokens.push_back(std::make_pair(token, is_literal));
		}

		void output_log(const Log& log, const char* s, size_t n) const {
			if (message_pos) {
				if (message_pos > last_output_pos) {
					last_output_pos = message_pos;
					auto line = line_info(s, message_pos);
					std::string msg;
					if (auto unexpected_token = heuristic_error_token(s, n, message_pos);
						!unexpected_token.empty()) {
						msg = replace_all(message, "%t", unexpected_token);

						auto unexpected_char = unexpected_token.substr(
							0, codepoint_length(unexpected_token.data(),
								unexpected_token.size()));

						msg = replace_all(msg, "%c", unexpected_char);
					}
					else {
						msg = message;
					}
					log(line.first, line.second, msg);
				}
			}
			else if (error_pos) {
				if (error_pos > last_output_pos) {
					last_output_pos = error_pos;
					auto line = line_info(s, error_pos);

					std::string msg;
					if (expected_tokens.empty()) {
						msg = "syntax error.";
					}
					else {
						msg = "syntax error";

						// unexpected token
						if (auto unexpected_token = heuristic_error_token(s, n, error_pos);
							!unexpected_token.empty()) {
							msg += ", unexpected '";
							msg += unexpected_token;
							msg += "'";
						}

						auto first_item = true;
						size_t i = 0;
						while (i < expected_tokens.size()) {
							auto [token, is_literal] =
								expected_tokens[expected_tokens.size() - i - 1];

							// Skip rules start with '_'
							if (!is_literal && token[0] != '_') {
								msg += (first_item ? ", expecting " : ", ");
								if (is_literal) {
									msg += "'";
									msg += token;
									msg += "'";
								}
								else {
									msg += "<";
									msg += token;
									msg += ">";
								}
								first_item = false;
							}

							i++;
						}
						msg += ".";
					}

					log(line.first, line.second, msg);
				}
			}
		}

	private:
		int cast_char(char c) const { return static_cast<unsigned char>(c); }

		std::string heuristic_error_token(const char* s, size_t n,
			const char* pos) const {
			auto len = n - std::distance(s, pos);
			if (len) {
				size_t i = 0;
				auto c = cast_char(pos[i++]);
				if (!std::ispunct(c) && !std::isspace(c)) {
					while (i < len && !std::ispunct(cast_char(pos[i])) &&
						!std::isspace(cast_char(pos[i]))) {
						i++;
					}
				}

				size_t count = 8;
				size_t j = 0;
				while (count > 0 && j < i) {
					j += codepoint_length(&pos[j], i - j);
					count--;
				}

				return escape_characters(pos, j);
			}
			return std::string();
		}

		std::string replace_all(std::string str, const std::string& from,
			const std::string& to) const {
			size_t pos = 0;
			while ((pos = str.find(from, pos)) != std::string::npos) {
				str.replace(pos, from.length(), to);
				pos += to.length();
			}
			return str;
		}
	};

	/*
	 * Context
	 */
	class Context;
	class Ope;
	class Definition;

	using TracerEnter = std::function<void(const Ope& name, const char* s, size_t n,
		const SemanticValues& vs,
		const Context& c, const std::any& dt)>;

	using TracerLeave = std::function<void(
		const Ope& ope, const char* s, size_t n, const SemanticValues& vs,
		const Context& c, const std::any& dt, size_t)>;

	class Context {
	public:
		const char* path;
		const char* s;
		const size_t l;
		std::vector<size_t> source_line_index;

		ErrorInfo error_info;
		bool recovered = false;

		std::vector<std::shared_ptr<SemanticValues>> value_stack;
		size_t value_stack_size = 0;

		std::vector<Definition*> rule_stack;
		std::vector<std::vector<std::shared_ptr<Ope>>> args_stack;

		size_t in_token_boundary_count = 0;

		std::shared_ptr<Ope> whitespaceOpe;
		bool in_whitespace = false;

		std::shared_ptr<Ope> wordOpe;

		std::vector<std::map<std::string_view, std::string>> capture_scope_stack;
		size_t capture_scope_stack_size = 0;

		std::vector<bool> cut_stack;

		const size_t def_count;
		const bool enablePackratParsing;
		std::vector<bool> cache_registered;
		std::vector<bool> cache_success;

		std::map<std::pair<size_t, size_t>, std::tuple<size_t, std::any>>
			cache_values;

		TracerEnter tracer_enter;
		TracerLeave tracer_leave;

		Log log;

		Context(const char* path, const char* s, size_t l, size_t def_count,
			std::shared_ptr<Ope> whitespaceOpe, std::shared_ptr<Ope> wordOpe,
			bool enablePackratParsing, TracerEnter tracer_enter,
			TracerLeave tracer_leave, Log log)
			: path(path), s(s), l(l), whitespaceOpe(whitespaceOpe), wordOpe(wordOpe),
			def_count(def_count), enablePackratParsing(enablePackratParsing),
			cache_registered(enablePackratParsing ? def_count * (l + 1) : 0),
			cache_success(enablePackratParsing ? def_count * (l + 1) : 0),
			tracer_enter(tracer_enter), tracer_leave(tracer_leave), log(log) {

			args_stack.resize(1);

			push_capture_scope();
		}

		~Context() { assert(!value_stack_size); }

		Context(const Context&) = delete;
		Context(Context&&) = delete;
		Context operator=(const Context&) = delete;

		template <typename T>
		void packrat(const char* a_s, size_t def_id, size_t& len, std::any& val,
			T fn) {
			if (!enablePackratParsing) {
				fn(val);
				return;
			}

			auto col = a_s - s;
			auto idx = def_count * static_cast<size_t>(col) + def_id;

			if (cache_registered[idx]) {
				if (cache_success[idx]) {
					auto key = std::pair(col, def_id);
					std::tie(len, val) = cache_values[key];
					return;
				}
				else {
					len = static_cast<size_t>(-1);
					return;
				}
			}
			else {
				fn(val);
				cache_registered[idx] = true;
				cache_success[idx] = success(len);
				if (success(len)) {
					auto key = std::pair(col, def_id);
					cache_values[key] = std::pair(len, val);
				}
				return;
			}
		}

		SemanticValues& push() {
			assert(value_stack_size <= value_stack.size());
			if (value_stack_size == value_stack.size()) {
				value_stack.emplace_back(std::make_shared<SemanticValues>());
			}
			else {
				auto& vs = *value_stack[value_stack_size];
				if (!vs.empty()) {
					vs.clear();
					if (!vs.tags.empty()) { vs.tags.clear(); }
				}
				vs.sv_ = std::string_view();
				vs.choice_count_ = 0;
				vs.choice_ = 0;
				if (!vs.tokens.empty()) { vs.tokens.clear(); }
			}

			auto& vs = *value_stack[value_stack_size++];
			vs.path = path;
			vs.ss = s;
			vs.source_line_index = [&]() -> const std::vector<size_t> &{
				if (source_line_index.empty()) {
					for (size_t pos = 0; pos < l; pos++) {
						if (s[pos] == '\n') { source_line_index.push_back(pos); }
					}
					source_line_index.push_back(l);
				}
				return source_line_index;
			};

			return vs;
		}

		void pop() { value_stack_size--; }

		void push_args(std::vector<std::shared_ptr<Ope>>&& args) {
			args_stack.emplace_back(args);
		}

		void pop_args() { args_stack.pop_back(); }

		const std::vector<std::shared_ptr<Ope>>& top_args() const {
			return args_stack[args_stack.size() - 1];
		}

		void push_capture_scope() {
			assert(capture_scope_stack_size <= capture_scope_stack.size());
			if (capture_scope_stack_size == capture_scope_stack.size()) {
				capture_scope_stack.emplace_back(
					std::map<std::string_view, std::string>());
			}
			else {
				auto& cs = capture_scope_stack[capture_scope_stack_size];
				if (!cs.empty()) { cs.clear(); }
			}
			capture_scope_stack_size++;
		}

		void pop_capture_scope() { capture_scope_stack_size--; }

		void shift_capture_values() {
			assert(capture_scope_stack.size() >= 2);
			auto curr = &capture_scope_stack[capture_scope_stack_size - 1];
			auto prev = curr - 1;
			for (const auto& [k, v] : *curr) {
				(*prev)[k] = v;
			}
		}

		void set_error_pos(const char* a_s, const char* literal = nullptr);

		// void trace_enter(const char *name, const char *a_s, size_t n,
		void trace_enter(const Ope& ope, const char* a_s, size_t n,
			SemanticValues& vs, std::any& dt) const;
		// void trace_leave(const char *name, const char *a_s, size_t n,
		void trace_leave(const Ope& ope, const char* a_s, size_t n,
			SemanticValues& vs, std::any& dt, size_t len) const;
		bool is_traceable(const Ope& ope) const;

		mutable size_t next_trace_id = 0;
		mutable std::list<size_t> trace_ids;
	};

	/*
	 * Parser operators
	 */
	class Ope {
	public:
		struct Visitor;

		virtual ~Ope() = default;
		size_t parse(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const;
		virtual size_t parse_core(const char* s, size_t n, SemanticValues& vs,
			Context& c, std::any& dt) const = 0;
		virtual void accept(Visitor& v) = 0;
	};

	class Sequence : public Ope {
	public:
		template <typename... Args>
		Sequence(const Args &... args)
			: opes_{ static_cast<std::shared_ptr<Ope>>(args)... } {}
		Sequence(const std::vector<std::shared_ptr<Ope>>& opes) : opes_(opes) {}
		Sequence(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(opes) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			auto& chldsv = c.push();
			auto pop_se = scope_exit([&]() { c.pop(); });
			size_t i = 0;
			for (const auto& ope : opes_) {
				const auto& rule = *ope;
				auto len = rule.parse(s + i, n - i, chldsv, c, dt);
				if (fail(len)) { return len; }
				i += len;
			}
			if (!chldsv.empty()) {
				for (size_t j = 0; j < chldsv.size(); j++) {
					vs.emplace_back(std::move(chldsv[j]));
				}
			}
			if (!chldsv.tags.empty()) {
				for (size_t j = 0; j < chldsv.tags.size(); j++) {
					vs.tags.emplace_back(std::move(chldsv.tags[j]));
				}
			}
			vs.sv_ = chldsv.sv_;
			if (!chldsv.tokens.empty()) {
				for (size_t j = 0; j < chldsv.tokens.size(); j++) {
					vs.tokens.emplace_back(std::move(chldsv.tokens[j]));
				}
			}
			return i;
		}

		void accept(Visitor& v) override;

		std::vector<std::shared_ptr<Ope>> opes_;
	};

	class PrioritizedChoice : public Ope {
	public:
		template <typename... Args>
		PrioritizedChoice(bool for_label, const Args &... args)
			: opes_{ static_cast<std::shared_ptr<Ope>>(args)... },
			for_label_(for_label) {}
		PrioritizedChoice(const std::vector<std::shared_ptr<Ope>>& opes)
			: opes_(opes) {}
		PrioritizedChoice(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(opes) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			size_t len = static_cast<size_t>(-1);

			if (!for_label_) { c.cut_stack.push_back(false); }

			size_t id = 0;
			for (const auto& ope : opes_) {
				if (!c.cut_stack.empty()) { c.cut_stack.back() = false; }

				auto& chldsv = c.push();
				c.push_capture_scope();

				auto se = scope_exit([&]() {
					c.pop();
					c.pop_capture_scope();
					});

				len = ope->parse(s, n, chldsv, c, dt);

				if (success(len)) {
					if (!chldsv.empty()) {
						for (size_t i = 0; i < chldsv.size(); i++) {
							vs.emplace_back(std::move(chldsv[i]));
						}
					}
					if (!chldsv.tags.empty()) {
						for (size_t i = 0; i < chldsv.tags.size(); i++) {
							vs.tags.emplace_back(std::move(chldsv.tags[i]));
						}
					}
					vs.sv_ = chldsv.sv_;
					vs.choice_count_ = opes_.size();
					vs.choice_ = id;
					if (!chldsv.tokens.empty()) {
						for (size_t i = 0; i < chldsv.tokens.size(); i++) {
							vs.tokens.emplace_back(std::move(chldsv.tokens[i]));
						}
					}
					c.shift_capture_values();
					break;
				}
				else if (!c.cut_stack.empty() && c.cut_stack.back()) {
					break;
				}

				id++;
			}

			if (!for_label_) { c.cut_stack.pop_back(); }

			return len;
		}

		void accept(Visitor& v) override;

		size_t size() const { return opes_.size(); }

		std::vector<std::shared_ptr<Ope>> opes_;
		bool for_label_ = false;
	};

	class Repetition : public Ope {
	public:
		Repetition(const std::shared_ptr<Ope>& ope, size_t min, size_t max)
			: ope_(ope), min_(min), max_(max) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			size_t count = 0;
			size_t i = 0;
			while (count < min_) {
				c.push_capture_scope();
				auto se = scope_exit([&]() { c.pop_capture_scope(); });
				const auto& rule = *ope_;
				auto len = rule.parse(s + i, n - i, vs, c, dt);
				if (success(len)) {
					c.shift_capture_values();
				}
				else {
					return len;
				}
				i += len;
				count++;
			}

			while (n - i > 0 && count < max_) {
				c.push_capture_scope();
				auto se = scope_exit([&]() { c.pop_capture_scope(); });
				auto save_sv_size = vs.size();
				auto save_tok_size = vs.tokens.size();
				const auto& rule = *ope_;
				auto len = rule.parse(s + i, n - i, vs, c, dt);
				if (success(len)) {
					c.shift_capture_values();
				}
				else {
					if (vs.size() != save_sv_size) {
						vs.erase(vs.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
						vs.tags.erase(vs.tags.begin() +
							static_cast<std::ptrdiff_t>(save_sv_size));
					}
					if (vs.tokens.size() != save_tok_size) {
						vs.tokens.erase(vs.tokens.begin() +
							static_cast<std::ptrdiff_t>(save_tok_size));
					}
					break;
				}
				i += len;
				count++;
			}
			return i;
		}

		void accept(Visitor& v) override;

		bool is_zom() const {
			return min_ == 0 && max_ == std::numeric_limits<size_t>::max();
		}

		static std::shared_ptr<Repetition> zom(const std::shared_ptr<Ope>& ope) {
			return std::make_shared<Repetition>(ope, 0,
				std::numeric_limits<size_t>::max());
		}

		static std::shared_ptr<Repetition> oom(const std::shared_ptr<Ope>& ope) {
			return std::make_shared<Repetition>(ope, 1,
				std::numeric_limits<size_t>::max());
		}

		static std::shared_ptr<Repetition> opt(const std::shared_ptr<Ope>& ope) {
			return std::make_shared<Repetition>(ope, 0, 1);
		}

		std::shared_ptr<Ope> ope_;
		size_t min_;
		size_t max_;
	};

	class AndPredicate : public Ope {
	public:
		AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& dt) const override {
			auto& chldsv = c.push();
			c.push_capture_scope();
			auto se = scope_exit([&]() {
				c.pop();
				c.pop_capture_scope();
				});
			const auto& rule = *ope_;
			auto len = rule.parse(s, n, chldsv, c, dt);
			if (success(len)) {
				return 0;
			}
			else {
				return len;
			}
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class NotPredicate : public Ope {
	public:
		NotPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& dt) const override {
			auto& chldsv = c.push();
			c.push_capture_scope();
			auto se = scope_exit([&]() {
				c.pop();
				c.pop_capture_scope();
				});
			auto len = ope_->parse(s, n, chldsv, c, dt);
			if (success(len)) {
				c.set_error_pos(s);
				return static_cast<size_t>(-1);
			}
			else {
				return 0;
			}
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class Dictionary : public Ope, public std::enable_shared_from_this<Dictionary> {
	public:
		Dictionary(const std::vector<std::string>& v) : trie_(v) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		Trie trie_;
	};

	class LiteralString : public Ope,
		public std::enable_shared_from_this<LiteralString> {
	public:
		LiteralString(std::string&& s, bool ignore_case)
			: lit_(s), ignore_case_(ignore_case), is_word_(false) {}

		LiteralString(const std::string& s, bool ignore_case)
			: lit_(s), ignore_case_(ignore_case), is_word_(false) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::string lit_;
		bool ignore_case_;
		mutable std::once_flag init_is_word_;
		mutable bool is_word_;
	};

	class CharacterClass : public Ope,
		public std::enable_shared_from_this<CharacterClass> {
	public:
		CharacterClass(const std::string& s, bool negated) : negated_(negated) {
			auto chars = decode(s.data(), s.length());
			auto i = 0u;
			while (i < chars.size()) {
				if (i + 2 < chars.size() && chars[i + 1] == '-') {
					auto cp1 = chars[i];
					auto cp2 = chars[i + 2];
					ranges_.emplace_back(std::pair(cp1, cp2));
					i += 3;
				}
				else {
					auto cp = chars[i];
					ranges_.emplace_back(std::pair(cp, cp));
					i += 1;
				}
			}
			assert(!ranges_.empty());
		}

		CharacterClass(const std::vector<std::pair<char32_t, char32_t>>& ranges,
			bool negated)
			: ranges_(ranges), negated_(negated) {
			assert(!ranges_.empty());
		}

		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& /*dt*/) const override {
			if (n < 1) {
				c.set_error_pos(s);
				return static_cast<size_t>(-1);
			}

			char32_t cp = 0;
			auto len = decode_codepoint(s, n, cp);

			for (const auto& range : ranges_) {
				if (range.first <= cp && cp <= range.second) {
					if (negated_) {
						c.set_error_pos(s);
						return static_cast<size_t>(-1);
					}
					else {
						return len;
					}
				}
			}

			if (negated_) {
				return len;
			}
			else {
				c.set_error_pos(s);
				return static_cast<size_t>(-1);
			}
		}

		void accept(Visitor& v) override;

		std::vector<std::pair<char32_t, char32_t>> ranges_;
		bool negated_;
	};

	class Character : public Ope, public std::enable_shared_from_this<Character> {
	public:
		Character(char ch) : ch_(ch) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& /*dt*/) const override {
			if (n < 1 || s[0] != ch_) {
				c.set_error_pos(s);
				return static_cast<size_t>(-1);
			}
			return 1;
		}

		void accept(Visitor& v) override;

		char ch_;
	};

	class AnyCharacter : public Ope,
		public std::enable_shared_from_this<AnyCharacter> {
	public:
		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& /*dt*/) const override {
			auto len = codepoint_length(s, n);
			if (len < 1) {
				c.set_error_pos(s);
				return static_cast<size_t>(-1);
			}
			return len;
		}

		void accept(Visitor& v) override;
	};

	class CaptureScope : public Ope {
	public:
		CaptureScope(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			c.push_capture_scope();
			auto se = scope_exit([&]() { c.pop_capture_scope(); });
			const auto& rule = *ope_;
			auto len = rule.parse(s, n, vs, c, dt);
			return len;
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class Capture : public Ope {
	public:
		using MatchAction = std::function<void(const char* s, size_t n, Context& c)>;

		Capture(const std::shared_ptr<Ope>& ope, MatchAction ma)
			: ope_(ope), match_action_(ma) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			const auto& rule = *ope_;
			auto len = rule.parse(s, n, vs, c, dt);
			if (success(len) && match_action_) { match_action_(s, len, c); }
			return len;
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
		MatchAction match_action_;
	};

	class TokenBoundary : public Ope {
	public:
		TokenBoundary(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class Ignore : public Ope {
	public:
		Ignore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& /*vs*/,
			Context& c, std::any& dt) const override {
			const auto& rule = *ope_;
			auto& chldsv = c.push();
			auto se = scope_exit([&]() { c.pop(); });
			return rule.parse(s, n, chldsv, c, dt);
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	using Parser = std::function<size_t(const char* s, size_t n, SemanticValues& vs,
		std::any& dt)>;

	class User : public Ope {
	public:
		User(Parser fn) : fn_(fn) {}
		size_t parse_core(const char* s, size_t n, SemanticValues& vs,
			Context& /*c*/, std::any& dt) const override {
			assert(fn_);
			return fn_(s, n, vs, dt);
		}
		void accept(Visitor& v) override;
		std::function<size_t(const char* s, size_t n, SemanticValues& vs,
			std::any& dt)>
			fn_;
	};

	class WeakHolder : public Ope {
	public:
		WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			auto ope = weak_.lock();
			assert(ope);
			const auto& rule = *ope;
			return rule.parse(s, n, vs, c, dt);
		}

		void accept(Visitor& v) override;

		std::weak_ptr<Ope> weak_;
	};

	class Holder : public Ope {
	public:
		Holder(Definition* outer) : outer_(outer) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::any reduce(SemanticValues& vs, std::any& dt) const;

		const char* trace_name() const;

		std::shared_ptr<Ope> ope_;
		Definition* outer_;
		mutable std::string trace_name_;

		friend class Definition;
	};

	using Grammar = std::unordered_map<std::string, Definition>;

	class Reference : public Ope, public std::enable_shared_from_this<Reference> {
	public:
		Reference(const Grammar& grammar, const std::string& name, const char* s,
			bool is_macro, const std::vector<std::shared_ptr<Ope>>& args)
			: grammar_(grammar), name_(name), s_(s), is_macro_(is_macro), args_(args),
			rule_(nullptr), iarg_(0) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> get_core_operator() const;

		const Grammar& grammar_;
		const std::string name_;
		const char* s_;

		const bool is_macro_;
		const std::vector<std::shared_ptr<Ope>> args_;

		Definition* rule_;
		size_t iarg_;
	};

	class Whitespace : public Ope {
	public:
		Whitespace(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			if (c.in_whitespace) { return 0; }
			c.in_whitespace = true;
			auto se = scope_exit([&]() { c.in_whitespace = false; });
			const auto& rule = *ope_;
			return rule.parse(s, n, vs, c, dt);
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class BackReference : public Ope {
	public:
		BackReference(std::string&& name) : name_(name) {}

		BackReference(const std::string& name) : name_(name) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::string name_;
	};

	class PrecedenceClimbing : public Ope {
	public:
		using BinOpeInfo = std::map<std::string_view, std::pair<size_t, char>>;

		PrecedenceClimbing(const std::shared_ptr<Ope>& atom,
			const std::shared_ptr<Ope>& binop, const BinOpeInfo& info,
			const Definition& rule)
			: atom_(atom), binop_(binop), info_(info), rule_(rule) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override {
			return parse_expression(s, n, vs, c, dt, 0);
		}

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> atom_;
		std::shared_ptr<Ope> binop_;
		BinOpeInfo info_;
		const Definition& rule_;

	private:
		size_t parse_expression(const char* s, size_t n, SemanticValues& vs,
			Context& c, std::any& dt, size_t min_prec) const;

		Definition& get_reference_for_binop(Context& c) const;
	};

	class Recovery : public Ope {
	public:
		Recovery(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

		size_t parse_core(const char* s, size_t n, SemanticValues& vs, Context& c,
			std::any& dt) const override;

		void accept(Visitor& v) override;

		std::shared_ptr<Ope> ope_;
	};

	class Cut : public Ope, public std::enable_shared_from_this<Cut> {
	public:
		size_t parse_core(const char* /*s*/, size_t /*n*/, SemanticValues& /*vs*/,
			Context& c, std::any& /*dt*/) const override {
			c.cut_stack.back() = true;
			return 0;
		}

		void accept(Visitor& v) override;
	};

	/*
	 * Factories
	 */
	template <typename... Args> std::shared_ptr<Ope> seq(Args &&... args) {
		return std::make_shared<Sequence>(static_cast<std::shared_ptr<Ope>>(args)...);
	}

	template <typename... Args> std::shared_ptr<Ope> cho(Args &&... args) {
		return std::make_shared<PrioritizedChoice>(
			false, static_cast<std::shared_ptr<Ope>>(args)...);
	}

	template <typename... Args> std::shared_ptr<Ope> cho4label_(Args &&... args) {
		return std::make_shared<PrioritizedChoice>(
			true, static_cast<std::shared_ptr<Ope>>(args)...);
	}

	inline std::shared_ptr<Ope> zom(const std::shared_ptr<Ope> &ope) {
		return Repetition::zom(ope);
	}

	inline std::shared_ptr<Ope> oom(const std::shared_ptr<Ope> &ope) {
		return Repetition::oom(ope);
	}

	inline std::shared_ptr<Ope> opt(const std::shared_ptr<Ope> &ope) {
		return Repetition::opt(ope);
	}

	inline std::shared_ptr<Ope> rep(const std::shared_ptr<Ope> &ope, size_t min,
		size_t max) {
		return std::make_shared<Repetition>(ope, min, max);
	}

	inline std::shared_ptr<Ope> apd(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<AndPredicate>(ope);
	}

	inline std::shared_ptr<Ope> npd(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<NotPredicate>(ope);
	}

	inline std::shared_ptr<Ope> dic(const std::vector<std::string> &v) {
		return std::make_shared<Dictionary>(v);
	}

	inline std::shared_ptr<Ope> lit(std::string && s) {
		return std::make_shared<LiteralString>(s, false);
	}

	inline std::shared_ptr<Ope> liti(std::string && s) {
		return std::make_shared<LiteralString>(s, true);
	}

	inline std::shared_ptr<Ope> cls(const std::string & s) {
		return std::make_shared<CharacterClass>(s, false);
	}

	inline std::shared_ptr<Ope>
		cls(const std::vector<std::pair<char32_t, char32_t>> &ranges) {
		return std::make_shared<CharacterClass>(ranges, false);
	}

	inline std::shared_ptr<Ope> ncls(const std::string & s) {
		return std::make_shared<CharacterClass>(s, true);
	}

	inline std::shared_ptr<Ope>
		ncls(const std::vector<std::pair<char32_t, char32_t>> &ranges) {
		return std::make_shared<CharacterClass>(ranges, true);
	}

	inline std::shared_ptr<Ope> chr(char dt) {
		return std::make_shared<Character>(dt);
	}

	inline std::shared_ptr<Ope> dot() { return std::make_shared<AnyCharacter>(); }

	inline std::shared_ptr<Ope> csc(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<CaptureScope>(ope);
	}

	inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope> &ope,
		Capture::MatchAction ma) {
		return std::make_shared<Capture>(ope, ma);
	}

	inline std::shared_ptr<Ope> tok(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<TokenBoundary>(ope);
	}

	inline std::shared_ptr<Ope> ign(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<Ignore>(ope);
	}

	inline std::shared_ptr<Ope>
		usr(std::function<size_t(const char* s, size_t n, SemanticValues& vs,
			std::any& dt)>
			fn) {
		return std::make_shared<User>(fn);
	}

	inline std::shared_ptr<Ope> ref(const Grammar & grammar, const std::string & name,
		const char* s, bool is_macro,
		const std::vector<std::shared_ptr<Ope>> &args) {
		return std::make_shared<Reference>(grammar, name, s, is_macro, args);
	}

	inline std::shared_ptr<Ope> wsp(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<Whitespace>(std::make_shared<Ignore>(ope));
	}

	inline std::shared_ptr<Ope> bkr(std::string && name) {
		return std::make_shared<BackReference>(name);
	}

	inline std::shared_ptr<Ope> pre(const std::shared_ptr<Ope> &atom,
		const std::shared_ptr<Ope> &binop,
		const PrecedenceClimbing::BinOpeInfo & info,
		const Definition & rule) {
		return std::make_shared<PrecedenceClimbing>(atom, binop, info, rule);
	}

	inline std::shared_ptr<Ope> rec(const std::shared_ptr<Ope> &ope) {
		return std::make_shared<Recovery>(ope);
	}

	inline std::shared_ptr<Ope> cut() { return std::make_shared<Cut>(); }

	/*
	 * Visitor
	 */
	struct Ope::Visitor {
		virtual ~Visitor() {}
		virtual void visit(Sequence&) {}
		virtual void visit(PrioritizedChoice&) {}
		virtual void visit(Repetition&) {}
		virtual void visit(AndPredicate&) {}
		virtual void visit(NotPredicate&) {}
		virtual void visit(Dictionary&) {}
		virtual void visit(LiteralString&) {}
		virtual void visit(CharacterClass&) {}
		virtual void visit(Character&) {}
		virtual void visit(AnyCharacter&) {}
		virtual void visit(CaptureScope&) {}
		virtual void visit(Capture&) {}
		virtual void visit(TokenBoundary&) {}
		virtual void visit(Ignore&) {}
		virtual void visit(User&) {}
		virtual void visit(WeakHolder&) {}
		virtual void visit(Holder&) {}
		virtual void visit(Reference&) {}
		virtual void visit(Whitespace&) {}
		virtual void visit(BackReference&) {}
		virtual void visit(PrecedenceClimbing&) {}
		virtual void visit(Recovery&) {}
		virtual void visit(Cut&) {}
	};

	struct IsReference : public Ope::Visitor {
		void visit(Reference&) override { is_reference_ = true; }

		static bool check(Ope& ope) {
			IsReference vis;
			ope.accept(vis);
			return vis.is_reference_;
		}

	private:
		bool is_reference_ = false;
	};

	struct TraceOpeName : public Ope::Visitor {
		void visit(Sequence&) override { name_ = "Sequence"; }
		void visit(PrioritizedChoice&) override { name_ = "PrioritizedChoice"; }
		void visit(Repetition&) override { name_ = "Repetition"; }
		void visit(AndPredicate&) override { name_ = "AndPredicate"; }
		void visit(NotPredicate&) override { name_ = "NotPredicate"; }
		void visit(Dictionary&) override { name_ = "Dictionary"; }
		void visit(LiteralString&) override { name_ = "LiteralString"; }
		void visit(CharacterClass&) override { name_ = "CharacterClass"; }
		void visit(Character&) override { name_ = "Character"; }
		void visit(AnyCharacter&) override { name_ = "AnyCharacter"; }
		void visit(CaptureScope&) override { name_ = "CaptureScope"; }
		void visit(Capture&) override { name_ = "Capture"; }
		void visit(TokenBoundary&) override { name_ = "TokenBoundary"; }
		void visit(Ignore&) override { name_ = "Ignore"; }
		void visit(User&) override { name_ = "User"; }
		void visit(WeakHolder&) override { name_ = "WeakHolder"; }
		void visit(Holder& ope) override { name_ = ope.trace_name(); }
		void visit(Reference&) override { name_ = "Reference"; }
		void visit(Whitespace&) override { name_ = "Whitespace"; }
		void visit(BackReference&) override { name_ = "BackReference"; }
		void visit(PrecedenceClimbing&) override { name_ = "PrecedenceClimbing"; }
		void visit(Recovery&) override { name_ = "Recovery"; }
		void visit(Cut&) override { name_ = "Cut"; }

		static std::string get(Ope& ope) {
			TraceOpeName vis;
			ope.accept(vis);
			return vis.name_;
		}

	private:
		const char* name_ = nullptr;
	};

	struct AssignIDToDefinition : public Ope::Visitor {
		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(Repetition& ope) override { ope.ope_->accept(*this); }
		void visit(AndPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(NotPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override;
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override;
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		std::unordered_map<void*, size_t> ids;
	};

	struct IsLiteralToken : public Ope::Visitor {
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				if (!IsLiteralToken::check(*op)) { return; }
			}
			result_ = true;
		}

		void visit(Dictionary&) override { result_ = true; }
		void visit(LiteralString&) override { result_ = true; }

		static bool check(Ope& ope) {
			IsLiteralToken vis;
			ope.accept(vis);
			return vis.result_;
		}

	private:
		bool result_ = false;
	};

	struct TokenChecker : public Ope::Visitor {
		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(Repetition& ope) override { ope.ope_->accept(*this); }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary&) override { has_token_boundary_ = true; }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder&) override { has_rule_ = true; }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		static bool is_token(Ope& ope) {
			if (IsLiteralToken::check(ope)) { return true; }

			TokenChecker vis;
			ope.accept(vis);
			return vis.has_token_boundary_ || !vis.has_rule_;
		}

	private:
		bool has_token_boundary_ = false;
		bool has_rule_ = false;
	};

	struct FindLiteralToken : public Ope::Visitor {
		void visit(LiteralString& ope) override { token_ = ope.lit_.c_str(); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		static const char* token(Ope& ope) {
			FindLiteralToken vis;
			ope.accept(vis);
			return vis.token_;
		}

	private:
		const char* token_ = nullptr;
	};

	struct DetectLeftRecursion : public Ope::Visitor {
		DetectLeftRecursion(const std::string& name) : name_(name) {}

		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (done_) {
					break;
				}
				else if (error_s) {
					done_ = true;
					break;
				}
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (error_s) {
					done_ = true;
					break;
				}
			}
		}
		void visit(Repetition& ope) override {
			ope.ope_->accept(*this);
			done_ = ope.min_ > 0;
		}
		void visit(AndPredicate& ope) override {
			ope.ope_->accept(*this);
			done_ = false;
		}
		void visit(NotPredicate& ope) override {
			ope.ope_->accept(*this);
			done_ = false;
		}
		void visit(Dictionary&) override { done_ = true; }
		void visit(LiteralString& ope) override { done_ = !ope.lit_.empty(); }
		void visit(CharacterClass&) override { done_ = true; }
		void visit(Character&) override { done_ = true; }
		void visit(AnyCharacter&) override { done_ = true; }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(User&) override { done_ = true; }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(BackReference&) override { done_ = true; }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }
		void visit(Cut&) override { done_ = true; }

		const char* error_s = nullptr;

	private:
		std::string name_;
		std::set<std::string> refs_;
		bool done_ = false;
	};

	struct HasEmptyElement : public Ope::Visitor {
		HasEmptyElement(std::list<std::pair<const char*, std::string>>& refs)
			: refs_(refs) {}

		void visit(Sequence& ope) override {
			auto save_is_empty = false;
			const char* save_error_s = nullptr;
			std::string save_error_name;
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (!is_empty) { return; }
				save_is_empty = is_empty;
				save_error_s = error_s;
				save_error_name = error_name;
				is_empty = false;
				error_name.clear();
			}
			is_empty = save_is_empty;
			error_s = save_error_s;
			error_name = save_error_name;
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (is_empty) { return; }
			}
		}
		void visit(Repetition& ope) override {
			if (ope.min_ == 0) {
				set_error();
			}
			else {
				ope.ope_->accept(*this);
			}
		}
		void visit(AndPredicate&) override { set_error(); }
		void visit(NotPredicate&) override { set_error(); }
		void visit(LiteralString& ope) override {
			if (ope.lit_.empty()) { set_error(); }
		}
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		bool is_empty = false;
		const char* error_s = nullptr;
		std::string error_name;

	private:
		void set_error() {
			is_empty = true;
			tie(error_s, error_name) = refs_.back();
		}
		std::list<std::pair<const char*, std::string>>& refs_;
	};

	struct DetectInfiniteLoop : public Ope::Visitor {
		DetectInfiniteLoop(const char* s, const std::string& name) {
			refs_.emplace_back(s, name);
		}

		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (has_error) { return; }
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
				if (has_error) { return; }
			}
		}
		void visit(Repetition& ope) override {
			if (ope.max_ == std::numeric_limits<size_t>::max()) {
				HasEmptyElement vis(refs_);
				ope.ope_->accept(vis);
				if (vis.is_empty) {
					has_error = true;
					error_s = vis.error_s;
					error_name = vis.error_name;
				}
			}
			else {
				ope.ope_->accept(*this);
			}
		}
		void visit(AndPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(NotPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		bool has_error = false;
		const char* error_s = nullptr;
		std::string error_name;

	private:
		std::list<std::pair<const char*, std::string>> refs_;
	};

	struct ReferenceChecker : public Ope::Visitor {
		ReferenceChecker(const Grammar& grammar,
			const std::vector<std::string>& params)
			: grammar_(grammar), params_(params) {}

		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(Repetition& ope) override { ope.ope_->accept(*this); }
		void visit(AndPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(NotPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

		std::unordered_map<std::string, const char*> error_s;
		std::unordered_map<std::string, std::string> error_message;
		std::unordered_set<std::string> referenced;

	private:
		const Grammar& grammar_;
		const std::vector<std::string>& params_;
	};

	struct LinkReferences : public Ope::Visitor {
		LinkReferences(Grammar& grammar, const std::vector<std::string>& params)
			: grammar_(grammar), params_(params) {}

		void visit(Sequence& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(PrioritizedChoice& ope) override {
			for (auto op : ope.opes_) {
				op->accept(*this);
			}
		}
		void visit(Repetition& ope) override { ope.ope_->accept(*this); }
		void visit(AndPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(NotPredicate& ope) override { ope.ope_->accept(*this); }
		void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
		void visit(Capture& ope) override { ope.ope_->accept(*this); }
		void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
		void visit(Ignore& ope) override { ope.ope_->accept(*this); }
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
		void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
		void visit(Recovery& ope) override { ope.ope_->accept(*this); }

	private:
		Grammar& grammar_;
		const std::vector<std::string>& params_;
	};

	struct FindReference : public Ope::Visitor {
		FindReference(const std::vector<std::shared_ptr<Ope>>& args,
			const std::vector<std::string>& params)
			: args_(args), params_(params) {}

		void visit(Sequence& ope) override {
			std::vector<std::shared_ptr<Ope>> opes;
			for (auto o : ope.opes_) {
				o->accept(*this);
				opes.push_back(found_ope);
			}
			found_ope = std::make_shared<Sequence>(opes);
		}
		void visit(PrioritizedChoice& ope) override {
			std::vector<std::shared_ptr<Ope>> opes;
			for (auto o : ope.opes_) {
				o->accept(*this);
				opes.push_back(found_ope);
			}
			found_ope = std::make_shared<PrioritizedChoice>(opes);
		}
		void visit(Repetition& ope) override {
			ope.ope_->accept(*this);
			found_ope = rep(found_ope, ope.min_, ope.max_);
		}
		void visit(AndPredicate& ope) override {
			ope.ope_->accept(*this);
			found_ope = apd(found_ope);
		}
		void visit(NotPredicate& ope) override {
			ope.ope_->accept(*this);
			found_ope = npd(found_ope);
		}
		void visit(Dictionary& ope) override { found_ope = ope.shared_from_this(); }
		void visit(LiteralString& ope) override {
			found_ope = ope.shared_from_this();
		}
		void visit(CharacterClass& ope) override {
			found_ope = ope.shared_from_this();
		}
		void visit(Character& ope) override { found_ope = ope.shared_from_this(); }
		void visit(AnyCharacter& ope) override { found_ope = ope.shared_from_this(); }
		void visit(CaptureScope& ope) override {
			ope.ope_->accept(*this);
			found_ope = csc(found_ope);
		}
		void visit(Capture& ope) override {
			ope.ope_->accept(*this);
			found_ope = cap(found_ope, ope.match_action_);
		}
		void visit(TokenBoundary& ope) override {
			ope.ope_->accept(*this);
			found_ope = tok(found_ope);
		}
		void visit(Ignore& ope) override {
			ope.ope_->accept(*this);
			found_ope = ign(found_ope);
		}
		void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
		void visit(Holder& ope) override { ope.ope_->accept(*this); }
		void visit(Reference& ope) override;
		void visit(Whitespace& ope) override {
			ope.ope_->accept(*this);
			found_ope = wsp(found_ope);
		}
		void visit(PrecedenceClimbing& ope) override {
			ope.atom_->accept(*this);
			found_ope = csc(found_ope);
		}
		void visit(Recovery& ope) override {
			ope.ope_->accept(*this);
			found_ope = rec(found_ope);
		}
		void visit(Cut& ope) override { found_ope = ope.shared_from_this(); }

		std::shared_ptr<Ope> found_ope;

	private:
		const std::vector<std::shared_ptr<Ope>>& args_;
		const std::vector<std::string>& params_;
	};

	struct IsPrioritizedChoice : public Ope::Visitor {
		void visit(PrioritizedChoice&) override { result_ = true; }

		static bool check(Ope& ope) {
			IsPrioritizedChoice vis;
			ope.accept(vis);
			return vis.result_;
		}

	private:
		bool result_ = false;
	};

	/*
	 * Keywords
	 */
	static const char* WHITESPACE_DEFINITION_NAME = "%whitespace";
	static const char* WORD_DEFINITION_NAME = "%word";
	static const char* RECOVER_DEFINITION_NAME = "%recover";

	/*
	 * Definition
	 */
	class Definition {
	public:
		struct Result {
			bool ret;
			bool recovered;
			size_t len;
			ErrorInfo error_info;
		};

		Definition() : holder_(std::make_shared<Holder>(this)) {}

		Definition(const Definition& rhs) : name(rhs.name), holder_(rhs.holder_) {
			holder_->outer_ = this;
		}

		Definition(const std::shared_ptr<Ope>& ope)
			: holder_(std::make_shared<Holder>(this)) {
			*this <= ope;
		}

		operator std::shared_ptr<Ope>() {
			return std::make_shared<WeakHolder>(holder_);
		}

		Definition& operator<=(const std::shared_ptr<Ope>& ope) {
			holder_->ope_ = ope;
			return *this;
		}

		Result parse(const char* s, size_t n, const char* path = nullptr,
			Log log = nullptr) const {
			SemanticValues vs;
			std::any dt;
			return parse_core(s, n, vs, dt, path, log);
		}

		Result parse(const char* s, const char* path = nullptr,
			Log log = nullptr) const {
			auto n = strlen(s);
			return parse(s, n, path, log);
		}

		Result parse(const char* s, size_t n, std::any& dt,
			const char* path = nullptr, Log log = nullptr) const {
			SemanticValues vs;
			return parse_core(s, n, vs, dt, path, log);
		}

		Result parse(const char* s, std::any& dt, const char* path = nullptr,
			Log log = nullptr) const {
			auto n = strlen(s);
			return parse(s, n, dt, path, log);
		}

		template <typename T>
		Result parse_and_get_value(const char* s, size_t n, T& val,
			const char* path = nullptr,
			Log log = nullptr) const {
			SemanticValues vs;
			std::any dt;
			auto r = parse_core(s, n, vs, dt, path, log);
			if (r.ret && !vs.empty() && vs.front().has_value()) {
				val = std::any_cast<T>(vs[0]);
			}
			return r;
		}

		template <typename T>
		Result parse_and_get_value(const char* s, T& val, const char* path = nullptr,
			Log log = nullptr) const {
			auto n = strlen(s);
			return parse_and_get_value(s, n, val, path, log);
		}

		template <typename T>
		Result parse_and_get_value(const char* s, size_t n, std::any& dt, T& val,
			const char* path = nullptr,
			Log log = nullptr) const {
			SemanticValues vs;
			auto r = parse_core(s, n, vs, dt, path, log);
			if (r.ret && !vs.empty() && vs.front().has_value()) {
				val = std::any_cast<T>(vs[0]);
			}
			return r;
		}

		template <typename T>
		Result parse_and_get_value(const char* s, std::any& dt, T& val,
			const char* path = nullptr,
			Log log = nullptr) const {
			auto n = strlen(s);
			return parse_and_get_value(s, n, dt, val, path, log);
		}

		void operator=(Action a) { action = a; }

		template <typename T> Definition& operator,(T fn) {
			operator=(fn);
			return *this;
		}

		Definition& operator~() {
			ignoreSemanticValue = true;
			return *this;
		}

		void accept(Ope::Visitor& v) { holder_->accept(v); }

		std::shared_ptr<Ope> get_core_operator() const { return holder_->ope_; }

		bool is_token() const {
			std::call_once(is_token_init_, [this]() {
				is_token_ = TokenChecker::is_token(*get_core_operator());
				});
			return is_token_;
		}

		std::string name;
		const char* s_ = nullptr;

		size_t id = 0;
		Action action;
		std::function<void(const char* s, size_t n, std::any& dt)> enter;
		std::function<void(const char* s, size_t n, size_t matchlen, std::any& value,
			std::any& dt)>
			leave;
		bool ignoreSemanticValue = false;
		std::shared_ptr<Ope> whitespaceOpe;
		std::shared_ptr<Ope> wordOpe;
		bool enablePackratParsing = false;
		bool is_macro = false;
		std::vector<std::string> params;
		TracerEnter tracer_enter;
		TracerLeave tracer_leave;
		bool disable_action = false;

		std::string error_message;
		bool no_ast_opt = false;

	private:
		friend class Reference;
		friend class ParserGenerator;

		Definition& operator=(const Definition& rhs);
		Definition& operator=(Definition&& rhs);

		void initialize_definition_ids() const {
			std::call_once(definition_ids_init_, [&]() {
				AssignIDToDefinition vis;
				holder_->accept(vis);
				if (whitespaceOpe) { whitespaceOpe->accept(vis); }
				if (wordOpe) { wordOpe->accept(vis); }
				definition_ids_.swap(vis.ids);
				});
		}

		Result parse_core(const char* s, size_t n, SemanticValues& vs, std::any& dt,
			const char* path, Log log) const {
			initialize_definition_ids();

			std::shared_ptr<Ope> ope = holder_;
			if (whitespaceOpe) { ope = std::make_shared<Sequence>(whitespaceOpe, ope); }

			Context cxt(path, s, n, definition_ids_.size(), whitespaceOpe, wordOpe,
				enablePackratParsing, tracer_enter, tracer_leave, log);

			auto len = ope->parse(s, n, vs, cxt, dt);
			return Result{ success(len), cxt.recovered, len, cxt.error_info };
		}

		std::shared_ptr<Holder> holder_;
		mutable std::once_flag is_token_init_;
		mutable bool is_token_ = false;
		mutable std::once_flag assign_id_to_definition_init_;
		mutable std::once_flag definition_ids_init_;
		mutable std::unordered_map<void*, size_t> definition_ids_;
	};

	/*
	 * Implementations
	 */

	inline size_t parse_literal(const char* s, size_t n, SemanticValues & vs,
		Context & c, std::any & dt, const std::string & lit,
		std::once_flag & init_is_word, bool& is_word,
		bool ignore_case) {
		size_t i = 0;
		for (; i < lit.size(); i++) {
			if (i >= n || (ignore_case ? (std::tolower(s[i]) != std::tolower(lit[i]))
				: (s[i] != lit[i]))) {
				c.set_error_pos(s, lit.c_str());
				return static_cast<size_t>(-1);
			}
		}

		// Word check
		if (c.wordOpe) {
			std::call_once(init_is_word, [&]() {
				SemanticValues dummy_vs;
				Context dummy_c(nullptr, c.s, c.l, 0, nullptr, nullptr, false, nullptr,
					nullptr, nullptr);
				std::any dummy_dt;

				auto len =
					c.wordOpe->parse(lit.data(), lit.size(), dummy_vs, dummy_c, dummy_dt);
				is_word = success(len);
				});

			if (is_word) {
				SemanticValues dummy_vs;
				Context dummy_c(nullptr, c.s, c.l, 0, nullptr, nullptr, false, nullptr,
					nullptr, nullptr);
				std::any dummy_dt;

				NotPredicate ope(c.wordOpe);
				auto len = ope.parse(s + i, n - i, dummy_vs, dummy_c, dummy_dt);
				if (fail(len)) { return len; }
				i += len;
			}
		}

		// Skip whiltespace
		if (!c.in_token_boundary_count) {
			if (c.whitespaceOpe) {
				auto len = c.whitespaceOpe->parse(s + i, n - i, vs, c, dt);
				if (fail(len)) { return len; }
				i += len;
			}
		}

		return i;
	}

	inline void Context::set_error_pos(const char* a_s, const char* literal) {
		if (log) {
			if (error_info.error_pos <= a_s) {
				if (error_info.error_pos < a_s) {
					error_info.error_pos = a_s;
					error_info.expected_tokens.clear();
				}
				if (literal) {
					error_info.add(literal, true);
				}
				else if (!rule_stack.empty()) {
					auto rule = rule_stack.back();
					auto ope = rule->get_core_operator();
					if (auto token = FindLiteralToken::token(*ope);
						token && token[0] != '\0') {
						error_info.add(token, true);
					}
					else {
						error_info.add(rule->name.c_str(), false);
					}
				}
			}
		}
	}

	inline void Context::trace_enter(const Ope & ope, const char* a_s, size_t n,
		SemanticValues & vs, std::any & dt) const {
		trace_ids.push_back(next_trace_id++);
		tracer_enter(ope, a_s, n, vs, *this, dt);
	}

	inline void Context::trace_leave(const Ope & ope, const char* a_s, size_t n,
		SemanticValues & vs, std::any & dt,
		size_t len) const {
		tracer_leave(ope, a_s, n, vs, *this, dt, len);
		trace_ids.pop_back();
	}

	inline bool Context::is_traceable(const Ope & ope) const {
		if (tracer_enter && tracer_leave) {
			return !IsReference::check(const_cast<Ope&>(ope));
		}
		return false;
	}

	inline size_t Ope::parse(const char* s, size_t n, SemanticValues & vs,
		Context & c, std::any & dt) const {
		if (c.is_traceable(*this)) {
			c.trace_enter(*this, s, n, vs, dt);
			auto len = parse_core(s, n, vs, c, dt);
			c.trace_leave(*this, s, n, vs, dt, len);
			return len;
		}
		return parse_core(s, n, vs, c, dt);
	}

	inline size_t Dictionary::parse_core(const char* s, size_t n,
		SemanticValues& /*vs*/, Context & c,
		std::any& /*dt*/) const {
		auto len = trie_.match(s, n);
		if (len > 0) { return len; }
		c.set_error_pos(s);
		return static_cast<size_t>(-1);
	}

	inline size_t LiteralString::parse_core(const char* s, size_t n,
		SemanticValues & vs, Context & c,
		std::any & dt) const {
		return parse_literal(s, n, vs, c, dt, lit_, init_is_word_, is_word_,
			ignore_case_);
	}

	inline size_t TokenBoundary::parse_core(const char* s, size_t n,
		SemanticValues & vs, Context & c,
		std::any & dt) const {
		size_t len;
		{
			c.in_token_boundary_count++;
			auto se = scope_exit([&]() { c.in_token_boundary_count--; });
			len = ope_->parse(s, n, vs, c, dt);
		}

		if (success(len)) {
			vs.tokens.emplace_back(std::string_view(s, len));

			if (!c.in_token_boundary_count) {
				if (c.whitespaceOpe) {
					auto l = c.whitespaceOpe->parse(s + len, n - len, vs, c, dt);
					if (fail(l)) { return l; }
					len += l;
				}
			}
		}
		return len;
	}

	inline size_t Holder::parse_core(const char* s, size_t n, SemanticValues & vs,
		Context & c, std::any & dt) const {
		if (!ope_) {
			throw std::logic_error("Uninitialized definition ope was used...");
		}

		// Macro reference
		if (outer_->is_macro) {
			c.rule_stack.push_back(outer_);
			auto len = ope_->parse(s, n, vs, c, dt);
			c.rule_stack.pop_back();
			return len;
		}

		size_t len;
		std::any val;

		c.packrat(s, outer_->id, len, val, [&](std::any& a_val) {
			if (outer_->enter) { outer_->enter(s, n, dt); }

			auto se2 = scope_exit([&]() {
				c.pop();
				if (outer_->leave) { outer_->leave(s, n, len, a_val, dt); }
				});

			auto& chldsv = c.push();

			c.rule_stack.push_back(outer_);
			len = ope_->parse(s, n, chldsv, c, dt);
			c.rule_stack.pop_back();

			// Invoke action
			if (success(len)) {
				chldsv.sv_ = std::string_view(s, len);
				chldsv.name_ = outer_->name;

				if (!IsPrioritizedChoice::check(*ope_)) {
					chldsv.choice_count_ = 0;
					chldsv.choice_ = 0;
				}

				try {
					a_val = reduce(chldsv, dt);
				}
				catch (const parse_error& e) {
					if (c.log) {
						if (e.what()) {
							if (c.error_info.message_pos < s) {
								c.error_info.message_pos = s;
								c.error_info.message = e.what();
							}
						}
					}
					len = static_cast<size_t>(-1);
				}
			}
			});

		if (success(len)) {
			if (!outer_->ignoreSemanticValue) {
				vs.emplace_back(std::move(val));
				vs.tags.emplace_back(str2tag(outer_->name));
			}
		}

		return len;
	}

	inline std::any Holder::reduce(SemanticValues & vs, std::any & dt) const {
		if (outer_->action && !outer_->disable_action) {
			return outer_->action(vs, dt);
		}
		else if (vs.empty()) {
			return std::any();
		}
		else {
			return std::move(vs.front());
		}
	}

	inline const char* Holder::trace_name() const {
		if (trace_name_.empty()) { trace_name_ = "[" + outer_->name + "]"; }
		return trace_name_.data();
	}

	inline size_t Reference::parse_core(const char* s, size_t n, SemanticValues & vs,
		Context & c, std::any & dt) const {
		if (rule_) {
			// Reference rule
			if (rule_->is_macro) {
				// Macro
				FindReference vis(c.top_args(), c.rule_stack.back()->params);

				// Collect arguments
				std::vector<std::shared_ptr<Ope>> args;
				for (auto arg : args_) {
					arg->accept(vis);
					args.emplace_back(std::move(vis.found_ope));
				}

				c.push_args(std::move(args));
				auto se = scope_exit([&]() { c.pop_args(); });
				auto ope = get_core_operator();
				return ope->parse(s, n, vs, c, dt);
			}
			else {
				// Definition
				c.push_args(std::vector<std::shared_ptr<Ope>>());
				auto se = scope_exit([&]() { c.pop_args(); });
				auto ope = get_core_operator();
				return ope->parse(s, n, vs, c, dt);
			}
		}
		else {
			// Reference parameter in macro
			const auto& args = c.top_args();
			return args[iarg_]->parse(s, n, vs, c, dt);
		}
	}

	inline std::shared_ptr<Ope> Reference::get_core_operator() const {
		return rule_->holder_;
	}

	inline size_t BackReference::parse_core(const char* s, size_t n,
		SemanticValues & vs, Context & c,
		std::any & dt) const {
		auto size = static_cast<int>(c.capture_scope_stack_size);
		for (auto i = size - 1; i >= 0; i--) {
			auto index = static_cast<size_t>(i);
			const auto& cs = c.capture_scope_stack[index];
			if (cs.find(name_) != cs.end()) {
				const auto& lit = cs.at(name_);
				std::once_flag init_is_word;
				auto is_word = false;
				return parse_literal(s, n, vs, c, dt, lit, init_is_word, is_word, false);
			}
		}
		throw std::runtime_error("Invalid back reference...");
	}

	inline Definition&
		PrecedenceClimbing::get_reference_for_binop(Context & c) const {
		if (rule_.is_macro) {
			// Reference parameter in macro
			const auto& args = c.top_args();
			auto iarg = dynamic_cast<Reference&>(*binop_).iarg_;
			auto arg = args[iarg];
			return *dynamic_cast<Reference&>(*arg).rule_;
		}

		return *dynamic_cast<Reference&>(*binop_).rule_;
	}

	inline size_t PrecedenceClimbing::parse_expression(const char* s, size_t n,
		SemanticValues & vs,
		Context & c, std::any & dt,
		size_t min_prec) const {
		auto len = atom_->parse(s, n, vs, c, dt);
		if (fail(len)) { return len; }

		std::string tok;
		auto& rule = get_reference_for_binop(c);
		auto action = std::move(rule.action);

		rule.action = [&](SemanticValues& vs2, std::any& dt2) {
			tok = vs2.token();
			if (action) {
				return action(vs2, dt2);
			}
			else if (!vs2.empty()) {
				return vs2[0];
			}
			return std::any();
		};
		auto action_se = scope_exit([&]() { rule.action = std::move(action); });

		auto i = len;
		while (i < n) {
			std::vector<std::any> save_values(vs.begin(), vs.end());
			auto save_tokens = vs.tokens;

			auto chv = c.push();
			auto chl = binop_->parse(s + i, n - i, chv, c, dt);
			c.pop();

			if (fail(chl)) { break; }

			auto it = info_.find(tok);
			if (it == info_.end()) { break; }

			auto level = std::get<0>(it->second);
			auto assoc = std::get<1>(it->second);

			if (level < min_prec) { break; }

			vs.emplace_back(std::move(chv[0]));
			i += chl;

			auto next_min_prec = level;
			if (assoc == 'L') { next_min_prec = level + 1; }

			chv = c.push();
			chl = parse_expression(s + i, n - i, chv, c, dt, next_min_prec);
			c.pop();

			if (fail(chl)) {
				vs.assign(save_values.begin(), save_values.end());
				vs.tokens = save_tokens;
				i = chl;
				break;
			}

			vs.emplace_back(std::move(chv[0]));
			i += chl;

			std::any val;
			if (rule_.action) {
				vs.sv_ = std::string_view(s, i);
				val = rule_.action(vs, dt);
			}
			else if (!vs.empty()) {
				val = vs[0];
			}
			vs.clear();
			vs.emplace_back(std::move(val));
		}

		return i;
	}

	inline size_t Recovery::parse_core(const char* s, size_t n,
		SemanticValues& /*vs*/, Context & c,
		std::any& /*dt*/) const {
		const auto& rule = dynamic_cast<Reference&>(*ope_);

		// Custom error message
		if (c.log) {
			auto label = dynamic_cast<Reference*>(rule.args_[0].get());
			if (label) {
				if (!label->rule_->error_message.empty()) {
					c.error_info.message_pos = s;
					c.error_info.message = label->rule_->error_message;
				}
			}
		}

		// Recovery
		size_t len = static_cast<size_t>(-1);
		{
			auto save_log = c.log;
			c.log = nullptr;
			auto se = scope_exit([&]() { c.log = save_log; });

			SemanticValues dummy_vs;
			std::any dummy_dt;

			len = rule.parse(s, n, dummy_vs, c, dummy_dt);
		}

		if (success(len)) {
			c.recovered = true;

			if (c.log) {
				c.error_info.output_log(c.log, c.s, c.l);
				c.error_info.clear();
			}
		}

		// Cut
		if (!c.cut_stack.empty()) {
			c.cut_stack.back() = true;

			if (c.cut_stack.size() == 1) {
				// TODO: Remove unneeded entries in packrat memoise table
			}
		}

		return len;
	}

	inline void Sequence::accept(Visitor & v) { v.visit(*this); }
	inline void PrioritizedChoice::accept(Visitor & v) { v.visit(*this); }
	inline void Repetition::accept(Visitor & v) { v.visit(*this); }
	inline void AndPredicate::accept(Visitor & v) { v.visit(*this); }
	inline void NotPredicate::accept(Visitor & v) { v.visit(*this); }
	inline void Dictionary::accept(Visitor & v) { v.visit(*this); }
	inline void LiteralString::accept(Visitor & v) { v.visit(*this); }
	inline void CharacterClass::accept(Visitor & v) { v.visit(*this); }
	inline void Character::accept(Visitor & v) { v.visit(*this); }
	inline void AnyCharacter::accept(Visitor & v) { v.visit(*this); }
	inline void CaptureScope::accept(Visitor & v) { v.visit(*this); }
	inline void Capture::accept(Visitor & v) { v.visit(*this); }
	inline void TokenBoundary::accept(Visitor & v) { v.visit(*this); }
	inline void Ignore::accept(Visitor & v) { v.visit(*this); }
	inline void User::accept(Visitor & v) { v.visit(*this); }
	inline void WeakHolder::accept(Visitor & v) { v.visit(*this); }
	inline void Holder::accept(Visitor & v) { v.visit(*this); }
	inline void Reference::accept(Visitor & v) { v.visit(*this); }
	inline void Whitespace::accept(Visitor & v) { v.visit(*this); }
	inline void BackReference::accept(Visitor & v) { v.visit(*this); }
	inline void PrecedenceClimbing::accept(Visitor & v) { v.visit(*this); }
	inline void Recovery::accept(Visitor & v) { v.visit(*this); }
	inline void Cut::accept(Visitor & v) { v.visit(*this); }

	inline void AssignIDToDefinition::visit(Holder & ope) {
		auto p = static_cast<void*>(ope.outer_);
		if (ids.count(p)) { return; }
		auto id = ids.size();
		ids[p] = id;
		ope.outer_->id = id;
		ope.ope_->accept(*this);
	}

	inline void AssignIDToDefinition::visit(Reference & ope) {
		if (ope.rule_) {
			for (auto arg : ope.args_) {
				arg->accept(*this);
			}
			ope.rule_->accept(*this);
		}
	}

	inline void AssignIDToDefinition::visit(PrecedenceClimbing & ope) {
		ope.atom_->accept(*this);
		ope.binop_->accept(*this);
	}

	inline void TokenChecker::visit(Reference & ope) {
		if (ope.is_macro_) {
			for (auto arg : ope.args_) {
				arg->accept(*this);
			}
		}
		else {
			has_rule_ = true;
		}
	}

	inline void FindLiteralToken::visit(Reference & ope) {
		if (ope.is_macro_) {
			ope.rule_->accept(*this);
			for (auto arg : ope.args_) {
				arg->accept(*this);
			}
		}
	}

	inline void DetectLeftRecursion::visit(Reference & ope) {
		if (ope.name_ == name_) {
			error_s = ope.s_;
		}
		else if (!refs_.count(ope.name_)) {
			refs_.insert(ope.name_);
			if (ope.rule_) {
				ope.rule_->accept(*this);
				if (done_ == false) { return; }
			}
		}
		done_ = true;
	}

	inline void HasEmptyElement::visit(Reference & ope) {
		auto it = std::find_if(refs_.begin(), refs_.end(),
			[&](const std::pair<const char*, std::string>& ref) {
				return ope.name_ == ref.second;
			});
		if (it != refs_.end()) { return; }

		if (ope.rule_) {
			refs_.emplace_back(ope.s_, ope.name_);
			ope.rule_->accept(*this);
			refs_.pop_back();
		}
	}

	inline void DetectInfiniteLoop::visit(Reference & ope) {
		auto it = std::find_if(refs_.begin(), refs_.end(),
			[&](const std::pair<const char*, std::string>& ref) {
				return ope.name_ == ref.second;
			});
		if (it != refs_.end()) { return; }

		if (ope.rule_) {
			refs_.emplace_back(ope.s_, ope.name_);
			ope.rule_->accept(*this);
			refs_.pop_back();
		}

		if (ope.is_macro_) {
			for (auto arg : ope.args_) {
				arg->accept(*this);
			}
		}
	}

	inline void ReferenceChecker::visit(Reference & ope) {
		auto it = std::find(params_.begin(), params_.end(), ope.name_);
		if (it != params_.end()) { return; }

		if (!grammar_.count(ope.name_)) {
			error_s[ope.name_] = ope.s_;
			error_message[ope.name_] = "'" + ope.name_ + "' is not defined.";
		}
		else {
			if (!referenced.count(ope.name_)) { referenced.insert(ope.name_); }
			const auto& rule = grammar_.at(ope.name_);
			if (rule.is_macro) {
				if (!ope.is_macro_ || ope.args_.size() != rule.params.size()) {
					error_s[ope.name_] = ope.s_;
					error_message[ope.name_] = "incorrect number of arguments.";
				}
			}
			else if (ope.is_macro_) {
				error_s[ope.name_] = ope.s_;
				error_message[ope.name_] = "'" + ope.name_ + "' is not macro.";
			}
			for (auto arg : ope.args_) {
				arg->accept(*this);
			}
		}
	}

	inline void LinkReferences::visit(Reference & ope) {
		// Check if the reference is a macro parameter
		auto found_param = false;
		for (size_t i = 0; i < params_.size(); i++) {
			const auto& param = params_[i];
			if (param == ope.name_) {
				ope.iarg_ = i;
				found_param = true;
				break;
			}
		}

		// Check if the reference is a definition rule
		if (!found_param && grammar_.count(ope.name_)) {
			auto& rule = grammar_.at(ope.name_);
			ope.rule_ = &rule;
		}

		for (auto arg : ope.args_) {
			arg->accept(*this);
		}
	}

	inline void FindReference::visit(Reference & ope) {
		for (size_t i = 0; i < args_.size(); i++) {
			const auto& name = params_[i];
			if (name == ope.name_) {
				found_ope = args_[i];
				return;
			}
		}
		found_ope = ope.shared_from_this();
	}

	/*-----------------------------------------------------------------------------
	 *  PEG parser generator
	 *---------------------------------------------------------------------------*/

	using Rules = std::unordered_map<std::string, std::shared_ptr<Ope>>;

	class ParserGenerator {
	public:
		static std::shared_ptr<Grammar> parse(const char* s, size_t n,
			const Rules& rules, std::string& start,
			bool& enablePackratParsing, Log log) {
			return get_instance().perform_core(s, n, rules, start, enablePackratParsing,
				log);
		}

		static std::shared_ptr<Grammar> parse(const char* s, size_t n,
			std::string& start,
			bool& enablePackratParsing, Log log) {
			Rules dummy;
			return parse(s, n, dummy, start, enablePackratParsing, log);
		}

		// For debuging purpose
		static Grammar& grammar() { return get_instance().g; }

	private:
		static ParserGenerator& get_instance() {
			static ParserGenerator instance;
			return instance;
		}

		ParserGenerator() {
			make_grammar();
			setup_actions();
		}

		struct Instruction {
			std::string type;
			std::any data;
		};

		struct Data {
			std::shared_ptr<Grammar> grammar;
			std::string start;
			const char* start_pos = nullptr;
			std::vector<std::pair<std::string, const char*>> duplicates;
			std::map<std::string, Instruction> instructions;
			std::set<std::string_view> captures;
			bool enablePackratParsing = true;

			Data() : grammar(std::make_shared<Grammar>()) {}
		};

		void make_grammar() {
			// Setup PEG syntax parser
			g["Grammar"] <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
			g["Definition"] <=
				cho(seq(g["Ignore"], g["IdentCont"], g["Parameters"], g["LEFTARROW"],
					g["Expression"], opt(g["Instruction"])),
					seq(g["Ignore"], g["Identifier"], g["LEFTARROW"], g["Expression"],
						opt(g["Instruction"])));
			g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
			g["Sequence"] <= zom(cho(g["CUT"], g["Prefix"]));
			g["Prefix"] <= seq(opt(cho(g["AND"], g["NOT"])), g["SuffixWithLabel"]);
			g["SuffixWithLabel"] <=
				seq(g["Suffix"], opt(seq(g["LABEL"], g["Identifier"])));
			g["Suffix"] <= seq(g["Primary"], opt(g["Loop"]));
			g["Loop"] <= cho(g["QUESTION"], g["STAR"], g["PLUS"], g["Repetition"]);
			g["Primary"] <=
				cho(seq(g["Ignore"], g["IdentCont"], g["Arguments"],
					npd(g["LEFTARROW"])),
					seq(g["Ignore"], g["Identifier"],
						npd(seq(opt(g["Parameters"]), g["LEFTARROW"]))),
					seq(g["OPEN"], g["Expression"], g["CLOSE"]),
					seq(g["BeginTok"], g["Expression"], g["EndTok"]),
					seq(g["BeginCapScope"], g["Expression"], g["EndCapScope"]),
					seq(g["BeginCap"], g["Expression"], g["EndCap"]), g["BackRef"],
					g["LiteralI"], g["Dictionary"], g["Literal"], g["NegatedClass"],
					g["Class"], g["DOT"]);

			g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
			g["IdentCont"] <= seq(g["IdentStart"], zom(g["IdentRest"]));

			const static std::vector<std::pair<char32_t, char32_t>> range = {
				{0x0080, 0xFFFF} };
			g["IdentStart"] <= seq(npd(lit(u8(u8"↑"))), npd(lit(u8(u8"⇑"))),
				cho(cls("a-zA-Z_%"), cls(range)));

			g["IdentRest"] <= cho(g["IdentStart"], cls("0-9"));

			g["Dictionary"] <= seq(g["LiteralD"], oom(seq(g["PIPE"], g["LiteralD"])));

			auto lit_ope = cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))),
				cls("'"), g["Spacing"]),
				seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))),
					cls("\""), g["Spacing"]));
			g["Literal"] <= lit_ope;
			g["LiteralD"] <= lit_ope;

			g["LiteralI"] <=
				cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))), lit("'i"),
					g["Spacing"]),
					seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))), lit("\"i"),
						g["Spacing"]));

			// NOTE: The original Brian Ford's paper uses 'zom' instead of 'oom'.
			g["Class"] <= seq(chr('['), npd(chr('^')),
				tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
				g["Spacing"]);
			g["NegatedClass"] <= seq(lit("[^"),
				tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
				g["Spacing"]);

			g["Range"] <= cho(seq(g["Char"], chr('-'), g["Char"]), g["Char"]);
			g["Char"] <=
				cho(seq(chr('\\'), cls("nrt'\"[]\\^")),
					seq(chr('\\'), cls("0-3"), cls("0-7"), cls("0-7")),
					seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
					seq(lit("\\x"), cls("0-9a-fA-F"), opt(cls("0-9a-fA-F"))),
					seq(lit("\\u"),
						cho(seq(cho(seq(chr('0'), cls("0-9a-fA-F")), lit("10")),
							rep(cls("0-9a-fA-F"), 4, 4)),
							rep(cls("0-9a-fA-F"), 4, 5))),
					seq(npd(chr('\\')), dot()));

			g["Repetition"] <=
				seq(g["BeginBlacket"], g["RepetitionRange"], g["EndBlacket"]);
			g["RepetitionRange"] <= cho(seq(g["Number"], g["COMMA"], g["Number"]),
				seq(g["Number"], g["COMMA"]), g["Number"],
				seq(g["COMMA"], g["Number"]));
			g["Number"] <= seq(oom(cls("0-9")), g["Spacing"]);

			g["LEFTARROW"] <= seq(cho(lit("<-"), lit(u8(u8"←"))), g["Spacing"]);
			~g["SLASH"] <= seq(chr('/'), g["Spacing"]);
			~g["PIPE"] <= seq(chr('|'), g["Spacing"]);
			g["AND"] <= seq(chr('&'), g["Spacing"]);
			g["NOT"] <= seq(chr('!'), g["Spacing"]);
			g["QUESTION"] <= seq(chr('?'), g["Spacing"]);
			g["STAR"] <= seq(chr('*'), g["Spacing"]);
			g["PLUS"] <= seq(chr('+'), g["Spacing"]);
			~g["OPEN"] <= seq(chr('('), g["Spacing"]);
			~g["CLOSE"] <= seq(chr(')'), g["Spacing"]);
			g["DOT"] <= seq(chr('.'), g["Spacing"]);

			g["CUT"] <= seq(lit(u8(u8"↑")), g["Spacing"]);
			~g["LABEL"] <= seq(cho(chr('^'), lit(u8(u8"⇑"))), g["Spacing"]);

			~g["Spacing"] <= zom(cho(g["Space"], g["Comment"]));
			g["Comment"] <=
				seq(chr('#'), zom(seq(npd(g["EndOfLine"]), dot())), g["EndOfLine"]);
			g["Space"] <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
			g["EndOfLine"] <= cho(lit("\r\n"), chr('\n'), chr('\r'));
			g["EndOfFile"] <= npd(dot());

			~g["BeginTok"] <= seq(chr('<'), g["Spacing"]);
			~g["EndTok"] <= seq(chr('>'), g["Spacing"]);

			~g["BeginCapScope"] <= seq(chr('$'), chr('('), g["Spacing"]);
			~g["EndCapScope"] <= seq(chr(')'), g["Spacing"]);

			g["BeginCap"] <= seq(chr('$'), tok(g["IdentCont"]), chr('<'), g["Spacing"]);
			~g["EndCap"] <= seq(chr('>'), g["Spacing"]);

			g["BackRef"] <= seq(chr('$'), tok(g["IdentCont"]), g["Spacing"]);

			g["IGNORE"] <= chr('~');

			g["Ignore"] <= opt(g["IGNORE"]);
			g["Parameters"] <= seq(g["OPEN"], g["Identifier"],
				zom(seq(g["COMMA"], g["Identifier"])), g["CLOSE"]);
			g["Arguments"] <= seq(g["OPEN"], g["Expression"],
				zom(seq(g["COMMA"], g["Expression"])), g["CLOSE"]);
			~g["COMMA"] <= seq(chr(','), g["Spacing"]);

			// Instruction grammars
			g["Instruction"] <= seq(g["BeginBlacket"],
				cho(cho(g["PrecedenceClimbing"]),
					cho(g["ErrorMessage"]), cho(g["NoAstOpt"])),
				g["EndBlacket"]);

			~g["SpacesZom"] <= zom(g["Space"]);
			~g["SpacesOom"] <= oom(g["Space"]);
			~g["BeginBlacket"] <= seq(chr('{'), g["Spacing"]);
			~g["EndBlacket"] <= seq(chr('}'), g["Spacing"]);

			// PrecedenceClimbing instruction
			g["PrecedenceClimbing"] <=
				seq(lit("precedence"), g["SpacesOom"], g["PrecedenceInfo"],
					zom(seq(g["SpacesOom"], g["PrecedenceInfo"])), g["SpacesZom"]);
			g["PrecedenceInfo"] <=
				seq(g["PrecedenceAssoc"],
					oom(seq(ign(g["SpacesOom"]), g["PrecedenceOpe"])));
			g["PrecedenceOpe"] <=
				cho(seq(cls("'"),
					tok(zom(seq(npd(cho(g["Space"], cls("'"))), g["Char"]))),
					cls("'")),
					seq(cls("\""),
						tok(zom(seq(npd(cho(g["Space"], cls("\""))), g["Char"]))),
						cls("\"")),
					tok(oom(seq(npd(cho(g["PrecedenceAssoc"], g["Space"], chr('}'))),
						dot()))));
			g["PrecedenceAssoc"] <= cls("LR");

			// Error message instruction
			g["ErrorMessage"] <=
				seq(lit("message"), g["SpacesOom"], g["LiteralD"], g["SpacesZom"]);

			// No Ast node optimazation instruction
			g["NoAstOpt"] <= seq(lit("no_ast_opt"), g["SpacesZom"]);

			// Set definition names
			for (auto& x : g) {
				x.second.name = x.first;
			}
		}

		void setup_actions() {
			g["Definition"] = [&](const SemanticValues& vs, std::any& dt) {
				auto& data = *std::any_cast<Data*>(dt);

				auto is_macro = vs.choice() == 0;
				auto ignore = std::any_cast<bool>(vs[0]);
				auto name = std::any_cast<std::string>(vs[1]);

				std::vector<std::string> params;
				std::shared_ptr<Ope> ope;
				if (is_macro) {
					params = std::any_cast<std::vector<std::string>>(vs[2]);
					ope = std::any_cast<std::shared_ptr<Ope>>(vs[4]);
					if (vs.size() == 6) {
						data.instructions[name] = std::any_cast<Instruction>(vs[5]);
					}
				}
				else {
					ope = std::any_cast<std::shared_ptr<Ope>>(vs[3]);
					if (vs.size() == 5) {
						data.instructions[name] = std::any_cast<Instruction>(vs[4]);
					}
				}

				auto& grammar = *data.grammar;
				if (!grammar.count(name)) {
					auto& rule = grammar[name];
					rule <= ope;
					rule.name = name;
					rule.s_ = vs.sv().data();
					rule.ignoreSemanticValue = ignore;
					rule.is_macro = is_macro;
					rule.params = params;

					if (data.start.empty()) {
						data.start = name;
						data.start_pos = vs.sv().data();
					}
				}
				else {
					data.duplicates.emplace_back(name, vs.sv().data());
				}
			};

			g["Definition"].enter = [](const char* /*s*/, size_t /*n*/, std::any& dt) {
				auto& data = *std::any_cast<Data*>(dt);
				data.captures.clear();
			};

			g["Expression"] = [&](const SemanticValues& vs) {
				if (vs.size() == 1) {
					return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				}
				else {
					std::vector<std::shared_ptr<Ope>> opes;
					for (auto i = 0u; i < vs.size(); i++) {
						opes.emplace_back(std::any_cast<std::shared_ptr<Ope>>(vs[i]));
					}
					const std::shared_ptr<Ope> ope =
						std::make_shared<PrioritizedChoice>(opes);
					return ope;
				}
			};

			g["Sequence"] = [&](const SemanticValues& vs) {
				if (vs.empty()) {
					return npd(lit(""));
				}
				else if (vs.size() == 1) {
					return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				}
				else {
					std::vector<std::shared_ptr<Ope>> opes;
					for (const auto& x : vs) {
						opes.emplace_back(std::any_cast<std::shared_ptr<Ope>>(x));
					}
					const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
					return ope;
				}
			};

			g["Prefix"] = [&](const SemanticValues& vs) {
				std::shared_ptr<Ope> ope;
				if (vs.size() == 1) {
					ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				}
				else {
					assert(vs.size() == 2);
					auto tok = std::any_cast<char>(vs[0]);
					ope = std::any_cast<std::shared_ptr<Ope>>(vs[1]);
					if (tok == '&') {
						ope = apd(ope);
					}
					else { // '!'
						ope = npd(ope);
					}
				}
				return ope;
			};

			g["SuffixWithLabel"] = [&](const SemanticValues& vs, std::any& dt) {
				auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				if (vs.size() == 1) {
					return ope;
				}
				else {
					assert(vs.size() == 2);
					auto& data = *std::any_cast<Data*>(dt);
					const auto& ident = std::any_cast<std::string>(vs[1]);
					auto label = ref(*data.grammar, ident, vs.sv().data(), false, {});
					auto recovery = rec(ref(*data.grammar, RECOVER_DEFINITION_NAME,
						vs.sv().data(), true, { label }));
					return cho4label_(ope, recovery);
				}
			};

			struct Loop {
				enum class Type { opt = 0, zom, oom, rep };
				Type type;
				std::pair<size_t, size_t> range;
			};

			g["Suffix"] = [&](const SemanticValues& vs) {
				auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				if (vs.size() == 1) {
					return ope;
				}
				else {
					assert(vs.size() == 2);
					auto loop = std::any_cast<Loop>(vs[1]);
					switch (loop.type) {
					case Loop::Type::opt: return opt(ope);
					case Loop::Type::zom: return zom(ope);
					case Loop::Type::oom: return oom(ope);
					default: // Regex-like repetition
						return rep(ope, loop.range.first, loop.range.second);
					}
				}
			};

			g["Loop"] = [&](const SemanticValues& vs) {
				switch (vs.choice()) {
				case 0: // Option
					return Loop{ Loop::Type::opt, std::pair<size_t, size_t>() };
				case 1: // Zero or More
					return Loop{ Loop::Type::zom, std::pair<size_t, size_t>() };
				case 2: // One or More
					return Loop{ Loop::Type::oom, std::pair<size_t, size_t>() };
				default: // Regex-like repetition
					return Loop{ Loop::Type::rep,
								std::any_cast<std::pair<size_t, size_t>>(vs[0]) };
				}
			};

			g["RepetitionRange"] = [&](const SemanticValues& vs) {
				switch (vs.choice()) {
				case 0: { // Number COMMA Number
					auto min = std::any_cast<size_t>(vs[0]);
					auto max = std::any_cast<size_t>(vs[1]);
					return std::pair(min, max);
				}
				case 1: // Number COMMA
					return std::pair(std::any_cast<size_t>(vs[0]),
						std::numeric_limits<size_t>::max());
				case 2: { // Number
					auto n = std::any_cast<size_t>(vs[0]);
					return std::pair(n, n);
				}
				default: // COMMA Number
					return std::pair(std::numeric_limits<size_t>::min(),
						std::any_cast<size_t>(vs[0]));
				}
			};
			g["Number"] = [&](const SemanticValues& vs) {
				return vs.token_to_number<size_t>();
			};

			g["Primary"] = [&](const SemanticValues& vs, std::any& dt) {
				auto& data = *std::any_cast<Data*>(dt);

				switch (vs.choice()) {
				case 0:   // Macro Reference
				case 1: { // Reference
					auto is_macro = vs.choice() == 0;
					auto ignore = std::any_cast<bool>(vs[0]);
					const auto& ident = std::any_cast<std::string>(vs[1]);

					std::vector<std::shared_ptr<Ope>> args;
					if (is_macro) {
						args = std::any_cast<std::vector<std::shared_ptr<Ope>>>(vs[2]);
					}

					auto ope = ref(*data.grammar, ident, vs.sv().data(), is_macro, args);
					if (ident == RECOVER_DEFINITION_NAME) { ope = rec(ope); }

					if (ignore) {
						return ign(ope);
					}
					else {
						return ope;
					}
				}
				case 2: { // (Expression)
					return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				}
				case 3: { // TokenBoundary
					return tok(std::any_cast<std::shared_ptr<Ope>>(vs[0]));
				}
				case 4: { // CaptureScope
					return csc(std::any_cast<std::shared_ptr<Ope>>(vs[0]));
				}
				case 5: { // Capture
					const auto& name = std::any_cast<std::string_view>(vs[0]);
					auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[1]);

					data.captures.insert(name);

					return cap(ope, [name](const char* a_s, size_t a_n, Context& c) {
						auto& cs = c.capture_scope_stack[c.capture_scope_stack_size - 1];
						cs[name] = std::string(a_s, a_n);
						});
				}
				default: {
					return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
				}
				}
			};

			g["IdentCont"] = [](const SemanticValues& vs) {
				return std::string(vs.sv().data(), vs.sv().length());
			};

			g["Dictionary"] = [](const SemanticValues& vs) {
				auto items = vs.transform<std::string>();
				return dic(items);
			};

			g["Literal"] = [](const SemanticValues& vs) {
				const auto& tok = vs.tokens.front();
				return lit(resolve_escape_sequence(tok.data(), tok.size()));
			};
			g["LiteralI"] = [](const SemanticValues& vs) {
				const auto& tok = vs.tokens.front();
				return liti(resolve_escape_sequence(tok.data(), tok.size()));
			};
			g["LiteralD"] = [](const SemanticValues& vs) {
				auto& tok = vs.tokens.front();
				return resolve_escape_sequence(tok.data(), tok.size());
			};

			g["Class"] = [](const SemanticValues& vs) {
				auto ranges = vs.transform<std::pair<char32_t, char32_t>>();
				return cls(ranges);
			};
			g["NegatedClass"] = [](const SemanticValues& vs) {
				auto ranges = vs.transform<std::pair<char32_t, char32_t>>();
				return ncls(ranges);
			};
			g["Range"] = [](const SemanticValues& vs) {
				switch (vs.choice()) {
				case 0: {
					auto s1 = std::any_cast<std::string>(vs[0]);
					auto s2 = std::any_cast<std::string>(vs[1]);
					auto cp1 = decode_codepoint(s1.data(), s1.length());
					auto cp2 = decode_codepoint(s2.data(), s2.length());
					return std::pair(cp1, cp2);
				}
				case 1: {
					auto s = std::any_cast<std::string>(vs[0]);
					auto cp = decode_codepoint(s.data(), s.length());
					return std::pair(cp, cp);
				}
				}
				return std::pair<char32_t, char32_t>(0, 0);
			};
			g["Char"] = [](const SemanticValues& vs) {
				return resolve_escape_sequence(vs.sv().data(), vs.sv().length());
			};

			g["AND"] = [](const SemanticValues& vs) { return *vs.sv().data(); };
			g["NOT"] = [](const SemanticValues& vs) { return *vs.sv().data(); };
			g["QUESTION"] = [](const SemanticValues& vs) { return *vs.sv().data(); };
			g["STAR"] = [](const SemanticValues& vs) { return *vs.sv().data(); };
			g["PLUS"] = [](const SemanticValues& vs) { return *vs.sv().data(); };

			g["DOT"] = [](const SemanticValues& /*vs*/) { return dot(); };

			g["CUT"] = [](const SemanticValues& /*vs*/) { return cut(); };

			g["BeginCap"] = [](const SemanticValues& vs) { return vs.token(); };

			g["BackRef"] = [&](const SemanticValues& vs, std::any& dt) {
				auto& data = *std::any_cast<Data*>(dt);
				if (data.captures.find(vs.token()) == data.captures.end()) {
					data.enablePackratParsing = false;
				}
				return bkr(vs.token_to_string());
			};

			g["Ignore"] = [](const SemanticValues& vs) { return vs.size() > 0; };

			g["Parameters"] = [](const SemanticValues& vs) {
				return vs.transform<std::string>();
			};

			g["Arguments"] = [](const SemanticValues& vs) {
				return vs.transform<std::shared_ptr<Ope>>();
			};

			g["PrecedenceClimbing"] = [](const SemanticValues& vs) {
				PrecedenceClimbing::BinOpeInfo binOpeInfo;
				size_t level = 1;
				for (auto v : vs) {
					auto tokens = std::any_cast<std::vector<std::string_view>>(v);
					auto assoc = tokens[0][0];
					for (size_t i = 1; i < tokens.size(); i++) {
						binOpeInfo[tokens[i]] = std::pair(level, assoc);
					}
					level++;
				}
				Instruction instruction;
				instruction.type = "precedence";
				instruction.data = binOpeInfo;
				return instruction;
			};
			g["PrecedenceInfo"] = [](const SemanticValues& vs) {
				return vs.transform<std::string_view>();
			};
			g["PrecedenceOpe"] = [](const SemanticValues& vs) { return vs.token(); };
			g["PrecedenceAssoc"] = [](const SemanticValues& vs) { return vs.token(); };

			g["ErrorMessage"] = [](const SemanticValues& vs) {
				Instruction instruction;
				instruction.type = "message";
				instruction.data = std::any_cast<std::string>(vs[0]);
				return instruction;
			};

			g["NoAstOpt"] = [](const SemanticValues& /*vs*/) {
				Instruction instruction;
				instruction.type = "no_ast_opt";
				return instruction;
			};
		}

		bool apply_precedence_instruction(Definition& rule,
			const PrecedenceClimbing::BinOpeInfo& info,
			const char* s, Log log) {
			try {
				auto& seq = dynamic_cast<Sequence&>(*rule.get_core_operator());
				auto atom = seq.opes_[0];
				auto& rep = dynamic_cast<Repetition&>(*seq.opes_[1]);
				auto& seq1 = dynamic_cast<Sequence&>(*rep.ope_);
				auto binop = seq1.opes_[0];
				auto atom1 = seq1.opes_[1];

				auto atom_name = dynamic_cast<Reference&>(*atom).name_;
				auto binop_name = dynamic_cast<Reference&>(*binop).name_;
				auto atom1_name = dynamic_cast<Reference&>(*atom1).name_;

				if (!rep.is_zom() || atom_name != atom1_name || atom_name == binop_name) {
					if (log) {
						auto line = line_info(s, rule.s_);
						log(line.first, line.second,
							"'precedence' instruction cannot be applied to '" + rule.name +
							"'.");
					}
					return false;
				}

				rule.holder_->ope_ = pre(atom, binop, info, rule);
				rule.disable_action = true;
			}
			catch (...) {
				if (log) {
					auto line = line_info(s, rule.s_);
					log(line.first, line.second,
						"'precedence' instruction cannot be applied to '" + rule.name +
						"'.");
				}
				return false;
			}
			return true;
		}

		std::shared_ptr<Grammar> perform_core(const char* s, size_t n,
			const Rules& rules, std::string& start,
			bool& enablePackratParsing, Log log) {
			Data data;
			auto& grammar = *data.grammar;

			// Built-in macros
			{
				// `%recover`
				{
					auto& rule = grammar[RECOVER_DEFINITION_NAME];
					rule <= ref(grammar, "x", "", false, {});
					rule.name = RECOVER_DEFINITION_NAME;
					rule.s_ = "[native]";
					rule.ignoreSemanticValue = true;
					rule.is_macro = true;
					rule.params = { "x" };
				}
			}

			std::any dt = &data;
			auto r = g["Grammar"].parse(s, n, dt, nullptr, log);

			if (!r.ret) {
				if (log) {
					if (r.error_info.message_pos) {
						auto line = line_info(s, r.error_info.message_pos);
						log(line.first, line.second, r.error_info.message);
					}
					else {
						auto line = line_info(s, r.error_info.error_pos);
						log(line.first, line.second, "syntax error");
					}
				}
				return nullptr;
			}

			// User provided rules
			for (auto [user_name, user_rule] : rules) {
				auto name = user_name;
				auto ignore = false;
				if (!name.empty() && name[0] == '~') {
					ignore = true;
					name.erase(0, 1);
				}
				if (!name.empty()) {
					auto& rule = grammar[name];
					rule <= user_rule;
					rule.name = name;
					rule.ignoreSemanticValue = ignore;
				}
			}

			// Check duplicated definitions
			auto ret = data.duplicates.empty();

			for (const auto& [name, ptr] : data.duplicates) {
				if (log) {
					auto line = line_info(s, ptr);
					log(line.first, line.second, "'" + name + "' is already defined.");
				}
			}

			// Set root definition
			auto& start_rule = grammar[data.start];

			// Check if the start rule has ignore operator
			{
				if (start_rule.ignoreSemanticValue) {
					if (log) {
						auto line = line_info(s, start_rule.s_);
						log(line.first, line.second,
							"Ignore operator cannot be applied to '" + start_rule.name +
							"'.");
					}
					ret = false;
				}
			}

			if (!ret) { return nullptr; }

			// Check missing definitions
			auto referenced = std::unordered_set<std::string>{
				WHITESPACE_DEFINITION_NAME,
				WORD_DEFINITION_NAME,
				RECOVER_DEFINITION_NAME,
				start_rule.name,
			};

			for (auto& [_, rule] : grammar) {
				ReferenceChecker vis(grammar, rule.params);
				rule.accept(vis);
				referenced.insert(vis.referenced.begin(), vis.referenced.end());
				for (const auto& [name, ptr] : vis.error_s) {
					if (log) {
						auto line = line_info(s, ptr);
						log(line.first, line.second, vis.error_message[name]);
					}
					ret = false;
				}
			}

			for (auto& [name, rule] : grammar) {
				if (!referenced.count(name)) {
					if (log) {
						auto line = line_info(s, rule.s_);
						auto msg = "'" + name + "' is not referenced.";
						log(line.first, line.second, msg);
					}
				}
			}

			if (!ret) { return nullptr; }

			// Link references
			for (auto& x : grammar) {
				auto& rule = x.second;
				LinkReferences vis(grammar, rule.params);
				rule.accept(vis);
			}

			// Check left recursion
			ret = true;

			for (auto& [name, rule] : grammar) {
				DetectLeftRecursion vis(name);
				rule.accept(vis);
				if (vis.error_s) {
					if (log) {
						auto line = line_info(s, vis.error_s);
						log(line.first, line.second, "'" + name + "' is left recursive.");
					}
					ret = false;
				}
			}

			if (!ret) { return nullptr; }

			// Check infinite loop
			{
				DetectInfiniteLoop vis(data.start_pos, data.start);
				start_rule.accept(vis);
				if (vis.has_error) {
					if (log) {
						auto line = line_info(s, vis.error_s);
						log(line.first, line.second,
							"infinite loop is detected in '" + vis.error_name + "'.");
					}
					return nullptr;
				}
			}

			// Automatic whitespace skipping
			if (grammar.count(WHITESPACE_DEFINITION_NAME)) {
				for (auto& x : grammar) {
					auto& rule = x.second;
					auto ope = rule.get_core_operator();
					if (IsLiteralToken::check(*ope)) { rule <= tok(ope); }
				}

				start_rule.whitespaceOpe =
					wsp(grammar[WHITESPACE_DEFINITION_NAME].get_core_operator());
			}

			// Word expression
			if (grammar.count(WORD_DEFINITION_NAME)) {
				start_rule.wordOpe = grammar[WORD_DEFINITION_NAME].get_core_operator();
			}

			// Apply instructions
			for (const auto& [name, instruction] : data.instructions) {
				auto& rule = grammar[name];

				if (instruction.type == "precedence") {
					const auto& info =
						std::any_cast<PrecedenceClimbing::BinOpeInfo>(instruction.data);

					if (!apply_precedence_instruction(rule, info, s, log)) {
						return nullptr;
					}
				}
				else if (instruction.type == "message") {
					rule.error_message = std::any_cast<std::string>(instruction.data);
				}
				else if (instruction.type == "no_ast_opt") {
					rule.no_ast_opt = true;
				}
			}

			// Set root definition
			start = data.start;
			enablePackratParsing = data.enablePackratParsing;

			return data.grammar;
		}

		Grammar g;
	};

	/*-----------------------------------------------------------------------------
	 *  AST
	 *---------------------------------------------------------------------------*/

	template <typename Annotation> struct AstBase : public Annotation {
		AstBase(const char* path, size_t line, size_t column, const char* name,
			const std::vector<std::shared_ptr<AstBase>>& nodes,
			size_t position = 0, size_t length = 0, size_t choice_count = 0,
			size_t choice = 0)
			: path(path ? path : ""), line(line), column(column), name(name),
			position(position), length(length), choice_count(choice_count),
			choice(choice), original_name(name),
			original_choice_count(choice_count), original_choice(choice),
			tag(str2tag(name)), original_tag(tag), is_token(false), nodes(nodes) {}

		AstBase(const char* path, size_t line, size_t column, const char* name,
			const std::string_view& token, size_t position = 0, size_t length = 0,
			size_t choice_count = 0, size_t choice = 0)
			: path(path ? path : ""), line(line), column(column), name(name),
			position(position), length(length), choice_count(choice_count),
			choice(choice), original_name(name),
			original_choice_count(choice_count), original_choice(choice),
			tag(str2tag(name)), original_tag(tag), is_token(true), token(token) {}

		AstBase(const AstBase& ast, const char* original_name, size_t position = 0,
			size_t length = 0, size_t original_choice_count = 0,
			size_t original_choise = 0)
			: path(ast.path), line(ast.line), column(ast.column), name(ast.name),
			position(position), length(length), choice_count(ast.choice_count),
			choice(ast.choice), original_name(original_name),
			original_choice_count(original_choice_count),
			original_choice(original_choise), tag(ast.tag),
			original_tag(str2tag(original_name)), is_token(ast.is_token),
			token(ast.token), nodes(ast.nodes), parent(ast.parent) {}

		const std::string path;
		const size_t line = 1;
		const size_t column = 1;

		const std::string name;
		size_t position;
		size_t length;
		const size_t choice_count;
		const size_t choice;
		const std::string original_name;
		const size_t original_choice_count;
		const size_t original_choice;
		const unsigned int tag;
		const unsigned int original_tag;

		const bool is_token;
		const std::string_view token;

		std::vector<std::shared_ptr<AstBase<Annotation>>> nodes;
		std::weak_ptr<AstBase<Annotation>> parent;

		std::string token_to_string() const {
			assert(is_token);
			return std::string(token);
		}

		template <typename T> T token_to_number() const {
			return token_to_number_<T>(token);
		}
	};

	template <typename T>
	void ast_to_s_core(const std::shared_ptr<T> &ptr, std::string & s, int level,
		std::function<std::string(const T& ast, int level)> fn) {
		const auto& ast = *ptr;
		for (auto i = 0; i < level; i++) {
			s += "  ";
		}
		auto name = ast.original_name;
		if (ast.original_choice_count > 0) {
			name += "/" + std::to_string(ast.original_choice);
		}
		if (ast.name != ast.original_name) { name += "[" + ast.name + "]"; }
		if (ast.is_token) {
			s += "- " + name + " (";
			s += ast.token;
			s += ")\n";
		}
		else {
			s += "+ " + name + "\n";
		}
		if (fn) { s += fn(ast, level + 1); }
		for (auto node : ast.nodes) {
			ast_to_s_core(node, s, level + 1, fn);
		}
	}

	template <typename T>
	std::string
		ast_to_s(const std::shared_ptr<T> &ptr,
			std::function<std::string(const T& ast, int level)> fn = nullptr) {
		std::string s;
		ast_to_s_core(ptr, s, 0, fn);
		return s;
	}

	struct AstOptimizer {
		AstOptimizer(bool mode, const std::vector<std::string>& rules = {})
			: mode_(mode), rules_(rules) {}

		template <typename T>
		std::shared_ptr<T> optimize(std::shared_ptr<T> original,
			std::shared_ptr<T> parent = nullptr) {
			auto found =
				std::find(rules_.begin(), rules_.end(), original->name) != rules_.end();
			bool opt = mode_ ? !found : found;

			if (opt && original->nodes.size() == 1) {
				auto child = optimize(original->nodes[0], parent);
				return std::make_shared<T>(*child, original->name.data(),
					original->choice_count, original->position,
					original->length, original->choice);
			}

			auto ast = std::make_shared<T>(*original);
			ast->parent = parent;
			ast->nodes.clear();
			for (auto node : original->nodes) {
				auto child = optimize(node, ast);
				ast->nodes.push_back(child);
			}
			return ast;
		}

	private:
		const bool mode_;
		const std::vector<std::string> rules_;
	};

	struct EmptyType {};
	using Ast = AstBase<EmptyType>;

	template <typename T = Ast> void add_ast_action(Definition & rule) {
		rule.action = [&](const SemanticValues& vs) {
			auto line = vs.line_info();

			if (rule.is_token()) {
				return std::make_shared<T>(
					vs.path, line.first, line.second, rule.name.data(), vs.token(),
					std::distance(vs.ss, vs.sv().data()), vs.sv().length(),
					vs.choice_count(), vs.choice());
			}

			auto ast =
				std::make_shared<T>(vs.path, line.first, line.second, rule.name.data(),
					vs.transform<std::shared_ptr<T>>(),
					std::distance(vs.ss, vs.sv().data()),
					vs.sv().length(), vs.choice_count(), vs.choice());

			for (auto node : ast->nodes) {
				node->parent = ast;
			}
			return ast;
		};
	}

#define PEG_EXPAND(...) __VA_ARGS__
#define PEG_CONCAT(a, b) a##b
#define PEG_CONCAT2(a, b) PEG_CONCAT(a, b)

#define PEG_PICK(                                                              \
	a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, \
	a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, \
	a32, a33, a34, a35, a36, a37, a38, a39, a40, a41, a42, a43, a44, a45, a46, \
	a47, a48, a49, a50, a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, \
	a62, a63, a64, a65, a66, a67, a68, a69, a70, a71, a72, a73, a74, a75, a76, \
	a77, a78, a79, a80, a81, a82, a83, a84, a85, a86, a87, a88, a89, a90, a91, \
	a92, a93, a94, a95, a96, a97, a98, a99, a100, ...)                         \
  a100

#define PEG_COUNT(...)                                                         \
  PEG_EXPAND(PEG_PICK(                                                         \
	  __VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87,    \
	  86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69,  \
	  68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51,  \
	  50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33,  \
	  32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,  \
	  14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define PEG_DEF_1(r)                                                           \
  peg::Definition r;                                                           \
  r.name = #r;                                                                 \
  peg::add_ast_action(r);

#define PEG_DEF_2(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_1(__VA_ARGS__))
#define PEG_DEF_3(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_2(__VA_ARGS__))
#define PEG_DEF_4(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_3(__VA_ARGS__))
#define PEG_DEF_5(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_4(__VA_ARGS__))
#define PEG_DEF_6(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_5(__VA_ARGS__))
#define PEG_DEF_7(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_6(__VA_ARGS__))
#define PEG_DEF_8(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_7(__VA_ARGS__))
#define PEG_DEF_9(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_8(__VA_ARGS__))
#define PEG_DEF_10(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_9(__VA_ARGS__))
#define PEG_DEF_11(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_10(__VA_ARGS__))
#define PEG_DEF_12(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_11(__VA_ARGS__))
#define PEG_DEF_13(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_12(__VA_ARGS__))
#define PEG_DEF_14(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_13(__VA_ARGS__))
#define PEG_DEF_15(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_14(__VA_ARGS__))
#define PEG_DEF_16(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_15(__VA_ARGS__))
#define PEG_DEF_17(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_16(__VA_ARGS__))
#define PEG_DEF_18(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_17(__VA_ARGS__))
#define PEG_DEF_19(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_18(__VA_ARGS__))
#define PEG_DEF_20(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_19(__VA_ARGS__))
#define PEG_DEF_21(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_20(__VA_ARGS__))
#define PEG_DEF_22(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_21(__VA_ARGS__))
#define PEG_DEF_23(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_22(__VA_ARGS__))
#define PEG_DEF_24(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_23(__VA_ARGS__))
#define PEG_DEF_25(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_24(__VA_ARGS__))
#define PEG_DEF_26(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_25(__VA_ARGS__))
#define PEG_DEF_27(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_26(__VA_ARGS__))
#define PEG_DEF_28(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_27(__VA_ARGS__))
#define PEG_DEF_29(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_28(__VA_ARGS__))
#define PEG_DEF_30(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_29(__VA_ARGS__))
#define PEG_DEF_31(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_30(__VA_ARGS__))
#define PEG_DEF_32(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_31(__VA_ARGS__))
#define PEG_DEF_33(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_32(__VA_ARGS__))
#define PEG_DEF_34(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_33(__VA_ARGS__))
#define PEG_DEF_35(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_34(__VA_ARGS__))
#define PEG_DEF_36(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_35(__VA_ARGS__))
#define PEG_DEF_37(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_36(__VA_ARGS__))
#define PEG_DEF_38(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_37(__VA_ARGS__))
#define PEG_DEF_39(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_38(__VA_ARGS__))
#define PEG_DEF_40(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_39(__VA_ARGS__))
#define PEG_DEF_41(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_40(__VA_ARGS__))
#define PEG_DEF_42(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_41(__VA_ARGS__))
#define PEG_DEF_43(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_42(__VA_ARGS__))
#define PEG_DEF_44(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_43(__VA_ARGS__))
#define PEG_DEF_45(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_44(__VA_ARGS__))
#define PEG_DEF_46(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_45(__VA_ARGS__))
#define PEG_DEF_47(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_46(__VA_ARGS__))
#define PEG_DEF_48(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_47(__VA_ARGS__))
#define PEG_DEF_49(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_48(__VA_ARGS__))
#define PEG_DEF_50(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_49(__VA_ARGS__))
#define PEG_DEF_51(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_50(__VA_ARGS__))
#define PEG_DEF_52(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_51(__VA_ARGS__))
#define PEG_DEF_53(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_52(__VA_ARGS__))
#define PEG_DEF_54(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_53(__VA_ARGS__))
#define PEG_DEF_55(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_54(__VA_ARGS__))
#define PEG_DEF_56(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_55(__VA_ARGS__))
#define PEG_DEF_57(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_56(__VA_ARGS__))
#define PEG_DEF_58(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_57(__VA_ARGS__))
#define PEG_DEF_59(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_58(__VA_ARGS__))
#define PEG_DEF_60(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_59(__VA_ARGS__))
#define PEG_DEF_61(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_60(__VA_ARGS__))
#define PEG_DEF_62(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_61(__VA_ARGS__))
#define PEG_DEF_63(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_62(__VA_ARGS__))
#define PEG_DEF_64(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_63(__VA_ARGS__))
#define PEG_DEF_65(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_64(__VA_ARGS__))
#define PEG_DEF_66(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_65(__VA_ARGS__))
#define PEG_DEF_67(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_66(__VA_ARGS__))
#define PEG_DEF_68(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_67(__VA_ARGS__))
#define PEG_DEF_69(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_68(__VA_ARGS__))
#define PEG_DEF_70(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_69(__VA_ARGS__))
#define PEG_DEF_71(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_70(__VA_ARGS__))
#define PEG_DEF_72(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_71(__VA_ARGS__))
#define PEG_DEF_73(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_72(__VA_ARGS__))
#define PEG_DEF_74(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_73(__VA_ARGS__))
#define PEG_DEF_75(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_74(__VA_ARGS__))
#define PEG_DEF_76(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_75(__VA_ARGS__))
#define PEG_DEF_77(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_76(__VA_ARGS__))
#define PEG_DEF_78(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_77(__VA_ARGS__))
#define PEG_DEF_79(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_78(__VA_ARGS__))
#define PEG_DEF_80(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_79(__VA_ARGS__))
#define PEG_DEF_81(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_80(__VA_ARGS__))
#define PEG_DEF_82(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_81(__VA_ARGS__))
#define PEG_DEF_83(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_82(__VA_ARGS__))
#define PEG_DEF_84(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_83(__VA_ARGS__))
#define PEG_DEF_85(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_84(__VA_ARGS__))
#define PEG_DEF_86(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_85(__VA_ARGS__))
#define PEG_DEF_87(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_86(__VA_ARGS__))
#define PEG_DEF_88(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_87(__VA_ARGS__))
#define PEG_DEF_89(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_88(__VA_ARGS__))
#define PEG_DEF_90(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_89(__VA_ARGS__))
#define PEG_DEF_91(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_90(__VA_ARGS__))
#define PEG_DEF_92(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_91(__VA_ARGS__))
#define PEG_DEF_93(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_92(__VA_ARGS__))
#define PEG_DEF_94(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_93(__VA_ARGS__))
#define PEG_DEF_95(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_94(__VA_ARGS__))
#define PEG_DEF_96(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_95(__VA_ARGS__))
#define PEG_DEF_97(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_96(__VA_ARGS__))
#define PEG_DEF_98(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_97(__VA_ARGS__))
#define PEG_DEF_99(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_98(__VA_ARGS__))
#define PEG_DEF_100(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_99(__VA_ARGS__))

#define AST_DEFINITIONS(...)                                                   \
  PEG_EXPAND(PEG_CONCAT2(PEG_DEF_, PEG_COUNT(__VA_ARGS__))(__VA_ARGS__))

	/*-----------------------------------------------------------------------------
	 *  parser
	 *---------------------------------------------------------------------------*/

	class parser {
	public:
		parser() = default;

		parser(const char* s, size_t n, const Rules& rules) {
			load_grammar(s, n, rules);
		}

		parser(std::string_view sv, const Rules& rules)
			: parser(sv.data(), sv.size(), rules) {}

		parser(const char* s, size_t n) : parser(s, n, Rules()) {}

		parser(std::string_view sv) : parser(sv.data(), sv.size(), Rules()) {}

		operator bool() { return grammar_ != nullptr; }

		bool load_grammar(const char* s, size_t n, const Rules& rules) {
			grammar_ =
				ParserGenerator::parse(s, n, rules, start_, enablePackratParsing_, log);
			return grammar_ != nullptr;
		}

		bool load_grammar(const char* s, size_t n) {
			return load_grammar(s, n, Rules());
		}

		bool load_grammar(std::string_view sv, const Rules& rules) {
			return load_grammar(sv.data(), sv.size(), rules);
		}

		bool load_grammar(std::string_view sv) {
			return load_grammar(sv.data(), sv.size());
		}

		bool parse_n(const char* s, size_t n, const char* path = nullptr) const {
			if (grammar_ != nullptr) {
				const auto& rule = (*grammar_)[start_];
				return post_process(s, n, rule.parse(s, n, path, log));
			}
			return false;
		}

		bool parse(std::string_view sv, const char* path = nullptr) const {
			return parse_n(sv.data(), sv.size(), path);
		}

		bool parse_n(const char* s, size_t n, std::any& dt,
			const char* path = nullptr) const {
			if (grammar_ != nullptr) {
				const auto& rule = (*grammar_)[start_];
				return post_process(s, n, rule.parse(s, n, dt, path, log));
			}
			return false;
		}

		bool parse(std::string_view sv, std::any& dt,
			const char* path = nullptr) const {
			return parse_n(sv.data(), sv.size(), dt, path);
		}

		template <typename T>
		bool parse_n(const char* s, size_t n, T& val,
			const char* path = nullptr) const {
			if (grammar_ != nullptr) {
				const auto& rule = (*grammar_)[start_];
				return post_process(s, n, rule.parse_and_get_value(s, n, val, path, log));
			}
			return false;
		}

		template <typename T>
		bool parse(std::string_view sv, T& val, const char* path = nullptr) const {
			return parse_n(sv.data(), sv.size(), val, path);
		}

		template <typename T>
		bool parse_n(const char* s, size_t n, std::any& dt, T& val,
			const char* path = nullptr) const {
			if (grammar_ != nullptr) {
				const auto& rule = (*grammar_)[start_];
				return post_process(s, n,
					rule.parse_and_get_value(s, n, dt, val, path, log));
			}
			return false;
		}

		template <typename T>
		bool parse(const char* s, std::any& dt, T& val,
			const char* path = nullptr) const {
			auto n = strlen(s);
			return parse_n(s, n, dt, val, path);
		}

		Definition& operator[](const char* s) { return (*grammar_)[s]; }

		const Definition& operator[](const char* s) const { return (*grammar_)[s]; }

		std::vector<std::string> get_rule_names() const {
			std::vector<std::string> rules;
			for (auto& [name, _] : *grammar_) {
				rules.push_back(name);
			}
			return rules;
		}

		void enable_packrat_parsing() {
			if (grammar_ != nullptr) {
				auto& rule = (*grammar_)[start_];
				rule.enablePackratParsing = enablePackratParsing_ && true;
			}
		}

		void enable_trace(TracerEnter tracer_enter, TracerLeave tracer_leave) {
			if (grammar_ != nullptr) {
				auto& rule = (*grammar_)[start_];
				rule.tracer_enter = tracer_enter;
				rule.tracer_leave = tracer_leave;
			}
		}

		template <typename T = Ast> parser& enable_ast() {
			for (auto& [_, rule] : *grammar_) {
				if (!rule.action) { add_ast_action<T>(rule); }
			}
			return *this;
		}

		template <typename T>
		std::shared_ptr<T> optimize_ast(std::shared_ptr<T> ast,
			bool opt_mode = true) const {
			return AstOptimizer(opt_mode, get_no_ast_opt_rules()).optimize(ast);
		}

		Log log;

	private:
		bool post_process(const char* s, size_t n,
			const Definition::Result& r) const {
			auto ret = r.ret && r.len == n;
			if (log && !ret) { r.error_info.output_log(log, s, n); }
			return ret && !r.recovered;
		}

		std::vector<std::string> get_no_ast_opt_rules() const {
			std::vector<std::string> rules;
			for (auto& [name, rule] : *grammar_) {
				if (rule.no_ast_opt) { rules.push_back(name); }
			}
			return rules;
		}

		std::shared_ptr<Grammar> grammar_;
		std::string start_;
		bool enablePackratParsing_ = false;
	};

	} // namespace peg
