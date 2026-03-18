#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstring>

DWORD MakeJump(const DWORD jmp_address, const DWORD hookAddr, BYTE* prologue, const size_t prologue_size, bool enableTrampoline = false)
{
	__try
	{
		DWORD old_prot = 0;
		if (prologue == nullptr || jmp_address == 0x0 || prologue_size < 5) return 0x0;

		if (!VirtualProtect((void*)jmp_address, prologue_size, PAGE_EXECUTE_READWRITE, &old_prot))
			return 0x0;

		memcpy(prologue, (void*)jmp_address, prologue_size);

		BYTE addrToBYTEs[5] = { 0xE9, 0x90, 0x90, 0x90, 0x90 };
		DWORD relOffset = hookAddr - jmp_address - 5;
		memcpy(&addrToBYTEs[1], &relOffset, 4);
		memcpy((void*)jmp_address, addrToBYTEs, 5);
		FlushInstructionCache(GetCurrentProcess(), (void*)jmp_address, prologue_size);

		if (!enableTrampoline)
		{
			VirtualProtect((void*)jmp_address, prologue_size, old_prot, &old_prot);
			return 0x0;
		}

		size_t trampolineSize = 5 + ((prologue_size > 5) ? (prologue_size - 5) : 0);
		PVOID Trampoline = VirtualAlloc(0, trampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (Trampoline == nullptr)
		{
			VirtualProtect((void*)jmp_address, prologue_size, old_prot, &old_prot);
			return 0x0;
		}

		BYTE trampolineJmp[5] = { 0xE9, 0x90, 0x90, 0x90, 0x90 };

		if (prologue_size > 5)
		{
			BYTE nop = 0x90;
			for (size_t x = 0; x < (prologue_size - 5); x++)
			{
				memcpy((void*)(jmp_address + 5 + x), &nop, 1);
			}
			memcpy(Trampoline, &prologue[3], (prologue_size - 3));

			DWORD delta = (jmp_address + prologue_size) - (((DWORD)Trampoline + (prologue_size - 3)) + 5);
			memcpy(&trampolineJmp[1], &delta, 4);
			memcpy((void*)((DWORD)Trampoline + (prologue_size - 3)), trampolineJmp, 5);
		}
		else
		{
			DWORD delta = (jmp_address + prologue_size) - ((DWORD)Trampoline + 5);
			memcpy(&trampolineJmp[1], &delta, 4);
			memcpy(Trampoline, trampolineJmp, 5);
		}

		VirtualProtect((void*)jmp_address, prologue_size, old_prot, &old_prot);
		return (DWORD)Trampoline;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return 0x0;
	}
}

bool RestorePrologue(DWORD addr, BYTE* prologue, size_t prologue_size, DWORD trampoline = 0x0)
{
	__try
	{
		if (prologue == nullptr || addr == 0x0 || prologue_size == 0)
			return false;

		DWORD old_prot = 0;
		if (!VirtualProtect((void*)addr, prologue_size, PAGE_EXECUTE_READWRITE, &old_prot))
			return false;

		memcpy((void*)addr, prologue, prologue_size);
		FlushInstructionCache(GetCurrentProcess(), (void*)addr, prologue_size);
		VirtualProtect((void*)addr, prologue_size, old_prot, &old_prot);

		if (trampoline != 0)
		{
			VirtualFree((LPVOID)trampoline, 0, MEM_RELEASE);
		}

		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}
