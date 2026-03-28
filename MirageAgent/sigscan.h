#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <cstdlib>
#include <string>
#include <vector>
#pragma comment (lib, "Psapi.lib")

class SigScan
{
public:
	static constexpr size_t kJumpHookSize = 5;

	MODULEINFO GetModuleInfo(const char* szModule)
	{
		MODULEINFO modinfo = { 0 };
		HMODULE hModule = GetModuleHandleA(szModule);
		if (hModule == 0) return modinfo;
		K32GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
		return modinfo;
	}

	void ParseIdaPattern(const std::string& pattern, std::vector<BYTE>& patternBytes, std::vector<bool>& patternMask)
	{
		patternBytes.clear();
		patternMask.clear();

		for (size_t i = 0; i < pattern.size(); )
		{
			if (pattern[i] == ' ')
			{
				i++;
				continue;
			}
			if (pattern[i] == '?')
			{
				patternBytes.push_back(0x00);
				patternMask.push_back(false);
				if (i + 1 < pattern.size() && pattern[i + 1] == '?')
					i++;
			}
			else
			{
				patternBytes.push_back(static_cast<BYTE>(std::strtoul(pattern.substr(i, 2).c_str(), nullptr, 16)));
				patternMask.push_back(true);
				i++;
			}
			i++;
		}
	}

	void ApplyHookAwareMask(std::vector<bool>& patternMask)
	{
		const size_t count = patternMask.size() < kJumpHookSize ? patternMask.size() : kJumpHookSize;
		for (size_t i = 0; i < count; ++i)
		{
			patternMask[i] = false;
		}
	}

	DWORD FindPatternBytes(DWORD base, DWORD size, const std::vector<BYTE>& patternBytes, const std::vector<bool>& patternMask)
	{
		if (base == 0 || size == 0 || patternBytes.empty() || patternBytes.size() != patternMask.size() || size < patternBytes.size())
			return NULL;

		for (DWORD i = 0; i <= size - patternBytes.size(); i++)
		{
			bool found = true;
			for (DWORD j = 0; j < patternBytes.size(); j++)
			{
				if (patternMask[j] && patternBytes[j] != *(BYTE*)(base + i + j))
				{
					found = false;
					break;
				}
			}
			if (found)
				return base + i;
		}

		return NULL;
	}

	DWORD FindCallPattern(const char* module, const std::string& pattern, bool isJmp = true)
	{
		MODULEINFO mInfo = GetModuleInfo(module);
		DWORD base = (DWORD)mInfo.lpBaseOfDll;
		DWORD size = (DWORD)mInfo.SizeOfImage;
		if (base == 0 || size == 0)
			return NULL;

		std::vector<BYTE> patternBytes;
		std::vector<bool> patternMask;
		ParseIdaPattern(pattern, patternBytes, patternMask);

		DWORD match = FindPatternBytes(base, size, patternBytes, patternMask);
		if (match == NULL)
		{
			ApplyHookAwareMask(patternMask);
			match = FindPatternBytes(base, size, patternBytes, patternMask);
		}
		if (match == NULL)
			return NULL;

		if (isJmp)
		{
			int32_t offset = *(int32_t*)(match + 1);
			return match + 5 + offset;
		}

		return match;
	}

	DWORD FindPattern(const char* module, const char* pattern, const char* mask)
	{
		MODULEINFO mInfo = GetModuleInfo(module);
		DWORD base = (DWORD)mInfo.lpBaseOfDll;
		DWORD size = (DWORD)mInfo.SizeOfImage;
		if (base == 0 || size == 0)
			return NULL;

		const DWORD patternLength = (DWORD)strlen(mask);
		if (patternLength == 0 || size < patternLength)
			return NULL;

		std::vector<BYTE> patternBytes;
		std::vector<bool> patternMask;
		patternBytes.reserve(patternLength);
		patternMask.reserve(patternLength);
		for (DWORD i = 0; i < patternLength; ++i)
		{
			patternBytes.push_back(static_cast<BYTE>(pattern[i]));
			patternMask.push_back(mask[i] != '?');
		}

		DWORD match = FindPatternBytes(base, size, patternBytes, patternMask);
		if (match != NULL)
			return match;

		ApplyHookAwareMask(patternMask);
		return FindPatternBytes(base, size, patternBytes, patternMask);
	}

	DWORD FindPatternIDA(const char* module, const std::string& pattern)
	{
		MODULEINFO mInfo = GetModuleInfo(module);
		if (mInfo.lpBaseOfDll == nullptr) return 0;
		DWORD base = (DWORD)mInfo.lpBaseOfDll;
		DWORD size = (DWORD)mInfo.SizeOfImage;
		if (base == 0 || size == 0)
			return NULL;

		std::vector<BYTE> patternBytes;
		std::vector<bool> patternMask;
		ParseIdaPattern(pattern, patternBytes, patternMask);

		DWORD match = FindPatternBytes(base, size, patternBytes, patternMask);
		if (match != NULL)
			return match;

		ApplyHookAwareMask(patternMask);
		return FindPatternBytes(base, size, patternBytes, patternMask);
	}
};
