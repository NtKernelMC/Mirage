#pragma once

#include <array>
#include <string>
#include <unordered_map>

inline wchar_t to_lower_wchar(wchar_t ch)
{
	return (ch >= L'A' && ch <= L'Z') ? (ch + (L'a' - L'A')) : ch;
}

bool w_findStringIC(const std::wstring& haystack, const std::wstring& needle)
{
	const size_t hLen = haystack.size();
	const size_t nLen = needle.size();

	if (nLen == 0) return true;
	if (nLen > hLen) return false;

	std::unordered_map<wchar_t, size_t> shift;
	for (size_t i = 0; i < nLen - 1; ++i)
	{
		wchar_t key = to_lower_wchar(needle[i]);
		shift[key] = nLen - 1 - i;
	}

	size_t i = 0;
	while (i <= hLen - nLen)
	{
		int j = static_cast<int>(nLen) - 1;
		while (j >= 0 && to_lower_wchar(haystack[i + j]) == to_lower_wchar(needle[j]))
		{
			--j;
		}
		if (j < 0)
		{
			return true;
		}
		wchar_t current = to_lower_wchar(haystack[i + nLen - 1]);
		size_t shiftValue = nLen;
		auto it = shift.find(current);
		if (it != shift.end())
		{
			shiftValue = it->second;
		}
		i += shiftValue;
	}
	return false;
}

inline char to_lower_ascii(char c)
{
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

bool findStringIC(const std::string& haystack, const std::string& needle)
{
	const size_t hLen = haystack.size();
	const size_t nLen = needle.size();

	if (nLen == 0) return true;
	if (nLen > hLen) return false;

	std::array<size_t, 256> shift;
	shift.fill(nLen);
	for (size_t i = 0; i < nLen - 1; ++i)
	{
		shift[static_cast<unsigned char>(to_lower_ascii(needle[i]))] = nLen - 1 - i;
	}

	size_t i = 0;
	while (i <= hLen - nLen)
	{
		int j = static_cast<int>(nLen) - 1;
		while (j >= 0 && to_lower_ascii(haystack[i + j]) == to_lower_ascii(needle[j]))
		{
			--j;
		}
		if (j < 0)
		{
			return true;
		}
		i += shift[static_cast<unsigned char>(to_lower_ascii(haystack[i + nLen - 1]))];
	}
	return false;
}
