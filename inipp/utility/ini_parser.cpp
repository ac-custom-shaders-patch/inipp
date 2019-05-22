#include "stdafx.h"
#include "ini_parser.h"
#include "variant.h"
#include <lua.hpp>
#include <utility/alphanum.h>
#include <utility/json.h>
#include <iomanip>

#ifdef USE_SIMPLE
#define DBG(v) std::cout << "[" << __func__ << ":" << __LINE__ << "] " << #v << "=" << (v) << '\n';
#define HERE std::cout << __FILE__ << ":" << __func__ << ":" << __LINE__ << '\n';
#endif

namespace utils
{
	using section = std::unordered_map<std::string, variant>;

	static void trim_self(std::string& str, const char* chars = " \t\r")
	{
		const auto i = str.find_first_not_of(chars);
		if (i == std::string::npos)
		{
			str = std::string();
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

	static bool matches_from(const std::string& s, int index, const char* c)
	{
		for (auto h = c; *h; h++)
		{
			if (index >= int(s.size()) || s[index++] != *h) return false;
		}
		return true;
	}

	inline bool is_solid(const std::string& s, int index)
	{
		return index >= 0 && matches_from(s, index, "data:image/png;base64,");
	}

	inline bool is_identifier_part(char c, bool is_first)
	{
		return !((is_first || !isdigit(c)) && !isupper(c) && !islower(c) && c != '_');
	}

	static bool is_identifier(const std::string& s)
	{
		for (auto i = 0, t = int(s.size()); i < t; i++)
		{
			if (!is_identifier_part(s[i], i == 0)) return false;
		}
		return true;
	}

	static bool sort_sections(const std::pair<std::string, section>& a, const std::pair<std::string, section>& b)
	{
		return doj::alphanum_comp(a.first, b.first) < 0;
	}

	static bool sort_items(const std::pair<std::string, variant>& a, const std::pair<std::string, variant>& b)
	{
		return doj::alphanum_comp(a.first, b.first) < 0;
	}

	struct variable_scope
	{
		std::vector<section> scopes;

		const variant* find(const std::string& name) const
		{
			for (const auto& s : scopes)
			{
				const auto v = s.find(name);
				if (v != s.end())
				{
					return &v->second;
				}
			}
			return nullptr;
		}
	};

	struct script_params
	{
		path file;
		const ini_parser_error_handler* error_handler{};
		bool allow_lua;
	};

	static const std::string SPECIAL_MISSING_VARIABLE = "[[SPEC:MISSING:";
	static const std::string SPECIAL_CALCULATE = "[[SPEC:CALCULATE:";
	static const std::string SPECIAL_END = ":SPEC]]";

	std::string wrap_special(const std::string& special, const std::string& value)
	{
		return special + value + SPECIAL_END;
	}

	static struct
	{
		lua_State* ptr{};
		bool is_clean{};
		std::vector<std::string> imported;
	} lua_state;

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
			lua_state.is_clean = true;
		}
		return lua_state.ptr;
	}

