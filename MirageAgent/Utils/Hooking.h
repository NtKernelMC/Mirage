#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstring>

inline bool IsJumpHookOpcode(BYTE opcode)
{
	return opcode == 0xE9;
}

inline DWORD ResolveJumpHookTarget(DWORD addr)
{
	__try
	{
		if (addr == 0x0 || !IsJumpHookOpcode(*(BYTE*)addr))
			return 0x0;

		int32_t rel = *(int32_t*)(addr + 1);
		return addr + 5 + rel;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return 0x0;
	}
}

inline bool IsJumpHookInstalled(DWORD addr)
{
	return ResolveJumpHookTarget(addr) != 0x0;
}

inline bool IsJumpHookInstalledTo(DWORD addr, DWORD detour)
{
	return detour != 0x0 && ResolveJumpHookTarget(addr) == detour;
}

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

inline bool EnsureInlineJumpHook(DWORD addr, DWORD detour, BYTE* prologue, size_t prologue_size, const char* hook_name)
{
	if (addr == 0x0 || detour == 0x0 || prologue == nullptr || prologue_size < 5)
		return false;

	const DWORD current_target = ResolveJumpHookTarget(addr);
	if (current_target != 0x0)
	{
		if (current_target == detour)
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] %s inline hook already installed.\n"), hook_name ? hook_name : "hook");
			return true;
		}

		LogInFile(LOG_NAME, xorstr_("[WARN] %s inline hook already patched to %p, skipped.\n"),
			hook_name ? hook_name : "hook",
			(void*)current_target);
		return false;
	}

	MakeJump(addr, detour, prologue, prologue_size);
	if (ResolveJumpHookTarget(addr) == detour)
	{
		LogInFile(LOG_NAME, xorstr_("[LOG] %s inline hook installed.\n"), hook_name ? hook_name : "hook");
		return true;
	}

	LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to install inline hook for %s.\n"), hook_name ? hook_name : "hook");
	return false;
}

inline bool EnsureMinHookCreated(void* target, void* detour, void** original, const char* hook_name)
{
	if (!target || !detour)
		return false;

	const DWORD current_target = ResolveJumpHookTarget((DWORD)target);
	if (current_target != 0x0)
	{
		if (current_target == (DWORD)detour)
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] %s MinHook jump already installed.\n"), hook_name ? hook_name : "hook");
			return true;
		}

		LogInFile(LOG_NAME, xorstr_("[WARN] %s target already patched to %p, MinHook skipped.\n"),
			hook_name ? hook_name : "hook",
			(void*)current_target);
		return false;
	}

	const MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<LPVOID*>(original));
	if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED)
	{
		if (status == MH_ERROR_ALREADY_CREATED)
			LogInFile(LOG_NAME, xorstr_("[LOG] %s MinHook already created.\n"), hook_name ? hook_name : "hook");
		return true;
	}

	LogInFile(LOG_NAME, xorstr_("[ERROR] MH_CreateHook failed for %s: %d\n"), hook_name ? hook_name : "hook", status);
	return false;
}

inline bool RefreshMinHook(void* target, void* detour, void** original, const char* hook_name)
{
	if (!target || !detour)
		return false;

	MH_STATUS status = MH_DisableHook(target);
	if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
	{
		LogInFile(LOG_NAME, xorstr_("[WARN] MH_DisableHook failed for %s: %d\n"), hook_name ? hook_name : "hook", status);
	}

	status = MH_RemoveHook(target);
	if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
	{
		LogInFile(LOG_NAME, xorstr_("[ERROR] MH_RemoveHook failed for %s: %d\n"), hook_name ? hook_name : "hook", status);
		return false;
	}

	if (original)
		*original = nullptr;

	status = MH_CreateHook(target, detour, reinterpret_cast<LPVOID*>(original));
	if (status != MH_OK)
	{
		LogInFile(LOG_NAME, xorstr_("[ERROR] MH_CreateHook refresh failed for %s: %d\n"), hook_name ? hook_name : "hook", status);
		return false;
	}

	LogInFile(LOG_NAME, xorstr_("[LOG] %s MinHook refreshed.\n"), hook_name ? hook_name : "hook");
	return true;
}

inline bool EnsureMinHookEnabled(void* target, const char* hook_name, void* detour = nullptr)
{
	if (!target)
		return false;

	const MH_STATUS status = MH_EnableHook(target);
	if (status == MH_OK)
	{
		if (detour && !IsJumpHookInstalledTo((DWORD)target, (DWORD)detour))
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] %s MinHook enable finished without jump patch.\n"), hook_name ? hook_name : "hook");
			return false;
		}
		return true;
	}

	if (status == MH_ERROR_ENABLED)
	{
		if (!detour || IsJumpHookInstalledTo((DWORD)target, (DWORD)detour))
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] %s MinHook already enabled.\n"), hook_name ? hook_name : "hook");
			return true;
		}

		LogInFile(LOG_NAME, xorstr_("[WARN] %s MinHook enable state is stale, forcing re-enable.\n"), hook_name ? hook_name : "hook");
		MH_STATUS disable_status = MH_DisableHook(target);
		if (disable_status != MH_OK && disable_status != MH_ERROR_DISABLED)
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] MH_DisableHook failed for %s: %d\n"), hook_name ? hook_name : "hook", disable_status);
			return false;
		}

		const MH_STATUS retry_status = MH_EnableHook(target);
		if (retry_status == MH_OK || retry_status == MH_ERROR_ENABLED)
		{
			if (!detour || IsJumpHookInstalledTo((DWORD)target, (DWORD)detour))
				return true;
		}

		LogInFile(LOG_NAME, xorstr_("[ERROR] MH_EnableHook retry failed for %s: %d\n"), hook_name ? hook_name : "hook", retry_status);
		return false;
	}

	LogInFile(LOG_NAME, xorstr_("[ERROR] MH_EnableHook failed for %s: %d\n"), hook_name ? hook_name : "hook", status);
	return false;
}

inline bool EnsureMinHook(void* target, void* detour, void** original, const char* hook_name)
{
	if (!target || !detour)
		return false;

	const DWORD current_target = ResolveJumpHookTarget((DWORD)target);
	if (current_target != 0x0)
	{
		if (current_target == (DWORD)detour)
			return EnsureMinHookEnabled(target, hook_name, detour);

		LogInFile(LOG_NAME, xorstr_("[WARN] %s target already patched to %p, MinHook skipped.\n"),
			hook_name ? hook_name : "hook",
			(void*)current_target);
		return false;
	}

	const MH_STATUS create_status = MH_CreateHook(target, detour, reinterpret_cast<LPVOID*>(original));
	if (create_status == MH_ERROR_ALREADY_CREATED)
	{
		LogInFile(LOG_NAME, xorstr_("[WARN] %s MinHook record already exists, refreshing target.\n"), hook_name ? hook_name : "hook");
		if (!RefreshMinHook(target, detour, original, hook_name))
			return false;
	}
	else if (create_status != MH_OK)
	{
		LogInFile(LOG_NAME, xorstr_("[ERROR] MH_CreateHook failed for %s: %d\n"), hook_name ? hook_name : "hook", create_status);
		return false;
	}

	return EnsureMinHookEnabled(target, hook_name, detour);
}
