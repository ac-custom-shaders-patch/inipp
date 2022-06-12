// ReSharper disable CppClangTidyCppcoreguidelinesProTypeMemberInit
#pragma once

#include <utility/str_view.h>

namespace utils
{
	#ifndef USE_SIMPLE
	using namespace math;
	#endif

	struct variant
	{
		template <uint8_t Size, uint8_t Count = 1>
		struct inline_str
		{
			uint32_t inline_store;
			struct item
			{
				char data[Size];
			} items[Count];

			static consteval int address_bit()
			{
				return 8 * (Count > 2 ? Count - 1 : Count);
			}

			static consteval int address_mask()
			{
				return 0x1 | 0x1 << address_bit();
			}

			static size_t size_offset(size_t i)
			{
				return 2 + 8 * i;
			}

			bool set(int i, const char* value, size_t l) noexcept
			{
				if (l > Size) return false;
				memcpy(items[i].data, value, l);
				inline_store &= ~(31 << (2 + 8 * i));
				inline_store |= uint8_t(l) << size_offset(i) | (address_mask());
				return true;
			}

			[[nodiscard]] bool fits(const std::string& s) const noexcept
			{
				return s.size() <= Size;
			}

			bool set(int i, const char* value) noexcept
			{
				return set(i, value, strlen(value));
			}

			bool set(int i, const std::string& value) noexcept
			{
				return set(i, value.c_str(), value.size());
			}

			template <std::size_t N>
			bool set(int i, char const (&value)[N]) noexcept
			{
				return set(i, value, N - 1);
			}

			bool set(const std::vector<std::string>& v) noexcept
			{
				for (auto i = 0; i < Count; ++i)
				{
					if (!set(i, v[i])) return false;
				}
				return true;
			}

			template <typename Callback>
			void set(Callback item_callback, std::vector<std::string>& fallback)
			{
				std::array<std::string, Count> tmp_items;
				auto all_good = true;
				for (auto i = 0; i < Count; ++i)
				{
					tmp_items[i] = item_callback(i);
					all_good = all_good && tmp_items[i].size() <= Size;
				}
				if (all_good)
				{
					for (auto i = 0; i < Count; ++i)
					{
						set(i, tmp_items[i]);
					}
				}
				else
				{
					for (auto& i : tmp_items)
					{
						fallback.emplace_back(std::move(i));
					}
				}
			}

			[[nodiscard]] bool set() const noexcept
			{
				return (inline_store & address_mask()) == address_mask();
			}

			[[nodiscard]] bool any_set() const noexcept
			{
				return (inline_store & 0x1) != 0;
			}

			[[nodiscard]] size_t get_size(size_t i) const noexcept
			{
				return size_t(31 & inline_store >> size_offset(i));
			}

			[[nodiscard]] str_view at(size_t i) const noexcept
			{
				if (i >= Count) return str_view{};
				return str_view{items[i].data, 0ULL, get_size(i)};
			}

			[[nodiscard]] size_t size_at(size_t i) const noexcept
			{
				if (i >= Count) return 0ULL;
				return get_size(i);
			}

			[[nodiscard]] std::vector<std::string> vectorize() const
			{
				std::vector<std::string> r;
				r.reserve(Count);
				for (auto i = 0; i < Count; ++i)
				{
					r.emplace_back(items[i].data, get_size(i));
				}
				return r;
			}
		};

		struct optional_vec
		{
			int64_t begin;
			int64_t end;
			int64_t capacity;

			[[nodiscard]] bool empty() const noexcept { return begin == end; }

			std::vector<std::string>& reset() noexcept
			{
				begin = end = capacity = 0;
				return *(std::vector<std::string>*)&begin;
			}

			std::vector<std::string>& get() noexcept { return *(std::vector<std::string>*)&begin; }
			[[nodiscard]] const std::vector<std::string>& get() const noexcept { return *(std::vector<std::string>*)&begin; }
			[[nodiscard]] size_t size() const noexcept { return begin ? get().size() : 0; }

			[[nodiscard]] str_view at(size_t i) const noexcept
			{
				if (begin)
				{
					if (const auto& v = get(); i < v.size())
					{
						return str_view::from_str(v[i]);
					}
				}
				return str_view{};
			}

			[[nodiscard]] size_t size_at(size_t i) const noexcept
			{
				if (begin)
				{
					if (const auto& v = get(); i < v.size())
					{
						return v[i].size();
					}
				}
				return 0ULL;
			}

