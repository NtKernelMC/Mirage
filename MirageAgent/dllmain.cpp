#include "Utils.h"
#include "XorCryptor.h"
#include "CShooting.h"
#include "LuaInjector.h"
#include "AnticheatBypass.h"
#include "Signatures.h"
#include "ScriptConfig.h"

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam)
{
    return CallNextHookEx(NULL, code, wParam, lParam);
}

typedef void(__stdcall* ptrExitProcess)(UINT uExitCode);
ptrExitProcess callExitProcess = nullptr;
void __stdcall hkExitProcess(UINT uExitCode) {}
void AsyncThread()
{
    MH_Initialize();
    
    // Хуй найдут пидоры
    CEasyRegistry* reg = new CEasyRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\MicrosoftUpdate_8246G", false);
    if (reg != nullptr)
    {
		lua_scripts_dir = reg->ReadString(xorstr_(L"MicrosoftDir")); // папка с нашими луа скриптами и конфигами
        mapped_image_dir = reg->ReadString(xorstr_(L"WindowsDir")); // папка где лежит длл инжектор и его дллка
        mirage_config_dir = reg->ReadString(xorstr_(L"UpdateDir")); // путь с конфигом луа инжектора
        EnsureDumpDirectory();
        delete reg;
    }
    //lua_scripts_dir = xorstr_(L"C:\\Users\\RAID\\source\\repos\\Karakurt\\Win32\\Debug");
	callExitProcess = (ptrExitProcess)GetProcAddress(GetModuleHandleA(xorstr_("kernel32.dll")), xorstr_("ExitProcess"));
	if (callExitProcess != nullptr)
	{
		MakeJump((DWORD)callExitProcess, (DWORD)hkExitProcess, exit_prologue, sizeof(exit_prologue));
		LogInFile(LOG_NAME, xorstr_("[LOG] ExitProcess is Hooked!\n"));
	}
	else LogInFile(LOG_NAME, xorstr_("[LOG] ExitProcess export is NULL!\n"));
    RemoveOldLog();
	RemoveOldDumpedScripts(xorstr_("DumpedScripts"));
    RemoveOldDumpedScripts(xorstr_("Chunks"));
    LogInFile(LOG_NAME, xorstr_("[LOG] Mirage Injector By DroidZero! Build Version: %s\n"), MIRAGE_VERSION);
    ParseLuaConfig(); // Читаем луашные конфиги, 1 конфиг на 1 луа скрипт
    ParseMirageConfig(); // Грузим настройки луа инжектора
    if (GetModuleHandleA(xorstr_("core.dll")))
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] core.dll hooks completely destroyed!\n"));
        RemoveProcedureHook();
        CorePatcher();
	}
	else LogInFile(LOG_NAME, xorstr_("[LOG] core.dll module is not loaded!\n"));    
    callGetThreadContext = (ptrGetThreadContext)GetProcAddress(GetModuleHandleA(xorstr_("kernel32.dll")), xorstr_("GetThreadContext"));
    if (callGetThreadContext != nullptr)
    {
        if ((!DbgHook || mirage.hwbp_hooking == HookingType::HWBP_HOOK) && mirage.hwbp_hooking != HookingType::IAT)
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
    static bool tarantul = false;
    while (true)
    {
        //if (GetModuleHandleA(xorstr_("client.dll")) && tarantul) PlayerModInfoRealDoS();
        if (GetModuleHandleA(xorstr_("client.dll")) && GetAsyncKeyState(VK_DELETE))
        {
            MessageBeep(MB_ICONERROR);
            SplitPacket(0x8000);
            Sleep(150);
        }
        Sleep(10);
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

