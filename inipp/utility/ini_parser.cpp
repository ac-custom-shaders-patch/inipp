#include "stdafx.h"
#include "ini_parser.h"
#include "ini_parser_lua_lib.h"
#include <iomanip>
#include <lua.hpp>
#include <utility/alphanum.h>
#include <utility/json.h>
#include <utility/string_parse.h>
#include <utility/variant.h>
#include <utility>

#ifdef USE_SIMPLE
#define DBG(v) std::cout << "[" << __func__ << ":" << __LINE__ << "] " << #v << "=" << (v) << '\n';
#define LOG(v) std::cerr
#define HERE std::cout << __FILE__ << ":" << __func__ << ":" << __LINE__ <<  '\n';
#define FATAL_CRASH(x) exit(1)
#define _assert(x) assert(x)
#endif

inline bool is_whitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

struct str_view
{
	str_view() {}

	template <std::size_t N>
	explicit str_view(char const (&cs)[N])
		: data_(cs), data_size_(uint32_t(N - 1)), length_(uint32_t(N - 1)) { }

	explicit str_view(const std::string& data)
		: str_view(data, 0U, uint32_t(data.size())) { }

	str_view(const std::string& data, uint32_t start)
		: str_view(data, start, uint32_t(data.size())) { }

	str_view(const std::string& data, uint32_t start, uint32_t length)
		: str_view(data.c_str(), uint32_t(data.size()), start, length) {}

	str_view(const char* data, uint32_t data_size, uint32_t start, uint32_t length)
		: data_(data), data_size_(data_size), start_(start), length_(length)
	{
		if (start_ + length_ > data_size)
		{
			length_ = data_size > start_ ? data_size - start_ : 0;
		}
	}

	size_t find_first_of(char c, uint32_t index = 0U) const
	{
		for (auto i = index; i < length_; ++i)
		{
			if (data_[start_ + i] == c) return i;
		}
		return std::string::npos;
	}

	char operator[](uint32_t index) const
	{
		return data_[start_ + index];
	}

	void trim()
	{
		for (; length_ > 0U && is_whitespace(data_[start_ + length_ - 1]); --length_) {}
		for (; length_ > 0U && is_whitespace(data_[start_]); ++start_, --length_) {}
	}

	bool empty() const { return length_ == 0U; }

	str_view substr(uint32_t offset) const
	{
		if (offset >= length_) return {};
		return {data_, data_size_, start_ + offset, length_ - offset};
	}

	str_view substr(uint32_t offset, uint32_t length) const
	{
		if (offset >= length_) return {};
		return {data_, data_size_, start_ + offset, std::min(length, length_ - offset)};
	}

	template <std::size_t N>
	bool equals(char const (&cs)[N]) const
	{
		return N == 1 ? empty() : length_ == N - 1 && utils::tpl_equals<(N - 1) * sizeof(char)>(&data_[start_], cs);
	}

	template <std::size_t N>
	bool starts_with(char const (&cs)[N]) const
	{
		return N == 1 || length_ >= N - 1 && utils::tpl_equals<(N - 1) * sizeof(char)>(&data_[start_], cs);
	}

	template <std::size_t N>
	bool ends_with(char const (&cs)[N]) const
	{
		return N == 1 || length_ >= N - 1 && utils::tpl_equals<(N - 1) * sizeof(char)>(&data_[start_ + length_ - (N - 1)], cs);
	}

	uint32_t hash_code() const
	{
		if (hash_code_ == 0)
		{
			const auto code = utils::hash_code(&data_[start_], length_);
			((str_view*)this)->hash_code_ = uint32_t(code >> 32) ^ uint32_t(code);
		}
		return hash_code_;
	}

	uint32_t size() const
	{
		return length_;
	}

	std::string str() const
	{
		return length_ ? std::string(&data_[start_], length_) : std::string();
	}

	const char* data() const
	{
		_assert(length_ > 0);
		return &data_[start_];
	}

	bool operator==(const str_view& o) const
	{
		return size() == o.size() && (empty() || memcmp(&data_[start_], &o.data_[o.start_], length_) == 0);
	}
	
	template <std::size_t N>
	bool operator==(char const (&cs)[N]) const
	{
		return N == 1 ? empty() : length_ == N - 1 && utils::tpl_equals<(N - 1) * sizeof(char)>(&data_[start_], cs);
	}

	std::vector<str_view> split(char separator, bool skip_empty, bool trim_result)
	{
		std::vector<str_view> result;
		auto index = 0;
		const auto size = int(this->size());
		while (index < size)
		{
			auto next = find_first_of(separator, index);
			if (next == std::string::npos) next = size_t(size);
			auto piece = substr(index, next - index);
			if (trim_result) piece.trim();
			if (!skip_empty || !piece.empty()) result.push_back(piece);
			index = int(next) + 1;
		}
		return result;
	}

	struct hash_fn
	{
		size_t operator()(const str_view& pos) const
		{
			return pos.hash_code();
		}
	};

private:
	const char* data_{};
	uint32_t data_size_{};
	uint32_t start_{};
	uint32_t length_{};
	uint32_t hash_code_{};
};

std::string& operator +=(std::string& l, const str_view& r)
{
	if (r.size() > 0U)
	{
		const auto l_size = l.size();
		l.resize(l_size + r.size());
		memcpy(&l[l_size], r.data(), r.size());
	}
	return l;
}

typedef std::vector<str_view> variant_sv;

namespace std
{
	template <>
	struct less<str_view>
	{
		bool operator()(const str_view& lhs, const str_view& rhs) const
		{
			return lhs.hash_code() < rhs.hash_code();
		}
	};
}

namespace utils
{
	struct creating_section
	{
		typedef std::pair<std::string, variant> item;
		std::map<std::string, variant> values;

		void set(const std::string& k, variant v)
		{
			values[k] = std::move(v);
		}

		void set(const std::string& k, str_view v)
		{
			values[k] = v.str(); // TODO
		}

		auto find(const std::string& a) const
		{
			return values.find(a);
		}

		auto end() const
		{
			return values.end();
		}

		variant& get(const std::string& a)
		{
			return values[a];
		}

		void erase(const std::string& a)
		{
			values.erase(a);
		}

		void clear()
		{
			values.clear();
		}

		bool empty() const
		{
			return values.empty();
		}

		size_t fingerprint() const
		{
			size_t ret{};
			for (const auto& p : values)
			{
				auto r = std::hash<std::string>{}(p.first);
				for (const auto& v : p.second)
				{
					r = (r * 397) ^ std::hash<std::string>{}(v);
				}
				ret ^= r;
			}
			return ret;
		}
	};

	using resulting_section = std::unordered_map<std::string, variant>;
	using template_section = std::vector<std::pair<std::string, variant>>;

	template <typename TValue>
	auto gen_find(const std::vector<std::pair<std::string, TValue>>& l, const std::string& a)
	{
		return std::find_if(l.begin(), l.end(), [=](const std::pair<std::string, TValue>& s) { return s.first == a; });
	}

	template <typename TValue>
	auto gen_find(const std::map<std::string, TValue>& l, const std::string& a)
	{
		return l.find(a);
	}

	template <typename TValue>
	auto gen_find(const std::unordered_map<std::string, TValue>& l, const std::string& a)
	{
		return l.find(a);
	}

