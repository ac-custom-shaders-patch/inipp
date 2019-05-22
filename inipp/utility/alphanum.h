#pragma once

/*
 Released under the MIT License - https://opensource.org/licenses/MIT

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <functional>
#include <string>
#include <sstream>

namespace doj
{
	bool alphanum_isdigit(char c);
	int alphanum_impl(const char* l, const char* r);

	template <typename lT, typename rT>
	int alphanum_comp(const lT& left, const rT& right)
	{
		std::ostringstream l;
		l << left;
		std::ostringstream r;
		r << right;
		return alphanum_impl(l.str().c_str(), r.str().c_str());
	}

	template <>
	int alphanum_comp<std::string>(const std::string& l, const std::string& r);

	int alphanum_comp(char* l, char* r);
	int alphanum_comp(const char* l, const char* r);
	int alphanum_comp(char* l, const char* r);
	int alphanum_comp(const char* l, char* r);
	int alphanum_comp(const std::string& l, char* r);
	int alphanum_comp(char* l, const std::string& r);
	int alphanum_comp(const std::string& l, const char* r);
	int alphanum_comp(const char* l, const std::string& r);

	template <class Ty>
	struct alphanum_less : public std::binary_function<Ty, Ty, bool>
	{
		bool operator()(const Ty& left, const Ty& right) const
		{
			return alphanum_comp(left, right) < 0;
		}
	};
}
