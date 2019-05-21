#include "stdafx.h"
#include <utility/ini_parser.h>
#include <utility/variant.h>
#include <filesystem>
#include <utility/json.h>
#include <iomanip>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "lua53.lib")

FILE _iob[] = {*stdin, *stdout, *stderr};
extern "C" FILE* __cdecl __iob_func(void) { return _iob; }

static void show_usage(const std::string& name)
{
	// Trying to match those Linux format so it all would be neat and tidy
	std::cerr << "Usage: " << utils::path(name).filename_without_extension().string() << " [OPTION]... [FILE]...\n"
			<< "Convert INIpp FILEs into more regular format, resolving includes, replacing\n"
			<< "templates and so on.\n"
			<< "Example: inipp -i config/cars config/cars/kunos/ks_mclaren_p1.ini\n\n"
			<< "Options:\n"
			<< "  -p, --postfix=TEXT         postfix for new files in batch processing\n"
			<< "  -d, --destination=FILE     destination for single input FILE mode\n"
			<< "  -i, --include=DIR          directory to look included files in\n"
			<< "  -o, --output-ini           output in INI format instead of JSON\n"
			<< "  -f, --format               format resulting JSON\n"
			<< "  -v, --verbose              print warnings to STDERR\n"
			<< "  -q, --quiet                do not report any errors\n"
			<< "      --no-include           disable includes support\n"
			<< "      --no-maths             disable calculations support\n"
			<< "  -h, --help     display this help and exit\n"
			<< "      --version  output version information and exit\n\n"
			<< "If no files given, reads INIpp file from STDIN and prints flatten result\n"
			<< "in STDOUT, looking for included files in current directory\n\n"
			<< "Source code is available at: <https://github.com/ac-custom-shaders-patch/inipp>\n";
}

static void show_version()
{
	std::cerr << "inipp 1.0\n"
			<< "Copyright (C) 2019 x4fab\n"
			<< "License MIT <https://opensource.org/licenses/MIT>.\n"
			<< "This is free software: you are free to change and redistribute it.\n"
			<< "There is NO WARRANTY, to the extent permitted by law.\n";
}

using section = std::unordered_map<std::string, utils::variant>;
using ini_file = std::unordered_map<std::string, section>;

struct simple_reader : utils::ini_parser_reader
{
	std::string read(const utils::path& filename) const override
	{
		std::ifstream file(filename.wstring());
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
};

struct error_handler : utils::ini_parser_error_handler
{
	bool quiet;
	bool verbose;

	error_handler(bool quiet, bool verbose) : quiet(quiet), verbose(verbose) {}

	void on_error(const utils::path& filename, const char* message) const override
	{
		if (quiet) return;
		std::cerr << "Error in `" << filename.filename() << "`: " << message << '\n';
	}

	void on_warning(const utils::path& filename, const char* message) const override
	{
		if (quiet) return;
		std::cerr << "Error in `" << filename.filename() << "`: " << message << '\n';
	}
};

static std::string serialize(const utils::ini_parser& parser, bool output_format, bool output_ini)
{
	if (output_ini) return parser.to_string();

	using namespace nlohmann;
	auto sections = parser.get_sections();
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
	if (output_format) r << std::setw(2) << result << '\n';
	else r << result << '\n';
	return r.str();
}

int main(int argc, const char* argv[])
{
	try
	{
		auto allow_includes = true;
		auto allow_lua = true;
		auto quiet = false;
		auto output_ini = false;
		auto output_format = false;
		auto verbose = false;
		auto separator = std::string("\n\n");
		std::string postfix;
		std::string destination;
		std::vector<utils::path> resolve_within;
		std::vector<utils::path> input_files;

		resolve_within.push_back(std::experimental::filesystem::current_path().string());

#define GET_VALUE(SHORT, LONG, APPLY)\
		else if (arg == "-d") { if (i == argc - 1) { show_usage(argv[0]); return 1; } APPLY(argv[i + 1]); }\
		else if (arg.find("--destination=") == 0) APPLY(arg.substr(arg.find_first_of('=') + 1));

		for (auto i = 1; i < argc; i++)
		{
			auto arg = std::string(argv[i]);
			if (arg == "-h" || arg == "--help" || arg == "--postfix" || arg == "--destination" || arg == "--include" || arg == "--separator")
			{
				show_usage(argv[0]);
				return 0;
			}

			if (arg == "--version")
			{
				show_version();
				return 0;
			}

			if (arg == "--no-maths") allow_lua = false;
			else if (arg == "--no-include") allow_lua = false;
			else if (arg == "-q" || arg == "--quiet") quiet = true;
			else if (arg == "-o" || arg == "--output-ini") output_ini = true;
			else if (arg == "-f" || arg == "--format") output_format = true;
			else if (arg == "-v" || arg == "--verbose") verbose = true;
			GET_VALUE(d, destination, destination=)
			GET_VALUE(s, separator, separator=)
			GET_VALUE(p, postfix, postfix=)
			GET_VALUE(i, include, resolve_within.push_back)
			else if (arg[0] != '-') input_files.push_back(arg);
		}

		auto handler = error_handler(quiet, verbose);

		if (input_files.empty())
		{
			std::istreambuf_iterator<char> begin(std::cin), end;
			std::string s(begin, end);
			auto processed = serialize(
				utils::ini_parser(allow_includes, resolve_within).allow_lua(allow_lua).parse(s, simple_reader(), handler).finalize(),
				output_format, output_ini);
			if (!destination.empty()) std::ofstream(destination) << processed;
			else std::cout << processed;
			return 0;
		}

		auto first = true;
		for (const auto& f : input_files)
		{
			auto processed = serialize(
				utils::ini_parser(allow_includes, resolve_within).allow_lua(allow_lua).parse_file(f, simple_reader(), handler).finalize(),
				output_format, output_ini);
			if (!destination.empty())
			{
				std::ofstream(destination) << processed;
				break;
			}

			if (!postfix.empty())
			{
				std::ofstream(f.string() + postfix) << processed;
				continue;
			}

			if (!first) std::cout << separator;
			std::cout << processed;
			first = false;
		}

		return 0;
	}
	catch (std::exception const& e)
	{
		std::cerr << e.what();
		return 1;
	}
}
