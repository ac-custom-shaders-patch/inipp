#include "stdafx.h"
#include "string_codecvt.h"

#include <utility/str_view.h>

namespace utils
{
	const std::string& utf8(const std::string& s, bool fast_mode)
	{
		return s;
	}

	std::string utf8(const wchar_t* s, size_t len, bool fast_mode)
	{
		std::string result;
		if (fast_mode)
		{
			result.resize(len);
			for (auto i = 0U; i < len; ++i)
			{
				result[i] = s[i] > 255 ? '?' : char(s[i]);
			}
		}
		else if (len > 0)
		{
			result.resize(len * 2);
			auto r = WideCharToMultiByte(CP_UTF8, 0, s, int(len), &result[0], int(result.size()), nullptr, nullptr);
			if (r == 0)
			{
				result.resize(WideCharToMultiByte(CP_UTF8, 0, s, int(len), nullptr, 0, nullptr, nullptr));
				r = WideCharToMultiByte(CP_UTF8, 0, s, int(len), &result[0], int(result.size()), nullptr, nullptr);
			}
			result.resize(r);
		}
		return result;
	}

	std::string utf8(const std::wstring& s, bool fast_mode)
	{
		return utf8(s.c_str(), s.size(), fast_mode);
	}

	const std::wstring& utf16(const std::wstring& s, bool fast_mode)
	{
		return s;
	}

	std::wstring utf16(const char* s, size_t len, bool fast_mode)
	{
		// return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s, s + len);

		std::wstring result;
		result.resize(len);
		if (fast_mode)
		{
			for (auto i = 0U; i < len; ++i)
			{
				result[i] = wchar_t(s[i]);
			}
		}
		else if (len > 0)
		{
			auto r = MultiByteToWideChar(CP_UTF8, 0, s, int(len), &result[0], int(result.size()));
			if (r == 0)
			{
				result.resize(MultiByteToWideChar(CP_UTF8, 0, s, int(len), nullptr, 0));
				r = MultiByteToWideChar(CP_UTF8, 0, s, int(len), &result[0], int(result.size()));
			}
			result.resize(r);
		}
		return result;
	}

	std::wstring utf16(const std::string& s, bool fast_mode)
	{
		return utf16(s.c_str(), s.size(), fast_mode);
	}

	std::wstring utf16(const str_view& s, bool fast_mode)
	{
		return utf16(s.data(), s.size(), fast_mode);
	}

	static void utf16(std::wstring& ret, const char* text, size_t len, bool fast_mode)
	{
		if (len == 0)
		{
			ret.clear();
		}
		else if (fast_mode)
		{
			ret.resize(len);
			for (auto i = 0ULL; i < len; ++i)
			{
				ret[i] = text[i];
			}
			ret[len] = 0;
		}
		else
		{
			auto r = MultiByteToWideChar(CP_UTF8, 0, text, int(len), &ret[0], int(ret.capacity()));
			if (r == 0)
			{
				ret.reserve(MultiByteToWideChar(CP_UTF8, 0, text, int(len), nullptr, 0) + 128);
				r = MultiByteToWideChar(CP_UTF8, 0, text, int(len), &ret[0], int(ret.capacity()));
			}
			ret[r] = '\0';
			((size_t*)&ret)[2] = size_t(r);
		}
	}

	static void utf8(std::string& ret, const wchar_t* text, size_t len, bool fast_mode)
	{
		if (len == 0)
		{
			ret.clear();
		}
		else if (fast_mode)
		{
			ret.resize(len);
			for (auto i = 0ULL; i < len; ++i)
			{
				ret[i] = text[i] < 128 ? char(text[i]) : '?';
			}
			ret[len] = 0;
		}
		else
		{
			auto r = WideCharToMultiByte(CP_UTF8, 0, text, int(len), &ret[0], int(ret.capacity()), nullptr, nullptr);
			if (r == 0)
			{
				ret.reserve(WideCharToMultiByte(CP_UTF8, 0, text, int(len), nullptr, 0, nullptr, nullptr) + 128);
				r = WideCharToMultiByte(CP_UTF8, 0, text, int(len), &ret[0], int(ret.capacity()), nullptr, nullptr);
			}
			ret[r] = '\0';
			((size_t*)&ret)[2] = size_t(r);
		}
	}

	std::wstring& utf16(std::wstring& ret, const char* text, bool fast_mode)
	{
		utf16(ret, text, text ? strlen(text) : 0ULL, fast_mode);
		return ret;
	}

	std::string& utf8(std::string& ret, const wchar_t* text, bool fast_mode)
	{
		utf8(ret, text, text ? wcslen(text) : 0ULL, fast_mode);
		return ret;
	}

	std::wstring& utf16(std::wstring& ret, const std::string& text, bool fast_mode)
	{
		utf16(ret, text.c_str(), text.size(), fast_mode);
		return ret;
	}

	std::string& utf8(std::string& ret, const std::wstring& text, bool fast_mode)
	{
		utf8(ret, text.c_str(), text.size(), fast_mode);
		return ret;
	}
}