			void dispose() noexcept
			{
				((std::vector<std::string>*)&begin)->~vector();
			}
		};

		union
		{
			inline_str<20> s1;
			inline_str<6, 3> s3;
			inline_str<5, 4> s4;
			optional_vec vec{};
		};

		variant() noexcept
		{
			vec.reset();
		}

		variant(const char* value)
		{
			if (!s1.set(0, value))
			{
				vec.reset().emplace_back(value);
			}
		}

		template <std::size_t N>
		variant(char const (&value)[N])
		{
			if (!s1.set(0, value))
			{
				vec.reset().emplace_back(value);
			}
		}

		variant(const str_view& value)
		{
			if (!s1.set(0, value.data(), value.size()))
			{
				vec.reset().emplace_back(value.str());
			}
		}

		template <typename Callback>
		variant(size_t size, Callback item_callback)
		{
			auto& v = vec.reset();
			if (size == 1)
			{
				s1.set(item_callback, v);
			}
			else if (size == 3)
			{
				s3.set(item_callback, v);
			}
			else if (size == 4)
			{
				s4.set(item_callback, v);
			}
			else if (size > 0)
			{
				v.reserve(size);
				for (auto i = 0U; i < size; ++i)
				{
					v.emplace_back(item_callback(i));
				}
			}
		}

		variant(const std::string& value)
		{
			if (!s1.set(0, value.c_str(), value.size()))
			{
				vec.reset().emplace_back(value);
			}
		}

		variant(std::string&& value)
		{
			if (!s1.set(0, value.c_str(), value.size()))
			{
				vec.reset().emplace_back(std::move(value));
			}
		}

		variant(const std::vector<std::string>& values)
		{
			const auto vs = values.size();
			if (vs == 0) vec.reset();
			else if (!(vs == 1 ? s1.set(values) : vs == 3 ? s3.set(values) : vs == 4 && s4.set(values)))
			{
				vec.reset() = values;
			}
		}

		variant(std::vector<std::string>&& values)
		{
			const auto vs = values.size();
			if (vs == 0) vec.reset();
			else if (!(vs == 1 ? s1.set(values) : vs == 3 ? s3.set(values) : vs == 4 && s4.set(values)))
			{
				vec.reset() = std::move(values);
			}
		}

		variant(const variant& v)
		{
			if (v.has_vec() && !v.vec.empty()) vec.reset() = v.vec.get();
			else vec = v.vec;
		}

		variant& operator=(const variant& v)
		{
			if (this != &v)
			{
				dispose();
				new(this) variant(v);
			}
			return *this;
		}

		variant(variant&& v) noexcept
		{
			if (v.has_vec() && !v.vec.empty()) vec.reset().swap(v.vec.get());
			else vec = v.vec;
		}

		variant& operator=(variant&& v) noexcept
		{
			if (this != &v)
			{
				dispose();
				new(this) variant(std::move(v));
			}
			return *this;
		}

		void swap(variant& o) noexcept { std::swap(vec, o.vec); }
		void dispose() noexcept { if (has_vec()) vec.dispose(); }

		template <std::size_t N>
		bool contains(char const (&cs)[N]) const
		{
			for (auto i = 0U; i < size(); ++i)
			{
				if (at(i) == cs) return true;
			}
			return false;
		}

		[[nodiscard]] std::string join(char c = ',') const;
		[[nodiscard]] variant slice(size_t index) const;
		[[nodiscard]] bool empty() const noexcept { return has_vec() && vec.empty(); }
		[[nodiscard]] size_t size() const noexcept { return s1.set() ? 1 : s3.set() ? 3 : s4.set() ? 4 : vec.size(); }

		[[nodiscard]] str_view operator[](const size_t i) const noexcept
		{
			return at(i);
		}

		[[nodiscard]] str_view at(size_t i) const noexcept
		{
			if (s1.set()) return s1.at(i);
			if (s3.set()) return s3.at(i);
			if (s4.set()) return s4.at(i);
			return vec.at(i);
		}

		[[nodiscard]] size_t size_at(size_t i) const noexcept
		{
			if (s1.set()) return s1.size_at(i);
			if (s3.set()) return s3.size_at(i);
			if (s4.set()) return s4.size_at(i);
			return vec.size_at(i);
		}

		struct iterator
		{
			const variant* t;
			size_t i;
			str_view s;

			bool operator!=(const iterator& o) { return i != o.i; }
			const str_view& operator*() { return s = t->at(i); }

