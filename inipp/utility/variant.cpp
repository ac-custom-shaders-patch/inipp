#include "stdafx.h"

#include <utility/variant.h>
#include <utility/string_parse.h>
#ifndef USE_SIMPLE
#include <utility/variant_parse.h>
#endif

// #define TRACK_USAGE
#ifdef TRACK_USAGE
#include <unordered_set>
#endif

namespace utils
{
	#ifndef USE_SIMPLE
	variant::variant(const int2& v) : variant{2, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const int3& v) : variant{3, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const int4& v) : variant{4, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const uint2& v) : variant{2, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const uint3& v) : variant{3, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const uint4& v) : variant{4, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const float2& v) : variant{2, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const float3& v) : variant{3, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const float4& v) : variant{4, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const rgb& v) : variant{3, [&](auto i) { return std::to_string(v[i]); }} {}
	variant::variant(const rgbm& v) : variant{4, [&](auto i) { return std::to_string(v[i]); }} {}
	#endif

	std::string variant::join(const char c) const
	{
		std::string r;
		for (const auto& i : *this)
		{
			if (!r.empty())
			{
				r.push_back(c);
			}
			r += i;
		}
		return r;
	}

	variant variant::slice(size_t index) const
	{
		if (index == 0) return *this;
		const auto s = size();
		if (index >= s || s1.set()) return {};
		return variant{s - index, [this, index](size_t i) { return at(i + index).str(); }};
	}

	#ifdef TRACK_USAGE
	static std::unordered_set<variant*> _vectorized;
	static std::unordered_set<variant*> _rearranged;
	#endif

	std::vector<std::string>& variant::vectorize()
	{
		if (!has_vec())
		{
			#ifdef TRACK_USAGE
			if (_rearranged.contains(this)) std::cout << "VECTORIZING REARRANGED!\n";
			_vectorized.insert(this);
			#endif

			if (s1.set())
			{
				auto v = s1.vectorize();
				vec.reset() = std::move(v);
			}
			else if (s3.set())
			{
				auto v = s3.vectorize();
				vec.reset() = std::move(v);
			}
			else
			{
				auto v = s4.vectorize();
				vec.reset() = std::move(v);
			}
		}
		return vec.get();
	}

	void variant::rearrange()
	{
		if (!has_vec())
		{
			#ifdef TRACK_USAGE
			std::cout << "ALREADY REARRANGED\n";
			#endif
			return;
		}

		#ifdef TRACK_USAGE
		if (_vectorized.contains(this)) std::cout << "REARRANGING VECTORIZED!\n";
		_rearranged.insert(this);
		#endif

		auto& v_in = vec.get();
		if (const auto s = v_in.size(); s == 1)
		{
			if (s1.fits(v_in[0]))
			{
				const auto v_own = std::move(v_in);
				s1.set(v_own);
			}
		}
		else if (s == 3)
		{
			if (s3.fits(v_in[0]) && s3.fits(v_in[1]) && s3.fits(v_in[2]))
			{
				const auto v_own = std::move(v_in);
				s3.set(v_own);
			}
		}
		else if (s == 4)
		{
			if (s4.fits(v_in[0]) && s4.fits(v_in[1]) && s4.fits(v_in[2]) && s4.fits(v_in[3]))
			{
				const auto v_own = std::move(v_in);
				s4.set(v_own);
			}
		}
	}

	bool variant::as_bool(size_t i) const { return parse(at(i), false); }
	uint64_t variant::as_uint64_t(size_t i) const { return parse(at(i), 0ULL); }
	int64_t variant::as_int64_t(size_t i) const { return parse(at(i), 0LL); }
	double variant::as_double(size_t i) const { return parse(at(i), 0.); }
	float variant::as_float(size_t i) const { return parse(at(i), 0.f); }
	std::string variant::as_string(size_t i) const { return at(i).str(); }
	std::wstring variant::as_wstring(size_t i) const { return utf16(at(i)); }

	#ifndef USE_SIMPLE

	int2 variant::as_int2(size_t i) const { return variant_parse<int2, variant>(*this, i); }
	int3 variant::as_int3(size_t i) const { return variant_parse<int3>(*this, i); }
	int4 variant::as_int4(size_t i) const { return variant_parse<int4>(*this, i); }
	uint2 variant::as_uint2(size_t i) const { return variant_parse<uint2>(*this, i); }
	uint3 variant::as_uint3(size_t i) const { return variant_parse<uint3>(*this, i); }
	uint4 variant::as_uint4(size_t i) const { return variant_parse<uint4>(*this, i); }
	float2 variant::as_float2(size_t i) const { return variant_parse<float2>(*this, i); }
	float3 variant::as_float3(size_t i) const { return variant_parse<float3>(*this, i); }
	float4 variant::as_float4(size_t i) const { return variant_parse<float4>(*this, i); }
	rgbm variant::as_rgbm(size_t i) const { return variant_parse<rgbm>(*this, i); }
	#endif
}
