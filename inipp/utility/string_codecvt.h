#pragma once

const std::string& utf16_to_utf8(const std::string& s, bool fast_mode = false);
std::string utf16_to_utf8(const wchar_t* s, size_t len, bool fast_mode = false);
std::string utf16_to_utf8(const std::wstring& s, bool fast_mode = false);

const std::wstring& utf8_to_utf16(const std::wstring& s, bool fast_mode = false);
std::wstring utf8_to_utf16(const char* s, size_t len, bool fast_mode = false);
std::wstring utf8_to_utf16(const std::string& s, bool fast_mode = false);
