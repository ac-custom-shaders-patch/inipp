#include "stdafx.h"
#include "ini_parser.h"
#include "variant.h"
#include <lua.hpp>
#include <utility/alphanum.hpp>

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

		std::string calculate(const std::string& expr) const
		{
			if (!params.allow_lua) return expr;
			static lua_State* L{};
			if (!L)
			{
				L = luaL_newstate();
				luaL_requiref(L, "math", luaopen_math, 1);
				luaL_requiref(L, "string", luaopen_string, 1);
			}
			if (luaL_loadstring(L, fix_expression(expr).c_str()) || lua_pcall(L, 0, -1, 0))
			{
				if (params.error_handler)
				{
					params.error_handler->on_error(params.file, lua_tolstring(L, -1, nullptr));
				}
				return "";
			}
			return lua_tolstring(L, -1, nullptr);
		}

		void add(const std::string& value) const
		{
			if (process_values && value.size() > 1)
			{
				if (value[0] == '"' && value[value.size() - 1] == '"'
					|| value[0] == '\'' && value[value.size() - 1] == '\'')
				{
					dest.push_back(value.substr(1, value.size() - 2));
					return;
				}

				if (value.size() > 2 && value[0] == '$' && (value[1] == '"' && value[value.size() - 1] == '"'
					|| value[1] == '\'' && value[value.size() - 1] == '\''))
				{
					dest.push_back(calculate(value.substr(2, value.size() - 3)));
					return;
				}
			}
			dest.push_back(value);
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
		special_mode mode{};

		variable_info() {}
		variable_info(const std::string& name) : name(name) {}
		variable_info(const std::string& name, int from, int to) : name(name), substr_from(from), substr_to(to) {}
		variable_info(const std::string& name, special_mode mode) : name(name), mode(mode) {}

		void substitute(const variable_scope& include_vars, const value_finalizer& dest)
		{
			const auto v = include_vars.find(name);
			if (!v) return;

			switch (mode)
			{
			case special_mode::size:
				{
					dest.add(std::to_string(v->data().size()));
					break;
				}
			case special_mode::none:
				{
					if (substr_from < 0) substr_from += int(v->data().size());
					if (substr_to < 0) substr_to += int(v->data().size());
					for (auto j = substr_from, jt = int(v->data().size()); j < jt && j < substr_to; j++)
					{
						dest.add(v->data()[j]);
					}
					break;
				}
			default:
				{
					break;
				}
			}
		}

		void substitute(const variable_scope& include_vars, const std::string& prefix, const std::string& postfix, const value_finalizer& dest)
		{
			const auto v = include_vars.find(name);
			if (!v)
			{
				if (!prefix.empty() || !postfix.empty())
				{
					dest.add(prefix + postfix);
				}
				return;
			}
			if (substr_from < 0) substr_from += int(v->data().size());
			if (substr_to < 0) substr_to += int(v->data().size());
			for (auto j = substr_from, jt = int(v->data().size()); j < jt && j < substr_to; j++)
			{
				auto s = prefix;
				s += v->data()[j];
				s += postfix;
				dest.add(s);
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
			if (pieces.size() > 1 && (pieces[1].empty() || !isdigit(pieces[1][0]))) return {};
			if (pieces.size() > 2 && (pieces[2].empty() || !isdigit(pieces[2][0]))) return {};
			const auto from = pieces.size() == 1 ? 0 : std::stoi(pieces[1]);
			const auto to = pieces.size() == 1 ? 1 : pieces.size() == 2 ? from + 1 : std::stoi(pieces[2]);
			return {pieces[0], from, to};
		}
		const auto vname = s.substr(1);
		if (!is_identifier(vname)) return {};
		return {vname};
	}

	static void substitute_variable(const std::string& value, const variable_scope& include_vars, const value_finalizer& dest, int stack)
	{
		if (stack < 10 && value[0] != '\'')
		{
			auto var = check_variable(value);
			if (!var.name.empty())
			{
				// Either $VariableName or ${VariableName}
				std::vector<std::string> temp;
				var.substitute(include_vars, dest);
				for (const auto& v : temp)
				{
					substitute_variable(v, include_vars, dest, stack++);
				}
				return;
			}

			{
				// Concatenation with ${VariableName}
				const auto var_begin = value.find("${");
				const auto var_end = var_begin == std::string::npos ? std::string::npos : value.find_first_of('}', var_begin);
				if (var_end != std::string::npos)
				{
					var = check_variable(value.substr(var_begin, var_end - var_begin + 1));
					std::vector<std::string> temp;
					const auto finalizer = value_finalizer{temp, {}, false};
					var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end + 1), finalizer);
					for (const auto& v : temp)
					{
						substitute_variable(v, include_vars, dest, stack++);
					}
					return;
				}
			}

			if (value.size() > 1 && (value[0] == '"' || value[0] == '$' && value[1] == '"'))
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
						std::vector<std::string> temp;
						const auto finalizer = value_finalizer{temp, {}, false};
						var.substitute(include_vars, value.substr(0, var_begin), value.substr(var_end), finalizer);
						for (const auto& v : temp)
						{
							substitute_variable(v, include_vars, dest, stack++);
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
		section params;
		section include_vars;
		std::vector<section_template*> parents;
	};

	struct current_section_info
	{
		std::string section_key;
		section* target_section;
		section_template* target_template;
		std::vector<section_template*> referenced_templates;
	};

	using ini_data = std::unordered_map<std::string, section>;

	struct ini_parser_data
	{
		ini_data* own_sections{};
		ini_data& sections;
		std::unordered_map<std::string, section_template> templates;
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

		void parse_ini_section_finish(current_section_info& c)
		{
			if (!c.target_section) return;

			if (!c.referenced_templates.empty())
			{
				for (auto t : c.referenced_templates)
				{
					variable_scope sc;
					sc.scopes.push_back(*c.target_section);
					sc.scopes.push_back(t->params);
					sc.scopes.push_back(t->include_vars);
					sc.scopes.push_back(include_vars);
					for (const auto& v : t->values)
					{
						if ((*c.target_section).find(v.first) != (*c.target_section).end()) continue;
						auto& dest = (*c.target_section)[v.first].data();
						for (const auto& piece : v.second.data())
						{
							substitute_variable(piece, sc, get_value_finalizer(dest), 0);
						}
						if (v.first == "ACTIVE" && !(*c.target_section)[v.first].as<bool>())
						{
							(*c.target_section).clear();
							(*c.target_section)["ACTIVE"] = "0";
							return;
						}
					}
				}
			}

			auto to_include = c.target_section->find("INCLUDE");
			if (to_include != c.target_section->end())
			{
				const auto previous_file = current_file;
				const auto previous_vars = include_vars;

				for (const auto& p : *c.target_section)
				{
					if (p.first.find("VAR") == 0)
					{
						include_vars[p.second.data()[0]] = p.second.as<variant>(1);
					}
				}

				for (auto& i : to_include->second.data())
				{
					parse_file(find_referenced(i));
				}

				current_file = previous_file;
				include_vars = previous_vars;
			}
		}

		static bool is_whitespace(char c)
		{
			return c == ' ' || c == '\t' || c == '\r';
		}

		static std::vector<std::string> split_string_quotes(std::string& str)
		{
			std::vector<std::string> result;
			auto first_nonspace = 0, last_nonspace = 0;
			auto q = -1;
			for (auto i = 0, t = int(str.size()); i < t; i++)
			{
				const auto c = str[i];

				if (c == '"' || c == '\'')
				{
					if (q == c) q = -1;
					else q = c;
					last_nonspace = i + 1;
				}
				else if (q == -1 && c == ',')
				{
					result.push_back(str.substr(first_nonspace, last_nonspace - first_nonspace));
					first_nonspace = i + 1;
					last_nonspace = i + 1;
				}
				else if (!is_whitespace(c))
				{
					last_nonspace = i + 1;
				}
				else if (i == first_nonspace)
				{
					first_nonspace++;
				}
			}
			result.push_back(str.substr(first_nonspace, last_nonspace - first_nonspace));
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
			if (!is_solid(data, started) && end_at == -1)
			{
				std::vector<std::string> value_splitted;
				for (const auto& value_piece : split_string_quotes(value))
				{
					if (key.find("VAR_") == 0 || c.target_template)
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
								sc.scopes.push_back(r->params);
								sc.scopes.push_back(r->include_vars);
							}
						}
						sc.scopes.push_back(include_vars);
						substitute_variable(value_piece, sc, get_value_finalizer(value_splitted), 0);
					}
				}

				if (c.target_section)
				{
					if (key == "ACTIVE" && !variant(value_splitted).as<bool>())
					{
						(*c.target_section)[key] = value_splitted;
						c.target_section = nullptr;
					}
					else if (c.section_key == "DEFAULTS")
					{
						if (include_vars.find(value_splitted[0]) == include_vars.end())
						{
							include_vars[value_splitted[0]] = std::vector<std::string>{value_splitted.begin() + 1, value_splitted.end()};
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
					if (key.find("VAR_DEFAULT") == 0)
					{
						(*c.target_template).params[value_splitted[0]] = std::vector<std::string>{value_splitted.begin() + 1, value_splitted.end()};
					}
					else if (new_key)
					{
						(*c.target_template).values[key] = value_splitted;
					}
				}
			}
			else if (new_key)
			{
				if (c.target_section) (*c.target_section)[key] = std::vector<std::string>{value};
				else (*c.target_template).values[key] = std::vector<std::string>{value};
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
						const auto cs_keys = std::string(&data[s], i - s);
						cs.clear();

						const auto section_names = split_string(cs_keys, ",", true, true);
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

						if (referenced_templates.empty())
						{
							for (auto cs_key : section_names)
							{
								if (cs_key.find("TEMPLATE:") == 0)
								{
									cs_key = cs_key.substr(9);
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
									cs.push_back({"", nullptr, &templates[template_name], {}});
								}
								else
								{
									parse_ini_fix_array(cs_key);
									cs.push_back({cs_key, &sections[cs_key], nullptr, {}});
								}
							}
						}
						else
						{
							std::string section_name;
							for (auto t : referenced_templates)
							{
								auto name = t->values.find("OUTPUT");
								if (name != t->values.end())
								{
									variable_scope sc;
									sc.scopes.push_back(t->params);
									sc.scopes.push_back(t->include_vars);
									sc.scopes.push_back(include_vars);
									std::vector<std::string> dest;
									for (const auto& piece : name->second.data())
									{
										substitute_variable(piece, sc, get_value_finalizer(dest), 0);
										if (!dest.empty()) break;
									}
									section_name = dest[0];
									break;
								}
							}
							parse_ini_fix_array(section_name);
							auto& templated = sections[section_name];
							cs.push_back({section_name, &templated, nullptr, referenced_templates});
						}
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
							if (started == -1 || started == i - 1 && data[started] == '$')
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
		data_->resolve_sequential();
		return *this;
	}

	std::string ini_parser::to_string() const
	{
		return to_string(data_->sections);
	}

	const std::unordered_map<std::string, section>& ini_parser::get_sections() const
	{
		return data_->sections;
	}

	std::string ini_parser::to_string(const ini_data& sections)
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
}
