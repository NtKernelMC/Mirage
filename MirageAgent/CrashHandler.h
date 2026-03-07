#pragma once

#include <windows.h>
#include <imagehlp.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "Imagehlp.lib")

namespace CrashHandler
{
#if defined(_M_IX86)
    inline std::atomic_bool s_installed = false;
    inline LONG s_inCrashHandler = 0;
    inline LPTOP_LEVEL_EXCEPTION_FILTER s_prevFilter = nullptr;
    inline HANDLE s_process = nullptr;
    inline bool s_symbolsInitialized = false;
    inline uintptr_t s_mainBase = 0;
    inline uint32_t s_mainSize = 0;

    inline const char* BaseName(const char* path)
    {
        if (!path)
            return "unknown";
        const char* tail = path;
        for (const char* p = path; *p; ++p)
        {
            if (*p == '\\' || *p == '/')
                tail = p + 1;
        }
        return tail;
    }

    inline void BuildPath(char* out, size_t outSize, const char* fileName)
    {
        if (!out || outSize == 0)
            return;
        out[0] = '\0';

        char exePath[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
            return;

        char* slash = std::strrchr(exePath, '\\');
        if (slash)
            *(slash + 1) = '\0';

        sprintf_s(out, outSize, "%s%s", exePath, fileName ? fileName : "mirage_crash.log");
    }

    inline void AppendLog(const char* fmt, ...)
    {
        if (!fmt)
            return;

        char text[4096]{};
        va_list args;
        va_start(args, fmt);
        vsnprintf_s(text, sizeof(text), _TRUNCATE, fmt, args);
        va_end(args);

        char path[MAX_PATH]{};
        BuildPath(path, sizeof(path), "mirage_crash.log");

        HANDLE hFile = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD written = 0;
        const DWORD len = static_cast<DWORD>(std::strlen(text));
        WriteFile(hFile, text, len, &written, nullptr);
        CloseHandle(hFile);
    }

    inline void RefreshMainModuleInfo()
    {
        s_mainBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        s_mainSize = 0;
        if (!s_mainBase)
            return;

        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(s_mainBase);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return;
        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(s_mainBase + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return;
        s_mainSize = nt->OptionalHeader.SizeOfImage;
    }

    inline bool IsMainAddress(uintptr_t addr)
    {
        return s_mainBase && s_mainSize && addr >= s_mainBase && addr < (s_mainBase + s_mainSize);
    }

    inline uint32_t RuntimeToIda(uintptr_t runtimeAddr)
    {
        if (!IsMainAddress(runtimeAddr))
            return 0;
        return static_cast<uint32_t>(0x400000u + (runtimeAddr - s_mainBase));
    }

    inline void EnsureSymbols()
    {
        if (s_symbolsInitialized)
            return;

        if (!s_process)
            s_process = GetCurrentProcess();

        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        if (SymInitialize(s_process, nullptr, TRUE))
            s_symbolsInitialized = true;
    }

    inline void DescribeAddress(uintptr_t addr, char* out, size_t outSize)
    {
        if (!out || outSize == 0)
            return;
        out[0] = '\0';

        char modulePath[MAX_PATH]{};
        const char* moduleName = "unknown";

        DWORD64 moduleBase = SymGetModuleBase64(s_process, static_cast<DWORD64>(addr));
        if (!moduleBase)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)))
                moduleBase = reinterpret_cast<DWORD64>(mbi.AllocationBase);
        }

        if (moduleBase)
        {
            if (GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), modulePath, MAX_PATH))
                moduleName = BaseName(modulePath);
        }

        char symbolStorage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<PSYMBOL_INFO>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const bool hasSymbol = SymFromAddr(s_process, static_cast<DWORD64>(addr), &displacement, symbol) == TRUE;

        if (IsMainAddress(addr))
        {
            const uint32_t idaAddr = RuntimeToIda(addr);
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX [IDA 0x%08X]", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), idaAddr);
            else
                sprintf_s(out, outSize, "%s+0x%llX [IDA 0x%08X]", moduleName, static_cast<unsigned long long>(addr - static_cast<uintptr_t>(moduleBase)), idaAddr);
        }
        else
        {
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX", moduleName, symbol->Name, static_cast<unsigned long long>(displacement));
            else if (moduleBase)
                sprintf_s(out, outSize, "%s+0x%llX", moduleName, static_cast<unsigned long long>(addr - static_cast<uintptr_t>(moduleBase)));
            else
                sprintf_s(out, outSize, "0x%08X", static_cast<unsigned int>(addr));
        }
    }

    inline void DumpStackTrace(const CONTEXT& sourceCtx, bool onlyMainModule)
    {
        CONTEXT ctx = sourceCtx;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = ctx.Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx.Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx.Esp;
        frame.AddrStack.Mode = AddrModeFlat;

        HANDLE thread = GetCurrentThread();
        for (uint32_t i = 0; i < 64; ++i)
        {
            BOOL ok = StackWalk64(
                IMAGE_FILE_MACHINE_I386,
                s_process,
                thread,
                &frame,
                &ctx,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr);
            if (!ok || frame.AddrPC.Offset == 0)
                break;

            const uintptr_t pc = static_cast<uintptr_t>(frame.AddrPC.Offset);
            if (onlyMainModule && !IsMainAddress(pc))
                continue;

            char desc[1024]{};
            DescribeAddress(pc, desc, sizeof(desc));
            AppendLog("  #%02u  0x%08X  %s\n", i, static_cast<unsigned int>(pc), desc);
        }
    }

    inline void DumpRawStack(const CONTEXT& ctx)
    {
        AppendLog("Raw stack (ESP):\n");
        for (uint32_t i = 0; i < 48; ++i)
        {
            const uintptr_t slot = static_cast<uintptr_t>(ctx.Esp) + i * sizeof(uint32_t);
            uint32_t value = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(s_process, reinterpret_cast<LPCVOID>(slot), &value, sizeof(value), &read) || read != sizeof(value))
                break;

            if (IsMainAddress(value))
                AppendLog("  [esp+0x%02X] = 0x%08X  [IDA 0x%08X]\n", i * 4, value, RuntimeToIda(value));
            else
                AppendLog("  [esp+0x%02X] = 0x%08X\n", i * 4, value);
        }
    }

    inline bool IsFatalException(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
            return true;
        default:
            return false;
        }
    }

    inline LONG WINAPI UnhandledFilter(PEXCEPTION_POINTERS ep)
    {
        if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        if (!IsFatalException(ep->ExceptionRecord->ExceptionCode))
            return EXCEPTION_CONTINUE_SEARCH;

        if (InterlockedCompareExchange(&s_inCrashHandler, 1, 0) != 0)
            return EXCEPTION_EXECUTE_HANDLER;

        EnsureSymbols();
        RefreshMainModuleInfo();

        SYSTEMTIME st{};
        GetLocalTime(&st);
        AppendLog("\n================ CRASH =================\n");
        AppendLog("Time: %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        AppendLog("Code: 0x%08X\n", ep->ExceptionRecord->ExceptionCode);
        AppendLog("Address: 0x%08X\n", static_cast<unsigned int>(reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress)));
        AppendLog("Registers: EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X EIP=%08X\n",
            ep->ContextRecord->Eax,
            ep->ContextRecord->Ebx,
            ep->ContextRecord->Ecx,
            ep->ContextRecord->Edx,
            ep->ContextRecord->Esi,
            ep->ContextRecord->Edi,
            ep->ContextRecord->Ebp,
            ep->ContextRecord->Esp,
            ep->ContextRecord->Eip);

        AppendLog("GTA SA main-module stack trace:\n");
        DumpStackTrace(*ep->ContextRecord, true);
        AppendLog("Full stack trace:\n");
        DumpStackTrace(*ep->ContextRecord, false);
        DumpRawStack(*ep->ContextRecord);
        AppendLog("========================================\n");

        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline void Install()
    {
        if (s_installed.exchange(true))
            return;

        s_process = GetCurrentProcess();
        RefreshMainModuleInfo();
        EnsureSymbols();
        s_prevFilter = SetUnhandledExceptionFilter(UnhandledFilter);
        AppendLog("[CrashHandler] Installed.\n");
    }

    inline void Shutdown()
    {
        if (!s_installed.exchange(false))
            return;

        SetUnhandledExceptionFilter(s_prevFilter);
        s_prevFilter = nullptr;
        AppendLog("[CrashHandler] Removed.\n");
    }
#else
    inline void Install() {}
    inline void Shutdown() {}
#endif
} // namespace CrashHandler
