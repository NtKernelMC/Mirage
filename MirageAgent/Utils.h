#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#pragma warning (disable : 4244)
#define LOG_NAME xorstr_("Mirage.log") // Имя лог файла
#define WITH_LOGGING // Закоментить чтобы отключить вывод в лог файл
#define MIRAGE_VERSION xorstr_("V6.1") // Версия инжектора
#define TO_ELEMENTID(x) ((ElementID) reinterpret_cast < unsigned long > (x) )
#include <Windows.h>
#include <stdio.h>
#include <filesystem>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <string>
#include <shlobj_core.h>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <direct.h>
#include <vector>
#include <map>
#include <codecvt>
#include <tuple>
#include "sigscan.h"
#include "xorstr.h"
#include "Registry.h"
#include "HWBP.h"
#include "HWID.h"
#include <intrin.h>
#include "MinHook.h"
#include "CVector.h"
#include "CVector2D.h"
#include <random>
#include "Nirmata.h"
#include "magic_enum/include/magic_enum.hpp"
#include "WepTypes.h"
#include <winternl.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "libMinHook.x86.lib")
#pragma comment(lib, "Winmm.lib")
#include "NetAPI/CNet.h"
#include "NetAPI/Packets.h"
#include "NetAPI/SyncStructures.h"
#include "BreakPoint.h"
#include "Utils/Encoding.h"
#include "Utils/Hooking.h"
#include "Utils/StringSearch.h"
typedef void(__cdecl* ptrWriteCameraOrientation)(const CVector& vecPositionBase, NetBitStreamInterface& BitStream);
ptrWriteCameraOrientation callWriteCameraOrientation = nullptr;
void __stdcall LogInFile(std::string log_name, const char* log, ...);
#include "IAT.h"
CNet* g_pNet = nullptr;
bool crasher = false;
bool cursed_voice = false;
bool DbgHook = true; // по умолчанию разрешаем работу дебаг хуков
void* CNetAPI = nullptr;
bool HideCall = false;
BYTE exit_prologue[5] = { 0x0 };
typedef BOOL(__stdcall* ptrGetThreadContext)(HANDLE hThread, LPCONTEXT lpContext);
ptrGetThreadContext callGetThreadContext = nullptr;
BOOL __stdcall hookGetThreadContext(HANDLE hThread, LPCONTEXT lpContext);
typedef HMODULE(WINAPI* ptrLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
ptrLoadLibraryExW callLoadLibraryExW = nullptr;
BYTE getproc_prologue[5];
void SignatureScanner();
typedef void(__cdecl* ptrScreenShot)(const char* szParameters);
ptrScreenShot callScreenShot = nullptr;
void* CLocalGUI = nullptr;
typedef bool(__thiscall* ptrIsChatBoxInputEnabled)(void* ECX);
ptrIsChatBoxInputEnabled callIsChatBoxInputEnabled = nullptr;
typedef bool(__cdecl* f_SendChat)(const char* szCommand, const char* szArguments, bool bHandleRemotely, bool bHandled, bool bIsScriptedBind, bool AllowBinded);
f_SendChat sendMTAChat = nullptr;
typedef int(__cdecl* t_LuaLoadBuffer)(void* L, const char* buff, size_t sz, const char* name);
t_LuaLoadBuffer callLuaLoadBuffer = nullptr;
typedef int(__cdecl* ptrAddDebugHook)(void* luaVM);
ptrAddDebugHook callAddDebugHook = nullptr;
typedef int(__cdecl* ptrRemoveDebugHook)(void* luaVM);
ptrRemoveDebugHook callRemoveDebugHook = nullptr;
using lua_CFunction = int(__cdecl*)(void*);
#define LUA_GLOBALSINDEX (-10002)
typedef void(__cdecl* lua_pushcclosure)(void* L, lua_CFunction fn, int n);
lua_pushcclosure call_pushcclosure = nullptr;
typedef void(__cdecl* lua_setfield)(void* L, int idx, const char* k);
lua_setfield call_setfield = nullptr;
typedef void(__cdecl* lua_pushboolean)(void* L, int bValue);
lua_pushboolean call_pushboolean = nullptr;
typedef int(__cdecl* lua_toboolean)(void* L, int idx);
lua_toboolean call_toboolean = nullptr;
typedef int(__cdecl* lua_strlen)(void* L);
lua_strlen call_strlen = nullptr;
typedef const char* (__cdecl* lua_tostring)(void* L, int idx, unsigned int* len);
lua_tostring call_tostring = nullptr;
typedef void(__cdecl* lua_pushnumber)(void* L, long double n);
lua_pushnumber call_pushnumber = nullptr;
typedef void(__cdecl* lua_pushstring)(void* L, const char* s);
lua_pushstring call_pushstring = nullptr;
typedef void* (__cdecl* plua_touserdata)(void* L, int idx);
plua_touserdata call_touserdata = nullptr;
typedef int (__cdecl *ptr_lua_remove)(void* L, int a2);
ptr_lua_remove call_lua_remove = nullptr;
typedef int (__cdecl* lua_objlen)(void* L, int a2);
lua_objlen call_objlen = nullptr;
typedef int(__cdecl* lua_rawgeti)(void* L, int a2, int a3);
lua_rawgeti call_rawgeti = nullptr;
typedef int(__cdecl* lua_rawseti)(void* L, int a2, int a3);
lua_rawseti call_rawseti = nullptr;
typedef void* (__cdecl* lua_settop)(void* L, int a2);
lua_settop call_settop = nullptr;
int __cdecl hkLuaLoadBuffer(void* L, char* buff, size_t sz, const char* name);

BYTE loadlib_prologue[5] = { 0x0 };
BYTE icf_prologue[4] = { 0x0 };
BYTE logger_prologue[4] = { 0x0 };
BYTE packet_prologue[5] = { 0x0 };
BYTE violation1_prologue[8] = { 0x0 };
BYTE violation2_prologue[8] = { 0x0 };
BYTE violation3_prologue[8] = { 0x0 };
BYTE violation4_prologue[8] = { 0x0 };
BYTE gamestart_prologue[8] = { 0x0 };
BYTE loadbuff_prologue[6] = { 0x0 };
BYTE load_prologue[6] = { 0x0 };
BYTE thread_prologue[5] = { 0x0 };
BYTE pmsg_prologue[8] = { 0x0 };
BYTE vertex_static_prologue[5] = { 0x0 };
BYTE vertex_dynamic_prologue[5] = { 0x0 };

bool OneClientLoad = false;

void lua_register(void* L, const char* func_name, lua_CFunction f)
{
	call_pushcclosure(L, (f), 1);
	call_setfield(L, LUA_GLOBALSINDEX, (func_name));
}
FARPROC GetProcedure(const char* szModuleName, const char* szProcName)
{
	PEB* pPeb = (PEB*)__readfsdword(0x30);
	PEB_LDR_DATA* pLdr = pPeb->Ldr;
	LIST_ENTRY* pListHead = &pLdr->InMemoryOrderModuleList;
	LIST_ENTRY* pListEntry = pListHead->Flink;

	while (pListEntry != pListHead)
	{
		LDR_DATA_TABLE_ENTRY* pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

		if (pEntry->FullDllName.Buffer)
		{
			wchar_t* pDllName = pEntry->FullDllName.Buffer;
			wchar_t* pLastSlash = nullptr;

			for (wchar_t* p = pDllName; *p; p++)
			{
				if (*p == L'\\') pLastSlash = p;
			}

			if (pLastSlash) pDllName = pLastSlash + 1;

			bool bMatch = true;
			for (int i = 0; szModuleName[i]; i++)
			{
				if ((pDllName[i] | 0x20) != (szModuleName[i] | 0x20))
				{
					bMatch = false;
					break;
				}
			}

			if (bMatch)
			{
				HMODULE hModule = (HMODULE)pEntry->DllBase;
				IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)hModule;
				IMAGE_NT_HEADERS* pNtHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + pDosHeader->e_lfanew);
				IMAGE_EXPORT_DIRECTORY* pExportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)hModule +
					pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

				DWORD* pNameRVAs = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfNames);
				WORD* pOrdinals = (WORD*)((BYTE*)hModule + pExportDir->AddressOfNameOrdinals);
				DWORD* pFuncRVAs = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfFunctions);

				for (DWORD i = 0; i < pExportDir->NumberOfNames; i++)
				{
					char* pFuncName = (char*)((BYTE*)hModule + pNameRVAs[i]);

					bool bFuncMatch = true;
					for (int j = 0; szProcName[j]; j++)
					{
						if (pFuncName[j] != szProcName[j])
						{
							bFuncMatch = false;
							break;
						}
					}

					if (bFuncMatch)
					{
						WORD wOrdinal = pOrdinals[i];
						return (FARPROC)((BYTE*)hModule + pFuncRVAs[wOrdinal]);
					}
				}
			}
		}

		pListEntry = pListEntry->Flink;
	}

	return nullptr;
}
namespace fs = std::filesystem;

