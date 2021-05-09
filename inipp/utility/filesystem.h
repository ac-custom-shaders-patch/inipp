/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <functional>
#include "string_codecvt.h"

#ifndef USE_SIMPLE
#include "blob.h"
#include "special_folder.h"
#endif

typedef unsigned char byte;

namespace utils
{
	enum class special_folder : unsigned;

	struct path
	{
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
		bool extension_matches(const std::vector<std::string>& extensions) const;
		path replace_extension(const std::string& extension) const;
		bool is_child_of(const path& parent) const;
		path operator/(const path& more) const;
		path operator+(char c) const { return data_ + c; }
		path operator+(const path& more) const { return data_ + more.data_; }

	private:
		std::string data_;
	};

	bool exists(const path& path);
	bool create_dir(const path& path);
	long long get_file_size(const path& path);
	path resolve(const path& filename, const std::vector<path>& paths);
	path absolute(const path& filename, const path& parent_path);

	path get_module_path(void* handle);

	void set_special_folders_path(const path& ac_root);
	const path& get_special_folder_path(special_folder id);

	#ifndef USE_SIMPLE
	path scan_dir(const path& dir, const char* mask,
		const std::function<bool(const WIN32_FIND_DATAW& data)>& callback);
	path scan_dir_recursive(const path& dir, const char* mask,
		const std::function<bool(const WIN32_FIND_DATAW& data, const path& parent)>& callback,
		const std::function<bool(const WIN32_FIND_DATAW& data, const path& parent)>& filter_dir = {});
	std::vector<path> list_files(const path& path_val, const char* mask = "*", bool recursive = false);

	bool try_read_file(const path& filename, blob& result);
	void write_file(const path& filename, const blob_view& data);

	blob read_file(const path& filename);
	#endif
	std::string read_file_str(const path& filename);
}
