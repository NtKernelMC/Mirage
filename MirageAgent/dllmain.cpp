#include "Utils.h"
#include "AesCryptor.h"
#include "CShooting.h"
#include "LuaInjector.h"
#include "AnticheatBypass.h"
#include "Signatures.h"
#include "ScriptConfig.h"
#include "CrashHandler.h"

#include <cstdint>

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam)
{
    return CallNextHookEx(NULL, code, wParam, lParam);
}

typedef void(__stdcall* ptrExitProcess)(UINT uExitCode);
ptrExitProcess callExitProcess = nullptr;
void __stdcall hkExitProcess(UINT uExitCode) {}

constexpr DWORD MIRAGE_MMAP_CONTEXT_MAGIC = 0x47524D4Du;
constexpr DWORD MIRAGE_MMAP_CONTEXT_VERSION = 1u;

struct MIRAGE_MMAP_CONTEXT
{
    DWORD magic;
    DWORD version;
    ULONG_PTR imageBase;
    DWORD imageSize;
    ULONG_PTR originalReserved;
};

static uint32_t GetModuleImageSize(HMODULE hModule)
{
    if (!hModule)
        return 0;

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hModule);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(hModule) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;

    return nt->OptionalHeader.SizeOfImage;
}

static void ApplyMirageImageInfo(HMODULE hModule, LPVOID lpReserved)
{
    uintptr_t imageBase = reinterpret_cast<uintptr_t>(hModule);
    uint32_t imageSize = GetModuleImageSize(hModule);

    __try
    {
        if (lpReserved)
        {
            auto* ctx = reinterpret_cast<const MIRAGE_MMAP_CONTEXT*>(lpReserved);
            if (ctx->magic == MIRAGE_MMAP_CONTEXT_MAGIC && ctx->version == MIRAGE_MMAP_CONTEXT_VERSION)
            {
                imageBase = static_cast<uintptr_t>(ctx->imageBase);
                imageSize = ctx->imageSize ? ctx->imageSize : imageSize;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    CrashHandler::SetMirageImageInfo(imageBase, imageSize);
}

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
    RemoveOldLog();
    CrashHandler::Install();
    CrashHandler::LogStatus();
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
			EnsureInlineJumpHook((DWORD)callExitProcess, (DWORD)hkExitProcess, exit_prologue, sizeof(exit_prologue), "ExitProcess");
		}
		else LogInFile(LOG_NAME, xorstr_("[LOG] ExitProcess export is NULL!\n"));
	}
	RemoveOldDumpedScripts(xorstr_("DumpedScripts"));
    RemoveOldDumpedScripts(xorstr_("Chunks"));
    LogInFile(LOG_NAME, xorstr_("[LOG] Mirage Injector By DroidZero! Build Version: %s (%s)\n"), MIRAGE_VERSION, MIRAGE_VERSION_INFO);
    ParseLuaConfig(); // Читаем луашные конфиги, 1 конфиг на 1 луа скрипт
    ParseMirageConfig(); // Грузим настройки луа инжектора
	if (!GetModuleHandleA(xorstr_("netc.dll"))) Sleep(1);
    if (mirage.fork_version == ForkVersion::FORK_VERSION_1_6)
    {
        ModernBypass::EvadeAnticheat();
    }
	else LegacyBypass::EvadeAnticheat();
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
                EnsureInlineJumpHook((DWORD)callGetThreadContext, (DWORD)hookGetThreadContext, thread_prologue, sizeof(thread_prologue), "GetThreadContext");
            }
        }
        else LogInFile(LOG_NAME, xorstr_("[LOG] GetThreadContext export is NULL!\n"));
    }
	HMODULE hKernelBase = GetModuleHandleA(xorstr_("kernelbase.dll")); //kernelbase
	HMODULE hKernel32_2 = GetModuleHandleA(xorstr_("kernel32.dll"));
	HMODULE hForLoadLib = hKernelBase ? hKernelBase : hKernel32_2;
    if (hForLoadLib)
	{
		loadLibraryExWTarget = GetProcAddress(hForLoadLib, xorstr_("LoadLibraryExW"));
		if (loadLibraryExWTarget != nullptr)
		{
			EnsureMinHook(loadLibraryExWTarget, &hkLoadLibraryExW, reinterpret_cast<void**>(&callLoadLibraryExW), "LoadLibraryExW");
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
        ApplyMirageImageInfo(hModule, lpReserved);
        CrashHandler::Install();
        AsyncBitch();
        break;
    case DLL_PROCESS_DETACH:
        CrashHandler::Shutdown();
        ShutdownImGuiHooking();
        break;
    }
    return TRUE;
}