	static void lua_calculate(std::vector<std::string>& dest, const std::string& expr,
		const std::string& prefix, const std::string& postfix,
		const path& file, const ini_parser_error_handler* handler)
	{
		const auto L = lua_get_state();
		if (luaL_loadstring(L, expr.c_str()) || lua_pcall(L, 0, -1, 0))
		{
			if (handler) handler->on_error(file, lua_tolstring(L, -1, nullptr));
			if (!prefix.empty() || !postfix.empty()) dest.push_back(prefix + postfix);
			return;
		}

		if (lua_istable(L, -1))
		{
			lua_pushvalue(L, -1); // stack now contains: -1 => table
			lua_pushnil(L); // stack now contains: -1 => nil; -2 => table
			while (lua_next(L, -2))
			{
				// stack now contains: -1 => value; -2 => key; -3 => table
				// copy the key so that lua_tostring does not modify the original
				lua_pushvalue(L, -2); // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
				dest.push_back(prefix + lua_tostring(L, -2) + postfix);
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
		dest.push_back(s ? prefix + s + postfix : prefix + postfix);
	}

	static void lua_register_function(const std::string& name, const variant& args, const std::string& body,
		bool is_shared, const path& file, const ini_parser_error_handler* handler)
	{
		std::string args_line;
		for (auto& arg : args.data())
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
		int abs_i = lua_absindex(L, i);
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
		lua_pushcfunction( L, lua_traceback_fn );
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

	static void lua_import(const utils::path& name, bool is_shared, const path& file, const ini_parser_error_handler* handler)
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
		std::vector<std::string>& dest;
		script_params params;
		bool process_values;

		value_finalizer(std::vector<std::string>& dest, const script_params& params, bool process_values = true)
			: dest(dest), params(params), process_values(process_values) { }

		static void fix_word(std::string& result, const std::string& word)
		{
			if (word == "abs") result += "math.abs";
			else if (word == "acos") result += "math.acos";
			else if (word == "asin") result += "math.asin";
			else if (word == "atan") result += "math.atan";
			else if (word == "ceil") result += "math.ceil";
			else if (word == "cos") result += "math.cos";
			else if (word == "deg") result += "math.deg";
			else if (word == "exp") result += "math.exp";
			else if (word == "floor") result += "math.floor";
			else if (word == "fmod" || word == "mod") result += "math.fmod";
			else if (word == "log") result += "math.log";
			else if (word == "max") result += "math.max";
			else if (word == "min") result += "math.min";
			else if (word == "pi") result += "math.pi";
			else if (word == "pow") result += "math.pow";
			else if (word == "rad") result += "math.rad";
			else if (word == "sin") result += "math.sin";
			else if (word == "sqrt") result += "math.sqrt";
			else if (word == "tan") result += "math.tan";
			else result += word;
		}

		static std::string fix_expression(const std::string& expr)
		{
			std::string result = "return (";
			auto b = -1;
			for (auto i = 0, t = int(expr.size()); i < t; i++)
			{
				const auto c = expr[i];
				if (islower(c))
				{
					if (b == -1) b = i;
				}
				else
				{
					if (b != -1)
					{
						if (b != 0 && (expr[b - 1] == '.' || expr[b - 1] == ':')) result += expr.substr(b, i - b);
						else fix_word(result, expr.substr(b, i - b));
						b = -1;
					}
					result += c;
				}
			}
			result += ")";
			return result;
		}

		void calculate(const std::string& expr, const std::string& prefix, const std::string& postfix) const
		{
			if (!params.allow_lua)
			{
				if (!prefix.empty() || !postfix.empty()) dest.push_back(prefix + postfix);
				return;
			}
			lua_calculate(dest, fix_expression(expr), prefix, postfix, params.file, params.error_handler);
		}

		void unwrap(std::string& value, const std::string& type) const
		{
			const auto spec = value.find(type);
			if (spec == std::string::npos) return;

			const auto end = value.find(SPECIAL_END, spec + type.size());
			if (end == std::string::npos) return;

			const auto arg = value.substr(spec + type.size(), end - spec - type.size());

			auto orig = value;
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
				dest.push_back(value);
				return;
			}

			const auto end = value.find(SPECIAL_END, spec + SPECIAL_CALCULATE.size());
			if (end == std::string::npos)
			{
				dest.push_back(value);
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
				dest.push_back(value);
				return;
			}

			if (value.find(SPECIAL_CALCULATE) == 0)
			{
				auto modified = value;
				unwrap(modified, SPECIAL_MISSING_VARIABLE);
				unwrap_calculate(modified);
				return;
			}

			dest.push_back(value);
			unwrap(dest[dest.size() - 1], SPECIAL_MISSING_VARIABLE);
		}
	};

	struct variable_info
	{
		enum class special_mode
		{
			none,
			size
		};

		std::string name;
		int substr_from = 0, substr_to = std::numeric_limits<int>::max();
		bool with_fallback;
		special_mode mode{};

		variable_info() : with_fallback(false) {}
		variable_info(const std::string& name) : name(name), with_fallback(true) {}
		variable_info(const std::string& name, int from, int to) : name(name), substr_from(from), substr_to(to), with_fallback(false) {}
		variable_info(const std::string& name, special_mode mode) : name(name), with_fallback(false), mode(mode) {}

		bool get_values(const variable_scope& include_vars, std::vector<std::string>& result)
		{
			const auto v = include_vars.find(name);
			if (!v)
			{
				if (mode == special_mode::size)
				{
					result.emplace_back("0");
					return true;
				}
				return false;
			}

			switch (mode)
			{
			case special_mode::size:
				{
					result.push_back(std::to_string(v->data().size()));
					break;
				}
			case special_mode::none:
				{
					if (substr_to < 0 || substr_to == 0 && substr_from < 0) substr_to += int(v->data().size());
					if (substr_from < 0) substr_from += int(v->data().size());
					for (auto j = substr_from, jt = int(v->data().size()); j < jt && j < substr_to; j++)
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

		void substitute(const variable_scope& include_vars, const value_finalizer& dest)
		{
			std::vector<std::string> result;
			if (get_values(include_vars, result))
			{
				for (const auto& r : result) dest.add(r);
			}
			else if (with_fallback)
			{
				dest.add(wrap_special(SPECIAL_MISSING_VARIABLE, name));
			}
			else if (dest.params.error_handler)
			{
				dest.params.error_handler->on_warning(dest.params.file, ("Missing variable: " + name).c_str());
			}
		}

		static void substitute_wrap(std::string& s)
		{
			for (auto c : s)
			{
				if (!isdigit(c) && c != '-' && c != '.' && c != '+' && c != 'e' && c != 'E')
				{
					s = "\"" + s + "\"";
					return;
				}
			}
		}

		void substitute(const variable_scope& include_vars, const std::string& prefix, const std::string& postfix, const value_finalizer& dest,
			bool expr_mode)
		{
			std::vector<std::string> result;
			if (!get_values(include_vars, result) && !expr_mode)
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
				if (!with_fallback && dest.params.error_handler)
				{
					dest.params.error_handler->on_warning(dest.params.file, ("Missing variable: " + name).c_str());
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
					substitute_wrap(result[0]);
					s += result[0];
				}
				else
				{
					s += "{";
					for (auto j = 0, jt = int(result.size()); j < jt; j++)
					{
						if (j) s += ",";
						substitute_wrap(result[j]);
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

	static variable_info check_variable(const std::string& s)
	{
		if (s.size() < 2) return {};
		if (s[0] != '$') return {};
		if (s[1] == '{' && s[s.size() - 1] == '}')
		{
			const auto pieces = split_string(s.substr(2, s.size() - 3), ":", false, true);
			if (pieces.empty() || pieces.size() > 3) return {};
			if (!is_identifier(pieces[0])) return {};
			if (pieces.size() == 2 && (pieces[1] == "size" || pieces[1] == "count" || pieces[1] == "length"))
			{
				return {pieces[0], variable_info::special_mode::size};
			}
			if (pieces.size() > 1 && (pieces[1].empty() || !isdigit(pieces[1][0]) && pieces[1][0] != '-')) return {};
			if (pieces.size() > 2 && (pieces[2].empty() || !isdigit(pieces[2][0]) && pieces[2][0] != '-')) return {};
			const auto from = pieces.size() == 1 ? 0 : std::stoi(pieces[1]);
			const auto to = pieces.size() == 1 ? std::numeric_limits<int>::max() : pieces.size() == 2 ? from + 1 : std::stoi(pieces[2]);
			return {pieces[0], from, to};
		}
		const auto vname = s.substr(1);
		if (!is_identifier(vname)) return {};
		return {vname};
	}

	static void substitute_variable(const std::string& value, const variable_scope& include_vars, const value_finalizer& dest, int stack,
		std::vector<std::string>* referenced_variables)
	{
#if defined _DEBUG && defined USE_SIMPLE
		if (stack > 9)
		{
			std::cerr << "Stack overflow: " << stack << ", value=" << value;
		}
#endif

		if (stack < 10)
		{
			auto var = check_variable(value);
			if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
			if (!var.name.empty())
			{
				// Either $VariableName or ${VariableName}
				std::vector<std::string> temp;
				const auto finalizer = value_finalizer{temp, dest.params, false};
				var.substitute(include_vars, finalizer);
				for (const auto& v : temp)
				{
					substitute_variable(v, include_vars, dest, stack + 1, referenced_variables);
				}
				return;
			}

			auto expr_mode = value.find(SPECIAL_CALCULATE) == 0;

			{
				// Concatenation with ${VariableName}
				const auto var_begin = value.find("${");
				const auto var_end = var_begin == std::string::npos ? std::string::npos : value.find_first_of('}', var_begin);
				if (var_end != std::string::npos)
				{
					var = check_variable(value.substr(var_begin, var_end - var_begin + 1));
					if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
					std::vector<std::string> temp;
					const auto finalizer = value_finalizer{temp, dest.params, false};
					var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end + 1), finalizer, expr_mode);
					for (const auto& v : temp)
					{
						substitute_variable(v, include_vars, dest, stack + 1, referenced_variables);
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
						if (is_identifier_part(value[i], i == var_begin + 1)) var_end++;
						else break;
					}
					if (var_end != var_begin + 1)
					{
						var = check_variable(value.substr(var_begin, var_end - var_begin));
						if (!var.name.empty() && referenced_variables) referenced_variables->push_back(var.name);
						std::vector<std::string> temp;
						const auto finalizer = value_finalizer{temp, dest.params, false};
						var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end), finalizer, expr_mode);
						for (const auto& v : temp)
						{
							substitute_variable(v, include_vars, dest, stack + 1, referenced_variables);
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
		// std::string result_name;
		std::string name;
		section values;
		section include_vars;
		std::vector<section_template*> parents;
		std::vector<section_template*> referenced_mixins;
	};

	struct current_section_info
	{
		std::string section_key;
		section* target_section;
		section_template* target_template;
		std::vector<section_template*> referenced_templates;
		std::vector<section_template*> referenced_section_mixins;
	};

	using ini_data = std::unordered_map<std::string, section>;

	struct ini_parser_data
	{
		ini_data* own_sections{};
		ini_data& sections;
		std::unordered_map<std::string, section_template> templates;
		std::unordered_map<std::string, section_template> mixins;
		bool allow_includes = false;
		bool allow_override = true;
		bool allow_lua = true;
		std::vector<path> resolve_within;
		std::vector<std::string> processed;
		section include_vars;
		utils::path current_file;
		const ini_parser_reader* reader{};
		const ini_parser_error_handler* error_handler{};

		ini_parser_data(ini_data& sections)
			: sections(sections) { }

		ini_parser_data(ini_data& sections, bool allow_includes, const std::vector<path>& resolve_within)
			: sections(sections), allow_includes(allow_includes), resolve_within(resolve_within) { }

		ini_parser_data() : own_sections(new ini_data()), sections(*own_sections) { }

		ini_parser_data(bool allow_includes, const std::vector<path>& resolve_within)
			: own_sections(new ini_data()), sections(*own_sections), allow_includes(allow_includes), resolve_within(resolve_within) { }

		~ini_parser_data()
		{
			delete own_sections;
		}

		utils::path find_referenced(const std::string& file_name)
		{
			auto inc = file_name;
			trim_self(inc);
			const auto name = path(inc).filename().string();
			auto skip = false;
			for (auto& processed_value : processed)
			{
				if (_stricmp(processed_value.c_str(), name.c_str()) == 0)
				{
					skip = true;
					break;
				}
			}
			if (skip) return {};

			for (auto i = -1, t = int(resolve_within.size()); i < t; i++)
			{
				const auto filename = (i == -1 ? current_file.parent_path() : resolve_within[i]) / inc;
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

		value_finalizer get_value_finalizer(std::vector<std::string>& dest) const
		{
			script_params params;
			params.file = current_file;
			params.error_handler = error_handler;
			params.allow_lua = allow_lua;
			return {dest, params, true};
		}

		bool resolve_mixins(const variable_scope& sc, const std::vector<section_template*>& mixins,
			current_section_info& c, std::vector<std::string>& referenced_variables, int scope) const
		{
			if (scope > 10) return true;
			for (auto m : mixins)
			{
				auto scm = sc;
				scm.scopes.push_back(m->include_vars);
				scm.scopes.push_back(m->values);

				auto m_active = m->values.find("@ACTIVE");
				if (m_active != m->values.end())
				{
					variant m_active_v;
					for (const auto& piece : m_active->second.data())
					{
						substitute_variable(piece, scm, get_value_finalizer(m_active_v.data()), 0, &referenced_variables);
					}
					if (!m_active_v.as<bool>()) goto NextMixin_1;
				}

				for (const auto& v : m->values)
				{
					if (v.first == "@ACTIVE") continue;
					if ((*c.target_section).find(v.first) != (*c.target_section).end()) continue;
					auto& dest = (*c.target_section)[v.first].data();
					for (const auto& piece : v.second.data())
					{
						substitute_variable(piece, scm, get_value_finalizer(dest), 0, &referenced_variables);
					}
					if (v.first == "ACTIVE" && !(*c.target_section)[v.first].as<bool>())
					{
						(*c.target_section).clear();
						(*c.target_section)["ACTIVE"] = "0";
						return false;
					}
				}

				if (!resolve_mixins(sc, m->referenced_mixins, c, referenced_variables, scope + 1))
				{
					return false;
				}

			NextMixin_1: {}
			}
			return true;
		}

		void parse_ini_section_finish(current_section_info& c)
		{
			if (!c.target_section) return;

			if (!c.referenced_templates.empty())
			{
				std::vector<std::string> referenced_variables;
				for (auto t : c.referenced_templates)
				{
					variable_scope sc;
					sc.scopes.push_back(*c.target_section);
					sc.scopes.push_back(include_vars);
					sc.scopes.push_back(t->include_vars);
					sc.scopes.push_back(t->values);
					if (!resolve_mixins(sc, t->referenced_mixins, c, referenced_variables, 0))
					{
						return;
					}

					for (const auto& v : t->values)
					{
						if (v.first == "@OUTPUT") continue;
						if ((*c.target_section).find(v.first) != (*c.target_section).end()) continue;
						auto& dest = (*c.target_section)[v.first].data();
						for (const auto& piece : v.second.data())
						{
							substitute_variable(piece, sc, get_value_finalizer(dest), 0, &referenced_variables);
						}
						if (v.first == "ACTIVE" && !(*c.target_section)[v.first].as<bool>())
						{
							(*c.target_section).clear();
							(*c.target_section)["ACTIVE"] = "0";
							return;
						}
					}
				}
				for (const auto& v : referenced_variables)
				{
					(*c.target_section).erase(v);
				}
			}

			{
				std::vector<std::string> referenced_variables;
				variable_scope sc;
				sc.scopes.push_back(*c.target_section);
				sc.scopes.push_back(include_vars);
				if (!resolve_mixins(sc, c.referenced_section_mixins, c, referenced_variables, 0))
				{
					return;
				}
				for (const auto& v : referenced_variables)
				{
					(*c.target_section).erase(v);
				}
			}

			if (c.section_key == "FUNCTION")
			{
				lua_register_function(
					(*c.target_section)["NAME"].as<std::string>(),
					(*c.target_section)["ARGUMENTS"],
					(*c.target_section)["CODE"].as<std::string>(),
					!(*c.target_section)["PRIVATE"].as<bool>(),
					current_file, error_handler);
				c.target_section->clear();
			}
			else if (c.section_key == "USE")
			{
				const auto name = (*c.target_section)["FILE"].as<std::string>();
				const auto referenced = find_referenced(name);
				if (exists(referenced)) lua_import(referenced, !(*c.target_section)["PRIVATE"].as<bool>(), current_file, error_handler);
				else if (error_handler) error_handler->on_warning(current_file, ("Referenced file is missing: " + name).c_str());
				c.target_section->clear();
			}
			else if (c.section_key == "INCLUDE")
			{
				auto to_include = c.target_section->find("INCLUDE");
				if (to_include != c.target_section->end())
				{
					const auto previous_file = current_file;
					const auto previous_vars = include_vars;

					for (const auto& p : *c.target_section)
					{
						if (p.first != "INCLUDE") include_vars[p.first] = p.second;
					}

					for (auto& i : to_include->second.data())
					{
						parse_file(find_referenced(i));
					}

					current_file = previous_file;
					include_vars = previous_vars;
				}
				c.target_section->clear();
			}

			if (c.target_section->empty())
			{
				sections.erase(c.section_key);
			}
		}

		static bool is_whitespace(char c)
		{
			return c == ' ' || c == '\t' || c == '\r';
		}

		static std::vector<std::string> split_string_quotes(std::string& str)
		{
			std::vector<std::string> result;

			if (is_solid(str, 0))
			{
				result.push_back(str);
				return result;
			}

			auto last_nonspace = 0;
			auto q = -1;
			auto u = false, w = false;
			std::string item;
			for (auto i = 0, t = int(str.size()); i < t; i++)
			{
				const auto c = str[i];

				if (c == '\\' && i < t - 1 && ((str[i + 1] == '"' || str[i + 1] == '\'') && (item.empty() || q != -1) || str[i + 1] == ','))
				{
					item += str.substr(last_nonspace, i - last_nonspace);
					i++;
					item += str[i];
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
					else if (item.empty())
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
					result.push_back(item);
					item.clear();
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
				else if (i == last_nonspace && item.empty())
				{
					last_nonspace++;
				}
			}
			result.push_back(item);
			return result;
		}

		void parse_ini_finish(current_section_info& c, const std::string& data, const int non_space, std::string& key,
			int& started, int& end_at)
		{
			if (!c.target_section && !c.target_template) return;
			if (key.empty()) return;

			std::string value;
			if (started != -1)
			{
				const auto length = 1 + non_space - started;
				value = length < 0 ? "" : data.substr(started, length);
			}
			else
			{
				value = "";
			}

			const auto new_key = allow_override || !c.referenced_templates.empty() || (c.target_section
				? (*c.target_section).find(key) == (*c.target_section).end()
				: (*c.target_template).values.find(key) == (*c.target_template).values.end());

			std::vector<std::string> value_splitted;
			for (const auto& value_piece : split_string_quotes(value))
			{
				if (c.section_key == "DEFAULTS" || c.section_key == "INCLUDE" || c.target_template)
				{
					value_splitted.push_back(value_piece);
				}
				else
				{
					variable_scope sc;
					if (!c.referenced_templates.empty())
					{
						sc.scopes.push_back(*c.target_section);
						for (auto r : c.referenced_templates)
						{
							sc.scopes.push_back(r->values); // Could be a problem?
							sc.scopes.push_back(r->include_vars);
						}
					}
					sc.scopes.push_back(include_vars);
					substitute_variable(value_piece, sc, get_value_finalizer(value_splitted), 0, nullptr);
				}
			}

			if (key.find("@MIXIN") == 0)
			{
				if (c.target_section) c.referenced_section_mixins.push_back(&mixins[value_splitted[0]]);
				else c.target_template->referenced_mixins.push_back(&mixins[value_splitted[0]]);
			}
			else if (c.target_section)
			{
				if (key == "ACTIVE" && !variant(value_splitted).as<bool>())
				{
					(*c.target_section)[key] = value_splitted;
					c.target_section = nullptr;
				}
				else if (c.section_key == "DEFAULTS")
				{
					if (include_vars.find(key) == include_vars.end())
					{
						include_vars[key] = value_splitted;
					}
				}
				else if (new_key)
				{
					(*c.target_section)[key] = value_splitted;
				}
				else if (c.section_key == "INCLUDE" && key == "INCLUDE")
				{
					auto& existing = (*c.target_section)[key];
					for (const auto& piece : value_splitted)
					{
						existing.data().push_back(piece);
					}
				}
			}
			else
			{
				(*c.target_template).values[key] = value_splitted;
			}
		}

		void parse_ini_finish(std::vector<current_section_info>& cs, const std::string& data, const int non_space, std::string& key,
			int& started, int& end_at, bool finish_section)
		{
			for (auto& s : cs)
			{
				parse_ini_finish(s, data, non_space, key, started, end_at);
				if (finish_section)
				{
					parse_ini_section_finish(s);
				}
			}

			key.clear();
			started = -1;
			end_at = -1;
		}

		static void parse_ini_fix_array(std::string& key)
		{
			static auto seq_counter = 0;
			const auto is_array = key.size() > 4 && key.substr(key.size() - 4) == "_...";
			if (is_array)
			{
				key = key.substr(0, key.size() - 3) + ":$SEQ:" + std::to_string(++seq_counter);
			}
		}

		static void add_template(std::vector<section_template*>& templates, section_template* t)
		{
			templates.push_back(t);
			for (auto p : t->parents)
			{
				// TODO: Recursion protection
				add_template(templates, p);
			}
		}

		void set_sections(std::vector<current_section_info>& cs, std::string& cs_keys)
		{
			std::string final_name;
			auto separator = cs_keys.find_first_of(':');
			auto is_template = false;
			auto is_mixin = false;
			if (separator != std::string::npos)
			{
				final_name = cs_keys.substr(0, separator);
				trim_self(final_name);

				if (final_name == "INCLUDE")
				{
					auto file = cs_keys.substr(separator + 1);
					trim_self(file);
					cs.push_back({final_name, &sections[final_name], nullptr, {}, {}});
					(*cs[0].target_section)["INCLUDE"] = file;
					return;
				}

				if (final_name == "FUNCTION")
				{
					auto file = cs_keys.substr(separator + 1);
					trim_self(file);
					cs.push_back({final_name, &sections[final_name], nullptr, {}, {}});
					(*cs[0].target_section)["NAME"] = file;
					return;
				}

				if (final_name == "USE")
				{
					auto file = cs_keys.substr(separator + 1);
					trim_self(file);
					cs.push_back({final_name, &sections[final_name], nullptr, {}, {}});
					(*cs[0].target_section)["FILE"] = file;
					return;
				}

				is_template = final_name == "TEMPLATE";
				is_mixin = final_name == "MIXIN";
				cs_keys = cs_keys.substr(separator + 1);
			}

			const auto section_names = split_string(cs_keys, ",", true, true);

			if (is_template)
			{
				for (const auto& cs_key : section_names)
				{
					auto pieces = split_string(cs_key, " ", true, true);

					auto template_name = pieces[0];
					templates[template_name].name = pieces[0];
					templates[template_name].include_vars = include_vars;
					if (pieces.size() > 2 && (pieces[1] == "extends" || pieces[1] == "EXTENDS"))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							templates[template_name].parents.push_back(&templates[pieces[k]]);
						}
					}
					cs.push_back({"", nullptr, &templates[template_name], {}, {}});
				}
				return;
			}

			if (is_mixin)
			{
				for (const auto& cs_key : section_names)
				{
					auto pieces = split_string(cs_key, " ", true, true);

					auto template_name = pieces[0];
					mixins[template_name].name = pieces[0];
					mixins[template_name].include_vars = include_vars;
					if (pieces.size() > 2 && (pieces[1] == "extends" || pieces[1] == "EXTENDS"))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							mixins[template_name].parents.push_back(&mixins[pieces[k]]);
						}
					}
					cs.push_back({"", nullptr, &mixins[template_name], {}, {}});
				}
				return;
			}

			std::vector<section_template*> referenced_templates;
			auto section_names_inv = section_names;
			std::reverse(section_names_inv.begin(), section_names_inv.end());
			for (const auto& section_name : section_names_inv)
			{
				auto found_template = templates.find(section_name);
				if (found_template != templates.end())
				{
					add_template(referenced_templates, &found_template->second);
				}
			}

			if (!referenced_templates.empty())
			{
				if (final_name.empty())
				{
					for (auto t : referenced_templates)
					{
						auto name = t->values.find("@OUTPUT");
						if (name != t->values.end())
						{
							variable_scope sc;
							sc.scopes.push_back(t->include_vars);
							sc.scopes.push_back(include_vars);
							std::vector<std::string> dest;
							for (const auto& piece : name->second.data())
							{
								substitute_variable(piece, sc, get_value_finalizer(dest), 0, nullptr);
								if (!dest.empty()) break;
							}
							final_name = dest[0];
							break;
						}
					}
				}
				parse_ini_fix_array(final_name);
				auto& templated = sections[final_name];
				cs.push_back({final_name, &templated, nullptr, referenced_templates, {}});
				/*auto& cc = cs[cs.size() - 1];
				for (auto t : referenced_templates)
				{
					for (auto& m : t->referenced_mixins)
					{
						cc.referenced_section_mixins[m.first] = m.second;
					}
				}*/
				return;
			}

			for (auto cs_key : section_names)
			{
				parse_ini_fix_array(cs_key);
				cs.push_back({cs_key, &sections[cs_key], nullptr, {}, {}});
			}
		}

		void parse_ini_values(const char* data, const int data_size)
		{
			std::vector<current_section_info> cs;

			auto started = -1;
			auto non_space = -1;
			auto end_at = -1;
			std::string key;

			for (auto i = 0; i < data_size; i++)
			{
				const auto c = data[i];
				switch (c)
				{
				case '[':
					{
						if (end_at != -1 || is_solid(data, started)) goto LAB_DEF;
						parse_ini_finish(cs, data, non_space, key, started, end_at, true);

						const auto s = ++i;
						if (s == data_size) break;
						for (; i < data_size && data[i] != ']'; i++) {}
						auto cs_keys = std::string(&data[s], i - s);
						cs.clear();
						set_sections(cs, cs_keys);
						break;
					}

				case '\n':
					{
						if (end_at != -1) goto LAB_DEF;
						parse_ini_finish(cs, data, non_space, key, started, end_at, false);
						break;
					}

				case '=':
					{
						if (end_at != -1 || is_solid(data, started)) goto LAB_DEF;
						if (started != -1 && key.empty() && !cs.empty())
						{
							key = std::string(&data[started], 1 + non_space - started);
							started = -1;
							end_at = -1;
						}
						break;
					}

				case '/':
					{
						if (end_at != -1 || is_solid(data, started)) goto LAB_DEF;
						if (i + 1 < data_size && data[i + 1] == '/')
						{
							goto LAB_SEMIC;
						}
						goto LAB_DEF;
					}

				case ';':
					{
						if (end_at != -1 || is_solid(data, started)) goto LAB_DEF;
					LAB_SEMIC:
						parse_ini_finish(cs, data, non_space, key, started, end_at, false);
						for (i++; i < data_size && data[i] != '\n'; i++) {}
						break;
					}

				case '"':
				case '\'':
					{
						if (!key.empty())
						{
							if (end_at == -1)
							{
								end_at = c;
								if (started == -1)
								{
									started = i;
									non_space = i;
								}
							}
							else if (c == end_at)
							{
								end_at = -1;
							}
						}
					}

				default:
					{
					LAB_DEF:
						if (!is_whitespace(c))
						{
							non_space = i;
							if (started == -1)
							{
								started = i;
							}
						}
						break;
					}
				}
			}

			parse_ini_finish(cs, data, non_space, key, started, end_at, true);
		}

		void parse_file(const path& path)
		{
			if (path.empty() || !reader) return;
			processed.push_back(path.filename().string());
			current_file = path;
			auto data = reader->read(path);
			if (data.empty() && error_handler)
			{
				error_handler->on_warning(path, "File is missing or empty");
			}
			parse_ini_values(data.c_str(), int(data.size()));
		}

		void resolve_sequential() const
		{
			for (const auto& p : sections)
			{
				const auto index = p.first.find(":$SEQ:");
				if (index != std::string::npos) goto Remap;
			}
			return;

		Remap:
			std::vector<std::pair<std::string, section>> elems(sections.begin(), sections.end());
			std::sort(elems.begin(), elems.end(), sort_sections);

			ini_data renamed;
			for (const auto& p : elems)
			{
				const auto index = p.first.find(":$SEQ:");
				if (index == std::string::npos)
				{
					renamed[p.first] = p.second;
					continue;
				}

				auto prefix = p.first.substr(0, index);
				for (auto i = 0; i < 100000; i++)
				{
					auto candidate = prefix + std::to_string(i);
					if (sections.find(candidate) == sections.end()
						&& renamed.find(candidate) == renamed.end())
					{
						renamed[candidate] = p.second;
						break;
					}
				}
			}
			sections = renamed;
		}
	};

	ini_parser::ini_parser(ini_data& sections): data_(new ini_parser_data(sections)) { }

	ini_parser::ini_parser(ini_data& sections, bool allow_includes, const std::vector<path>& resolve_within)
		: data_(new ini_parser_data(sections, allow_includes, resolve_within)) { }

	ini_parser::ini_parser(): data_(new ini_parser_data()) { }

	ini_parser::ini_parser(bool allow_includes, const std::vector<path>& resolve_within)
		: data_(new ini_parser_data(allow_includes, resolve_within)) { }

	ini_parser::~ini_parser()
	{
		delete data_;
	}

	const ini_parser& ini_parser::allow_lua(const bool value) const
	{
		data_->allow_lua = value;
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, const ini_parser_error_handler& error_handler) const
	{
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data, data_size);
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, const ini_parser_error_handler& error_handler) const
	{
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data.c_str(), int(data.size()));
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, const ini_parser_reader& reader,
		const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data, data_size);
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, const ini_parser_reader& reader, const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data.c_str(), int(data.size()));
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse_file(const path& path, const ini_parser_reader& reader, const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_file(path);
		data_->reader = {};
		data_->error_handler = {};
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
		if (lua_state.ptr && !lua_state.is_clean)
		{
			lua_close(lua_state.ptr);
			lua_state.ptr = nullptr;
		}
	}

	std::string ini_parser::to_ini() const
	{
		return to_ini(data_->sections);
	}

	std::string ini_parser::to_json(bool format) const
	{
		return to_json(data_->sections, format);
	}

	const std::unordered_map<std::string, section>& ini_parser::get_sections() const
	{
		return data_->sections;
	}

	std::string ini_parser::to_ini(const ini_data& sections)
	{
		std::stringstream stream;

		const auto it = sections.find("");
		if (it != sections.end())
		{
			for (const auto& section_line : it->second)
			{
				stream << section_line.first << '=';
				size_t i = 0;
				for (const auto& item : section_line.second.data())
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

		std::vector<std::pair<std::string, section>> elems(sections.begin(), sections.end());
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
				for (const auto& item : section_line.second.data())
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

	std::string ini_parser::to_json(const ini_data& sections, bool format)
	{
		using namespace nlohmann;
		auto result = json::object();
		for (const auto& s : sections)
		{
			for (const auto& k : s.second)
			{
				auto& v = result[s.first][k.first] = json::array();
				for (auto d : k.second.data())
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
}
