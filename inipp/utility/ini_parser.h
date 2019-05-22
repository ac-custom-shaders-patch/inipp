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
		virtual void on_warning(const path& filename, const char* message) const {};
		virtual void on_error(const path& filename, const char* message) const {};
	};

	struct ini_parser
	{
		using section = std::unordered_map<std::string, variant>;
		ini_parser(std::unordered_map<std::string, section>& sections);
		ini_parser(std::unordered_map<std::string, section>& sections, bool allow_includes, const std::vector<path>& resolve_within);
		ini_parser();
		ini_parser(bool allow_includes, const std::vector<path>& resolve_within);
		~ini_parser();

		const ini_parser& allow_lua(bool value) const;
		const ini_parser& parse(const char* data, int data_size, const ini_parser_error_handler& error_handler = ini_parser_error_handler{}) const;
		const ini_parser& parse(const std::string& data, const ini_parser_error_handler& error_handler = ini_parser_error_handler{}) const;
		const ini_parser& parse(const char* data, int data_size, const ini_parser_reader& reader,
			const ini_parser_error_handler& error_handler = ini_parser_error_handler{}) const;
		const ini_parser& parse(const std::string& data, const ini_parser_reader& reader,
			const ini_parser_error_handler& error_handler = ini_parser_error_handler{}) const;
		const ini_parser& parse_file(const path& path, const ini_parser_reader& reader,
			const ini_parser_error_handler& error_handler = ini_parser_error_handler{}) const;
		const ini_parser& finalize() const;
		void finalize_end() const;

		const std::unordered_map<std::string, section>& get_sections() const;

		std::string to_ini() const;
		std::string to_json(bool format = false) const;

		static std::string to_ini(const std::unordered_map<std::string, section>& sections);
		static std::string to_json(const std::unordered_map<std::string, section>& sections, bool format = false);

	private:
		struct ini_parser_data* data_;
	};
}
