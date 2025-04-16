#pragma once
#include <Windows.h>
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")
class SigScan
{
public:
	MODULEINFO GetModuleInfo(const char* szModule)
	{
		MODULEINFO modinfo = { 0 };
		HMODULE hModule = GetModuleHandleA(szModule);
		if (hModule == 0) return modinfo;
		K32GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
		return modinfo;
	}
    DWORD FindPatternIDA(const char* module, const std::string& pattern)
    {
        MODULEINFO mInfo = GetModuleInfo(module);
        DWORD base = (DWORD)mInfo.lpBaseOfDll;
        DWORD size = (DWORD)mInfo.SizeOfImage;

        // Преобразуем строку IDA-стиля в байты и маску
        std::vector<BYTE> patternBytes;
        std::vector<bool> patternMask;

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

        // Поиск паттерна по байтам и маске
        for (DWORD i = 0; i < size - patternBytes.size(); i++)
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
            {
                return base + i;
            }
        }
        return NULL;
    }
	DWORD FindPattern(const char* module, const char* pattern, const char* mask)
	{
		MODULEINFO mInfo = GetModuleInfo(module);
		DWORD base = (DWORD)mInfo.lpBaseOfDll;
		DWORD size = (DWORD)mInfo.SizeOfImage;
		DWORD patternLength = (DWORD)strlen(mask);
		for (DWORD i = 0; i < size - patternLength; i++)
		{
			bool found = true;
			for (DWORD j = 0; j < patternLength; j++)
			{
				found &= mask[j] == '?' || pattern[j] == *(char*)(base + i + j);
			}
			if (found)
			{
				return base + i;
			}
		}
		return NULL;
	}
};