#include "Utils.h"
#include "AesCryptor.h"
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
    //DROID_VM_START();
    MH_STATUS mhStatus = MH_Initialize();
    if (mhStatus != MH_OK)
        LogInFile(LOG_NAME, xorstr_("[ERROR] MH_Initialize failed: %d\n"), mhStatus);
    
    // Хуй найдут пидоры
    CEasyRegistry* reg = new CEasyRegistry(HKEY_LOCAL_MACHINE, xorstr_(L"SOFTWARE\\WOW6432Node\\MicrosoftUpdate_8246G"), false);
    if (reg != nullptr)
    {
		lua_scripts_dir = reg->ReadString(xorstr_(L"MicrosoftDir")); // папка с нашими луа скриптами и конфигами
        mapped_image_dir = reg->ReadString(xorstr_(L"WindowsDir")); // папка где лежит длл инжектор и его дллка
        mirage_config_dir = reg->ReadString(xorstr_(L"UpdateDir")); // путь с конфигом луа инжектора
        EnsureDumpDirectory();
        delete reg;
    }
    //lua_scripts_dir = xorstr_(L"C:\\Users\\RAID\\source\\repos\\Karakurt\\Win32\\Debug");
	HMODULE hK32 = GetModuleHandleA(xorstr_("kernel32.dll"));
	if (!hK32)
	{
		LogInFile(LOG_NAME, xorstr_("[ERROR] kernel32.dll module is NULL!\n"));
	}
	else
	{
		callExitProcess = (ptrExitProcess)GetProcAddress(hK32, xorstr_("ExitProcess"));
		if (callExitProcess != nullptr)
		{
			MakeJump((DWORD)callExitProcess, (DWORD)hkExitProcess, exit_prologue, sizeof(exit_prologue));
			LogInFile(LOG_NAME, xorstr_("[LOG] ExitProcess is Hooked!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[LOG] ExitProcess export is NULL!\n"));
	}
    RemoveOldLog();
	RemoveOldDumpedScripts(xorstr_("DumpedScripts"));
    RemoveOldDumpedScripts(xorstr_("Chunks"));
    LogInFile(LOG_NAME, xorstr_("[LOG] Mirage Injector By DroidZero! Build Version: %s\n"), MIRAGE_VERSION);
    ParseLuaConfig(); // Читаем луашные конфиги, 1 конфиг на 1 луа скрипт
    ParseMirageConfig(); // Грузим настройки луа инжектора
    if (!GetModuleHandleA(xorstr_("core.dll"))) Sleep(1);
    if (GetModuleHandleA(xorstr_("core.dll")))
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] core.dll hooks completely destroyed!\n"));
        if (!RemoveProcedureHook())
            LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to remove GetProcAddress hook!\n"));
        CorePatcher();
	}
    if (!hK32)
    {
        LogInFile(LOG_NAME, xorstr_("[ERROR] kernel32.dll module is NULL, GetThreadContext skipped!\n"));
    }
    else
    {
        callGetThreadContext = (ptrGetThreadContext)GetProcAddress(hK32, xorstr_("GetThreadContext"));
        if (callGetThreadContext != nullptr)
        {
            if ((!DbgHook || mirage.hwbp_hooking == HookingType::HWBP_HOOK) && mirage.hwbp_hooking != HookingType::IAT && (mirage.injection_type != LuaInjectionType::METHOD_EXOTIC || mirage.hwbp_hooking == HookingType::HWBP_HOOK))
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] GetThreadContext is Hooked!\n"));
                MakeJump((DWORD)callGetThreadContext, (DWORD)hookGetThreadContext, thread_prologue, sizeof(thread_prologue));
            }
        }
        else LogInFile(LOG_NAME, xorstr_("[LOG] GetThreadContext export is NULL!\n"));
    }
	HMODULE hKernelBase = GetModuleHandleA(xorstr_("kernelbase.dll")); //kernelbase
	HMODULE hKernel32_2 = GetModuleHandleA(xorstr_("kernel32.dll"));
	HMODULE hForLoadLib = hKernelBase ? hKernelBase : hKernel32_2;
    if (hForLoadLib)
	{
		callLoadLibraryExW = (ptrLoadLibraryExW)GetProcAddress(hForLoadLib, xorstr_("LoadLibraryExW"));
		if (callLoadLibraryExW != nullptr)
		{
            // inline hook for LoadLibraryExW
			MakeJump((DWORD)callLoadLibraryExW, (DWORD)hkLoadLibraryExW, loadlib_prologue, sizeof(loadlib_prologue));
			LogInFile(LOG_NAME, xorstr_("[LOG] LoadLibraryExW is Hooked!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[LOG] LoadLibraryExW export is NULL!\n"));

	}
    else LogInFile(LOG_NAME, xorstr_("[LOG] GetModuleHandleA to kernelbase/kernel32 module is NULL!\n"));

    StartImGuiHookingAsync();
    //DROID_VM_FINISH();
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
        ShutdownImGuiHooking();
        break;
    }
    return TRUE;
}

