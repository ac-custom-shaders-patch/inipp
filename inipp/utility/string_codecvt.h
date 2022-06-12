#pragma once

namespace utils
{
	struct str_view;

	// Option fast_mode stops actual conversion and just copies supported symbols, use it carefully
	const std::string& utf8(const std::string& s, bool fast_mode = false);
	std::string utf8(const wchar_t* s, size_t len, bool fast_mode = false);
	std::string utf8(const std::wstring& s, bool fast_mode = false);

	const std::wstring& utf16(const std::wstring& s, bool fast_mode = false);
	std::wstring utf16(const char* s, size_t len, bool fast_mode = false);
	std::wstring utf16(const std::string& s, bool fast_mode = false);
	std::wstring utf16(const str_view& s, bool fast_mode = false);

	// Faster versions that use already existing strings to avoid allocating memory:
	std::wstring& utf16(std::wstring& ret, const char* text, bool fast_mode = false);
	std::string& utf8(std::string& ret, const wchar_t* text, bool fast_mode = false);
	std::wstring& utf16(std::wstring& ret, const std::string& text, bool fast_mode = false);
	std::string& utf8(std::string& ret, const std::wstring& text, bool fast_mode = false);
}
