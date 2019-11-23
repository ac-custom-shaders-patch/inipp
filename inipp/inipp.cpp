#include "stdafx.h"
#include <utility/ini_parser.h>
#include <utility/variant.h>
#include <filesystem>
#include <utility/json.h>
#include <iomanip>

#include <cstdio>
#include <windows.h>
#pragma execution_character_set("utf-8")

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
	if (output_ini) return parser.to_ini();
	return parser.to_json(output_format);
}

#define STYLE(X) u8"[" X "m"
#define STYLE_QUEUE STYLE("93")
#define STYLE_ERROR STYLE("91")
#define STYLE_SUCCESS STYLE("92")
#define STYLE_INFO STYLE("96")
#define STYLE_RESET STYLE("0")

void do_debug_run()
{
    SetConsoleOutputCP(65001);
	const auto handler = error_handler(false, true);
	for (const auto& entry : std::experimental::filesystem::directory_iterator("auto"))
	{
		if (entry.path().extension() == ".ini" && entry.path().native().find(L"__") == std::wstring::npos)
		{
			auto filename = utils::path(entry.path().native());
			if (filename.filename().string()[2] != '_') continue;

			std::cout << STYLE_QUEUE u8"• Testing " << filename.filename_without_extension().string().substr(3) << u8"… " STYLE_RESET;
			auto data = utils::ini_parser(true, {}).allow_lua(true).parse_file(filename, simple_reader(), handler).finalize().to_ini();
			auto required = filename.parent_path() / filename.filename_without_extension() + "__result.ini";

			if (exists(required))
			{
				if (simple_reader().read(required) == data)
				{
					std::cout << STYLE_SUCCESS u8"OK ✔" STYLE_RESET << std::endl;
				}
				else
				{
					std::cout << STYLE_ERROR u8"failed ⚠" STYLE_RESET << std::endl;
					std::cout << data;
				}
			} 
			else
			{
				std::cout << STYLE_INFO u8"new result ✳" STYLE_RESET << std::endl;
				std::ofstream(required.wstring()) << data;
			}
		}
	}

	const utils::path dev_input("dev/dev.ini");
	if (exists(dev_input))
	{
		std::cout << STYLE_QUEUE u8"• Running developing input:" STYLE_RESET << std::endl;
		std::cout << utils::ini_parser(true, {}).allow_lua(true).parse_file(dev_input, simple_reader(), handler).finalize().to_ini();
	}
}

int main(int argc, const char* argv[])
{
	#define THROW_STUFF
	#ifndef THROW_STUFF
	try
	{
	#endif
	auto allow_includes = true;
	auto allow_lua = true;
	auto quiet = false;
	auto output_ini = false;
	auto output_format = false;
	auto verbose = false;
	auto debug_run = false;
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
		else if (arg == "--debug") debug_run = true;
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

	if (debug_run)
	{
		do_debug_run();
		return 0;
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
	#ifndef THROW_STUFF
	}
	catch (std::exception const& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
	#endif
}