enum class LuaInjectionType
{
	METHOD_LUA_L_LOAD = 0,
	METHOD_LUA_L_LOADBUFFER = 1,
};

enum class ForkVersion
{
	FORK_VERSION_1_6 = 0,
	FORK_VERSION_1_5 = 1,
};

enum class HookingType
{
	HWBP_HOOK = 0,
	INLINE_JUMP = 1,
	IAT = 2,
};

enum class DllInjectionType
{
	MMAP = 0,
	SET_WINDOWS_HOOK = 1,
};

enum class FuckDbgHooksMode
{
	ALLOW_DBG_HOOKS = 0,
	FUCK_DBG_HOOKS = 1,
	PROTECTED_MODE = 2,
	STEALTH_MODE = 3,
};

typedef struct
{
	LuaInjectionType injection_type;
	ForkVersion fork_version;
	FuckDbgHooksMode fuck_dbg_hooks;
	HookingType hwbp_hooking;
	bool Dumper;
	std::string dump_resource_name;
	bool DumpAllScripts;
	DllInjectionType dll_injection_type;

} MIRAGE_CONFIG, *PMIRAGE_CONFIG;
MIRAGE_CONFIG mirage;

typedef struct
{
	std::wstring target_script;
	std::wstring our_script;
} LVM, *PLVM;

