#pragma once

#include <windows.h>
#include <imagehlp.h>

#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <new.h>

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
    inline uintptr_t s_mirageBase = 0;
    inline uint32_t s_mirageSize = 0;
    inline PVOID s_vectoredHandle = nullptr;
    inline _invalid_parameter_handler s_prevInvalidParameter = nullptr;
    inline _purecall_handler s_prevPureCall = nullptr;
    inline std::terminate_handler s_prevTerminate = nullptr;
    using SignalHandlerFn = void(__cdecl*)(int);
    inline SignalHandlerFn s_prevSigAbrt = SIG_DFL;
    inline SignalHandlerFn s_prevSigIll = SIG_DFL;
    inline SignalHandlerFn s_prevSigFpe = SIG_DFL;
    inline SignalHandlerFn s_prevSigSegv = SIG_DFL;
    inline SignalHandlerFn s_prevSigTerm = SIG_DFL;

    enum class AddressSpace
    {
        Unknown,
        MainModule,
        MirageImage
    };

    enum class StackTraceFilter
    {
        All,
        MainModule,
        MirageImage
    };

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

    inline uint32_t ReadImageSizeFromBase(uintptr_t imageBase)
    {
        if (!imageBase)
            return 0;

        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(imageBase + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return 0;

        return nt->OptionalHeader.SizeOfImage;
    }

    inline void RefreshMainModuleInfo()
    {
        s_mainBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        s_mainSize = ReadImageSizeFromBase(s_mainBase);
    }

    inline void SetMirageImageInfo(uintptr_t imageBase, uint32_t imageSize)
    {
        s_mirageBase = imageBase;
        s_mirageSize = imageSize ? imageSize : ReadImageSizeFromBase(imageBase);
    }

    inline void ClearMirageImageInfo()
    {
        s_mirageBase = 0;
        s_mirageSize = 0;
    }

    inline bool IsAddressInRange(uintptr_t addr, uintptr_t imageBase, uint32_t imageSize)
    {
        return imageBase && imageSize && addr >= imageBase && addr < (imageBase + imageSize);
    }

    inline bool IsMainAddress(uintptr_t addr)
    {
        return IsAddressInRange(addr, s_mainBase, s_mainSize);
    }

    inline bool IsMirageAddress(uintptr_t addr)
    {
        return IsAddressInRange(addr, s_mirageBase, s_mirageSize);
    }

    inline AddressSpace GetAddressSpace(uintptr_t addr)
    {
        if (IsMirageAddress(addr))
            return AddressSpace::MirageImage;
        if (IsMainAddress(addr))
            return AddressSpace::MainModule;
        return AddressSpace::Unknown;
    }

    inline const char* AddressSpaceName(AddressSpace space)
    {
        switch (space)
        {
        case AddressSpace::MirageImage:
            return "Mirage mapped image";
        case AddressSpace::MainModule:
            return "gta_sa.exe main module";
        default:
            return "external/unknown";
        }
    }

    inline uint32_t RuntimeToMainRva(uintptr_t runtimeAddr)
    {
        if (!IsMainAddress(runtimeAddr))
            return 0;
        return static_cast<uint32_t>(runtimeAddr - s_mainBase);
    }

    inline uint32_t RuntimeToMirageRva(uintptr_t runtimeAddr)
    {
        if (!IsMirageAddress(runtimeAddr))
            return 0;
        return static_cast<uint32_t>(runtimeAddr - s_mirageBase);
    }

    inline uint32_t RuntimeToIda(uintptr_t runtimeAddr)
    {
        if (!IsMainAddress(runtimeAddr))
            return 0;
        return static_cast<uint32_t>(0x400000u + RuntimeToMainRva(runtimeAddr));
    }

    inline uint32_t RuntimeToRva(uintptr_t runtimeAddr)
    {
        const AddressSpace space = GetAddressSpace(runtimeAddr);
        if (space == AddressSpace::MirageImage)
            return RuntimeToMirageRva(runtimeAddr);
        if (space == AddressSpace::MainModule)
            return RuntimeToMainRva(runtimeAddr);
        return 0;
    }

    inline bool MatchesStackFilter(StackTraceFilter filter, uintptr_t addr)
    {
        switch (filter)
        {
        case StackTraceFilter::MainModule:
            return IsMainAddress(addr);
        case StackTraceFilter::MirageImage:
            return IsMirageAddress(addr);
        default:
            return true;
        }
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

        const AddressSpace space = GetAddressSpace(addr);
        char modulePath[MAX_PATH]{};
        const char* moduleName = "unknown";
        DWORD64 moduleBase = SymGetModuleBase64(s_process, static_cast<DWORD64>(addr));

        if (space == AddressSpace::MirageImage)
        {
            moduleBase = static_cast<DWORD64>(s_mirageBase);
            if (GetModuleFileNameA(reinterpret_cast<HMODULE>(s_mirageBase), modulePath, MAX_PATH))
                moduleName = BaseName(modulePath);
            else
                moduleName = "MirageAgent(mmap)";
        }
        else
        {
            if (!moduleBase)
            {
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)))
                    moduleBase = reinterpret_cast<DWORD64>(mbi.AllocationBase);
            }

            if (space == AddressSpace::MainModule)
                moduleBase = static_cast<DWORD64>(s_mainBase);

            if (moduleBase && GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), modulePath, MAX_PATH))
                moduleName = BaseName(modulePath);
        }

        char symbolStorage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<PSYMBOL_INFO>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const bool hasSymbol = SymFromAddr(s_process, static_cast<DWORD64>(addr), &displacement, symbol) == TRUE;

        if (space == AddressSpace::MainModule)
        {
            const uint32_t rva = RuntimeToMainRva(addr);
            const uint32_t idaAddr = RuntimeToIda(addr);
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX [RVA 0x%08X] [IDA 0x%08X]", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), rva, idaAddr);
            else
                sprintf_s(out, outSize, "%s+0x%llX [RVA 0x%08X] [IDA 0x%08X]", moduleName, static_cast<unsigned long long>(addr - static_cast<uintptr_t>(moduleBase)), rva, idaAddr);
        }
        else if (space == AddressSpace::MirageImage)
        {
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX [RVA 0x%08X]", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), RuntimeToMirageRva(addr));
            else
                sprintf_s(out, outSize, "%s+0x%llX [RVA 0x%08X]", moduleName, static_cast<unsigned long long>(addr - s_mirageBase), RuntimeToMirageRva(addr));
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

    inline void AppendStackFrame(uint32_t index, uintptr_t pc)
    {
        char desc[1024]{};
        DescribeAddress(pc, desc, sizeof(desc));
        AppendLog("  #%02u  VA 0x%08X  %s\n", index, static_cast<unsigned int>(pc), desc);
    }

    inline void DumpStackTrace(const CONTEXT& sourceCtx, StackTraceFilter filter, uint32_t maxFrames = 64)
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
        uint32_t logged = 0;
        uintptr_t lastPc = 0;

        if (ctx.Eip != 0)
        {
            const uintptr_t pc = static_cast<uintptr_t>(ctx.Eip);
            if (MatchesStackFilter(filter, pc))
            {
                AppendStackFrame(logged++, pc);
                lastPc = pc;
            }
        }

        for (uint32_t i = 0; i < 128 && logged < maxFrames; ++i)
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
            if (!MatchesStackFilter(filter, pc))
                continue;
            if (pc == lastPc)
                continue;

            AppendStackFrame(logged++, pc);
            lastPc = pc;
        }

        if (logged == 0)
            AppendLog("  <empty>\n");
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

            const AddressSpace space = GetAddressSpace(value);
            if (space == AddressSpace::MainModule)
                AppendLog("  [esp+0x%02X] = VA 0x%08X [RVA 0x%08X] [IDA 0x%08X]\n", i * 4, value, RuntimeToMainRva(value), RuntimeToIda(value));
            else if (space == AddressSpace::MirageImage)
                AppendLog("  [esp+0x%02X] = VA 0x%08X [RVA 0x%08X] [Mirage]\n", i * 4, value, RuntimeToMirageRva(value));
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
        case 0xC0000409u:
        case 0xC0000602u:
        case 0x40000015u:
            return true;
        default:
            return false;
        }
    }

    inline bool IsSilentCrashException(DWORD code)
    {
        switch (code)
        {
        case 0xC0000409u:
        case 0xC0000602u:
        case 0x40000015u:
            return true;
        default:
            return false;
        }
    }

    inline CONTEXT CaptureCurrentContext()
    {
        CONTEXT ctx{};
        RtlCaptureContext(&ctx);
        return ctx;
    }

    inline LONG WriteCrashReport(const char* reason, DWORD code, uintptr_t faultAddr, const CONTEXT& ctx)
    {
        if (InterlockedCompareExchange(&s_inCrashHandler, 1, 0) != 0)
            return EXCEPTION_EXECUTE_HANDLER;

        EnsureSymbols();
        RefreshMainModuleInfo();

        if (!faultAddr)
            faultAddr = static_cast<uintptr_t>(ctx.Eip);

        const AddressSpace faultSpace = GetAddressSpace(faultAddr);
        char faultDesc[1024]{};
        DescribeAddress(faultAddr, faultDesc, sizeof(faultDesc));

        SYSTEMTIME st{};
        GetLocalTime(&st);
        AppendLog("\n================ CRASH =================\n");
        AppendLog("Time: %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        if (reason && *reason)
            AppendLog("Reason: %s\n", reason);
        AppendLog("Code: 0x%08X\n", code);
        AppendLog("Address space: %s\n", AddressSpaceName(faultSpace));
        if (faultSpace == AddressSpace::Unknown)
            AppendLog("Address: VA=0x%08X RVA=n/a\n", static_cast<unsigned int>(faultAddr));
        else
            AppendLog("Address: VA=0x%08X RVA=0x%08X\n", static_cast<unsigned int>(faultAddr), RuntimeToRva(faultAddr));
        AppendLog("Address detail: %s\n", faultDesc);
        AppendLog("Main module: base=0x%08X size=0x%08X\n", static_cast<unsigned int>(s_mainBase), s_mainSize);
        if (s_mirageBase && s_mirageSize)
            AppendLog("Mirage image: base=0x%08X size=0x%08X\n", static_cast<unsigned int>(s_mirageBase), s_mirageSize);
        AppendLog("Registers: EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X EIP=%08X\n",
            ctx.Eax,
            ctx.Ebx,
            ctx.Ecx,
            ctx.Edx,
            ctx.Esi,
            ctx.Edi,
            ctx.Ebp,
            ctx.Esp,
            ctx.Eip);
        AppendLog("Recent calls (top 16):\n");
        DumpStackTrace(ctx, StackTraceFilter::All, 16);
        AppendLog("GTA SA main-module stack trace:\n");
        DumpStackTrace(ctx, StackTraceFilter::MainModule, 64);
        AppendLog("Mirage image stack trace:\n");
        DumpStackTrace(ctx, StackTraceFilter::MirageImage, 64);
        AppendLog("Full stack trace:\n");
        DumpStackTrace(ctx, StackTraceFilter::All, 64);
        DumpRawStack(ctx);
        AppendLog("========================================\n");

        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline void HardTerminate(UINT exitCode)
    {
        TerminateProcess(GetCurrentProcess(), exitCode);
    }

    inline const char* SignalName(int sig)
    {
        switch (sig)
        {
        case SIGABRT:
            return "SIGABRT";
        case SIGILL:
            return "SIGILL";
        case SIGFPE:
            return "SIGFPE";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGTERM:
            return "SIGTERM";
        default:
            return "SIGUNKNOWN";
        }
    }

    inline void __cdecl SignalCrashHandler(int sig)
    {
        char reason[128]{};
        sprintf_s(reason, sizeof(reason), "CRT signal %s", SignalName(sig));
        CONTEXT ctx = CaptureCurrentContext();
        WriteCrashReport(reason, 0xE0000100u + static_cast<DWORD>(sig & 0xFF), static_cast<uintptr_t>(ctx.Eip), ctx);
        HardTerminate(0xE0u + static_cast<UINT>(sig & 0xFF));
    }

    inline void __cdecl InvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
    {
        CONTEXT ctx = CaptureCurrentContext();
        WriteCrashReport("CRT invalid parameter", 0xE0000001u, static_cast<uintptr_t>(ctx.Eip), ctx);
        HardTerminate(0xE1u);
    }

    inline void __cdecl PureCallHandler()
    {
        CONTEXT ctx = CaptureCurrentContext();
        WriteCrashReport("CRT pure virtual call", 0xE0000002u, static_cast<uintptr_t>(ctx.Eip), ctx);
        HardTerminate(0xE2u);
    }

    inline void TerminateHandler()
    {
        CONTEXT ctx = CaptureCurrentContext();
        WriteCrashReport("std::terminate", 0xE0000003u, static_cast<uintptr_t>(ctx.Eip), ctx);
        HardTerminate(0xE3u);
    }

    inline LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS ep)
    {
        if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        if (!IsSilentCrashException(ep->ExceptionRecord->ExceptionCode))
            return EXCEPTION_CONTINUE_SEARCH;

        WriteCrashReport("Vectored fatal exit", ep->ExceptionRecord->ExceptionCode, reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress), *ep->ContextRecord);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    inline LONG WINAPI UnhandledFilter(PEXCEPTION_POINTERS ep)
    {
        if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        if (!IsFatalException(ep->ExceptionRecord->ExceptionCode))
            return EXCEPTION_CONTINUE_SEARCH;

        return WriteCrashReport("Unhandled SEH exception", ep->ExceptionRecord->ExceptionCode, reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress), *ep->ContextRecord);
    }

    inline void Install()
    {
        if (s_installed.exchange(true))
            return;

        s_process = GetCurrentProcess();
        RefreshMainModuleInfo();
        EnsureSymbols();
        s_vectoredHandle = AddVectoredExceptionHandler(1, VectoredHandler);
        s_prevInvalidParameter = _set_invalid_parameter_handler(InvalidParameterHandler);
        s_prevPureCall = _set_purecall_handler(PureCallHandler);
        s_prevTerminate = std::set_terminate(TerminateHandler);
        s_prevSigAbrt = std::signal(SIGABRT, SignalCrashHandler);
        s_prevSigIll = std::signal(SIGILL, SignalCrashHandler);
        s_prevSigFpe = std::signal(SIGFPE, SignalCrashHandler);
        s_prevSigSegv = std::signal(SIGSEGV, SignalCrashHandler);
        s_prevSigTerm = std::signal(SIGTERM, SignalCrashHandler);
        s_prevFilter = SetUnhandledExceptionFilter(UnhandledFilter);
        AppendLog("[CrashHandler] Installed.\n");
        if (s_mirageBase && s_mirageSize)
            AppendLog("[CrashHandler] Mirage image tracked at VA 0x%08X size 0x%08X.\n", static_cast<unsigned int>(s_mirageBase), s_mirageSize);
    }

    inline void Shutdown()
    {
        if (!s_installed.exchange(false))
            return;

        if (s_vectoredHandle)
        {
            RemoveVectoredExceptionHandler(s_vectoredHandle);
            s_vectoredHandle = nullptr;
        }
        _set_invalid_parameter_handler(s_prevInvalidParameter);
        s_prevInvalidParameter = nullptr;
        _set_purecall_handler(s_prevPureCall);
        s_prevPureCall = nullptr;
        std::set_terminate(s_prevTerminate);
        s_prevTerminate = nullptr;
        std::signal(SIGABRT, s_prevSigAbrt);
        std::signal(SIGILL, s_prevSigIll);
        std::signal(SIGFPE, s_prevSigFpe);
        std::signal(SIGSEGV, s_prevSigSegv);
        std::signal(SIGTERM, s_prevSigTerm);
        SetUnhandledExceptionFilter(s_prevFilter);
        s_prevFilter = nullptr;
        ClearMirageImageInfo();
        AppendLog("[CrashHandler] Removed.\n");
    }
#else
    inline void Install() {}
    inline void Shutdown() {}
#endif
} // namespace CrashHandler
