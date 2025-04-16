#include "Utils.h"
#include "XorCryptor.h"
#include "LuaInjector.h"
#include "AnticheatBypass.h"
#include "Signatures.h"
#include "ScriptConfig.h"

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam)
{
    return CallNextHookEx(NULL, code, wParam, lParam);
}

typedef HMODULE(WINAPI* ptrLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
ptrLoadLibraryExW callLoadLibraryExW = nullptr;
HMODULE __stdcall hkLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    RestorePrologue((DWORD)callLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue)); // восстанавливаем пролог функции
    HMODULE hModule = callLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    if (lpLibFileName != nullptr && wcslen(lpLibFileName) >= 1)
    {
        std::wstring wstrLibFileName = std::wstring(lpLibFileName);
        if (w_findStringIC(wstrLibFileName, xorstr_(L"client.dll")) && !OneClientLoad)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Сработал хук LoadLibraryW на загрузку client.dll!\n"));
			hwbp_end1 = false; // сбрасываем флаг на хуки
			hwbp_end2 = false; // сбрасываем флаг на хуки
            SignatureScanner(); // Запускаем сканнер сигнатур и ставим хуки
            OneClientLoad = true; // запрещаем поиск сигнатур до реконнекта
        }
    }
    MakeJump((DWORD)callLoadLibraryExW, (DWORD)hkLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue));
    return hModule;
}

void AsyncThread()
{
    // Хуй найдут пидоры
	CEasyRegistry* reg = new CEasyRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\MicrosoftUpdate_8246G", false);
    if (reg != nullptr)
    {
		lua_scripts_dir = reg->ReadString(xorstr_(L"MicrosoftDir")); // папка с нашими луа скриптами и конфигами
        mapped_image_dir = reg->ReadString(xorstr_(L"WindowsDir")); // папка где лежит длл инжектор и его дллка
        mirage_config_dir = reg->ReadString(xorstr_(L"UpdateDir")); // путь с конфигом луа инжектора
        delete reg;
    }
    RemoveOldLog();
	RemoveOldDumpedScripts(xorstr_("DumpedScripts"));
    RemoveOldDumpedScripts(xorstr_("Chunks"));
    LogInFile(LOG_NAME, xorstr_("[LOG] Mirage Injector By DroidZero! Build Version: %s\n"), MIRAGE_VERSION);
    ParseLuaConfig(); // Читаем луашные конфиги, 1 конфиг на 1 луа скрипт
    ParseMirageConfig(); // Грузим настройки луа инжектора
    callGetThreadContext = (ptrGetThreadContext)GetProcAddress(GetModuleHandleA(xorstr_("kernel32.dll")), xorstr_("GetThreadContext"));
    if (callGetThreadContext != nullptr)
    {
        if (!DbgHook || mirage.hwbp_hooking == HookingType::HWBP_HOOK)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] GetThreadContext is Hooked!\n"));
            MakeJump((DWORD)callGetThreadContext, (DWORD)hookGetThreadContext, thread_prologue, sizeof(thread_prologue));
        }
    }
	HMODULE hKernel32 = GetModuleHandleA(xorstr_("kernel32.dll"));
	if (hKernel32)
	{
		callLoadLibraryExW = (ptrLoadLibraryExW)GetProcAddress(hKernel32, xorstr_("LoadLibraryExW"));
		if (callLoadLibraryExW != nullptr)
		{
            // ставим инлайн хук на загрузку дллок в процесс
			MakeJump((DWORD)callLoadLibraryExW, (DWORD)hkLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue));
			LogInFile(LOG_NAME, xorstr_("[LOG] LoadLibraryExW is Hooked!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[LOG] LoadLibraryExW export is NULL!\n"));
	}
    else LogInFile(LOG_NAME, xorstr_("[LOG] GetModuleHandleA to kernel32.dll module is NULL!\n"));
    if (GetModuleHandleA(xorstr_("client.dll")))
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] client.dll was loaded before injection! Executing sig-scanner...\n"));
        SignatureScanner();
    }
}

__forceinline void AsyncBitch()
{
    // отделяемся в другой ассинхронный поток от основного с игры
    std::thread t(AsyncThread);
    t.detach();
}

int __stdcall DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        AsyncBitch();
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