bool hwbp_end1 = false;
bool hwbp_end2 = false;

std::vector<LVM> lua_injection_list;

std::wstring lua_scripts_dir = L"";
std::wstring mapped_image_dir = L"";
std::wstring mirage_config_dir = L"";
#include "Utils/Logging.h"
#include "Utils/LuaHelpers.h"
#include "Utils/ScriptDump.h"
#include "Utils/InputUtils.h"
HANDLE hSniperThread = nullptr;
#define WEAPON_SNIPERRIFLE   34
#define MODEL_SNIPER         358

// --- глобальные константы адресов ---
#define ADDR_PLAYER_PED      0xB6F5F0
#define PED_WEAPONS_OFFSET   0x5A0
#define PED_WEAPON_SIZE      0x1C
#define PED_ACTIVE_SLOT      0x718
#define PED_CUR_WEAPON_ID    0x740

// --- типы оригинальных игровых функций ---
typedef void(__thiscall* GiveWeapon_t)(void* ped, int wep, unsigned ammo, bool equip);
typedef void(__thiscall* SetCurrentWep_t)(void* ped, int wep);
typedef void(__cdecl* RequestModel_t)(int model, int flags);
typedef void(__cdecl* LoadAllReq_t)(bool onlyPriority);
typedef void(__cdecl* SetModelDel_t)(int model);

