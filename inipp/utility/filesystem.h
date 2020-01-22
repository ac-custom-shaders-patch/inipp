/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <ostream>
#include "string_codecvt.h"

#ifndef USE_SIMPLE
#include "special_folder.h"
#endif

typedef unsigned char byte;

namespace utils
{
	enum class special_folder : unsigned;

	class path
	{
	public:
		path() {}
		path(const char* data) : data_(data) {}
		path(const wchar_t* data) : data_(utf16_to_utf8(data)) {}
		path(std::string data) : data_(std::move(data)) {}
		path(const std::wstring& data) : data_(utf16_to_utf8(data)) {}

		bool operator==(const path& other) const;
		bool operator!=(const path& other) const;

		std::string& string()
		{
			return data_;
		}

		const std::string& string() const
		{
			return data_;
		}

		std::wstring wstring() const;

		friend std::ostream& operator<<(std::ostream& stream, const path& path);

		bool empty() const
		{
			return data_.empty();
		}

		size_t length() const
		{
			return data_.length();
		}

		bool is_absolute() const;

		path parent_path() const;
		path filename() const;
		path filename_without_extension() const;
		std::string extension() const;

		path& remove_filename()
		{
			return operator=(parent_path());
		}

		path& replace_extension(const std::string& extension);

		path operator/(const path& more) const;

		path operator+(char c) const
		{
			return data_ + c;
		}

		path operator+(const path& more) const
		{
			return data_ + more.data_;
		}

	private:
		std::string data_;
	};

	bool exists(const path& path);
	bool create_dir(const path& path);
	std::string read(const path& path);
	long long get_file_size(const path& path);
	path resolve(const path& filename, const std::vector<path>& paths);
	path absolute(const path& filename, const path& parent_path);

	path get_module_path(void* handle);

	void set_special_folders_path(const path& ac_root);
	path get_special_folder_path(special_folder id);

	std::vector<path> list_files(const path& path_val, const std::string& mask = "*", bool recursive = false);
	bool try_find_file(const path& path_val, const std::string& file_name, path& result);

	std::vector<byte> read_file(const path& filename);
}
