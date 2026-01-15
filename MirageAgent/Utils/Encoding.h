#pragma once

#include <Windows.h>
#include <codecvt>
#include <locale>
#include <string>

std::string cp1251_to_utf8(const char* str)
{
	std::string res;
	WCHAR* ures = NULL;
	char* cres = NULL;
	int result_u = MultiByteToWideChar(1251, 0, str, -1, 0, 0);
	if (result_u != 0)
	{
		ures = new WCHAR[result_u];
		if (MultiByteToWideChar(1251, 0, str, -1, ures, result_u))
		{
			int result_c = WideCharToMultiByte(CP_UTF8, 0, ures, -1, 0, 0, 0, 0);
			if (result_c != 0)
			{
				cres = new char[result_c];
				if (WideCharToMultiByte(CP_UTF8, 0, ures, -1, cres, result_c, 0, 0))
				{
					res = cres;
				}
			}
		}
	}
	delete[] ures, cres;
	return res;
}

std::string utf8_to_cp1251(const char* str)
{
	std::string res;
	WCHAR* ures = NULL;
	char* cres = NULL;
	int result_u = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
	if (result_u != 0)
	{
		ures = new WCHAR[result_u];
		if (MultiByteToWideChar(CP_UTF8, 0, str, -1, ures, result_u))
		{
			int result_c = WideCharToMultiByte(1251, 0, ures, -1, 0, 0, 0, 0);
			if (result_c != 0)
			{
				cres = new char[result_c];
				if (WideCharToMultiByte(1251, 0, ures, -1, cres, result_c, 0, 0))
				{
					res = cres;
				}
			}
		}
	}
	delete[] ures, cres;
	return res;
}

std::string CvWideToAnsi(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;
	return converterX.to_bytes(wstr);
}

bool IsUtf8(const std::string& str)
{
	int c, i, ix, n, j;
	for (i = 0, ix = str.length(); i < ix; i++)
	{
		c = (unsigned char)str[i];
		if (c <= 0x7f) n = 0;
		else if ((c & 0xE0) == 0xC0) n = 1;
		else if ((c & 0xF0) == 0xE0) n = 2;
		else if ((c & 0xF8) == 0xF0) n = 3;
		else return false;
		for (j = 0; j < n && i < ix; j++)
		{
			if ((++i == ix) || (((unsigned char)str[i] & 0xC0) != 0x80)) return false;
		}
	}
	return true;
}