static GiveWeapon_t   GiveWeapon = (GiveWeapon_t)0x5E6080;
static SetCurrentWep_t SetCurrentWeapon = (SetCurrentWep_t)0x5E6280;
static RequestModel_t RequestModel = (RequestModel_t)0x4087E0;
static LoadAllReq_t   LoadAllRequested = (LoadAllReq_t)0x4096C0;
static SetModelDel_t  SetModelDeletable = (SetModelDel_t)0x40ADA0;
DWORD WINAPI SniperThread(LPVOID)
{
	while (true)
	{
		void* player = *reinterpret_cast<void**>(ADDR_PLAYER_PED);
		if (player)
		{
			BYTE slot = *reinterpret_cast<BYTE*>(
				reinterpret_cast<std::uintptr_t>(player) + PED_ACTIVE_SLOT);

			// 1. Гарантируем валидность слота
			if (slot < 13)
			{
				std::uintptr_t weaponBase = reinterpret_cast<std::uintptr_t>(player) +
					PED_WEAPONS_OFFSET + slot * PED_WEAPON_SIZE;

				int weaponType = *reinterpret_cast<int*>(weaponBase);
				DWORD& totalAmmo = *reinterpret_cast<DWORD*>(weaponBase + 0xC);

				if (weaponType != WEAPON_SNIPERRIFLE || totalAmmo < 2)
				{
					RequestModel(MODEL_SNIPER, 2);
					LoadAllRequested(false);
					
					// 2. Передаём this в ECX вручную для полной гарантии (__declspec(naked) вариант)
					__asm {
						mov  ecx, player
						push 1                // equip = true
						push 30               // ammo
						push WEAPON_SNIPERRIFLE
						call GiveWeapon
					}
					__asm {
						mov  ecx, player
						push WEAPON_SNIPERRIFLE
						call SetCurrentWeapon
					}
				}
			}
		}
		Sleep(2500);
	}
	return 0;
}
#include "Utils/NetPackets.h"
#include "Utils/FileSystem.h"
#include "Utils/LuaCommands.h"
void CorePatcher()
{
	SigScan scan; DWORD oldProtect = 0;
	DWORD patch_addr = (DWORD)scan.FindPatternIDA(xorstr_("core.dll"), xorstr_("68 ? ? ? ? 68 ? ? ? ? E8"));
	if (patch_addr != NULL)
	{
		BYTE nop[15] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		VirtualProtect((void*)patch_addr, sizeof(nop), PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy((void*)patch_addr, nop, sizeof(nop));
		VirtualProtect((void*)patch_addr, sizeof(nop), oldProtect, &oldProtect);
		LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AC_HookGetProcAddr!\n"));
	}
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for AC_HookGetProcAddr.\n"));
}
void FuckHWBP()
{
	THREADENTRY32 th32; HANDLE hSnapshot = NULL; th32.dwSize = sizeof(THREADENTRY32);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (Thread32First(hSnapshot, &th32))
	{
		do
		{
			if (th32.th32OwnerProcessID == GetCurrentProcessId() && th32.th32ThreadID != GetCurrentThreadId())
			{
				HANDLE pThread = OpenThread(THREAD_ALL_ACCESS, FALSE, th32.th32ThreadID);
				if (pThread)
				{
					SuspendThread(pThread); CONTEXT context = { 0 };
					context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
					GetThreadContext(pThread, &context);
					context.Dr7 = 0x0;
					*(DWORD_PTR*)&context.Dr0 = 0x0;
					*(DWORD_PTR*)&context.Dr1 = 0x0;
					*(DWORD_PTR*)&context.Dr2 = 0x0;
					*(DWORD_PTR*)&context.Dr3 = 0x0;
					SetThreadContext(pThread, &context);
					ResumeThread(pThread); CloseHandle(pThread);
					LogInFile(LOG_NAME, xorstr_("[LOG] Removed hardware breakpoints from thread ID: %d\n"), th32.th32ThreadID);
				}
			}
		} while (Thread32Next(hSnapshot, &th32));
	}
}
bool RemoveProcedureHook()
{
	HMODULE hK32 = GetModuleHandleA(xorstr_("kernel32.dll"));
	HMODULE hKB = GetModuleHandleA(xorstr_("kernelBase.dll"));

	if (!hK32 && !hKB) return false;

	using GETPROC_T = FARPROC(WINAPI*)(HMODULE, LPCSTR);
	GETPROC_T gp = reinterpret_cast<GETPROC_T>(&::GetProcAddress);

	FARPROC self = nullptr;
	if (hK32) self = gp(hK32, xorstr_("GetProcAddress"));
	if (!self && hKB) self = gp(hKB, xorstr_("GetProcAddress"));
	if (!self) return false;

	BYTE* p = reinterpret_cast<BYTE*>(self);
	const BYTE orig[5] = { 0x8B, 0xFF, 0x55, 0x8B, 0xEC };
	if (std::memcmp(p, orig, sizeof(orig)) == 0) return true;
	if (p[0] != 0xE9)
	{
		LogInFile(LOG_NAME, xorstr_("[ERROR] AC_HookGetProcAddr is not hooked!\n"));
		return false;
	}
	DWORD oldProtect = 0;
	if (!VirtualProtect(p, sizeof(orig), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;

	std::memcpy(p, orig, sizeof(orig));
	FlushInstructionCache(GetCurrentProcess(), p, sizeof(orig));

	DWORD tmp = 0;
	VirtualProtect(p, sizeof(orig), oldProtect, &tmp);

	return true;
}
HMODULE __stdcall hkLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	RestorePrologue((DWORD)callLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue)); // восстанавливаем пролог функции
	HMODULE hModule = callLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	if (lpLibFileName != nullptr && wcslen(lpLibFileName) >= 1)
	{
		std::wstring wstrLibFileName = std::wstring(lpLibFileName);
		bool mappedAsImage = (dwFlags & DONT_RESOLVE_DLL_REFERENCES) != 0;
		if (w_findStringIC(wstrLibFileName, xorstr_(L"client.dll")) && !mappedAsImage)
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] Сработал хук LoadLibraryW на загрузку client.dll!\n"));
			hwbp_end1 = false; // сбрасываем флаг на хуки
			hwbp_end2 = false; // сбрасываем флаг на хуки
			SignatureScanner(); // Запускаем сканнер сигнатур и ставим хуки
		}
	}
	MakeJump((DWORD)callLoadLibraryExW, (DWORD)hkLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue));
	return hModule;
}