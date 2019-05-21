#include "stdafx.h"
#include "variant.h"

namespace utils
{
	variant::variant(const char* value): values_(1, value) { }
	variant::variant(const wchar_t* value): values_(1, utf16_to_utf8(value)) { }
	variant::variant(const std::vector<std::string>&& values): values_(values) { }
	std::vector<std::string>& variant::data() { return values_; }
	const std::vector<std::string>& variant::data() const { return values_; }

	static bool isdigit(const char c)
	{
		return c >= '0' && c <= '9' || c == '-' || c == '.';
	}

	static float safe_strtof(const char* c)
	{
		if (!isdigit(c[0])) return 0.f;
		return std::strtof(c, nullptr);
	}

	variant::variant(const std::vector<std::wstring>&& values)
	{
		for (auto& v : values)
		{
			values_.push_back(utf16_to_utf8(v));
		}
	}

	bool variant::as_bool(size_t i) const
	{
		return as<int>(i) != 0 || i < values_.size() && (values_[i] == "true" || values_[i] == "True" || values_[i] == "TRUE");
	}

	uint64_t variant::as_uint64_t(size_t i) const
	{
		if (i >= values_.size())
		{
			return 0UL;
		}

		auto s = values_[i];
		if (s.size() > 2 && s[0] == '0' && s[1] == 'x') return std::strtoull(s.c_str() + 2, nullptr, 16);
		if (s.empty() || !isdigit(s[0])) return 0UL;
		return std::stoull(values_[i], nullptr, 10);
	}

	int64_t variant::as_int64_t(size_t i) const
	{
		if (i >= values_.size())
		{
			return 0L;
		}

		auto s = values_[i];
		if (s.size() > 2 && s[0] == '0' && s[1] == 'x') return std::strtoull(s.c_str() + 2, nullptr, 16);
		if (s.empty() || !isdigit(s[0])) return 0L;
		return std::stoll(s, nullptr, 10);
	}

	double variant::as_double(size_t i) const
	{
		if (i >= values_.size()) return 0.0;
		auto s = values_[i];
		if (s.empty() || !isdigit(s[0])) return 0.0;
		return std::strtod(s.c_str(), nullptr);
	}

	static std::string _default_string{};

	const std::string& variant::as_string(size_t i) const
	{
		if (i >= values_.size()) return _default_string;
		return values_[i];
	}

	std::wstring variant::as_wstring(size_t i) const
	{
		if (i >= values_.size()) return std::wstring();
		return utf8_to_utf16(values_[i]);
	}
	
#ifndef USE_SIMPLE
	math::float2 variant::as_float2(size_t i) const
	{
		math::float2 result;
		for (int k = 0; k < 2; k++)
		{
			result[k] = i + k >= values_.size() ? 0.0f : safe_strtof(values_[i + k].c_str());
		}
		if (values_.size() - i == 1)
		{
			result.y = result.x;
		}
		return result;
	}

	math::float3 variant::as_float3(size_t i) const
	{
		math::float3 result;
		for (int k = 0; k < 3; k++)
		{
			result[k] = i + k >= values_.size() ? 0.0f : safe_strtof(values_[i + k].c_str());
		}
		if (values_.size() - i == 1)
		{
			result.y = result.z = result.x;
		}
		return result;
	}

	math::float4 variant::as_float4(size_t i) const
	{
		math::float4 result;
		for (int k = 0; k < 4; k++)
		{
			result[k] = i + k >= values_.size() ? 0.0f : safe_strtof(values_[i + k].c_str());
		}
		if (values_.size() - i == 1)
		{
			result.y = result.z = result.w = result.x;
		}
		return result;
	}

	math::rgbm variant::as_rgbm(size_t i) const
	{
		math::rgbm result;
		if ((values_.size() == 1 || values_.size() == 2) && values_[0][0] == '#')
		{
			int r, g, b;
			if (sscanf_s(&values_[0][1], "%02x%02x%02x", &r, &g, &b) == 3)
			{
				result.rgb.r = float(r) / 255.f;
				result.rgb.g = float(g) / 255.f;
				result.rgb.b = float(b) / 255.f;
			}
			else if (sscanf_s(&values_[0][1], "%01x%01x%01x", &r, &g, &b) == 3)
			{
				result.rgb.r = float(r) / 15.f;
				result.rgb.g = float(g) / 15.f;
				result.rgb.b = float(b) / 15.f;
			}
			result.mult = values_.size() == 2 ? safe_strtof(values_[1].c_str()) : 1.f;
			return result;
		}
		for (int k = 0; k < 4; k++)
		{
			result[k] = i + k >= values_.size() ? 0.0f : safe_strtof(values_[i + k].c_str());
		}
		if (values_.size() == 1)
		{
			result.rgb.g = result.rgb.b = result.rgb.r;
			result.mult = 1.0f;
		}
		else if (values_.size() == 3)
		{
			result.mult = 1.0f;
		}
		return result;
	}
#endif
}
