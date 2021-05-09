#include "stdafx.h"
#include "string_codecvt.h"
#include <Windows.h>

const std::string& utf16_to_utf8(const std::string& s, bool fast_mode)
{
	return s;
}

std::string utf16_to_utf8(const wchar_t* s, size_t len, bool fast_mode)
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

std::string utf16_to_utf8(const std::wstring& s, bool fast_mode)
{
	return utf16_to_utf8(s.c_str(), s.size(), fast_mode);
}

const std::wstring& utf8_to_utf16(const std::wstring& s, bool fast_mode)
{
	return s;
}

std::wstring utf8_to_utf16(const char* s, size_t len, bool fast_mode)
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

std::wstring utf8_to_utf16(const std::string& s, bool fast_mode)
{
	return utf8_to_utf16(s.c_str(), s.size(), fast_mode);
}
