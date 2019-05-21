/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "stdafx.h"

#include <ShlObj.h>
#include <Shlwapi.h>

#undef max
#undef min

#include "filesystem.h"
#include "string_codecvt.h"

#ifndef USE_SIMPLE
#include "log.h"
#include "special_folder.h"
#endif

namespace utils
{
	bool path::operator==(const path& other) const
	{
		return _stricmp(data_.c_str(), other.data_.c_str()) == 0;
	}

	bool path::operator!=(const path& other) const
	{
		return !operator==(other);
	}

	std::wstring path::wstring() const
	{
		return utf8_to_utf16(data_);
	}

	std::ostream& operator<<(std::ostream& stream, const path& path)
	{
		return stream << '\'' << path.string() << '\'';
	}

	bool path::is_absolute() const
	{
		return data_.size() > 2 && data_[1] == ':' && (data_[2] == '/' || data_[2] == '\\');
		// return PathIsRelativeW(utf8_to_utf16(data_).c_str()) == FALSE;
	}

	path path::parent_path() const
	{
		const auto e0 = data_.find_last_of('/');
		const auto e1 = data_.find_last_of('\\');
		const auto e = e1 == std::string::npos ? e0
				: e0 == std::string::npos ? e1
				: std::max(e0, e1);
		return e == std::string::npos ? path() : data_.substr(0, e);
	}

	path path::filename() const
	{
		const auto e0 = data_.find_last_of('/');
		const auto e1 = data_.find_last_of('\\');
		const auto e = e1 == std::string::npos ? e0
				: e0 == std::string::npos ? e1
				: std::max(e0, e1);
		return e == std::string::npos ? data_ : data_.substr(e + 1);

		// WCHAR buffer[MAX_PATH] = {};
		// utf8_to_utf16(data_, buffer);
		// return utf16_to_utf8(PathFindFileNameW(buffer));
	}

	path path::filename_without_extension() const
	{
		WCHAR buffer[MAX_PATH] = {};
		utf8_to_utf16(data_, buffer);

		PathRemoveExtensionW(buffer);
		return utf16_to_utf8(PathFindFileNameW(buffer));
	}

	std::string path::extension() const
	{
		WCHAR buffer[MAX_PATH] = {};
		utf8_to_utf16(data_, buffer);

		return utf16_to_utf8(PathFindExtensionW(buffer));
	}

	path& path::replace_extension(const std::string& extension)
	{
		WCHAR buffer[MAX_PATH] = {};
		utf8_to_utf16(data_, buffer);

		PathRenameExtensionW(buffer, utf8_to_utf16(extension).c_str());
		return operator=(utf16_to_utf8(buffer));
	}

	path path::operator/(const path& more) const
	{
		if (data_.empty()) return more;
		if (data_[data_.size() - 1] == '\\') return data_ + more.string();
		if (data_[data_.size() - 1] == '/') return data_.substr(0, data_.size() - 1) + "\\" + more.string();
		return data_ + "\\" + more.string();
	}

