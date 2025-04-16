#define WITH_LOGGING
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
#define LOG_NAME xorstr_("MirageInjector.log") // Имя лог файла
#define WITH_LOGGING // Закоментить чтобы отключить вывод в лог файл
#include <Windows.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <codecvt>
#include <string>
#include <unordered_map>
#include <array>
#include <direct.h>
#include <vector>
#include <map>
#include "xorstr.h"
#include "Registry.h"
#include <TlHelp32.h>
#include "MMAP.h"

std::wstring lua_scripts_dir = L"";
std::wstring mapped_image_dir = L"";
std::wstring mirage_config_dir = L"";

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

} MIRAGE_CONFIG, * PMIRAGE_CONFIG;
MIRAGE_CONFIG mirage;
int GetGtaProc()
{
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    auto pe = PROCESSENTRY32{ sizeof(PROCESSENTRY32) };
    if (Process32First(snapshot, &pe))
    {
        do
        {
            if (strstr(pe.szExeFile, xorstr_("proxy_sa.exe")) != nullptr || strstr(pe.szExeFile, xorstr_("gta_sa.exe")) != nullptr)
            {
                return pe.th32ProcessID;
            }
        } while (Process32Next(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return -1;
}
void RemoveOldLog()
{
#ifdef WITH_LOGGING
    char new_dir[600];
    memset(new_dir, 0, sizeof(new_dir));
    sprintf(new_dir, xorstr_("%ls\\%s"), mapped_image_dir.c_str(), LOG_NAME);
    FILE* hFile = fopen(new_dir, xorstr_("rb"));
    if (hFile) { fclose(hFile); DeleteFileA(new_dir); }
#endif
}
void __stdcall LogInFile(std::string log_name, const char* log, ...)
{
#ifdef WITH_LOGGING
    char new_dir[600];
    memset(new_dir, 0, sizeof(new_dir));
    sprintf(new_dir, xorstr_("%ls\\%s"), mapped_image_dir.c_str(), log_name.c_str());
    FILE* hFile = fopen(new_dir, xorstr_("a+"));
    if (hFile)
    {
        time_t t = std::time(0);
        tm* now = std::localtime(&t);
        char tmp_stamp[600];
        memset(tmp_stamp, 0, sizeof(tmp_stamp));
        sprintf(tmp_stamp, xorstr_("[%d:%d:%d] "), now->tm_hour, now->tm_min, now->tm_sec);
        strcat(tmp_stamp, log);
        va_list args; va_start(args, log);
        va_list args_copy; va_copy(args_copy, args);
        vfprintf(hFile, tmp_stamp, args_copy);
        va_end(args_copy);
        vprintf(tmp_stamp, args);
        va_end(args);
        fclose(hFile);
    }
#endif
}
// Вспомогательная функция для обрезки пробелов с начала и конца строки
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

void ParseMirageConfig()
{
    // Пытаемся открыть конфигурационный файл для чтения
    std::ifstream file(mirage_config_dir);
    if (!file.is_open())
    {
        // Файл не найден, создаём файл конфигурации с содержимым по умолчанию
        std::ofstream outConfig(mirage_config_dir);
        if (outConfig.is_open())
        {
            outConfig << xorstr_("LUA_INJECTION_TYPE=METHOD_LUA_L_LOADBUFFER\n")
                << xorstr_("FORK_VERSION=FORK_VERSION_1_5\n")
                << xorstr_("FUCK_DBG_HOOKS=ALLOW_DBG_HOOKS\n")
                << xorstr_("HOOKING_METHOD=INLINE_JUMP\n")
                << xorstr_("DUMPER=0\n")
                << xorstr_("DUMP_RESOURCE_NAME=admin\n")
                << xorstr_("DUMP_ALL_SCRIPTS=0\n")
                << xorstr_("DLL_INJECTION_TYPE=MMAP");
            outConfig.close();
            LogInFile(LOG_NAME, xorstr_("[LOG] Default config file created.\n"));
            // Пытаемся снова открыть файл для чтения
            file.open(mirage_config_dir);
            if (!file.is_open())
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to open config file after creation!\n"));
                return;
            }
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to create default config file!\n"));
            return;
        }
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Удаляем пробелы по краям строки
        line = trim(line);
        if (line.empty() || line[0] == '#') // Пропуск пустых строк или комментариев
            continue;

        // Разбиваем строку по символу '='
        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid config line (no '=')!\n"));
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (key == xorstr_("LUA_INJECTION_TYPE"))
        {
            if (value == xorstr_("METHOD_LUA_L_LOAD"))
                mirage.injection_type = LuaInjectionType::METHOD_LUA_L_LOAD;
            else if (value == xorstr_("METHOD_LUA_L_LOADBUFFER"))
                mirage.injection_type = LuaInjectionType::METHOD_LUA_L_LOADBUFFER;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid LUA_INJECTION_TYPE value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("FORK_VERSION"))
        {
            if (value == xorstr_("FORK_VERSION_1_6"))
                mirage.fork_version = ForkVersion::FORK_VERSION_1_6;
            else if (value == xorstr_("FORK_VERSION_1_5"))
                mirage.fork_version = ForkVersion::FORK_VERSION_1_5;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid FORK_VERSION value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("FUCK_DBG_HOOKS"))
        {
            // Теперь значение читается как текст вместо числа.
            if (value == xorstr_("ALLOW_DBG_HOOKS"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::ALLOW_DBG_HOOKS;
            }
            else if (value == xorstr_("FUCK_DBG_HOOKS"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::FUCK_DBG_HOOKS;
            }
            else if (value == xorstr_("PROTECTED_MODE"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::PROTECTED_MODE;
            }
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid FUCK_DBG_HOOKS value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("HOOKING_METHOD"))
        {
            if (value == xorstr_("HWBP_HOOK"))
                mirage.hwbp_hooking = HookingType::HWBP_HOOK;
            else if (value == xorstr_("INLINE_JUMP"))
                mirage.hwbp_hooking = HookingType::INLINE_JUMP;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid HOOKING_METHOD value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DUMPER"))
        {
            try {
                int flag = std::stoi(value);
                if (flag == 0 || flag == 1)
                    mirage.Dumper = static_cast<bool>(flag);
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DUMPER value!\n"));
                    continue;
                }
            }
            catch (...)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: exception parsing DUMPER!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DUMP_RESOURCE_NAME"))
        {
            mirage.dump_resource_name = value; // Здесь можно также выполнить дополнительную обработку при необходимости
        }
        else if (key == xorstr_("DUMP_ALL_SCRIPTS"))
        {
            try {
                int flag = std::stoi(value);
                if (flag == 0 || flag == 1)
                    mirage.DumpAllScripts = static_cast<bool>(flag);
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DUMP_ALL_SCRIPTS value!\n"));
                    continue;
                }
            }
            catch (...)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: exception parsing DUMP_ALL_SCRIPTS!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DLL_INJECTION_TYPE"))
        {
            if (value == xorstr_("MMAP"))
            {
                mirage.dll_injection_type = DllInjectionType::MMAP;
				LogInFile(LOG_NAME, xorstr_("DLL_INJECTION_TYPE is MMAP\n"));
            }
            else if (value == xorstr_("SET_WINDOWS_HOOK"))
            {
                mirage.dll_injection_type = DllInjectionType::SET_WINDOWS_HOOK;
                LogInFile(LOG_NAME, xorstr_("DLL_INJECTION_TYPE is SET_WINDOWS_HOOK\n"));
            }
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DLL_INJECTION_TYPE value!\n"));
                continue;
            }
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Warning: Unknown key in config: %s\n"), key.c_str());
        }
    }
}
std::wstring CvAnsiToWide(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
    return converterX.from_bytes(str);
}
std::string CvWideToAnsi(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
    return converterX.to_bytes(wstr);
}
// Функция, которая ищет поток целевого процесса, у которого есть хотя бы одно окно
DWORD GetThreadWithWindow(DWORD processID)
{
    DWORD threadID = 0;
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(hThreadSnap, &te))
        {
            do {
                if (te.th32OwnerProcessID == processID)
                {
                    HWND hWnd = nullptr;
                    // Перебираем окна данного потока
                    EnumThreadWindows(te.th32ThreadID,
                        [](HWND hwnd, LPARAM lParam) -> BOOL {
                            HWND* phWnd = reinterpret_cast<HWND*>(lParam);
                            // Можно добавить дополнительные проверки (например, IsWindowVisible)
                            *phWnd = hwnd;
                            return FALSE; // Нашли окно – прекращаем перебор для данного потока
                        },
                        reinterpret_cast<LPARAM>(&hWnd));

                    if (hWnd != nullptr)
                    {
                        threadID = te.th32ThreadID;
                        break;
                    }
                }
            } while (Thread32Next(hThreadSnap, &te));
        }
        CloseHandle(hThreadSnap);
    }
    return threadID;
}