	inline bool is_whitespace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r';
	}

	inline bool is_special_symbol(char c, char& maps_to, bool quoted)
	{
		switch (c)
		{
			case '\n': maps_to = 0;
				return true;
			case 'n': maps_to = '\n';
				return true;
			case 't': maps_to = '\t';
				return true;
			case 'r': maps_to = '\r';
				return true;
			default: return false;
		}
	}

	static void trim_self(std::string& str, const char* chars = " \t\r")
	{
		const auto i = str.find_first_not_of(chars);
		if (i == std::string::npos)
		{
			str.clear();
			return;
		}
		if (i > 0) str.erase(0, i);
		const auto l = str.find_last_not_of(chars);
		if (l < str.size() - 1) str.erase(l + 1);
	}

	static std::vector<std::string> split_string(const std::string& input, const std::string& separator, bool skip_empty, bool trim_result)
	{
		std::vector<std::string> result;
		auto index = 0;
		const auto size = int(input.size());

		while (index < size)
		{
			auto next = input.find_first_of(separator, index);
			if (next == std::string::npos) next = size_t(size);

			auto piece = input.substr(index, next - index);
			if (trim_result) trim_self(piece);
			if (!skip_empty || !piece.empty()) result.push_back(piece);
			index = int(next) + 1;
		}

		return result;
	}

	inline bool is_solid(const str_view& s)
	{
		return s.starts_with("data:image/png;base64,");
	}

	inline bool is_identifier_part(char c)
	{
		return c == '_' || isupper(c) || islower(c) || isdigit(c);
	}

	static bool is_identifier(const std::string& s, bool allow_leading_zero = true)
	{
		if (s.empty() || !allow_leading_zero && isdigit(s[0])) return false;
		for (auto i : s)
		{
			if (!is_identifier_part(i)) return false;
		}
		return true;
	}

	static bool sort_sections(const std::pair<std::string, resulting_section>& a, const std::pair<std::string, resulting_section>& b)
	{
		return doj::alphanum_comp(a.first, b.first) < 0;
	}

	static bool sort_items(const std::pair<std::string, variant>& a, const std::pair<std::string, variant>& b)
	{
		return doj::alphanum_comp(a.first, b.first) < 0;
	}

	static struct
	{
		long variable_scope{};
		long current_sections{};
		long templates{};
	} counters;

	void ini_parser::leaks_check(void (* callback)(const char*, long))
	{
		if (!callback) return;
		callback("variable scopes", counters.variable_scope);
		callback("current sections", counters.current_sections);
		callback("templates", counters.templates);
		counters = {};
	}

	struct variable_scope : std::enable_shared_from_this<variable_scope>
	{
		creating_section explicit_values;
		creating_section include_params;
		creating_section default_values;
		const creating_section* target_section;
		std::vector<const variable_scope*> local_fallbacks;
		std::shared_ptr<variable_scope> parent;

		void fallback(const std::shared_ptr<variable_scope>& s)
		{
			local_fallbacks.push_back(s.get());
		}

		std::shared_ptr<variable_scope> inherit(const creating_section* target = nullptr)
		{
			return std::make_shared<variable_scope>(shared_from_this(), target);
		}

		variable_scope()
			: target_section(nullptr), parent(nullptr)
		{
			counters.variable_scope++;
		}

		variable_scope(std::shared_ptr<variable_scope> parent, const creating_section* target_section)
			: target_section(target_section), parent(std::move(parent))
		{
			counters.variable_scope++;
		}

		~variable_scope()
		{
			counters.variable_scope--;
		}

		const variant* find(const std::string& name) const
		{
			if (const auto ret = find_explicit(name)) return ret;
			if (const auto ret = find_value(name)) return ret;
			if (const auto ret = find_fallback(name)) return ret;
			return nullptr;
		}

	private:
		const variant* find_explicit(const std::string& name) const
		{
			const auto v = explicit_values.find(name);
			if (v != explicit_values.end())
			{
				return &v->second;
			}

			if (const auto ret = parent ? parent->find_explicit(name) : nullptr)
			{
				return ret;
			}

			return nullptr;
		}

		const variant* find_value(const std::string& name) const
		{
			if (target_section != nullptr)
			{
				const auto v = target_section->find(name);
				if (v != target_section->end())
				{
					return &v->second;
				}
			}

			if (const auto ret = parent ? parent->find_value(name) : nullptr)
			{
				return ret;
			}

			return nullptr;
		}

		const variant* find_fallback(const std::string& name) const
		{
			{
				const auto v = include_params.find(name);
				if (v != include_params.end())
				{
					return &v->second;
				}
			}

			if (const auto ret = parent ? parent->find_fallback(name) : nullptr)
			{
				return ret;
			}

			{
				const auto v = default_values.find(name);
				if (v != default_values.end())
				{
					return &v->second;
				}
			}

			for (auto f : local_fallbacks)
			{
				if (const auto ret = f->find(name))
				{
					return ret;
				}
			}

			return nullptr;
		}
	};

	struct script_params
	{
		path file;
		ini_parser_error_handler* error_handler{};
		bool allow_includes = false;
		bool allow_override = true;
		bool allow_lua = false;
		bool erase_referenced = false;

		script_params() = default;
		script_params(const script_params& other) = delete;
		script_params& operator=(const script_params& other) = delete;
	};
	
	#define SPECIAL_CALCULATE_STR "[[SPEC:CALCULATE:"
	#define SPECIAL_END_STR ":SPEC]]"

	static const uint32_t SPECIAL_AUTOINCREMENT_LIMIT = 10000;
	static const int SPECIAL_RANGE_LIMIT = 10000;
	static const std::string SPECIAL_KEY_AUTOINCREMENT = "[[SPEC:INC]]";
	static const std::string SPECIAL_MISSING_VARIABLE = "[[SPEC:MISSING:";
	static const std::string SPECIAL_CALCULATE = SPECIAL_CALCULATE_STR;
	static const std::string SPECIAL_END = SPECIAL_END_STR;

	std::string wrap_special(const std::string& special, const std::string& value)
	{
		return special + value + SPECIAL_END;
	}

	static thread_local struct
	{
		lua_State* ptr{};
		bool is_clean{};
		uint32_t used{};
		std::vector<std::string> imported;
	} lua_state;

	void ini_parser::reset_expressions_state()
	{
		if (lua_state.ptr)
		{
			lua_close(lua_state.ptr);
			lua_state.ptr = nullptr;
		}
	}

	static ini_parser_data_provider*& global_provider()
	{
		static ini_parser_data_provider* value{};
		return value;
	}

	static ini_parser_data_provider*& thread_provider()
	{
		static thread_local ini_parser_data_provider* value{};
		return value;
	}

	disposable ini_parser::set_data_provider(ini_parser_data_provider* provider)
	{
		if (!provider) return {};
		if (thread_provider())
		{
			FATAL_CRASH("Provider is set already");
		}
		thread_provider() = provider;
		return {[] { thread_provider() = nullptr; }};
	}

	disposable ini_parser::set_global_data_provider(ini_parser_data_provider* provider)
	{
		if (!provider) return {};
		if (global_provider())
		{
			FATAL_CRASH("Global provider is set already");
		}
		global_provider() = provider;
		return {[] { global_provider() = nullptr; }};
	}

	disposable ini_parser::set_data_provider_own(ini_parser_data_provider* provider)
	{
		if (!provider) return {};
		if (thread_provider())
		{
			FATAL_CRASH("Provider is set already");
		}
		thread_provider() = provider;
		return {
			[=]
			{
				thread_provider() = nullptr;
				delete provider;
			}
		};
	}

	disposable ini_parser::set_global_data_provider_own(ini_parser_data_provider* provider)
	{
		if (!provider) return {};
		if (global_provider())
		{
			FATAL_CRASH("Provider is set already");
		}
		global_provider() = provider;
		return {
			[=]
			{
				global_provider() = nullptr;
				delete provider;
			}
		};
	}

	static int read_fn(lua_State* L)
	{
		/* get number of arguments */
		const auto n = lua_gettop(L);
		if (n != 2)
		{
			lua_pushstring(L, "load: needs path and default value");
			lua_error(L);
			return 0;
		}

		auto thread = thread_provider();
		auto global = global_provider();
		if (!thread && !global)
		{
			lua_pushstring(L, "load: data provider is not set");
			lua_error(L);
			return 0;
		}

		try
		{
			const auto p = lua_tostring(L, 1);
			if (p == nullptr)
			{
				lua_pushstring(L, "load: path is not a string");
				lua_error(L);
				return 0;
			}

			if (lua_isnumber(L, 2))
			{
				auto v = float(lua_tonumber(L, 2));
				if ((!thread || !thread->read_number(p, v)) && global)
				{
					global->read_number(p, v);
				}
				lua_pushnumber(L, v);
				return 1;
			}

			if (lua_isstring(L, 2))
			{
				auto v = std::string(lua_tostring(L, 2));
				if ((!thread || !thread->read_string(p, v)) && global)
				{
					global->read_string(p, v);
				}
				lua_pushstring(L, v.c_str());
				return 1;
			}

			if (lua_isboolean(L, 2))
			{
				auto v = lua_toboolean(L, 2) != 0;
				if ((!thread || !thread->read_bool(p, v)) && global)
				{
					global->read_bool(p, v);
				}
				lua_pushboolean(L, v);
				return 1;
			}
		}
		catch (std::exception& e)
		{
			lua_pushstring(L, e.what());
			lua_error(L);
			return 0;
		}

		lua_pushnil(L);
		return 1;
	}

	static lua_State* lua_get_state()
	{
		if (!lua_state.ptr)
		{
			lua_state.ptr = luaL_newstate();
			#ifdef USE_SIMPLE
			luaL_requiref(lua_state.ptr, "_G", luaopen_base, 1);
			luaL_requiref(lua_state.ptr, "math", luaopen_math, 1);
			luaL_requiref(lua_state.ptr, "string", luaopen_string, 1);
			#else
			luaopen_base(lua_state.ptr);
			luaopen_math(lua_state.ptr);
			luaopen_string(lua_state.ptr);
			#endif
			luaL_loadstring(lua_state.ptr, LUA_STD_LUB) || lua_pcall(lua_state.ptr, 0, -1, 0);
			lua_register(lua_state.ptr, "read", read_fn);
			lua_state.is_clean = true;
		}
		lua_state.used++;
		return lua_state.ptr;
	}

	static void lua_calculate(const std::string& key, bool& include_value, variant& dest, const std::string& expr,
		const std::string& prefix, const std::string& postfix,
		const path& file, ini_parser_error_handler* handler)
	{
		const auto L = lua_get_state();

		auto ret = luaL_loadstring(L, expr.c_str());
		if (ret == LUA_ERRMEM)
		{
			LOG(ERROR) << "Failed to process `" << expr << "`: out of memory trying to load expression";
			include_value = false;
			return;
		}

		if (ret == LUA_ERRSYNTAX)
		{
			LOG(ERROR) << "Failed to process `" << expr << "`: syntax error";
			include_value = false;
			return;
		}

		ret = lua_pcall(L, 0, -1, 0);
		if (ret == LUA_ERRMEM)
		{
			LOG(ERROR) << "Failed to process `" << expr << "`: out of memory trying to run expression";
			include_value = false;
			return;
		}

		if (ret == LUA_ERRERR)
		{
			LOG(ERROR) << "Failed to process `" << expr << "`: error in error";
			include_value = false;
			return;
		}

		if (ret != 0)
		{
			const auto error_ptr = lua_tolstring(L, -1, nullptr);
			const auto error_msg = error_ptr ? std::string(error_ptr) : "";
			if (error_msg.find("__discardError__") != std::string::npos)
			{
				include_value = false;
				return;
			}
			if (handler) handler->on_error(file, (error_msg + "\nKey: " + key + "\nCommand: " + expr).c_str());
			if (!prefix.empty() || !postfix.empty()) dest.data().push_back(prefix + postfix);
			return;
		}

		if (lua_istable(L, -1))
		{
			lua_pushvalue(L, -1); // stack now contains: -1 => table
			lua_pushnil(L);       // stack now contains: -1 => nil; -2 => table
			while (lua_next(L, -2))
			{
				// stack now contains: -1 => value; -2 => key; -3 => table
				// copy the key so that lua_tostring does not modify the original
				lua_pushvalue(L, -2); // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
				dest.data().push_back(prefix + lua_tostring(L, -2) + postfix);
				// pop value + copy of key, leaving original key
				lua_pop(L, 2);
				// stack now contains: -1 => key; -2 => table
			}
			// stack now contains: -1 => table (when lua_next returns 0 it pops the key
			// but does not push anything.)
			// Pop table
			lua_pop(L, 1);
			// Stack is now the same as it was on entry to this function
			return;
		}

		const auto s = lua_tolstring(L, -1, nullptr);
		dest.data().push_back(s ? prefix + s + postfix : prefix + postfix);
	}

	static void lua_register_function(const std::string& name, const variant& args, const std::string& body,
		bool is_shared, const path& file, ini_parser_error_handler* handler)
	{
		std::string args_line;
		for (auto& arg : args)
		{
			if (!args_line.empty()) args_line += ',';
			args_line += arg;
		}
		const auto expr = "function " + name + "(" + args_line + ")\n" + body + "\nend";
		const auto L = lua_get_state();
		if ((luaL_loadstring(L, expr.c_str()) || lua_pcall(L, 0, -1, 0)) && handler)
		{
			handler->on_error(file, lua_tolstring(L, -1, nullptr));
		}
		if (!is_shared) lua_state.is_clean = false;
	}

	#ifndef USE_SIMPLE
	int lua_absindex(lua_State* L, int i)
	{
		if (i < 0 && i > LUA_REGISTRYINDEX) i += lua_gettop(L) + 1;
		return i;
	}
	#endif

	int lua_getsubtable(lua_State* L, int i, const char* name)
	{
		const auto abs_i = lua_absindex(L, i);
		luaL_checkstack(L, 3, "not enough stack slots");
		lua_pushstring(L, name);
		lua_gettable(L, abs_i);
		if (lua_istable(L, -1)) return 1;
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushstring(L, name);
		lua_pushvalue(L, -2);
		lua_settable(L, abs_i);
		return 0;
	}

	void lua_requiref(lua_State* L, const char* modname, lua_CFunction openf, int glb)
	{
		luaL_checkstack(L, 3, "not enough stack slots available");
		lua_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
		lua_getfield(L, -1, modname);
		if (lua_type(L, -1) == LUA_TNIL)
		{
			lua_pop(L, 1);
			lua_pushcfunction(L, openf);
			lua_pushstring(L, modname);
			lua_call(L, 1, 1);
			lua_pushvalue(L, -1);
			lua_setfield(L, -3, modname);
		}
		if (glb)
		{
			lua_pushvalue(L, -1);
			lua_setglobal(L, modname);
		}
		lua_replace(L, -2);
	}

	template <typename T>
	bool replace(std::basic_string<T>& str, const T* from, const T* to)
	{
		size_t start_from = 0;
		const auto from_length = std::basic_string<T>(from).length();
		const auto to_length = std::basic_string<T>(to).length();
		for (auto i = start_from;; i++)
		{
			const auto start_pos = str.find(from, start_from);
			if (start_pos == std::basic_string<T>::npos) return i > 0;
			str.replace(start_pos, from_length, to);
			start_from = start_pos + to_length;
		}
	}

	int lua_traceback_fn(lua_State* L)
	{
		if (!lua_isstring(L, 1)) return 1;
		#ifndef USE_SIMPLE
		lua_getfield(L, LUA_GLOBALSINDEX, "debug");
		#else
		lua_getfield(L, LUA_RIDX_GLOBALS, "debug");
		#endif
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			return 1;
		}
		lua_getfield(L, -1, "traceback");
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 2);
			return 1;
		}
		lua_pushvalue(L, 1);
		lua_pushinteger(L, 2);
		lua_call(L, 2, 1);
		return 1;
	}

	std::string lua_errorcleanup(const std::string& error)
	{
		auto result = error;
		replace(result, "\nstack traceback:", "");
		replace(result, "\n\t", "\n");
		replace(result, ".\\extension/", "");

		const auto found0 = result.find(":\\");
		const auto found1 = result.find(R"(\extension\)", found0);
		if (found0 != std::string::npos && found1 != std::string::npos)
		{
			result = result.substr(0, found0 - 1) + result.substr(found1 + 11);
		}

		return result;
	}

	bool lua_safecall(lua_State* L, int nargs, int nret, std::string& error)
	{
		const auto hpos = lua_gettop(L) - nargs;
		lua_pushcfunction(L, lua_traceback_fn);
		lua_insert(L, hpos);
		if (lua_pcall(L, nargs, nret, hpos))
		{
			error = lua_errorcleanup(lua_tostring(L, -1));
			lua_remove(L, hpos);
			return true;
		}
		lua_remove(L, hpos);
		return false;
	}

	static void lua_import(const path& name, bool is_shared, const path& file, ini_parser_error_handler* handler)
	{
		auto key = name.filename().string();
		std::transform(key.begin(), key.end(), key.begin(), tolower);
		for (const auto& i : lua_state.imported)
		{
			if (i == key) return;
		}

		lua_state.imported.push_back(key);
		const auto L = lua_get_state();
		if (luaL_loadfile(L, name.string().c_str()))
		{
			handler->on_error(name, lua_errorcleanup(lua_tostring(L, -1)).c_str());
			return;
		}
		std::string exception;
		if (lua_safecall(L, 0, 0, exception))
		{
			handler->on_error(name, exception.c_str());
		}
	}

	struct value_finalizer
	{
		std::string key;
		variant& dest;
		const script_params* params;
		bool& include_value;
		bool process_values;

		value_finalizer(std::string key, bool& include_value, variant& dest, const script_params* params, bool process_values = true)
			: key(std::move(key)), dest(dest), params(params), include_value(include_value), process_values(process_values) { }

		static std::string fix_expression(const std::string& expr)
		{
			return "return __conv_result(" + expr + ")";
		}

		void calculate(const std::string& expr, const std::string& prefix, const std::string& postfix) const
		{
			if (!params->allow_lua)
			{
				if (!prefix.empty() || !postfix.empty()) dest.data().push_back(prefix + postfix);
				return;
			}
			lua_calculate(key, include_value, dest, fix_expression(expr), prefix, postfix, params->file, params->error_handler);
		}

		static void unwrap(std::string& value, const std::string& type)
		{
			const auto spec = value.find(type);
			if (spec == std::string::npos) return;

			const auto end = value.find(SPECIAL_END, spec + type.size());
			if (end == std::string::npos) return;

			const auto arg = value.substr(spec + type.size(), end - spec - type.size());
			const auto orig = value;
			value = orig.substr(0, spec);

			if (type == SPECIAL_MISSING_VARIABLE)
			{
				value += "$";
				value += arg;
			}
			else
			{
				value += arg;
			}

			auto postfix = orig.substr(end + SPECIAL_END.size());
			unwrap(postfix, type);
			value += postfix;
		}

		void unwrap_calculate(std::string& value) const
		{
			const auto spec = value.find(SPECIAL_CALCULATE);
			if (spec == std::string::npos)
			{
				dest.data().push_back(value);
				return;
			}

			const auto end = value.find(SPECIAL_END, spec + SPECIAL_CALCULATE.size());
			if (end == std::string::npos)
			{
				dest.data().push_back(value);
				return;
			}

			calculate(
				value.substr(spec + SPECIAL_CALCULATE.size(), end - spec - SPECIAL_CALCULATE.size()),
				value.substr(0, spec),
				value.substr(end + SPECIAL_END.size()));
		}

		void add(const std::string& value) const
		{
			if (!process_values || value.size() <= 1)
			{
				dest.data().push_back(value);
				return;
			}

			if (value.find(SPECIAL_CALCULATE) == 0)
			{
				auto modified = value;
				unwrap(modified, SPECIAL_MISSING_VARIABLE);
				unwrap_calculate(modified);
				return;
			}

			dest.data().push_back(value);
			unwrap(dest.data()[dest.data().size() - 1], SPECIAL_MISSING_VARIABLE);
		}
	};

	struct variable_info
	{
		enum class special_mode
		{
			none,
			size,
			length,
			exists,
			vec2,
			vec3,
			vec4,
			x,
			y,
			z,
			w,
			number,
			boolean,
			string,
		};

		void wrap_auto(std::string& s) const
		{
			if (mode == special_mode::none || mode >= special_mode::x && mode <= special_mode::w)
			{
				wrap_if_not_a_number(s);
			}
			else if (mode == special_mode::exists)
			{
				s = variant(s).as<bool>() ? "true" : "false";
			}
			else if (mode == special_mode::string)
			{
				wrap(s);
			}
		}

		std::string name;
		int substr_from = 0;
		int substr_to = std::numeric_limits<int>::max();
		bool with_fallback;
		bool is_required;
		special_mode mode{};

		variable_info() : with_fallback(false), is_required(false) {}

		explicit variable_info(std::string name) : name(std::move(name)), with_fallback(true), is_required(false) {}

		variable_info(std::string name, const int from, const int to, const special_mode mode, const bool is_required)
			: name(std::move(name)), substr_from(from), substr_to(to), with_fallback(false), is_required(is_required), mode(mode) {}

		bool get_values(const std::shared_ptr<variable_scope>& include_vars, std::vector<std::string>& result, bool& include_value, const value_finalizer& dest)
		{
			const auto v = include_vars->find(name);
			if (!v)
			{
				if (is_required)
				{
					include_value = false;
				}

				switch (mode)
				{
					case special_mode::size:
					case special_mode::length:
					case special_mode::exists:
					case special_mode::number: result.emplace_back("0");
						return true;
					case special_mode::vec4: result.emplace_back("0");
					case special_mode::vec3: result.emplace_back("0");
					case special_mode::vec2: result.emplace_back("0");
						result.emplace_back("0");
						return true;
					case special_mode::boolean: result.emplace_back("false");
						return true;
					case special_mode::string: result.emplace_back("");
						return true;
					case special_mode::none:
					default: return false;
				}
			}

			if (substr_to < 0 || substr_to == 0 && substr_from < 0) substr_to += int(v->data().size());
			if (substr_from < 0) substr_from += int(v->data().size());

			auto data_size = int(v->data().size());
			if (data_size == 1 && v->data()[0].empty())
			{
				data_size = 0;
			}

			const auto count = std::min(substr_to - substr_from, data_size - substr_from);
			if (count <= 0 && is_required)
			{
				include_value = false;
			}

			switch (mode)
			{
				case special_mode::size:
				{
					result.push_back(std::to_string(count));
					break;
				}
				case special_mode::length:
				{
					size_t total_length = 0;
					for (auto j = substr_from, jt = data_size; j < jt && j < substr_to; j++)
					{
						total_length += v->data()[j].size();
					}
					result.push_back(std::to_string(total_length));
					break;
				}
				case special_mode::exists:
				{
					result.push_back(substr_from < data_size && substr_from < substr_to ? "1" : "0");
					break;
				}
				case special_mode::number:
				{
					result.push_back(std::to_string(v->as<double>(substr_from)));
					break;
				}
				case special_mode::boolean:
				{
					result.push_back(v->as<bool>(substr_from) ? "true" : "false");
					break;
				}
				case special_mode::string:
				{
					result.push_back(v->as<std::string>(substr_from));
					break;
				}
				case special_mode::x:
				case special_mode::y:
				case special_mode::z:
				case special_mode::w:
				{
					const auto index = int(mode) - int(special_mode::x);
					if (index >= count)
					{
						if (count == 1)
						{
							result.push_back(v->data()[substr_from]);
						}
						else
						{
							dest.params->error_handler->on_warning(dest.params->file, ("Expected item with at least " + std::to_string(index + 1) + " values, got "
								+ std::to_string(count) + ", variable " + name).c_str());
							result.emplace_back("0");
						}
					}
					else
					{
						result.push_back(v->data()[substr_from + index]);
					}
					break;
				}
				case special_mode::vec2:
				case special_mode::vec3:
				case special_mode::vec4:
				{
					const auto vec_size = int(2 + int(mode) - int(special_mode::vec2));
					for (auto j = substr_from, jt = data_size; j < jt && j < substr_to; j++)
					{
						if (is_number(v->data()[j]))
						{
							result.push_back(v->data()[j]);
						}
						else
						{
							result.push_back("0");
							dest.params->error_handler->on_warning(dest.params->file, ("Number expected, instead got '" + v->data()[j] + "', variable: " + name).c_str());
						}
						if (result.size() >= size_t(vec_size)) break;
					}

					if (result.size() != 1 && count != vec_size)
					{
						dest.params->error_handler->on_warning(dest.params->file, ("Expected item with " + std::to_string(vec_size) + " values, got "
							+ std::to_string(count) + ", variable " + name).c_str());
					}

					const auto fill_with = result.size() == 1 ? result[0] : "0";
					while (result.size() < size_t(vec_size))
					{
						result.push_back(fill_with);
					}
					break;
				}
				case special_mode::none:
				{
					for (auto j = substr_from, jt = data_size; j < jt && j < substr_to; j++)
					{
						result.push_back(v->data()[j]);
					}
					break;
				}
				default:
				{
					break;
				}
			}
			return true;
		}

		void substitute(const std::shared_ptr<variable_scope>& include_vars, bool& include_value, const value_finalizer& dest)
		{
			std::vector<std::string> result;
			if (get_values(include_vars, result, include_value, dest))
			{
				for (const auto& r : result) dest.add(r);
			}
			else if (with_fallback)
			{
				dest.add(wrap_special(SPECIAL_MISSING_VARIABLE, name));
			}
			else if (dest.params->error_handler && (name.empty() || !isdigit(name[0])) && !is_required)
			{
				dest.params->error_handler->on_warning(dest.params->file, ("Missing variable: " + name).c_str());
			}
		}

		static bool is_number(const std::string& s)
		{
			auto started = 0;
			auto ended = false;
			auto dot = false;
			auto exponent = false;
			auto exponent_end = false;
			auto hex = false;

			for (auto i = 0U; i < s.size(); i++)
			{
				const auto c = s[i];
				if (is_whitespace(c))
				{
					if (started) ended = true;
					continue;
				}

				if (ended) return false;

				if (c == 'x' && !hex && started == 1 && s[i - 1] == '0')
				{
					hex = true;
					started++;
					continue;
				}

				if (c == '-' || c == '+')
				{
					if (started) return false;
					started++;
					continue;
				}

				if (started && (c == 'e' || c == 'E') && !hex)
				{
					if (exponent) return false;
					exponent = true;
					started++;
					continue;
				}

				started++;

				if (c == '.' && !hex)
				{
					if (dot) return false;
					dot = true;
					continue;
				}

				if (isdigit(c))
				{
					if (exponent) exponent_end = true;
					continue;
				}
				return false;
			}
			return !exponent || exponent_end;
		}

		static void wrap(std::string& s)
		{
			std::string r;
			r.reserve(s.size() + 6);
			r.push_back('"');
			for (auto c : s)
			{
				switch (c)
				{
					case '\n': r.append("\\n");
						break;
					case '\r': r.append("\\r");
						break;
					case '\t': r.append("\\t");
						break;
					case '\b': r.append("\\b");
						break;
					case '"':
					case '\\': r.push_back('\\');
					default: r.push_back(c);
				}
			}
			r.push_back('"');
			s = r;
		}

		static void wrap_if_not_a_number(std::string& s)
		{
			if (!is_number(s))
			{
				wrap(s);
			}
		}

		static bool all_numbers(const std::vector<std::string>& v)
		{
			for (const auto& s : v)
			{
				if (!is_number(s)) return false;
			}
			return true;
		}

		void substitute(const std::shared_ptr<variable_scope>& include_vars, const std::string& prefix, const std::string& postfix, bool& include_value,
			const value_finalizer& dest, const bool expr_mode)
		{
			std::vector<std::string> result;
			if (!get_values(include_vars, result, include_value, dest) && !expr_mode)
			{
				#if defined _DEBUG && defined USE_SIMPLE
				// std::cerr << "Missing variable: " << name << '\n';
				#endif
				if (!prefix.empty() || !postfix.empty())
				{
					dest.add(with_fallback
						? prefix + wrap_special(SPECIAL_MISSING_VARIABLE, name) + postfix
						: prefix + postfix);
				}
				else if (with_fallback)
				{
					dest.add(wrap_special(SPECIAL_MISSING_VARIABLE, name));
				}
				if (!with_fallback && dest.params->error_handler && !is_required)
				{
					dest.params->error_handler->on_warning(dest.params->file, ("Missing variable: " + name).c_str());
				}
				return;
			}

			if (expr_mode)
			{
				auto s = prefix;
				if (result.empty())
				{
					s += "nil";
				}
				else if (result.size() == 1)
				{
					wrap_auto(result[0]);
					s += result[0];
				}
				else if (result.size() <= 4 && all_numbers(result))
				{
					s += result.size() == 2 ? "vec2(" : result.size() == 3 ? "vec3(" : "vec4(";
					for (auto j = 0, jt = int(result.size()); j < jt; j++)
					{
						if (j) s += ",";
						wrap_if_not_a_number(result[j]);
						s += result[j];
					}
					s += ")";
				}
				else
				{
					s += "{";
					for (auto j = 0, jt = int(result.size()); j < jt; j++)
					{
						if (j) s += ",";
						wrap_if_not_a_number(result[j]);
						s += result[j];
					}
					s += "}";
				}
				s += postfix;
				dest.add(s);
			}
			else
			{
				for (const auto& r : result)
				{
					auto s = prefix;
					s += r;
					s += postfix;
					dest.add(s);
				}
			}
		}
	};

	static int stoi(const std::string& s, int default_value, bool* set_ptr = nullptr)
	{
		try
		{
			if (s.empty() || !isdigit(s[0]) && s[0] != '-')
			{
				if (set_ptr) *set_ptr = false;
				return default_value;
			}
			const auto ret = std::stoi(s);
			if (set_ptr) *set_ptr = true;
			return ret;
		}
		catch (std::exception&)
		{
			if (set_ptr) *set_ptr = false;
			return default_value;
		}
	}

	static int stoi(const std::vector<std::string>& v, int index, int default_value, bool* set_ptr = nullptr)
	{
		if (index < 0 || index >= int(v.size()))
		{
			if (set_ptr) *set_ptr = false;
			return default_value;
		}
		return stoi(v[index], default_value, set_ptr);
	}

	static variable_info get_parametrized_variable_info(const std::string& s, const value_finalizer& dest)
	{
		const auto pieces = split_string(s, ":", false, true);
		const auto size = pieces.size();
		if (size == 0 || size > 5 || !is_identifier(pieces[0])) return {};
		bool from_set;
		auto from = stoi(pieces, 1, 1, &from_set);
		auto to = stoi(pieces, 2, from_set ? 1 : SPECIAL_RANGE_LIMIT) + from;
		if (size > 3 && pieces[2].empty()) to = stoi(pieces, 3, from_set ? from + 1 : SPECIAL_RANGE_LIMIT);
		auto mode = variable_info::special_mode::none;
		auto is_required = false;
		for (auto i = 1U; i < size; i++)
		{
			const auto& piece = pieces[i];
			if (piece.empty() || !islower(piece[0]) && piece[0] != '?') continue;
			if (equals(piece, "size") || equals(piece, "count")) mode = variable_info::special_mode::size;
			else if (equals(piece, "length")) mode = variable_info::special_mode::length;
			else if (equals(piece, "exists")) mode = variable_info::special_mode::exists;
			else if (equals(piece, "vec2")) mode = variable_info::special_mode::vec2;
			else if (equals(piece, "vec3")) mode = variable_info::special_mode::vec3;
			else if (equals(piece, "vec4")) mode = variable_info::special_mode::vec4;
			else if (equals(piece, "x")) mode = variable_info::special_mode::x;
			else if (equals(piece, "y")) mode = variable_info::special_mode::y;
			else if (equals(piece, "z")) mode = variable_info::special_mode::z;
			else if (equals(piece, "w")) mode = variable_info::special_mode::w;
			else if (equals(piece, "num") || equals(piece, "number")) mode = variable_info::special_mode::number;
			else if (equals(piece, "bool") || equals(piece, "boolean")) mode = variable_info::special_mode::boolean;
			else if (equals(piece, "str") || equals(piece, "string")) mode = variable_info::special_mode::string;
			if (equals(piece, "required") || equals(piece, "?")) is_required = true;
		}
		if (from == 0)
		{
			dest.params->error_handler->on_error(dest.params->file, ("Indices start with 1: " + pieces[0] + ", got: '" + s + "'").c_str());
		}
		if (from > 0) from--;
		if (to > 0) to--;
		return {pieces[0], from, to, mode, is_required};
	}

	static variable_info check_variable(const std::string& s, const value_finalizer& dest)
	{
		if (s.size() < 2) return {};
		if (s[0] != '$') return {};
		if (s[1] == '{' && s[s.size() - 1] == '}')
		{
			return get_parametrized_variable_info(s.substr(2, s.size() - 3), dest);
		}
		const auto vname = s.substr(1);
		if (!is_identifier(vname)) return {};
		return variable_info{vname};
	}

	static void substitute_variable(const std::string& value, const std::shared_ptr<variable_scope>& include_vars, bool& include_value, const value_finalizer& dest,
		const int stack, std::vector<std::string>* referenced_variables)
	{
		#if defined _DEBUG && defined USE_SIMPLE
		if (stack > 9)
		{
			std::cerr << "Stack overflow: " << stack << ", value=" << value;
		}
		#endif

		if (stack < 100)
		{
			auto var = check_variable(value, dest);
			if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
			if (!var.name.empty())
			{
				// Either $VariableName or ${VariableName}
				variant temp;
				const auto finalizer = value_finalizer{var.name, include_value, temp, dest.params, false};
				var.substitute(include_vars, include_value, finalizer);
				for (const auto& v : temp)
				{
					substitute_variable(v, include_vars, include_value, dest, stack + 1, referenced_variables);
				}
				return;
			}

			const auto expr_mode = value.find(SPECIAL_CALCULATE) == 0;

			{
				// Concatenation with ${VariableName}
				const auto var_begin = value.find("${");
				const auto var_end = var_begin == std::string::npos ? std::string::npos : value.find_first_of('}', var_begin);
				if (var_end != std::string::npos)
				{
					var = check_variable(value.substr(var_begin, var_end - var_begin + 1), dest);
					if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
					variant temp;
					const auto finalizer = value_finalizer{var.name, include_value, temp, dest.params, false};
					var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end + 1), include_value, finalizer, expr_mode);
					for (const auto& v : temp)
					{
						substitute_variable(v, include_vars, include_value, dest, stack + 1, referenced_variables);
					}
					return;
				}
			}

			{
				// Concatenation with $VariableName
				const auto var_begin = value.find_first_of('$', 1);
				if (var_begin != std::string::npos)
				{
					auto var_end = var_begin + 1;
					for (auto i = var_begin + 1, s = value.size(); i < s; i++)
					{
						if (is_identifier_part(value[i])) var_end++;
						else break;
					}
					if (var_end != var_begin + 1)
					{
						var = check_variable(value.substr(var_begin, var_end - var_begin), dest);
						if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
						variant temp;
						const auto finalizer = value_finalizer{var.name, include_value, temp, dest.params, false};
						var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end), include_value, finalizer, expr_mode);
						for (const auto& v : temp)
						{
							substitute_variable(v, include_vars, include_value, dest, stack + 1, referenced_variables);
						}
						return;
					}
				}
			}
		}

		// Raw value
		dest.add(value);
	}

	struct section_template
	{
		std::string name;
		template_section values;
		std::shared_ptr<variable_scope> template_scope;
		std::vector<std::shared_ptr<section_template>> parents;

		section_template(std::string name, const std::shared_ptr<variable_scope>& scope)
			: name(std::move(name)), template_scope(scope->inherit())
		{
			counters.templates++;
		}

		~section_template()
		{
			counters.templates--;
		}
	};

	struct current_section_info
	{
		bool terminated{};
		std::string section_key;
		creating_section target_section{};
		std::shared_ptr<section_template> target_template{};
		std::vector<std::shared_ptr<section_template>> referenced_templates;
		std::vector<std::string> referenced_variables;

		// This would allow to overwrite values by template
		std::unordered_map<section_template*, std::vector<std::string>> set_via_template;

		explicit current_section_info(std::shared_ptr<section_template> target_template)
			: target_template(std::move(target_template))
		{
			counters.current_sections++;
		}

		current_section_info(std::string key)
			: section_key(std::move(key))
		{
			counters.current_sections++;
		}

		current_section_info(str_view key)
			: section_key(key.data(), key.size())
		{
			counters.current_sections++;
		}

		current_section_info(std::string key, std::vector<std::shared_ptr<section_template>> templates)
			: section_key(std::move(key)), referenced_templates(std::move(templates))
		{
			counters.current_sections++;
		}

		current_section_info(str_view key, std::vector<std::shared_ptr<section_template>> templates)
			: section_key(key.data(), key.size()), referenced_templates(std::move(templates))
		{
			counters.current_sections++;
		}

		current_section_info(const current_section_info& other) = delete;
		current_section_info& operator=(const current_section_info& other) = delete;

		~current_section_info()
		{
			counters.current_sections--;
		}

		bool section_mode() const { return target_template == nullptr && !terminated; }
		void terminate() { terminated = true; }
	};

	using section_named = std::pair<std::string, creating_section>;
	using sections_list = std::vector<section_named>;
	using sections_map = std::unordered_map<std::string, resulting_section>;

	struct ini_parser_data
	{
		sections_list sections;
		sections_map sections_map;
		std::unordered_map<std::string, std::shared_ptr<section_template>> templates_map;
		std::unordered_map<std::string, std::shared_ptr<section_template>> mixins_map;
		std::vector<path> resolve_within;
		std::vector<std::string> processed_files;
		// section include_vars;
		// variable_scope main_scope{nullptr};
		script_params current_params;
		const ini_parser_reader* reader{};
		uint64_t key_autoinc_index{};

		bool get_template(const std::string& s, std::shared_ptr<section_template>& ref)
		{
			ref = templates_map[s];
			if (!ref) error("Template is missing: %s", s);
			return ref != nullptr;
		}

		bool get_mixin(const std::string& s, std::shared_ptr<section_template>& ref)
		{
			ref = mixins_map[s];
			if (!ref) error("Mixin is missing: %s", s);
			return ref != nullptr;
		}

		static auto warn_unwrap(const std::string& s) { return s.c_str(); }

		template <typename... Args>
		void warn(const char* format, const Args& ... args) const
		{
			if (!current_params.error_handler) return;
			const int size = std::snprintf(nullptr, 0, format, warn_unwrap(args)...) + 1; // Extra space for '\0'
			const std::unique_ptr<char[]> buf(new char[ size ]);
			std::snprintf(buf.get(), size, format, warn_unwrap(args)...);
			current_params.error_handler->on_warning(current_params.file, std::string(buf.get(), buf.get() + size - 1).c_str()); // We don't want the '\0' inside
		}

		template <typename... Args>
		void error(const char* format, const Args& ... args) const
		{
			if (!current_params.error_handler) return;
			const int size = std::snprintf(nullptr, 0, format, warn_unwrap(args)...) + 1; // Extra space for '\0'
			const std::unique_ptr<char[]> buf(new char[ size ]);
			std::snprintf(buf.get(), size, format, warn_unwrap(args)...);
			current_params.error_handler->on_error(current_params.file, std::string(buf.get(), buf.get() + size - 1).c_str()); // We don't want the '\0' inside
		}

		ini_parser_data(bool allow_includes = false, std::vector<path> resolve_within = {})
			: resolve_within(std::move(resolve_within))
		{
			current_params.allow_includes = allow_includes;
		}

		bool is_processed(const std::string& file_name, const size_t vars_fingerprint)
		{
			auto inc = file_name;
			trim_self(inc);
			const auto name = path(inc).filename().string() + std::to_string(vars_fingerprint);
			for (auto& processed_value : processed_files)
			{
				if (_stricmp(processed_value.c_str(), name.c_str()) == 0)
				{
					return true;
				}
			}
			return false;
		}

		void mark_processed(const std::string& file_name, const size_t vars_fingerprint)
		{
			auto inc = file_name;
			trim_self(inc);
			const auto name = path(inc).filename().string() + std::to_string(vars_fingerprint);
			processed_files.push_back(name);
		}

		path find_referenced(const std::string& file_name, const size_t vars_fingerprint)
		{
			if (is_processed(file_name, vars_fingerprint)) return {};

			auto inc = file_name;
			trim_self(inc);

			for (auto i = -1, t = int(resolve_within.size()); i < t; i++)
			{
				const auto filename = (i == -1 ? current_params.file.parent_path() : resolve_within[i]) / inc;
				if (!exists(filename)) continue;

				const auto new_resolve_within = filename.parent_path();
				auto add_new_resolve_within = true;
				for (auto& within : resolve_within)
				{
					if (within == new_resolve_within)
					{
						add_new_resolve_within = false;
						break;
					}
				}

				if (add_new_resolve_within)
				{
					resolve_within.push_back(new_resolve_within);
				}

				return filename;
			}

			return {};
		}

		value_finalizer get_value_finalizer(const std::string& key, bool& include_value, variant& dest) const
		{
			return {key, include_value, dest, &current_params, true};
		}

		static bool is_inline_param(const std::string& key, const std::string& value)
		{
			return (starts_with(key, "@MIXIN") || equals(key, "@") || starts_with(key, "@GENERATOR")) && value.find_first_of('=') != std::string::npos;
		}

		static bool delayed_substitute(current_section_info* c)
		{
			return c && (equals(c->section_key, "DEFAULTS") || equals(c->section_key, "INCLUDE") || c->target_template);
		}

		bool substitute_variable_array(const std::string& key, const variant& v, const std::shared_ptr<variable_scope>& sc,
			std::vector<std::string>* referenced_variables, variant& result) const
		{
			auto include_value_ret = true;
			for (const auto& piece : v)
			{
				if (!result.empty() && is_inline_param(key, piece))
				{
					result.data().push_back(piece);
				}
				else
				{
					substitute_variable(piece, sc, include_value_ret, get_value_finalizer(key, include_value_ret, result), 0, referenced_variables);
				}
			}
			return include_value_ret;
		}

		bool split_and_substitute(const std::string& key, current_section_info* c, const str_view& value, const std::shared_ptr<variable_scope>& sc,
			std::vector<std::string>* referenced_variables, variant& result) const
		{
			const auto split = split_string_quotes(value, !key.empty() && key[0] == '@');
			if (delayed_substitute(c))
			{
				result = split;
				return true;
			}

			return substitute_variable_array(key, split, sc, referenced_variables ? referenced_variables : c ? &c->referenced_variables : nullptr, result);
		}

		void resolve_generator_impl(const std::shared_ptr<section_template>& t, const std::string& key, const std::string& section_key,
			const std::shared_ptr<section_template>& tpl, const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables)
		{
			current_section_info generated(section_key, {tpl});
			auto gen_scope = scope;
			auto gen_param_prefix = key + ":";
			if (t)
			{
				for (const auto& v0 : t->values)
				{
					if (v0.first.find(key) != 0) continue;
					const auto v0_sep = v0.first.find_first_of(':');
					if (v0_sep == std::string::npos) continue;
					auto is_param = true;
					for (auto j = key.size(); j < v0_sep; j++)
					{
						if (!is_whitespace(v0.first[j]))
						{
							is_param = false;
						}
					}
					if (!is_param) continue;
					auto param_key = v0.first.substr(v0_sep + 1);
					trim_self(param_key);
					if (gen_scope == scope)
					{
						gen_scope = scope->inherit();
					}

					variant v;
					if (substitute_variable_array(param_key, v0.second, gen_scope, &referenced_variables, v))
					{
						gen_scope->explicit_values.set(param_key, v);
					}
				}
			}
			parse_ini_section_finish(generated, gen_scope, &referenced_variables);
		}

		void resolve_generator_iteration(const std::shared_ptr<section_template>& t, const std::string& key, const std::string& section_key,
			const std::shared_ptr<section_template>& tpl, const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables,
			const std::vector<int>& repeats, size_t repeats_phase)
		{
			if (repeats.size() > repeats_phase)
			{
				for (auto i = 0, n = repeats[repeats_phase]; i < n; i++)
				{
					auto gen_scope = scope->inherit();
					gen_scope->explicit_values.set(std::to_string(repeats_phase + 1), i + 1);
					resolve_generator_iteration(t, key, section_key, tpl, gen_scope, referenced_variables, repeats, repeats_phase + 1);
				}
			}
			else
			{
				resolve_generator_impl(t, key, section_key, tpl, scope, referenced_variables);
			}
		}

		void set_inline_values(std::shared_ptr<variable_scope>& scope_own, const std::shared_ptr<variable_scope>& scope,
			const variant& trigger, const int index, std::vector<std::string>& referenced_variables) const
		{
			for (auto i = index; i < int(trigger.data().size()); i++)
			{
				const auto& item = trigger.data()[i];
				const auto set = item.find_first_of('=');
				if (set != std::string::npos)
				{
					if (!scope_own)
					{
						scope_own = scope->inherit();
					}

					auto set_key = str_view{item, 0, uint32_t(set)};
					auto set_value = str_view{item, uint32_t(set + 1)};
					set_key.trim();
					set_value.trim();

					if (set_key.starts_with(SPECIAL_CALCULATE_STR)
						&& set_value.ends_with(SPECIAL_END_STR))
					{
						set_key = set_key.substr(uint32_t(SPECIAL_CALCULATE.size()));
						variant v;
						if (substitute_variable_array(set_key.str(), variant(SPECIAL_CALCULATE + set_value.str()), scope_own, &referenced_variables, v))
						{
							scope_own->explicit_values.set(set_key.str(), std::move(v));
						}
					}
					else
					{
						variant v;
						if (split_and_substitute(set_key.str(), nullptr, set_value, scope_own, &referenced_variables, v))
						{
							scope_own->explicit_values.set(set_key.str(), std::move(v));
						}
					}
				}
				else if (is_identifier(item, false))
				{
					if (!scope_own)
					{
						scope_own = scope->inherit();
					}
					scope_own->explicit_values.set(item, variant("1"));
				}
			}
		}

		void resolve_generator(const std::shared_ptr<section_template>& t, const std::string& key, const variant& trigger,
			const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables)
		{
			auto ref_template = trigger.as<std::string>();

			std::shared_ptr<variable_scope> scope_own;
			set_inline_values(scope_own, scope, trigger, 1, referenced_variables);

			std::vector<int> repeats;
			for (auto i = 1; i < int(trigger.data().size()); i++)
			{
				if (trigger.data()[i].find_first_of('=') == std::string::npos
					&& !is_identifier(trigger.data()[i], false))
				{
					repeats.push_back(trigger.as<int>(i));
				}
			}

			std::string section_key;
			const auto separator = ref_template.find_first_of(':');
			if (separator != std::string::npos)
			{
				section_key = ref_template.substr(0, separator);
				ref_template = ref_template.substr(separator + 1);
				trim_self(section_key);
				trim_self(ref_template);
			}

			std::shared_ptr<section_template> tpl;
			if (get_template(ref_template, tpl))
			{
				resolve_generator_iteration(t, key, section_key, tpl, scope_own ? scope_own : scope, referenced_variables, repeats, 0);
			}
		}

		static std::shared_ptr<variable_scope> prepare_section_scope(current_section_info& c, const std::shared_ptr<variable_scope>& scope)
		{
			auto sc = scope->inherit(&c.target_section);
			for (const auto& t : c.referenced_templates)
			{
				sc->fallback(t->template_scope);
			}

			const auto target_found = sc->explicit_values.find("TARGET");
			if (target_found == sc->explicit_values.end() && !c.section_key.empty())
			{
				sc->explicit_values.set("TARGET", c.section_key);
			}
			return sc;
		}

		void resolve_template(current_section_info& c, const std::shared_ptr<variable_scope>& scope, const std::shared_ptr<section_template>& t,
			std::vector<std::string>& referenced_variables)
		{
			const auto sc = scope->inherit();
			sc->fallback(t->template_scope);

			const auto inactive = gen_find(t->values, "@ACTIVE");
			if (inactive != t->values.end())
			{
				variant v;
				if (substitute_variable_array("@ACTIVE", inactive->second, sc, &referenced_variables, v) && !v.as<bool>()) return;
			}

			for (const auto& v : t->values)
			{
				if (starts_with(v.first, "@ACTIVE")) continue;

				const auto is_output = equals(v.first, "@OUTPUT");
				if (is_output && !c.section_key.empty()) continue;

				const auto is_generator = starts_with(v.first, "@GENERATOR");
				const auto is_generator_param = is_generator && v.first.find_first_of(':') != std::string::npos;
				const auto is_mixin = starts_with(v.first, "@MIXIN") || equals(v.first, "@");
				const auto is_virtual = is_output || is_generator || is_mixin;

				auto& set_via_template = c.set_via_template[t.get()];
				if (!is_virtual && c.target_section.find(v.first) != c.target_section.end()
					&& std::find(set_via_template.begin(), set_via_template.end(), v.first) == set_via_template.end()
					|| is_generator_param)
				{
					continue;
				}

				variant dest;
				if (!substitute_variable_array(v.first, v.second, sc, &referenced_variables, dest))
				{
					continue;
				}

				if (is_output)
				{
					c.section_key = dest.as<std::string>();
					sc->explicit_values.set("TARGET", c.section_key);
				}
				else if (is_generator)
				{
					resolve_generator(t, v.first, dest, sc, referenced_variables);
				}
				else if (is_mixin)
				{
					resolve_mixin(c, sc, dest);
				}
				else if (!is_virtual)
				{
					auto key = v.first;
					if (key.find("${") != std::string::npos || key.find("$\"") != std::string::npos
						|| key.find(SPECIAL_CALCULATE) != std::string::npos)
					{
						variant key_v;
						if (!substitute_variable_array(key, key, sc, &referenced_variables, key_v))
						{
							return;
						}
						key = key_v.as<std::string>();
					}

					key = convert_key_autoinc(key);
					c.target_section.set(key, dest);
					set_via_template.push_back(key);
				}
			}
		}

		void resolve_mixin(current_section_info& c, const std::shared_ptr<variable_scope>& sc, const std::string& mixin_name, const variant& trigger, int inline_values_index)
		{
			std::shared_ptr<section_template> t;
			if (!get_mixin(mixin_name, t)) return;

			std::shared_ptr<variable_scope> scope_own;
			set_inline_values(scope_own, sc, trigger, inline_values_index, c.referenced_variables);
			resolve_template(c, scope_own ? scope_own : sc, t, c.referenced_variables);
		}

		void resolve_mixin(current_section_info& c, const std::shared_ptr<variable_scope>& sc, const variant& trigger)
		{
			if (trigger.empty()) return;
			resolve_mixin(c, sc, trigger.data()[0], trigger, 1);
		}

		void parse_ini_section_finish(current_section_info& c, const std::shared_ptr<variable_scope>& scope,
			std::vector<std::string>* referenced_variables_ptr = nullptr)
		{
			if (!c.section_mode()) return;

			if (!c.referenced_templates.empty())
			{
				std::unique_ptr<std::vector<std::string>> referenced_variables_uptr;
				if (!referenced_variables_ptr)
				{
					referenced_variables_uptr = std::make_unique<std::vector<std::string>>(c.referenced_variables);
					referenced_variables_ptr = referenced_variables_uptr.get();
				}
				auto& referenced_variables = *referenced_variables_ptr;

				const auto sc = prepare_section_scope(c, scope);
				for (const auto& t : c.referenced_templates)
				{
					resolve_template(c, sc, t, referenced_variables);
				}

				if (current_params.erase_referenced)
				{
					for (const auto& v : referenced_variables)
					{
						c.target_section.erase(v);
					}
				}
			}

			if (current_params.erase_referenced)
			{
				for (const auto& v : c.referenced_variables)
				{
					c.target_section.erase(v);
				}
			}

			if (equals(c.section_key, "FUNCTION") && current_params.allow_lua)
			{
				lua_register_function(
					c.target_section.get("NAME").as<std::string>(),
					c.target_section.get("ARGUMENTS"),
					c.target_section.get("CODE").as<std::string>(),
					!c.target_section.get("PRIVATE").as<bool>(),
					current_params.file, current_params.error_handler);
				c.target_section.clear();
			}
			else if (equals(c.section_key, "USE") && current_params.allow_lua)
			{
				const auto name = c.target_section.get("FILE").as<std::string>();
				const auto referenced = find_referenced(name, 0);
				if (exists(referenced)) lua_import(referenced, !c.target_section.get("PRIVATE").as<bool>(), current_params.file, current_params.error_handler);
				else error("Referenced file is missing: %s", name);
				c.target_section.clear();
			}
			else if (starts_with(c.section_key, "INCLUDE"))
			{
				const auto to_include = c.target_section.find("INCLUDE");
				if (to_include != c.target_section.end())
				{
					const auto previous_file = current_params.file;
					auto include_scope = scope->inherit();

					for (const auto& p : c.target_section.values)
					{
						if (equals(p.first, "INCLUDE")) continue;
						if (starts_with(p.first, "VAR")) include_scope->include_params.set(p.second.as<std::string>(), p.second.as<variant>(1));
						else include_scope->include_params.set(p.first, p.second);
					}

					// It’s important to copy values and clear section before parsing included files: those
					// files might have their own includes, overwriting this one and breaking c.target_section pointer 
					auto values = to_include->second.data();
					c.target_section.clear();

					const auto vars_fp = include_scope->include_params.fingerprint();
					for (auto& i : values)
					{
						parse_file(find_referenced(i, vars_fp), include_scope, vars_fp);
					}

					current_params.file = previous_file;
					return;
				}
			}

			if (!c.target_section.empty())
			{
				const auto key_active = c.target_section.find("ACTIVE");
				if (key_active != c.target_section.end() && !key_active->second.as<bool>())
				{
					c.target_section = {};
					c.target_section.set("ACTIVE", "0");
				}

				sections.push_back({c.section_key, c.target_section});
			}
		}

		static variant split_string_quotes(const str_view& str, bool consider_inline_params)
		{
			variant result;

			if (is_solid(str))
			{
				result.data().push_back(str.str());
				return result;
			}
			
			auto last_nonspace = 0;
			auto q = -1;
			auto u = false, w = false;
			std::string item;
			for (auto i = 0U, t = str.size(); i < t; i++)
			{
				const auto c = str[i];

				if (c == '\\' && i < t - 1 && (str[i + 1] == '\n' || (str[i + 1] == '"' || str[i + 1] == '\'') && (item.empty() || q != -1) || str[i + 1] == ','))
				{
					item += str.substr(last_nonspace, i - last_nonspace);
					i++;
					if (str[i] != '\n') item += str[i];
					last_nonspace = i + 1;
					continue;
				}

				char maps_to;
				if (!u && q == '"' && c == '\\' && i < t - 1 && is_special_symbol(str[i + 1], maps_to, q != -1))
				{
					item += str.substr(last_nonspace, i - last_nonspace);
					if (maps_to) item += maps_to;
					i++;
					last_nonspace = i + 1;
					continue;
				}

				if (c == '"' || c == '\'')
				{
					if (q == c)
					{
						auto rewind = false;
						for (auto j = i + 1; j < t; j++)
						{
							if (str[j] == '\\') continue;
							if (str[j] == ',') break;
							if (!is_whitespace(str[j]))
							{
								rewind = true;
								break;
							}
						}

						if (rewind)
						{
							item = c + item + c;
							if (u) item = "$" + item;
						}
						else if (u)
						{
							item = wrap_special(SPECIAL_CALCULATE, item);
						}
						u = false;
						q = -1;
					}
					else if ((consider_inline_params || item.empty()) && q == -1)
					{
						q = c;
						if (i > 0 && str[i - 1] == '$') u = true;
						if (c == '\'') w = true;
					}
					else
					{
						item += str.substr(last_nonspace, i + 1 - last_nonspace);
					}
					last_nonspace = i + 1;
				}
				else if (q == -1 && c == ',')
				{
					result.data().emplace_back();
					result.data().rbegin()->swap(item);
					last_nonspace = i + 1;
				}
				else if (!is_whitespace(c) && (c != '$' || str[i + 1] != '"' && str[i + 1] != '\''))
				{
					if (c == '$' && w)
					{
						item += str.substr(last_nonspace, i - last_nonspace);
						item += wrap_special(SPECIAL_MISSING_VARIABLE, "");
					}
					else item += str.substr(last_nonspace, i + 1 - last_nonspace);
					last_nonspace = i + 1;
				}
				else if (i == uint32_t(last_nonspace) && item.empty())
				{
					last_nonspace++;
				}
			}
			result.data().push_back(item);
			return result;
		}

		std::string convert_key_autoinc(const std::string& key)
		{
			std::string group_us;
			return is_sequential(key, group_us)
				? group_us + SPECIAL_KEY_AUTOINCREMENT + std::to_string(key_autoinc_index++) : key;
		}

		void parse_ini_finish(current_section_info& c, const std::string& data, const int non_space, str_view& key_view,
			const int started, const std::shared_ptr<variable_scope>& scope)
		{
			if (!c.section_mode() && !c.target_template) return;
			if (key_view.empty()) return;

			str_view value;
			if (started != -1)
			{
				const auto length = 1 + non_space - started;
				value = {data, uint32_t(started), length < 0 ? 0 : uint32_t(length)};
			}
			else
			{
				value = {};
			}

			auto key = key_view.str();
			const auto sc = prepare_section_scope(c, scope);
			const auto new_key = current_params.allow_override || !c.referenced_templates.empty() || (c.section_mode()
				? c.target_section.find(key) == c.target_section.end()
				: gen_find((*c.target_template).values, key) == (*c.target_template).values.end());
			variant splitted;
			if (!split_and_substitute(key, &c, value, sc, &c.referenced_variables, splitted))
			{
				return;
			}

			const auto key_i = key.find_first_of('$');
			if (key_i != std::string::npos && (key[key_i + 1] == '{' || key[key_i + 1] == '\"'))
			{
				variant v;
				if (!split_and_substitute(key, &c, str_view{key}, sc, &c.referenced_variables, v))
				{
					return;
				}
				key = v.as<std::string>();
			}

			if (c.section_mode())
			{
				if (starts_with(key, "@MIXIN") || equals(key, "@"))
				{
					resolve_mixin(c, sc, splitted);
				}
				else if (starts_with(key, "@GENERATOR"))
				{
					resolve_generator(nullptr, "", splitted, sc, c.referenced_variables);
				}
				else if (equals(key, "@ERASE_REFERENCED") && equals(c.section_key, "@INIPP"))
				{
					current_params.erase_referenced = variant(splitted).as<bool>();
				}
				else if (equals(c.section_key, "DEFAULTS"))
				{
					const auto compatible_mode = starts_with(key, "VAR") && !splitted.empty();
					const auto& actual_key = compatible_mode ? splitted.data()[0] : key;
					scope->default_values.set(actual_key, compatible_mode ? variant(splitted).as<variant>(1) : splitted);
				}
				else if (equals(c.section_key, "INCLUDE") && equals(key, "INCLUDE"))
				{
					auto& existing = c.target_section.get(key);
					for (const auto& piece : splitted)
					{
						existing.data().push_back(piece);
					}
				}
				else if (new_key)
				{
					c.target_section.set(convert_key_autoinc(key), splitted);
				}
			}
			else
			{
				// ensure_generator_name_unique(key, c.target_template->values);
				(*c.target_template).values.push_back({key, splitted});
			}
		}

		static bool is_sequential(const std::string& s, std::string& group_with_underscore)
		{
			if (ends_with(s, "_...") || ends_with(s, u8"_…") /* how lucky they are the same size lol */)
			{
				group_with_underscore = s.substr(0, s.size() - 3);
				return true;
			}
			return false;
		}

		struct parse_status
		{
			str_view key;
			int started = -1;
			int end_at = -1;
			bool started_solid{};
		};

		void parse_ini_finish(std::vector<std::unique_ptr<current_section_info>>& cs, const std::string& data, const int non_space,
			parse_status& status, const bool finish_section, const std::shared_ptr<variable_scope>& scope)
		{
			for (auto& s : cs)
			{
				parse_ini_finish(*s, data, non_space, status.key, status.started, scope);
				if (finish_section)
				{
					parse_ini_section_finish(*s, scope);
				}
			}

			status = {};
		}

		static void add_template(std::vector<std::shared_ptr<section_template>>& templates, const std::shared_ptr<section_template>& t)
		{
			templates.push_back(t);
			for (const auto& p : t->parents)
			{
				// TODO: Recursion protection
				add_template(templates, p);
			}
		}

		creating_section& create_section(const std::string& final_name)
		{
			sections.push_back({final_name, {}});
			return sections[sections.size() - 1].second;
		}

		void set_sections(std::vector<std::unique_ptr<current_section_info>>& cs, str_view cs_keys, const std::shared_ptr<variable_scope>& scope)
		{
			str_view final_name;
			auto separator = cs_keys.find_first_of(':');
			auto is_template = false;
			auto is_mixin = false;
			if (separator != std::string::npos)
			{
				final_name = cs_keys.substr(0, separator);
				final_name.trim();

				if (final_name == "INCLUDE")
				{
					auto file = cs_keys.substr(separator + 1);
					file.trim();
					cs.push_back(std::make_unique<current_section_info>(final_name));
					for (const auto& s : cs) s->target_section.set("INCLUDE", file);
					return;
				}

				if (final_name == "FUNCTION")
				{
					auto file = cs_keys.substr(separator + 1);
					file.trim();
					cs.push_back(std::make_unique<current_section_info>(final_name));
					for (const auto& s : cs) s->target_section.set("NAME", file);
					return;
				}

				if (final_name == "USE")
				{
					auto file = cs_keys.substr(separator + 1);
					file.trim();
					cs.push_back(std::make_unique<current_section_info>(final_name));
					for (const auto& s : cs) s->target_section.set("FILE", file);
					return;
				}

				is_template = final_name == "TEMPLATE";
				is_mixin = final_name == "MIXIN";
				cs_keys = cs_keys.substr(separator + 1);
			}

			auto section_names = cs_keys.split(',', true, true);
			if (section_names.empty()) section_names.emplace_back("");

			if (is_template)
			{
				for (const auto& cs_key : section_names)
				{
					auto pieces = split_string(cs_key.str(), " ", true, true);
					auto template_name = pieces[0];
					if (templates_map.find(template_name) == templates_map.end())
					{
						templates_map[template_name] = std::make_unique<section_template>(template_name, scope);
					}

					if (pieces.size() > 2 && (equals(pieces[1], "extends") || equals(pieces[1], "EXTENDS")))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							const auto found = templates_map.find(pieces[k]);
							if (found == templates_map.end())
							{
								templates_map[pieces[k]] = std::make_unique<section_template>(pieces[k], scope);
								templates_map[template_name]->parents.push_back(templates_map[pieces[k]]);
							}
							else
							{
								templates_map[template_name]->parents.push_back(found->second);
							}
						}
					}
					cs.push_back(std::make_unique<current_section_info>(templates_map[template_name]));
				}
				return;
			}

			if (is_mixin)
			{
				for (const auto& cs_key : section_names)
				{
					auto pieces = split_string(cs_key.str(), " ", true, true);

					auto template_name = pieces[0];
					if (mixins_map.find(template_name) == mixins_map.end())
					{
						mixins_map[template_name] = std::make_unique<section_template>(template_name, scope);
					}

					if (pieces.size() > 2 && (equals(pieces[1], "extends") || equals(pieces[1], "EXTENDS")))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							const auto found = mixins_map.find(pieces[k]);
							if (found == mixins_map.end())
							{
								mixins_map[pieces[k]] = std::make_unique<section_template>(pieces[k], scope);
								mixins_map[template_name]->parents.push_back(mixins_map[pieces[k]]);
							}
							else
							{
								mixins_map[template_name]->parents.push_back(found->second);
							}
						}
					}
					cs.push_back(std::make_unique<current_section_info>(mixins_map[template_name]));
				}
				return;
			}

			std::vector<std::shared_ptr<section_template>> referenced_templates;

			auto section_names_inv = section_names;
			std::reverse(section_names_inv.begin(), section_names_inv.end());
			for (const auto& section_name : section_names_inv)
			{
				auto found_template = templates_map.find(section_name.str()); // TODO
				if (found_template != templates_map.end())
				{
					add_template(referenced_templates, found_template->second);
				}
			}

			if (!referenced_templates.empty())
			{
				cs.push_back(std::make_unique<current_section_info>(final_name, referenced_templates));
				return;
			}

			for (const auto& cs_key : section_names)
			{
				cs.push_back(std::make_unique<current_section_info>(cs_key));
			}
		}

		static bool is_quote_working(const char* data, const int from, const int to, const bool allow_$ = true)
		{
			for (auto i = to - 1; i >= from; i--)
			{
				const auto c = data[i];
				if (is_whitespace(c)) continue;
				if (allow_$ && c == '$') return is_quote_working(data, from, i, false);
				return c == ',' && (data[i - 1] != '\\' || data[i - 2] == '\\');
			}
			return true;
		}

		void parse_ini_values(const std::string& data, const std::shared_ptr<variable_scope>& parent_scope)
		{
			std::vector<std::unique_ptr<current_section_info>> cs;
			const auto scope = parent_scope ? parent_scope->inherit() : std::make_shared<variable_scope>();

			parse_status status;
			auto non_space = -1;
			auto consume_comment = false;

			const auto data_size = int(data.size());
			for (auto i = 0; i < data_size; i++)
			{
				const auto c = data[i];
				if (consume_comment || is_whitespace(c) || status.end_at != -1 && c != status.end_at)
				{
					if (c == '\n') consume_comment = false;
				}
				else if (c == '\n' && !(non_space > 0 && data[non_space] == '\\'))
				{
					parse_ini_finish(cs, data, non_space, status, false, scope);
				}
				else if (status.started_solid)
				{
					non_space = i;
				}
				else if (c == ';' || c == '/' && i + 1 < data_size && data[i + 1] == '/')
				{
					parse_ini_finish(cs, data, non_space, status, false, scope);
					consume_comment = true;
				}
				else if (c == '[')
				{
					parse_ini_finish(cs, data, non_space, status, true, scope);
					const auto s = ++i;
					if (s == data_size) continue;
					for (; i < data_size && data[i] != ']'; i++) {}
					cs.clear();
					set_sections(cs, str_view{data, uint32_t(s), uint32_t(i - s)}, scope);
				}
				else if (c == '=')
				{
					if (status.started != -1 && status.key.empty() && !cs.empty())
					{
						status.key = str_view(data, uint32_t(status.started), 1 + non_space > status.started ? 1 + non_space - status.started : 0);
						status.started = -1;
						status.started_solid = false;
						status.end_at = -1;
					}
				}
				else
				{
					if ((c == '"' || c == '\'') && !status.key.empty())
					{
						if (c == status.end_at && (data[i - 1] != '\\' || data[i - 2] == '\\'))
						{
							status.end_at = -1;
						}
						else if (status.end_at == -1 && (status.started == -1 || is_quote_working(data.c_str(), status.started, i)))
						{
							status.end_at = c;
							if (status.started == -1)
							{
								status.started = i;
								status.started_solid = false;
							}
						}
					}
					non_space = i;
					if (status.started == -1)
					{
						status.started = i;
						status.started_solid = c == 'd' && is_solid({data, uint32_t(i)});
					}
				}
			}

			parse_ini_finish(cs, data, non_space, status, true, scope);
		}

		void parse_file(const path& path, const std::shared_ptr<variable_scope>& scope, const size_t vars_fingerprint)
		{
			if (path.empty() || !reader) return;
			mark_processed(path.filename().string(), vars_fingerprint);
			current_params.file = path;
			const auto data = reader->read(path);
			if (data.empty() && current_params.error_handler)
			{
				warn("File is missing or empty: %s", path.string());
			}
			parse_ini_values(data, scope);
		}

		static resulting_section resolve_sequential_keys(creating_section& s)
		{
			resulting_section ret;
			for (const auto& p : s.values)
			{
				const auto x = p.first.find(SPECIAL_KEY_AUTOINCREMENT);
				if (x == std::string::npos)
				{
					ret[p.first] = p.second;
					continue;
				}

				const auto g = p.first.substr(0, x);
				for (auto i = 0U; i < SPECIAL_AUTOINCREMENT_LIMIT; i++)
				{
					auto cand = g + std::to_string(i);
					if (ret.find(cand) == ret.end() && s.find(cand) == s.end())
					{
						ret[cand] = p.second;
						break;
					}
				}
			}
			return ret;
		}

		void resolve_sequential()
		{
			std::unordered_map<std::string, uint32_t> indices;
			std::unordered_map<std::string, creating_section> temp_map;
			for (const auto& p : sections)
			{
				auto key = p.first;
				std::string group_us;
				if (is_sequential(key, group_us))
				{
					auto& counter = indices[group_us];
					key = group_us + std::to_string(counter++);
					for (auto i = 0U; gen_find(sections, key) != sections.end() && i < SPECIAL_AUTOINCREMENT_LIMIT; i++)
					{
						key = group_us + std::to_string(counter++);
					}
				}

				auto existing = temp_map.find(key);
				if (existing != temp_map.end())
				{
					for (auto& r : p.second.values)
					{
						existing->second.set(r.first, r.second);
					}
				}
				else
				{
					temp_map[key] = p.second;
				}
			}

			for (auto& p : temp_map)
			{
				sections_map[p.first] = resolve_sequential_keys(p.second);
			}
		}
	};

	ini_parser::ini_parser(): data_(new ini_parser_data()) { }

	ini_parser::ini_parser(bool allow_includes, const std::vector<path>& resolve_within)
		: data_(new ini_parser_data(allow_includes, resolve_within)) { }

	ini_parser::~ini_parser()
	{
		delete data_;
	}

	const ini_parser& ini_parser::allow_lua(const bool value) const
	{
		data_->current_params.allow_lua = value;
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, ini_parser_error_handler& error_handler) const
	{
		data_->current_params.error_handler = &error_handler;
		data_->parse_ini_values(std::string(data, data_size), {nullptr});
		data_->current_params.error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, ini_parser_error_handler& error_handler) const
	{
		data_->current_params.error_handler = &error_handler;
		data_->parse_ini_values(data, {nullptr});
		data_->reader = {};
		data_->current_params.error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, const ini_parser_reader& reader,
		ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->current_params.error_handler = &error_handler;
		data_->parse_ini_values(std::string(data, data_size), {nullptr});
		data_->reader = {};
		data_->current_params.error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, const ini_parser_reader& reader, ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->current_params.error_handler = &error_handler;
		data_->parse_ini_values(data, {nullptr});
		data_->reader = {};
		data_->current_params.error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse_file(const path& path, const ini_parser_reader& reader, ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->current_params.error_handler = &error_handler;
		data_->parse_file(path, {nullptr}, 0);
		data_->reader = {};
		data_->current_params.error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::finalize() const
	{
		finalize_end();
		return *this;
	}

	// Version to avoid warning with static analyzers
	void ini_parser::finalize_end() const
	{
		data_->resolve_sequential();
		if (data_->current_params.allow_lua && lua_state.ptr && (!lua_state.is_clean || lua_state.used > 40))
		{
			lua_close(lua_state.ptr);
			lua_state.ptr = nullptr;
		}
	}

	template <typename T>
	std::string gen_to_ini(const T& sections)
	{
		std::stringstream stream;

		const auto it = gen_find(sections, "");
		if (it != sections.end())
		{
			for (const auto& section_line : it->second)
			{
				stream << section_line.first << '=';
				size_t i = 0;
				for (const auto& item : section_line.second)
				{
					if (i++ != 0)
					{
						stream << ',';
					}
					stream << item;
				}
				stream << std::endl;
			}
			stream << std::endl;
		}

		std::vector<std::pair<std::string, resulting_section>> elems(sections.begin(), sections.end());
		std::sort(elems.begin(), elems.end(), sort_sections);
		for (const auto& section : elems)
		{
			if (section.first.empty()) continue;
			stream << '[' << section.first << ']' << std::endl;

			std::vector<std::pair<std::string, variant>> items(section.second.begin(), section.second.end());
			std::sort(items.begin(), items.end(), sort_items);
			for (const auto& section_line : items)
			{
				stream << section_line.first << '=';
				size_t i = 0;
				for (const auto& item : section_line.second)
				{
					if (i++ != 0)
					{
						stream << ',';
					}
					stream << item;
				}
				stream << std::endl;
			}
			stream << std::endl;
		}

		return stream.str();
	}

	template <typename T>
	std::string gen_to_json(const T& sections, bool format)
	{
		using namespace nlohmann;
		auto result = json::object();
		for (const auto& s : sections)
		{
			for (const auto& k : s.second)
			{
				auto& v = result[s.first][k.first] = json::array();
				for (const auto& d : k.second)
				{
					v.push_back(d);
				}
			}
		}

		std::stringstream r;
		if (format) r << std::setw(2) << result << '\n';
		else r << result << '\n';
		return r.str();
	}

	std::string ini_parser::to_ini() const
	{
		return gen_to_ini(data_->sections_map);
	}

	std::string ini_parser::to_json(bool format) const
	{
		return gen_to_json(data_->sections_map, format);
	}

	const std::unordered_map<std::string, resulting_section>& ini_parser::get_sections() const
	{
		return data_->sections_map;
	}

	std::string ini_parser::to_ini(const sections_map& sections)
	{
		return gen_to_ini(sections);
	}

	std::string ini_parser::to_json(const sections_map& sections, bool format)
	{
		return gen_to_json(sections, format);
	}
}
