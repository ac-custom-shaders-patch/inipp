#pragma once
#include <utility/blob.h>

namespace utils
{
	class variant;

	struct ini_parser_reader
	{
		virtual ~ini_parser_reader() = default;
		virtual std::string read(const path& filename) const = 0;
	};

	struct ini_parser_error_handler
	{
		virtual ~ini_parser_error_handler() = default;
		virtual void on_warning(const path& filename, const char* message) {}
		virtual void on_error(const path& filename, const char* message) {}
	};

	struct ini_parser_data_provider
	{
		virtual ~ini_parser_data_provider() = default;
		virtual bool read_number(const std::string& p, float& ret) { return false; }
		virtual bool read_string(const std::string& p, std::string& ret) { return false; }
		virtual bool read_bool(const std::string& p, bool& ret) { return false; }
	};

	struct ini_parser
	{
		using section = std::unordered_map<std::string, variant>;
		ini_parser();
		ini_parser(bool allow_includes, const std::vector<path>& resolve_within);
		~ini_parser();
		
		ini_parser& set_reader(ini_parser_reader* reader);
		ini_parser& set_error_handler(ini_parser_error_handler* handler);
		ini_parser& set_data_provider(ini_parser_data_provider* data_provider);
		ini_parser& allow_lua(bool value);
		
		const ini_parser& parse(const char* data, int data_size) const;
		const ini_parser& parse(const std::string& data) const;
		const ini_parser& parse_file(const path& path) const;
		const ini_parser& finalize() const;
		void finalize_end() const;

		const std::unordered_map<std::string, section>& get_sections() const;

		std::string to_ini() const;
		std::string to_json(bool format = false) const;

		static std::string to_ini(const std::unordered_map<std::string, section>& sections);
		static std::string to_json(const std::unordered_map<std::string, section>& sections, bool format = false);
		static void set_std_lib(pblob data);
		static void leaks_check(void (*callback)(const char*, long));

	private:
		struct ini_parser_data* data_;
	};
}
