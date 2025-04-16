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
#define MIRAGE_VERSION xorstr_("Alpha 0.1") // Версия инжектора
#include <Windows.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <string>
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

bool DbgHook = true; // по умолчанию разрешаем работу дебаг хуков

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

void lua_register(void* L, const char* func_name, lua_CFunction f)
{
	call_pushcclosure(L, (f), 1);
	call_setfield(L, LUA_GLOBALSINDEX, (func_name));
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
BYTE pmsg_prologue[9] = { 0x0 };
BYTE vertex_static_prologue[5] = { 0x0 };
BYTE vertex_dynamic_prologue[5] = { 0x0 };

bool OneClientLoad = false;

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
		if (callIsChatBoxInputEnabled != nullptr && CLocalGUI != nullptr)
		{
			if (callIsChatBoxInputEnabled(CLocalGUI) && block_input) return;
		}

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
int __cdecl invokeFunction(void* luaVM)
{
	unsigned int strLen = 500;
	std::string func_name = call_tostring(luaVM, 1, &strLen);
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
	call_pushboolean(luaVM, false);
	return 1;
}