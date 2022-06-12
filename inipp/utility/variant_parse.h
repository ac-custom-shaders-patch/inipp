#pragma once
#include <variant>
#include <utility/string_parse.h>

namespace utils
{
	using namespace math;

	namespace utils_inner
	{
		template<typename Container>
		struct variant_parse_helper
		{
			template <typename Vector>
			static Vector variant_parse(const Container& v, size_t i)
			{
				Vector result;
				for (size_t k = 0U; k < sizeof(Vector) / sizeof(result[0]); ++k)
				{
					result[k] = i + k < v.size() ? parse(v[i + k], result[k]) : v.size() - i == 1 ? result[0] : 0;
				}
				return result;
			}
			
			template <>
			static float variant_parse(const Container& v, size_t i)
			{
				return i < v.size() ? parse(v[i], 0.f) : 0.f;
			}

			template <>
			static rgbm variant_parse<>(const Container& v, size_t i)
			{
				rgbm result;
				if ((v.size() == 1 || v.size() == 2) && v[0][0] == '#')
				{
					uint r, g, b;
					if (sscanf_s(&v[0][1], "%02x%02x%02x", &r, &g, &b) == 3)
					{
						result.rgb.r = float(r) / 255.f;
						result.rgb.g = float(g) / 255.f;
						result.rgb.b = float(b) / 255.f;
					}
					else if (sscanf_s(&v[0][1], "%01x%01x%01x", &r, &g, &b) == 3)
					{
						result.rgb.r = float(r) / 15.f;
						result.rgb.g = float(g) / 15.f;
						result.rgb.b = float(b) / 15.f;
					}
					result.mult = v.size() == 2 ? parse(v[1], 0.f) : 1.f;
					return result;
				}
				if (v.size() == 1)
				{
					// Fix for refracting lights
					const str_view s = v[0];
					if (const auto j = s.find_first_of(','); j != std::string::npos)
					{
						return variant_parse_helper<std::vector<str_view>>::variant_parse<rgbm>(
							s.split(',', false, true), 0);
					}
				}
				for (auto k = 0; k < 4; k++)
				{
					result[k] = i + k >= v.size() ? 0.f : parse(v[i + k], 0.f);
				}
				if (v.size() == 1)
				{
					result.rgb.g = result.rgb.b = result.rgb.r;
					result.mult = 1.f;
				}
				else if (v.size() == 3)
				{
					result.mult = 1.f;
				}
				return result;
			}
		};
	}

	template <typename Item, typename Container>
	Item variant_parse(const Container& v, size_t i)
	{
		return utils_inner::variant_parse_helper<Container>::template variant_parse<Item>(v, i);
	}
}
