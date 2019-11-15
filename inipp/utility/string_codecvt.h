#pragma once

const std::string& utf16_to_utf8(const std::string& s);
std::string utf16_to_utf8(const wchar_t* s, size_t len);
std::string utf16_to_utf8(const std::wstring& s);

const std::wstring& utf8_to_utf16(const std::wstring& s);
std::wstring utf8_to_utf16(const char* s, size_t len);
std::wstring utf8_to_utf16(const std::string& s);

/*template <size_t OUTSIZE>
void utf16_to_utf8(const std::wstring& s, char (&target)[OUTSIZE])
{
	const auto o = utf16_to_utf8(s);
	std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
}

template <size_t OUTSIZE>
void utf8_to_utf16(const std::string& s, wchar_t (&target)[OUTSIZE])
{
	const auto o = utf8_to_utf16(s);
	std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
}*/
