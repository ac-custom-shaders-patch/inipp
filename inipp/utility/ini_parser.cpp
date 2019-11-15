// TODO: Use expressions when referring to mixins and other things
// Generators already use them

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

	static bool is_space(char c)
	{
		return c == ' ' || c == '\t' || c == '\r';
	}

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

	inline bool is_identifier_part(char c)
	{
		return c == '_' || isupper(c) || islower(c) || isdigit(c);
	}

	static bool is_identifier(const std::string& s)
	{
		for (auto i = 0, t = int(s.size()); i < t; i++)
		{
			if (!is_identifier_part(s[i])) return false;
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

	struct variable_scope : std::enable_shared_from_this<variable_scope>
	{
		section values;
		std::vector<const section*> local_extensions;
		std::vector<const variable_scope*> local_fallbacks;
		std::shared_ptr<variable_scope> parent;

		void extend(const section& s)
		{
			local_extensions.push_back(&s);
		}

		void fallback(const std::shared_ptr<variable_scope>& s)
		{
			local_fallbacks.push_back(s.get());
		}

		std::shared_ptr<variable_scope> inherit()
		{
			return std::make_shared<variable_scope>(shared_from_this());
		}

		const variant* find(const std::string& name) const
		{
			for (const auto& s : local_extensions)
			{
				const auto v = s->find(name);
				if (v != s->end())
				{
					return &v->second;
				}
			}

			const auto v = values.find(name);
			if (v != values.end())
			{
				return &v->second;
			}

			if (const auto ret = parent ? parent->find(name) : nullptr) return ret;
			for (auto f : local_fallbacks)
			{
				if (const auto ret = f->find(name)) return ret;
			}

			return nullptr;
		}

		//private:
		variable_scope() : parent(nullptr) {}
		variable_scope(const std::shared_ptr<variable_scope>& parent) : parent(parent) {}

		/*template <typename... Args>
		void extend(const section& s, Args&& ...args)
		{
			scopes.push_back(&s);
			extend(args);
		}*/

		/*void bake(section& target) const
		{
			for (const auto& s : scopes)
			{
				for (const auto& p : *s)
				{
					if (target.find(p.first) != target.end()) continue;
					target[p.first] = p.second;
				}
			}

			if (parent) parent->bake(target);
			if (fallback) fallback->bake(target);
		}*/
	};

	/*struct variable_scope_baked
	{
		// In general it seems like a good idea for variable_scope to operate with pointers. Apart from everything,
		// it allows to add neat inheritance system. There is a problem though: templates should hold variables 
		// of their files in their scopes, and they could be used outside of their files, and after files with their scopes 
		// are long gone. Solution: bake scopes for templates when those are created, copying everything within templates.
		// Not a nice solution, and I believe it could be done more optimally with std::shared_ptr, but at the moment I
		// really don’t want to go into all of those std::enable_shared_from_this stuff. I like when ownership of data is
		// clear.

		section flatten;
		variable_scope holding_scope;

		variable_scope_baked() : holding_scope(nullptr){}
		variable_scope_baked(const variable_scope& scope) : holding_scope(nullptr)
		{
			scope.bake(flatten);
			holding_scope.extend(flatten);
		}
	};*/

	struct script_params
	{
		path file;
		const ini_parser_error_handler* error_handler{};
		bool allow_lua{};
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
		#ifdef _DEBUG
		// std::cout << "Lua: `" << expr << "`\n";
		#endif

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
			lua_pushnil(L);       // stack now contains: -1 => nil; -2 => table
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

		bool get_values(const std::shared_ptr<variable_scope>& include_vars, std::vector<std::string>& result)
		{
			const auto v = include_vars->find(name);
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

		void substitute(const std::shared_ptr<variable_scope>& include_vars, const value_finalizer& dest)
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
				if (!isdigit(c) && c != '-' && c != '.' && c != '+' && c != 'e' && c != 'E') goto Wrap;
			}
			return;
		Wrap:
			s = "\"" + s + "\"";
		}

		void substitute(const std::shared_ptr<variable_scope>& include_vars, const std::string& prefix, const std::string& postfix, const value_finalizer& dest,
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

	static void substitute_variable(const std::string& value, const std::shared_ptr<variable_scope>& include_vars, const value_finalizer& dest, int stack,
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
						if (is_identifier_part(value[i])) var_end++;
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
		std::shared_ptr<variable_scope> scope;
		std::vector<std::shared_ptr<section_template>> parents;
		std::vector<std::shared_ptr<section_template>> referenced_mixins;

		section_template(const std::string& name, const std::shared_ptr<variable_scope>& scope)
			: name(name), scope(scope->inherit())
		{ }
	};

	struct current_section_info
	{
		bool terminated{};
		std::string section_key;
		section target_section{};
		std::shared_ptr<section_template> target_template{};
		std::vector<std::shared_ptr<section_template>> referenced_templates;
		std::vector<std::shared_ptr<section_template>> referenced_section_mixins;

		explicit current_section_info(std::shared_ptr<section_template> target_template)
			: target_template(target_template) { }

		current_section_info(const std::string& key)
			: section_key(key) { }

		current_section_info(const std::string& key, std::vector<std::shared_ptr<section_template>> templates)
			: section_key(key), referenced_templates(templates) { }

		bool section_mode() const { return target_template == nullptr && !terminated; }
		void terminate() { terminated = true; }
	};

	using section_named = std::pair<std::string, section>;
	using sections_list = std::vector<section_named>;
	using sections_map = std::unordered_map<std::string, section>;

	auto gen_find(const sections_list& l, const std::string& a) { return std::find_if(l.begin(), l.end(), [=](const section_named& s) { return s.first == a; }); }
	auto gen_find(const sections_map& l, const std::string& a) { return l.find(a); }

	struct ini_parser_data
	{
		sections_list sections;
		sections_map sections_map;
		std::unordered_map<std::string, std::shared_ptr<section_template>> templates_map;
		std::unordered_map<std::string, std::shared_ptr<section_template>> mixins_map;
		bool allow_includes = false;
		bool allow_override = true;
		bool allow_lua = true;
		std::vector<path> resolve_within;
		std::vector<std::string> processed_files;
		// section include_vars;
		// variable_scope main_scope{nullptr};
		utils::path current_file;
		const ini_parser_reader* reader{};
		const ini_parser_error_handler* error_handler{};

		bool get_template(const std::string& s, std::shared_ptr<section_template>& ref)
		{
			ref = templates_map[s];
			if (!ref) warn("Template is missing: %s", s);
			return ref != nullptr;
		}

		bool get_mixin(const std::string& s, std::shared_ptr<section_template>& ref)
		{
			ref = mixins_map[s];
			if (!ref) warn("Mixin is missing: %s", s);
			return ref != nullptr;
		}

		static auto warn_unwrap(const std::string& s) { return s.c_str(); }

		template <typename... Args>
		void warn(const char* format, const Args& ... args) const
		{
			if (!error_handler) return;
			const int size = std::snprintf(nullptr, 0, format, warn_unwrap(args)...) + 1; // Extra space for '\0'
			std::unique_ptr<char[]> buf(new char[ size ]);
			std::snprintf(buf.get(), size, format, warn_unwrap(args)...);
			error_handler->on_warning(current_file, std::string(buf.get(), buf.get() + size - 1).c_str()); // We don't want the '\0' inside
		}

		ini_parser_data(bool allow_includes = false, const std::vector<path>& resolve_within = {})
			: allow_includes(allow_includes), resolve_within(resolve_within) { }

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

		static size_t vars_fingerprint(const section& vars)
		{
			size_t ret{};
			for (const auto& p : vars)
			{
				auto r = std::hash<std::string>{}(p.first);
				for (const auto& v : p.second.data())
				{
					r = (r * 397) ^ std::hash<std::string>{}(v);
				}
				ret ^= r;
			}
			return ret;
		}

		utils::path find_referenced(const std::string& file_name, const size_t vars_fingerprint)
		{
			if (is_processed(file_name, vars_fingerprint)) return {};

			auto inc = file_name;
			trim_self(inc);

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

		bool resolve_mixins(const std::shared_ptr<variable_scope>& sc_par, const std::vector<std::shared_ptr<section_template>>& mixins,
			current_section_info& c, std::vector<std::string>& referenced_variables, int scope) const
		{
			if (scope > 10) return true;
			for (const auto& m : mixins)
			{
				auto sc = sc_par->inherit();
				sc->extend(c.target_section);
				sc->extend(m->values);
				sc->fallback(m->scope);

				// auto scope = sc->inherit();
				auto m_active = m->values.find("@ACTIVE");
				if (m_active != m->values.end())
				{
					variant m_active_v;
					for (const auto& piece : m_active->second.data())
					{
						substitute_variable(piece, sc, get_value_finalizer(m_active_v.data()), 0, &referenced_variables);
					}
					if (!m_active_v.as<bool>()) goto NextMixin_1;
				}

				for (const auto& v : m->values)
				{
					if (v.first == "@ACTIVE") continue;
					if (c.target_section.find(v.first) != c.target_section.end()) continue;
					auto& dest = c.target_section[v.first].data();
					for (const auto& piece : v.second.data())
					{
						substitute_variable(piece, sc, get_value_finalizer(dest), 0, &referenced_variables);
					}
					if (v.first == "ACTIVE" && !c.target_section[v.first].as<bool>())
					{
						c.target_section.clear();
						c.target_section["ACTIVE"] = "0";
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

		variant substitute_variable_array(const variant& v, const std::shared_ptr<variable_scope>& sc, std::vector<std::string>& referenced_variables) const
		{
			variant dest;
			for (const auto& piece : v.data())
			{
				substitute_variable(piece, sc, get_value_finalizer(dest.data()), 0, &referenced_variables);
			}
			return dest;
		}

		void resolve_generator_impl(const std::shared_ptr<section_template>& t, const std::string& key, const std::string& section_key,
			const std::shared_ptr<section_template>& tpl, const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables)
		{
			current_section_info generated(section_key, {tpl});
			auto gen_scope = scope;
			auto gen_param_prefix = key + ":";
			for (const auto& v0 : t->values)
			{
				if (v0.first.find(key) != 0) continue;
				const auto v0_sep = v0.first.find_first_of(':');
				if (v0_sep == std::string::npos) continue;
				auto is_param = true;
				for (auto j = key.size(); j < v0_sep; j++)
				{
					if (!is_space(v0.first[j]))
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
				gen_scope->values[param_key] = substitute_variable_array(v0.second, gen_scope, referenced_variables);
			}
			parse_ini_section_finish(generated, gen_scope, &referenced_variables);
		}

		void resolve_generator_iteration(const std::shared_ptr<section_template>& t, const std::string& key, const std::string& section_key,
			const std::shared_ptr<section_template>& tpl, const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables,
			const std::vector<int>& repeats, size_t repeats_phase)
		{
			if (repeats.size() > repeats_phase)
			{
				for (auto i = 0, n = repeats_phase >= repeats.size() ? 1 : repeats[repeats_phase]; i < n; i++)
				{
					auto gen_scope = scope->inherit();
					gen_scope->values[std::to_string(repeats_phase)] = i;
					resolve_generator_iteration(t, key, section_key, tpl, gen_scope, referenced_variables, repeats, repeats_phase + 1);
				}
			}
			else
			{
				resolve_generator_impl(t, key, section_key, tpl, scope, referenced_variables);
			}
		}

		void resolve_generator(const std::shared_ptr<section_template>& t, const std::string& key, const variant& trigger,
			const std::shared_ptr<variable_scope>& scope, std::vector<std::string>& referenced_variables)
		{
			auto ref_template = trigger.as<std::string>();

			std::vector<int> repeats;
			for (auto i = 1; i < int(trigger.data().size()); i++)
			{
				repeats.push_back(trigger.as<int>(i));
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
				resolve_generator_iteration(t, key, section_key, tpl, scope, referenced_variables, repeats, 0);
			}
		}

		void parse_ini_section_finish(current_section_info& c, const std::shared_ptr<variable_scope>& scope, 
			std::vector<std::string>* referenced_variables_ptr = nullptr)
		{
			if (!c.section_mode()) return;

			if (!c.referenced_templates.empty())
			{
				std::unique_ptr<std::vector<std::string>> referenced_variables_uptr{};
				if (!referenced_variables_ptr)
				{
					referenced_variables_uptr = std::make_unique<std::vector<std::string>>();
					referenced_variables_ptr = referenced_variables_uptr.get();
				}
				auto& referenced_variables = *referenced_variables_ptr;

				for (const auto& t : c.referenced_templates)
				{
					auto sc = scope->inherit();
					sc->fallback(t->scope);
					sc->extend(t->values);
					const auto target_found = sc->values.find("TARGET");
					if (target_found == sc->values.end() && !c.section_key.empty())
					{
						sc->values["TARGET"] = c.section_key;
					}
					if (!resolve_mixins(sc, t->referenced_mixins, c, referenced_variables, 0))
					{
						return;
					}

					sc->extend(c.target_section);
					for (const auto& v : t->values)
					{
						const auto is_output = v.first == "@OUTPUT";
						if (is_output && !c.section_key.empty()) continue;

						const auto is_generator = v.first.find("@GENERATOR") == 0;
						const auto is_generator_param = is_generator && v.first.find_first_of(':') != std::string::npos;
						const auto is_virtual = is_output || is_generator;

						if (!is_virtual && c.target_section.find(v.first) != c.target_section.end()
							|| is_generator_param)
						{
							continue;
						}

						auto dest = substitute_variable_array(v.second, sc, referenced_variables);
						if (is_output)
						{
							c.section_key = dest.as<std::string>();
							sc->values["TARGET"] = c.section_key;
						}
						else if (is_generator)
						{
							resolve_generator(t, v.first, dest, sc, referenced_variables);
						}
						else if (!is_virtual)
						{
							if (v.first == "ACTIVE" && !c.target_section[v.first].as<bool>())
							{
								c.target_section.clear();
								c.target_section["ACTIVE"] = "0";
								return;
							}

							c.target_section[v.first] = dest;
						}
					}
				}
				for (const auto& v : referenced_variables)
				{
					c.target_section.erase(v);
				}
			}

			{
				std::vector<std::string> referenced_variables;
				auto sc = scope->inherit();
				sc->extend(c.target_section);
				// TODO: Scopes
				if (!resolve_mixins(sc, c.referenced_section_mixins, c, referenced_variables, 0))
				{
					return;
				}
				for (const auto& v : referenced_variables)
				{
					c.target_section.erase(v);
				}
			}

			if (c.section_key == "FUNCTION")
			{
				lua_register_function(
					c.target_section["NAME"].as<std::string>(),
					c.target_section["ARGUMENTS"],
					c.target_section["CODE"].as<std::string>(),
					!c.target_section["PRIVATE"].as<bool>(),
					current_file, error_handler);
				c.target_section.clear();
			}
			else if (c.section_key == "USE")
			{
				const auto name = c.target_section["FILE"].as<std::string>();
				const auto referenced = find_referenced(name, false);
				if (exists(referenced)) lua_import(referenced, !c.target_section["PRIVATE"].as<bool>(), current_file, error_handler);
				else warn("Referenced file is missing: %s", name);
				c.target_section.clear();
			}
			else if (c.section_key.find("INCLUDE") == 0)
			{
				auto to_include = c.target_section.find("INCLUDE");
				if (to_include != c.target_section.end())
				{
					const auto previous_file = current_file;
					auto include_scope = scope->inherit();

					for (const auto& p : c.target_section)
					{
						if (p.first == "INCLUDE") continue;
						if (p.first.find("VAR") == 0) include_scope->values[p.second.as<std::string>()] = p.second.as<variant>(1);
						else include_scope->values[p.first] = p.second;
					}

					// It’s important to copy values and clear section before parsing included files: those
					// files might have their own includes, overwriting this one and breaking c.target_section pointer 
					auto values = to_include->second.data();
					c.target_section.clear();

					auto vars_fp = vars_fingerprint(include_scope->values);
					for (auto& i : values)
					{
						parse_file(find_referenced(i, vars_fp), include_scope, vars_fp);
					}

					current_file = previous_file;
					return;
				}
			}

			if (!c.target_section.empty())
			{
				sections.push_back({c.section_key, c.target_section});
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

				if (c == '\\' && i < t - 1 && str[i + 1] == '\n')
				{
					item += str.substr(last_nonspace, i - last_nonspace);
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
					else if (item.empty() && q == -1)
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

		void ensure_generator_name_unique(std::string& key, const section& section) const
		{
			static auto unique_name = 0;
			if (key.find("@GENERATOR") == 0 && key.find_first_of(':') == std::string::npos 
					&& section.find("@GENERATOR") != section.end()) {
				key ="@GENERATOR_;" + std::to_string(unique_name++);
			}
		}

		void parse_ini_finish(current_section_info& c, const std::string& data, const int non_space, std::string& key,
			int& started, int& end_at, const std::shared_ptr<variable_scope>& scope)
		{
			if (!c.section_mode() && !c.target_template) return;
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

			const auto new_key = allow_override || !c.referenced_templates.empty() || (c.section_mode()
				? c.target_section.find(key) == c.target_section.end()
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
					auto sc = scope;
					if (!c.referenced_templates.empty())
					{
						sc = sc->inherit();
						sc->extend(c.target_section);
						for (const auto& r : c.referenced_templates)
						{
							sc->extend(r->values);
							sc->fallback(r->scope);
						}
					}
					substitute_variable(value_piece, sc, get_value_finalizer(value_splitted), 0, nullptr);
				}
			}

			if (key.find("@MIXIN") == 0)
			{
				std::shared_ptr<section_template> tpl;
				if (!get_mixin(value_splitted[0], tpl)) return;
				if (c.section_mode()) c.referenced_section_mixins.push_back(tpl);
				else c.target_template->referenced_mixins.push_back(tpl);
			}
			else if (c.section_mode())
			{
				ensure_generator_name_unique(key, c.target_template->values);
				if (key == "ACTIVE" && !variant(value_splitted).as<bool>())
				{
					c.target_section[key] = value_splitted;
					c.terminate();
				}
				else if (c.section_key == "DEFAULTS")
				{
					const auto compatible_mode = key.find("VAR") == 0 && !value_splitted.empty();
					const auto& actual_key = compatible_mode ? value_splitted[0] : key;
					scope->values[actual_key] = compatible_mode ? variant(value_splitted).as<variant>(1) : value_splitted;
				}
				else if (new_key)
				{
					c.target_section[key] = value_splitted;
				}
				else if (c.section_key == "INCLUDE" && key == "INCLUDE")
				{
					auto& existing = c.target_section[key];
					for (const auto& piece : value_splitted)
					{
						existing.data().push_back(piece);
					}
				}
			}
			else
			{
				ensure_generator_name_unique(key, c.target_template->values);
				(*c.target_template).values[key] = value_splitted;
			}
		}

		void parse_ini_finish(std::vector<current_section_info>& cs, const std::string& data, const int non_space, std::string& key,
			int& started, int& end_at, bool finish_section, const std::shared_ptr<variable_scope>& scope)
		{
			for (auto& s : cs)
			{
				parse_ini_finish(s, data, non_space, key, started, end_at, scope);
				if (finish_section)
				{
					parse_ini_section_finish(s, scope);
				}
			}

			key.clear();
			started = -1;
			end_at = -1;
		}

		static void add_template(std::vector<std::shared_ptr<section_template>>& templates, std::shared_ptr<section_template> t)
		{
			templates.push_back(t);
			for (const auto& p : t->parents)
			{
				// TODO: Recursion protection
				add_template(templates, p);
			}
		}

		section& create_section(const std::string& final_name)
		{
			sections.push_back({final_name, {}});
			return sections[sections.size() - 1].second;
		}

		void set_sections(std::vector<current_section_info>& cs, std::string& cs_keys, const std::shared_ptr<variable_scope>& scope)
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
					cs.push_back(current_section_info{final_name});
					for (auto& s : cs) s.target_section["INCLUDE"] = file;
					return;
				}

				if (final_name == "FUNCTION")
				{
					auto file = cs_keys.substr(separator + 1);
					trim_self(file);
					cs.push_back(current_section_info{final_name});
					for (auto& s : cs) s.target_section["NAME"] = file;
					return;
				}

				if (final_name == "USE")
				{
					auto file = cs_keys.substr(separator + 1);
					trim_self(file);
					cs.push_back(current_section_info{final_name});
					for (auto& s : cs) s.target_section["FILE"] = file;
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
					templates_map[template_name] = std::make_unique<section_template>(template_name, scope);
					if (pieces.size() > 2 && (pieces[1] == "extends" || pieces[1] == "EXTENDS"))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							templates_map[template_name]->parents.push_back(templates_map[pieces[k]]);
						}
					}
					cs.push_back(current_section_info{templates_map[template_name]});
				}
				return;
			}

			if (is_mixin)
			{
				for (const auto& cs_key : section_names)
				{
					auto pieces = split_string(cs_key, " ", true, true);

					auto template_name = pieces[0];
					mixins_map[template_name] = std::make_unique<section_template>(template_name, scope);
					if (pieces.size() > 2 && (pieces[1] == "extends" || pieces[1] == "EXTENDS"))
					{
						for (auto k = 2, kt = int(pieces.size()); k < kt; k++)
						{
							trim_self(pieces[k], ", \t\r");
							mixins_map[template_name]->parents.push_back(mixins_map[pieces[k]]);
						}
					}
					cs.push_back(current_section_info{mixins_map[template_name]});
				}
				return;
			}

			std::vector<std::shared_ptr<section_template>> referenced_templates;

			auto section_names_inv = section_names;
			std::reverse(section_names_inv.begin(), section_names_inv.end());
			for (const auto& section_name : section_names_inv)
			{
				auto found_template = templates_map.find(section_name);
				if (found_template != templates_map.end())
				{
					add_template(referenced_templates, found_template->second);
				}
			}

			if (!referenced_templates.empty())
			{
				cs.push_back(current_section_info{final_name, referenced_templates});
				return;
			}

			for (const auto& cs_key : section_names)
			{
				cs.push_back(current_section_info{cs_key});
			}
		}

		static bool is_quote_working(const char* data, const int from, const int to, const bool allow_$)
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

		void parse_ini_values(const char* data, const int data_size, const std::shared_ptr<variable_scope>& parent_scope)
		{
			std::vector<current_section_info> cs;
			const auto scope = parent_scope ? parent_scope->inherit() : std::make_shared<variable_scope>();

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
						parse_ini_finish(cs, data, non_space, key, started, end_at, true, scope);

						const auto s = ++i;
						if (s == data_size) break;
						for (; i < data_size && data[i] != ']'; i++) {}
						auto cs_keys = std::string(&data[s], i - s);
						cs.clear();
						set_sections(cs, cs_keys, scope);
						break;
					}

					case '\n':
					{
						if (end_at != -1) goto LAB_DEF;
						if (non_space > 0 && data[non_space] == '\\') goto LAB_DEF;
						parse_ini_finish(cs, data, non_space, key, started, end_at, false, scope);
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
						parse_ini_finish(cs, data, non_space, key, started, end_at, false, scope);
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
								if (started != -1 && !is_quote_working(data, started, i, true))
								{
									goto LAB_DEF;
								}

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

			parse_ini_finish(cs, data, non_space, key, started, end_at, true, scope);
		}

		void parse_file(const path& path, const std::shared_ptr<variable_scope>& scope, const size_t vars_fingerprint)
		{
			if (path.empty() || !reader) return;
			mark_processed(path.filename().string(), vars_fingerprint);
			current_file = path;
			const auto data = reader->read(path);
			if (data.empty() && error_handler)
			{
				warn("File is missing or empty: %s", path.string());
			}
			parse_ini_values(data.c_str(), int(data.size()), scope);
		}

		void resolve_sequential()
		{
			std::unordered_map<std::string, uint32_t> indices;
			for (const auto& p : sections)
			{
				auto key = p.first;
				if (key.size() > 4)
				{
					const auto postfix = key.substr(key.size() - 4);
					if (postfix == "_..." || postfix == u8"_…" /* how lucky they are the same size lol */)
					{
						const auto group = key.substr(0, key.size() - 3);
						auto& counter = indices[group];
						key = group + std::to_string(counter++);
						for (auto i = 0; gen_find(sections, key) != sections.end() && i < 100; i++)
						{
							key = group + std::to_string(counter++);
						}
					}
				}

				auto existing = sections_map.find(key);
				if (existing != sections_map.end())
				{
					for (auto& r : p.second)
					{
						existing->second[r.first] = r.second;
					}
				}
				else
				{
					sections_map[key] = p.second;
				}
			}

			/*for (const auto& p : sections)
			{
				const auto index = p.first.find(":$SEQ:");
				if (index != std::string::npos) goto Remap;
			}
			return;

		Remap:
			std::vector<std::pair<std::string, section>> elems(sections.begin(), sections.end());
			std::sort(elems.begin(), elems.end(), sort_sections);

			sections_list renamed;
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
			sections = renamed;*/
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
		data_->allow_lua = value;
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, const ini_parser_error_handler& error_handler) const
	{
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data, data_size, {nullptr});
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, const ini_parser_error_handler& error_handler) const
	{
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data.c_str(), int(data.size()), {nullptr});
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const char* data, const int data_size, const ini_parser_reader& reader,
		const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data, data_size, {nullptr});
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse(const std::string& data, const ini_parser_reader& reader, const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_ini_values(data.c_str(), int(data.size()), {nullptr});
		data_->reader = {};
		data_->error_handler = {};
		return *this;
	}

	const ini_parser& ini_parser::parse_file(const path& path, const ini_parser_reader& reader, const ini_parser_error_handler& error_handler) const
	{
		data_->reader = &reader;
		data_->error_handler = &error_handler;
		data_->parse_file(path, {nullptr}, 0);
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

	std::string ini_parser::to_ini() const
	{
		return gen_to_ini(data_->sections_map);
	}

	std::string ini_parser::to_json(bool format) const
	{
		return gen_to_json(data_->sections_map, format);
	}

	const std::unordered_map<std::string, section>& ini_parser::get_sections() const
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
