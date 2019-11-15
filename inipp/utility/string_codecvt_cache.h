#include <utility>
#pragma once

namespace utils
{
	struct string_codecvt_cache
	{
		const std::wstring& get(const std::string& input);
		const std::string& get(const std::wstring& input);

		string_codecvt_cache() : conversion_utf8_(nullptr), conversion_utf16_(nullptr) {}
		string_codecvt_cache(std::function<void(std::string&)> str) : conversion_utf8_(std::move(str)), conversion_utf16_(nullptr) {}
		string_codecvt_cache(std::function<void(std::wstring&)> str) : conversion_utf8_(nullptr), conversion_utf16_(std::move(str)) {}

	private:
		std::function<void(std::string&)> conversion_utf8_;
		std::function<void(std::wstring&)> conversion_utf16_;
		std::unordered_map<std::string, std::wstring> data1_;
		std::unordered_map<std::wstring, std::string> data2_;
	};
}
