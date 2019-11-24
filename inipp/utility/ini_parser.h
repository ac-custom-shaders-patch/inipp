#pragma once

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
		virtual void on_warning(const path& filename, const char* message) {};
		virtual void on_error(const path& filename, const char* message) {};
	};

	struct ini_parser
	{
		using section = std::unordered_map<std::string, variant>;
		ini_parser();
		ini_parser(bool allow_includes, const std::vector<path>& resolve_within);
		~ini_parser();

		static ini_parser_error_handler& default_error_handler()
		{
			static ini_parser_error_handler ret;
			return ret;
		}

		const ini_parser& allow_lua(bool value) const;
		const ini_parser& parse(const char* data, int data_size, ini_parser_error_handler& error_handler = default_error_handler()) const;
		const ini_parser& parse(const std::string& data, ini_parser_error_handler& error_handler = default_error_handler()) const;
		const ini_parser& parse(const char* data, int data_size, const ini_parser_reader& reader,
			ini_parser_error_handler& error_handler = default_error_handler()) const;
		const ini_parser& parse(const std::string& data, const ini_parser_reader& reader,
			ini_parser_error_handler& error_handler = default_error_handler()) const;
		const ini_parser& parse_file(const path& path, const ini_parser_reader& reader,
			ini_parser_error_handler& error_handler = default_error_handler()) const;
		const ini_parser& finalize() const;
		void finalize_end() const;

		const std::unordered_map<std::string, section>& get_sections() const;

		std::string to_ini() const;
		std::string to_json(bool format = false) const;

		static std::string to_ini(const std::unordered_map<std::string, section>& sections);
		static std::string to_json(const std::unordered_map<std::string, section>& sections, bool format = false);
		static void leaks_check(void(*callback)(const char*, long));

	private:
		struct ini_parser_data* data_;
	};
}