	bool exists(const path& path)
	{
		return GetFileAttributesW(path.wstring().c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool create_dir(const path& path)
	{
		const auto parent = path.parent_path();
		if (parent.string().size() > 4 && parent != path && !exists(parent))
		{
			create_dir(parent);
		}

		return CreateDirectoryW(path.wstring().c_str(), nullptr) != 0;
	}

	long long get_file_size(const path& path)
	{
		WIN32_FIND_DATAW data;
		const auto h = FindFirstFileW(path.wstring().c_str(), &data);
		if (h == INVALID_HANDLE_VALUE) return -1;
		FindClose(h);
		return data.nFileSizeLow | (long long)data.nFileSizeHigh << 32;
	}

	path resolve(const path& filename, const std::vector<path>& paths)
	{
		for (const auto& path : paths)
		{
			auto result = absolute(filename, path);

			if (exists(result))
			{
				return result;
			}
		}

		return filename;
	}

	path absolute(const path& filename, const path& parent_path)
	{
		if (filename.is_absolute())
		{
			return filename;
		}

		WCHAR result[MAX_PATH] = {};
		PathCombineW(result, utf8_to_utf16(parent_path.string()).c_str(), utf8_to_utf16(filename.string()).c_str());

		return utf16_to_utf8(result);
	}

	path get_module_path(void* handle)
	{
		WCHAR result[MAX_PATH] = {};
		GetModuleFileNameW(static_cast<HMODULE>(handle), result, MAX_PATH);

		return utf16_to_utf8(result);
	}

	static std::unordered_map<special_folder, path> paths;
	static path default_path(".");
	
#ifndef USE_SIMPLE
	void set_special_folders_path(const path& ac_root)
	{
		WCHAR result[MAX_PATH] = {};

		SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, result);
		paths[special_folder::app_data] = path(result);

		GetSystemDirectoryW(result, MAX_PATH);
		paths[special_folder::system] = path(result);

		GetWindowsDirectoryW(result, MAX_PATH);
		paths[special_folder::windows] = path(result);

		SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, result);
		paths[special_folder::documents] = path(result);

		paths[special_folder::ac_cfg] = path(result) / "Assetto Corsa" / "cfg";
		paths[special_folder::ac_setups] = path(result) / "Assetto Corsa" / "setups";
		paths[special_folder::ac_logs] = path(result) / "Assetto Corsa" / "logs";
		paths[special_folder::ac_screenshots] = path(result) / "Assetto Corsa" / "screens";
		paths[special_folder::ac_replays] = path(result) / "Assetto Corsa" / "replay";
		paths[special_folder::ac_replays_temp] = path(result) / "Assetto Corsa" / "replay" / "temp";
		paths[special_folder::ac_root] = ac_root;

		paths[special_folder::ac_ppfilters] = ac_root / "system" / "cfg" / "ppfilters";
		paths[special_folder::ac_content_cars] = ac_root / "content" / "cars";
		paths[special_folder::ac_content_tracks] = ac_root / "content" / "tracks";

		paths[special_folder::ac_ext] = ac_root / "extension";
		paths[special_folder::ac_ext_cfg_sys] = ac_root / "extension" / "config";
		paths[special_folder::ac_ext_cfg_user] = paths[special_folder::ac_cfg] / "extension";

#ifdef DEVELOPMENT_CFG
		paths[special_folder::ac_ext_shaders] = paths[special_folder::ac_ext] / "shaders";
#else
		paths[special_folder::ac_ext_shaders] = paths[special_folder::ac_ext] / "shaders_dev";
#endif
		paths[special_folder::ac_ext_shaders_pack] = paths[special_folder::ac_ext] / "shaders.zip";
	}

	path get_special_folder_path(const special_folder id)
	{
		const auto found = paths.find(id);
		return found != paths.end() ? found->second : default_path;
	}
#endif

	std::vector<path> list_files(const path& path_val, const std::string& mask, const bool recursive)
	{
		if (!PathIsDirectoryW(path_val.wstring().c_str()))
		{
			return {};
		}

		WIN32_FIND_DATAW ffd;
		const auto handle = FindFirstFileW((path_val / mask).wstring().c_str(), &ffd);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return {};
		}

		std::vector<path> result;

		do
		{
			const auto filename = utf16_to_utf8(ffd.cFileName);
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (recursive)
				{
					const auto recursive_result = list_files(filename, mask, true);
					result.insert(result.end(), recursive_result.begin(), recursive_result.end());
				}
			}
			else
			{
				result.push_back(path_val / filename);
			}
		}
		while (FindNextFileW(handle, &ffd));
		FindClose(handle);
		return result;
	}

	bool try_find_file(const path& path_val, const std::string& file_name, path& result)
	{
		if (!PathIsDirectoryW(path_val.wstring().c_str())) return false;

		WIN32_FIND_DATAW ffd;
		const auto handle = FindFirstFileW((path_val / "*").wstring().c_str(), &ffd);
		if (handle == INVALID_HANDLE_VALUE) return false;

		do
		{
			if (ffd.cFileName[0] == '.') continue;

			const auto filename = utf16_to_utf8(ffd.cFileName);
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (try_find_file(path_val / filename, file_name, result)) return true;
			}
			else if (_stricmp(filename.c_str(), file_name.c_str()) == 0)
			{
				result = path_val / filename;
				return true;
			}
		}
		while (FindNextFileW(handle, &ffd));

		FindClose(handle);
		return false;
	}
	
#ifndef USE_SIMPLE
	std::vector<byte> read_file(const path& filename)
	{
		if (!exists(filename))
		{
			LOG_ERROR() << "File not found: " << filename;
			return std::vector<byte>();
		}

		std::ifstream file(filename.wstring(), std::ios::binary);
		file.unsetf(std::ios::skipws);

		file.seekg(0, std::ios::end);
		const auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<byte> vec;
		vec.reserve(file_size);
		vec.insert(vec.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
		return vec;
	}
#endif
}