			iterator& operator++()
			{
				++i;
				return *this;
			}
		};

		[[nodiscard]] iterator begin() const { return {this, 0, {}}; }
		[[nodiscard]] iterator end() const { return {this, size(), {}}; }

		void push_back(const std::string& s)
		{
			vectorize().push_back(s);
		}

		void push_back(std::string&& s)
		{
			vectorize().push_back(std::move(s));
		}

		std::vector<std::string>& vectorize();
		void rearrange();

		~variant() { dispose(); }

		#ifndef USE_SIMPLE
		variant(const int2& v);
		variant(const int3& v);
		variant(const int4& v);
		variant(const uint2& v);
		variant(const uint3& v);
		variant(const uint4& v);
		variant(const float2& v);
		variant(const float3& v);
		variant(const float4& v);
		variant(const rgb& v);
		variant(const rgbm& v);
		#endif

		variant(bool value) noexcept { s1.set(0, value ? "1" : "0", 1); }
		variant(int value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(int64_t value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(uint32_t value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(uint64_t value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(float value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(double value) { if (!s1.set(0, std::to_string(value))) vec.reset().emplace_back(std::to_string(value)); }
		variant(const path& value) noexcept : variant(value.string()) { }
		variant(const std::wstring& value) noexcept : variant(utf8(value)) { }
		variant(const wchar_t* value) noexcept : variant(utf8(value)) { }

		template <typename T>
		T as(size_t i = 0) const
		{
			const auto s = size();
			return T::deserialize(s > i ? s - i : 0ULL, [&](size_t j) { return at(i + j); });
		}

		template <>
		variant as(size_t i) const
		{
			if (i == 0) return *this;
			return slice(i);
		}

		template <>
		bool as(size_t i) const { return as_bool(i); }

		template <>
		int32_t as(size_t i) const { return int32_t(as_int64_t(i)); }

		template <>
		uint32_t as(size_t i) const { return uint32_t(as_uint64_t(i)); }

		template <>
		long as(size_t i) const { return long(as_int64_t(i)); }

		template <>
		unsigned long as(size_t i) const { return unsigned long(as_uint64_t(i)); }

		template <>
		int64_t as(size_t i) const { return as_int64_t(i); }

		template <>
		uint64_t as(size_t i) const { return as_uint64_t(i); }

		template <>
		float as(size_t i) const { return as_float(i); }

		template <>
		double as(size_t i) const { return as_double(i); }

		template <>
		str_view as(size_t i) const { return at(i); }

		template <>
		std::string as(size_t i) const { return as_string(i); }

		template <>
		std::wstring as(size_t i) const { return as_wstring(i); }

		template <>
		path as(size_t i) const { return path(as_string(i)); }

		#ifndef USE_SIMPLE
		template <>
		uint2 as(size_t i) const { return as_uint2(i); }

		template <>
		uint3 as(size_t i) const { return as_uint3(i); }

		template <>
		uint4 as(size_t i) const { return as_uint4(i); }

		template <>
		float2 as(size_t i) const { return as_float2(i); }

		template <>
		float3 as(size_t i) const { return as_float3(i); }

		template <>
		float4 as(size_t i) const { return as_float4(i); }

		template <>
		int2 as(size_t i) const { return as_int2(i); }

		template <>
		int3 as(size_t i) const { return as_int3(i); }

		template <>
		int4 as(size_t i) const { return as_int4(i); }

		template <>
		rgb as(size_t i) const { return as_rgbm(i).to_rgb(); }

		template <>
		rgbm as(size_t i) const { return as_rgbm(i); }
		#endif

	private:
		bool as_bool(size_t i) const;
		uint64_t as_uint64_t(size_t i) const;
		int64_t as_int64_t(size_t i) const;
		double as_double(size_t i) const;
		float as_float(size_t i) const;
		std::string as_string(size_t i) const;
		std::wstring as_wstring(size_t i) const;

		#ifndef USE_SIMPLE
		int2 as_int2(size_t i) const;
		int3 as_int3(size_t i) const;
		int4 as_int4(size_t i) const;
		uint2 as_uint2(size_t i) const;
		uint3 as_uint3(size_t i) const;
		uint4 as_uint4(size_t i) const;
		float2 as_float2(size_t i) const;
		float3 as_float3(size_t i) const;
		float4 as_float4(size_t i) const;
		rgbm as_rgbm(size_t i) const;
		#endif

		[[nodiscard]] bool has_vec() const noexcept { return !s1.any_set(); }
	};
}
