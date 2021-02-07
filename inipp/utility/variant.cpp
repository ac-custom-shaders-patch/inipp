#include "stdafx.h"
#include "variant.h"
#include <utility/string_parse.h>

namespace utils
{
	static std::string _default_string{};

	variant::variant(const char* value): values_(1, value) { }
	variant::variant(const wchar_t* value): values_(1, utf16_to_utf8(value)) { }
	variant::variant(const std::vector<std::string>&& values): values_(values) { }
	std::vector<std::string>& variant::data() { return values_; }
	const std::vector<std::string>& variant::data() const { return values_; }

	variant::variant(const std::vector<std::wstring>&& values)
	{
		for (auto& v : values)
		{
			values_.push_back(utf16_to_utf8(v));
		}
	}

	bool variant::as_bool(size_t i) const { return i < values_.size() && parse(values_[i], false); }
	uint64_t variant::as_uint64_t(size_t i) const { return i < values_.size() ? parse(values_[i], 0ULL) : 0; }
	int64_t variant::as_int64_t(size_t i) const { return i < values_.size() ? parse(values_[i], 0LL) : 0; }
	double variant::as_double(size_t i) const { return i < values_.size() ? parse(values_[i], 0.) : 0; }
	float variant::as_float(size_t i) const { return i < values_.size() ? parse(values_[i], 0.f) : 0; }
	const std::string& variant::as_string(size_t i) const { return i < values_.size() ? values_[i] : _default_string; }
	std::wstring variant::as_wstring(size_t i) const { return i < values_.size() ? utf8_to_utf16(values_[i]) : std::wstring(); }

	#ifndef USE_SIMPLE

	template <typename Vector>
	Vector as_vector(const std::vector<std::string>& v, size_t i)
	{
		Vector result;
		for (size_t k = 0U; k < sizeof(Vector) / sizeof(result.x); ++k)
		{
			result[k] = i + k < v.size() ? parse(v[i + k], result[k]) : v.size() - i == 1 ? result[0] : 0;
		}
		return result;
	}

	math::int2 variant::as_int2(size_t i) const { return as_vector<math::int2>(values_, i); }
	math::int3 variant::as_int3(size_t i) const{ return as_vector<math::int3>(values_, i); }
	math::int4 variant::as_int4(size_t i) const{ return as_vector<math::int4>(values_, i); }
	math::uint2 variant::as_uint2(size_t i) const{ return as_vector<math::uint2>(values_, i); }
	math::uint3 variant::as_uint3(size_t i) const{ return as_vector<math::uint3>(values_, i); }
	math::uint4 variant::as_uint4(size_t i) const{ return as_vector<math::uint4>(values_, i); }
	math::float2 variant::as_float2(size_t i) const{ return as_vector<math::float2>(values_, i); }	
	math::float3 variant::as_float3(size_t i) const{ return as_vector<math::float3>(values_, i); }
	math::float4 variant::as_float4(size_t i) const{ return as_vector<math::float4>(values_, i); }

	math::rgbm variant::as_rgbm(size_t i) const
	{
		math::rgbm result;
		if ((values_.size() == 1 || values_.size() == 2) && values_[0][0] == '#')
		{
			uint r, g, b;
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
			result.mult = values_.size() == 2 ? parse(values_[1], 0.f) : 1.f;
			return result;
		}
		for (auto k = 0; k < 4; k++)
		{
			result[k] = i + k >= values_.size() ? 0.f : parse(values_[i + k], 0.f);
		}
		if (values_.size() == 1)
		{
			result.rgb.g = result.rgb.b = result.rgb.r;
			result.mult = 1.f;
		}
		else if (values_.size() == 3)
		{
			result.mult = 1.f;
		}
		return result;
	}
	#endif
}
