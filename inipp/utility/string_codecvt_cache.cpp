#include "stdafx.h"
#include "string_codecvt_cache.h"

namespace utils
{
	const std::wstring& string_codecvt_cache::get(const std::string& input)
	{
		const auto ret = data1_.find(input);
		if (ret != data1_.end()) return ret->second;

		if (conversion_utf16_)
		{
			auto s = utf8_to_utf16(input);
			conversion_utf16_(s);
			return data1_[input] = s;
		}
		if (conversion_utf8_)
		{
			auto s = input;
			conversion_utf8_(s);
			return data1_[input] = utf8_to_utf16(s);
		}
		return data1_[input] = utf8_to_utf16(input);
	}

	const std::string& string_codecvt_cache::get(const std::wstring& input)
	{
		const auto ret = data2_.find(input);
		if (ret != data2_.end()) return ret->second;

		if (conversion_utf16_)
		{
			auto s = input;
			conversion_utf16_(s);
			return data2_[input] = utf16_to_utf8(s);
		}
		if (conversion_utf8_)
		{
			auto s = utf16_to_utf8(input);
			conversion_utf8_(s);
			return data2_[input] = s;
		}
		return data2_[input] = utf16_to_utf8(input);
	}
}
