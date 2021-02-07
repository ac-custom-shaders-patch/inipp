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
		const auto& d0 = data_;
		const auto& d1 = other.data_;
		const auto s0 = d0.size();
		if (s0 != d1.size()) return false;
		for (auto i = 0U; i < s0; ++i)
		{
			auto c0 = tolower(d0[i]);
			auto c1 = tolower(d1[i]);
			if (c0 == '/') c0 = '\\';
			if (c1 == '/') c1 = '\\';
			if (c0 != c1) return false;
		}
		return true;
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
		const auto e = data_.find_last_of("/\\");
		return e == std::string::npos ? path() : data_.substr(0, e);
	}

	path path::filename() const
	{
		const auto e = data_.find_last_of("/\\");
		return e == std::string::npos ? data_ : data_.substr(e + 1);

		// WCHAR buffer[MAX_PATH] = {};
		// utf8_to_utf16(data_, buffer);
		// return utf16_to_utf8(PathFindFileNameW(buffer));
	}

	template <size_t OUTSIZE>
	void _utf16_to_utf8(const std::wstring& s, char (&target)[OUTSIZE])
	{
		const auto o = utf16_to_utf8(s);
		std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
	}

	template <size_t OUTSIZE>
	void _utf8_to_utf16(const std::string& s, wchar_t (&target)[OUTSIZE])
	{
		const auto o = utf8_to_utf16(s);
		std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
	}

	path path::filename_without_extension() const
	{
		const auto e = data_.find_last_of("/\\");
		const auto o = e == std::string::npos ? 0 : e + 1;
		auto s = data_.find_last_of('.');
		if (s <= o) s = std::string::npos;
		return s == std::string::npos ? data_.substr(o) : data_.substr(o, s - o);
	}

	std::string path::extension() const
	{
		const auto e = data_.find_last_of("/\\");
		const auto o = e == std::string::npos ? 0 : e + 1;
		auto s = data_.find_last_of('.');
		if (s <= o) s = std::string::npos;
		return s == std::string::npos ? std::string() : data_.substr(s);
	}

	bool path::extension_matches(const std::vector<std::string>& extensions) const
	{
		auto ex = extension();
		std::transform(ex.begin(), ex.end(), ex.begin(), tolower);
		for (const auto& e : extensions)
		{
			if (ex == e) return true;
		}
		return false;
	}

	path path::replace_extension(const std::string& extension) const
	{
		const auto e = data_.find_last_of("/\\");
		const auto s = data_.find_last_of('.');
		return (s != std::string::npos && (e == std::string::npos || e < s) ? data_.substr(0, s) : data_) + extension;
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

		SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, result);
		paths[special_folder::app_data_local] = path(result);

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
		paths[special_folder::ac_user_setups] = path(result) / "Assetto Corsa" / "setups";
		paths[special_folder::ac_root] = ac_root;

		paths[special_folder::ac_ppfilters] = ac_root / "system" / "cfg" / "ppfilters";
		paths[special_folder::ac_content_cars] = ac_root / "content" / "cars";
		paths[special_folder::ac_content_drivers] = ac_root / "content" / "driver";
		paths[special_folder::ac_content_tracks] = ac_root / "content" / "tracks";
		paths[special_folder::ac_content_fonts] = ac_root / "content" / "fonts";

		paths[special_folder::ac_ext] = ac_root / "extension";
		paths[special_folder::ac_ext_cfg_sys] = ac_root / "extension" / "config";
		paths[special_folder::ac_ext_cfg_user] = paths[special_folder::ac_cfg] / "extension";

		paths[special_folder::ac_ext_textures] = paths[special_folder::ac_ext] / "textures";
		paths[special_folder::ac_ext_fonts] = paths[special_folder::ac_ext] / "fonts";
		paths[special_folder::ac_ext_app_icons] = paths[special_folder::ac_ext_textures] / "app_icons";
		paths[special_folder::ac_ext_shaders] = paths[special_folder::ac_ext] / "shaders";
		paths[special_folder::ac_ext_shaders_pack] = paths[special_folder::ac_ext] / "shaders.zip";
		paths[special_folder::ac_apps] = ac_root / "apps";
		paths[special_folder::ac_apps_python] = paths[special_folder::ac_apps] / "python";
		paths[special_folder::ac_ext_cfg_state] = paths[special_folder::ac_ext_cfg_user] / "state";
		if (!exists(paths[special_folder::ac_ext_cfg_state])) create_dir(paths[special_folder::ac_ext_cfg_state]);
		paths[special_folder::ac_results] = path(result) / "Assetto Corsa" / "out";
	}

	path get_special_folder_path(const special_folder id)
	{
		const auto found = paths.find(id);
		return found != paths.end() ? found->second : default_path;
	}

	std::vector<path> list_files(const path& path_val, const char* mask, const bool recursive)
	{
		std::vector<path> ret;
		if (recursive)
		{
			scan_dir_recursive(path_val, mask, [&](const WIN32_FIND_DATAW& data, const path& parent)
			{
				ret.push_back(parent / data.cFileName);
				return false;
			});
		}
		else
		{
			scan_dir(path_val, mask, [&](const WIN32_FIND_DATAW& data)
			{
				ret.push_back(path_val / data.cFileName);
				return false;
			});
		}
		return ret;
	}
	#endif

	path scan_dir(const path& dir, const char* mask, const std::function<bool(const WIN32_FIND_DATAW& data)>& callback)
	{
		path ret;
		const auto attributes = GetFileAttributesW(dir.wstring().c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES) return {};
		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			WIN32_FIND_DATAW ffd;
			const auto handle = FindFirstFileW((dir / mask).wstring().c_str(), &ffd);
			if (handle == INVALID_HANDLE_VALUE) return {};
			do
			{
				const auto is_dir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
				if (!is_dir)
				{
					if (callback(ffd))
					{
						ret = dir / ffd.cFileName;
						break;
					}
				}
			}
			while (FindNextFileW(handle, &ffd));
			FindClose(handle);
		}
		return ret;
	}

	path scan_dir_recursive(const path& dir, const char* mask,
		const std::function<bool(const WIN32_FIND_DATAW& data, const path& parent)>& callback,
		const std::function<bool(const WIN32_FIND_DATAW& data, const path& parent)>& filter_dir)
	{
		std::list<path> queue = {dir};
		path ret;
		while (!queue.empty())
		{
			auto item = queue.front();
			queue.pop_front();
			const auto insert_index = queue.begin();
			const auto attributes = GetFileAttributesW(item.wstring().c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) continue;
			if (attributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				WIN32_FIND_DATAW ffd;
				const auto handle = FindFirstFileW((item / mask).wstring().c_str(), &ffd);
				if (handle == INVALID_HANDLE_VALUE) continue;
				do
				{
					const auto is_dir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
					if (is_dir)
					{
						if ((ffd.cFileName[0] != L'.' || ffd.cFileName[1] != L'\0' && (ffd.cFileName[1] != L'.' || ffd.cFileName[2] != L'\0'))
							&& (!filter_dir || filter_dir(ffd, item)))
						{
							queue.insert(insert_index, item / ffd.cFileName);
						}
					}
					else
					{
						if (callback(ffd, item))
						{
							ret = item / ffd.cFileName;
							break;
						}
					}
				}
				while (FindNextFileW(handle, &ffd));
				FindClose(handle);
				if (!ret.empty()) return ret;
			}
		}
		return ret;
	}

	#ifndef USE_SIMPLE
	bool try_read_file(const path& filename, blob& result)
	{
		if (!exists(filename))
		{
			return false;
		}

		std::ifstream file(filename.wstring(), std::ios::binary);
		file.unsetf(std::ios::skipws);

		file.seekg(0, std::ios::end);
		const auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		result.resize(file_size);
		file.read(result.data(), result.size());
		return true;
	}

	blob read_file(const path& filename)
	{
		if (!exists(filename))
		{
			LOG(ERROR) << "File not found: " << filename;
			return blob();
		}

		std::ifstream file(filename.wstring(), std::ios::binary);
		file.unsetf(std::ios::skipws);

		file.seekg(0, std::ios::end);
		const auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		blob vec;
		vec.resize(file_size);
		file.read(&vec[0], vec.size());
		return vec;
	}

	void write_file(const path& filename, const blob& data)
	{
		auto s = std::ofstream(filename.wstring(), std::ios::binary);
		std::copy(data.begin(), data.end(), std::ostream_iterator<char>(s));
	}

	std::string read_file_str(const path& filename)
	{
		if (!exists(filename))
		{
			LOG(ERROR) << "File not found: " << filename;
			return std::string();
		}

		std::ifstream file(filename.wstring(), std::ios::binary);
		file.unsetf(std::ios::skipws);

		file.seekg(0, std::ios::end);
		const auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::string vec;
		vec.resize(file_size);
		file.read(&vec[0], vec.size());
		return vec;
	}
	#endif
}
