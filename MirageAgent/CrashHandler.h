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
#pragma comment(lib, "Dbghelp.lib")

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
    inline BYTE s_setUnhandledFilterBackup[5]{};
    inline BYTE* s_setUnhandledFilterTarget = nullptr;
    inline bool s_setUnhandledFilterPatched = false;
    inline bool s_mirageSymbolsLoaded = false;
    inline DWORD s_lastVectoredCode = 0;
    inline uintptr_t s_lastVectoredPc = 0;
    inline DWORD s_lastVectoredTick = 0;

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

        const char* useFileName = fileName ? fileName : LOG_NAME;

        if (!lua_scripts_dir.empty())
        {
            const std::string logDir = CvWideToAnsi(lua_scripts_dir);
            if (!logDir.empty())
            {
                sprintf_s(out, outSize, "%s\\%s", logDir.c_str(), useFileName);
                return;
            }
        }

        char exePath[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
            return;

        char* slash = std::strrchr(exePath, '\\');
        if (slash)
            *(slash + 1) = '\0';

        sprintf_s(out, outSize, "%s%s", exePath, useFileName);
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
        BuildPath(path, sizeof(path), LOG_NAME);

        HANDLE hFile = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD written = 0;
        const DWORD len = static_cast<DWORD>(std::strlen(text));
        WriteFile(hFile, text, len, &written, nullptr);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }

    inline bool PatchSetUnhandledExceptionFilter()
    {
        if (s_setUnhandledFilterPatched)
            return true;

        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        if (!kernel32)
            return false;

        auto* target = reinterpret_cast<BYTE*>(GetProcAddress(kernel32, "SetUnhandledExceptionFilter"));
        if (!target)
            return false;

        static const BYTE patch[5] = { 0x33, 0xC0, 0xC2, 0x04, 0x00 };

        DWORD oldProtect = 0;
        if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        std::memcpy(s_setUnhandledFilterBackup, target, sizeof(patch));
        std::memcpy(target, patch, sizeof(patch));
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

        DWORD restoreProtect = 0;
        VirtualProtect(target, sizeof(patch), oldProtect, &restoreProtect);

        s_setUnhandledFilterTarget = target;
        s_setUnhandledFilterPatched = true;
        return true;
    }

    inline void UnpatchSetUnhandledExceptionFilter()
    {
        if (!s_setUnhandledFilterPatched || !s_setUnhandledFilterTarget)
            return;

        DWORD oldProtect = 0;
        if (!VirtualProtect(s_setUnhandledFilterTarget, sizeof(s_setUnhandledFilterBackup), PAGE_EXECUTE_READWRITE, &oldProtect))
            return;

        std::memcpy(s_setUnhandledFilterTarget, s_setUnhandledFilterBackup, sizeof(s_setUnhandledFilterBackup));
        FlushInstructionCache(GetCurrentProcess(), s_setUnhandledFilterTarget, sizeof(s_setUnhandledFilterBackup));

        DWORD restoreProtect = 0;
        VirtualProtect(s_setUnhandledFilterTarget, sizeof(s_setUnhandledFilterBackup), oldProtect, &restoreProtect);

        s_setUnhandledFilterTarget = nullptr;
        s_setUnhandledFilterPatched = false;
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
        s_mirageSymbolsLoaded = false;
    }

    inline void ClearMirageImageInfo()
    {
        s_mirageBase = 0;
        s_mirageSize = 0;
        s_mirageSymbolsLoaded = false;
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

    inline void BuildMirageModulePath(char* out, size_t outSize)
    {
        if (!out || outSize == 0)
            return;
        out[0] = '\0';

        if (!mapped_image_dir.empty())
        {
            const std::string baseDir = CvWideToAnsi(mapped_image_dir);
            if (!baseDir.empty())
            {
                sprintf_s(out, outSize, "%s\\MirageAgent.dll", baseDir.c_str());
                return;
            }
        }
    }

    inline void EnsureMirageSymbols()
    {
        if (s_mirageSymbolsLoaded || !s_mirageBase || !s_mirageSize)
            return;

        EnsureSymbols();
        if (!s_symbolsInitialized || !s_process)
            return;

        if (SymGetModuleBase64(s_process, static_cast<DWORD64>(s_mirageBase)) == static_cast<DWORD64>(s_mirageBase))
        {
            s_mirageSymbolsLoaded = true;
            return;
        }

        char modulePath[MAX_PATH]{};
        BuildMirageModulePath(modulePath, sizeof(modulePath));
        if (!modulePath[0])
            return;

        if (SymLoadModuleEx(
            s_process,
            nullptr,
            modulePath,
            "MirageAgent(mmap)",
            static_cast<DWORD64>(s_mirageBase),
            s_mirageSize,
            nullptr,
            0))
        {
            s_mirageSymbolsLoaded = true;
        }
    }

    inline char LowerAscii(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c - 'A' + 'a');
        return c;
    }

    inline bool IsPathSeparator(char c)
    {
        return c == '\\' || c == '/';
    }

    inline bool PathEndsWithInsensitive(const char* path, const char* suffix)
    {
        if (!path || !suffix)
            return false;

        const size_t pathLen = std::strlen(path);
        const size_t suffixLen = std::strlen(suffix);
        if (suffixLen > pathLen)
            return false;

        const char* start = path + (pathLen - suffixLen);
        for (size_t i = 0; i < suffixLen; ++i)
        {
            const char a = start[i];
            const char b = suffix[i];
            if (IsPathSeparator(a) && IsPathSeparator(b))
                continue;
            if (LowerAscii(a) != LowerAscii(b))
                return false;
        }

        return true;
    }

    struct VectoredSourceWhitelistEntry
    {
        const char* fileSuffix;
        DWORD lineStart;
        DWORD lineEnd;
    };

    inline bool IsFatalException(DWORD code);

    inline bool IsMirageVectoredWhitelistHit(uintptr_t pc)
    {
        if (!IsMirageAddress(pc))
            return false;

        EnsureMirageSymbols();
        if (!s_mirageSymbolsLoaded)
            return false;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD displacement = 0;
        if (!SymGetLineFromAddr64(s_process, static_cast<DWORD64>(pc), &displacement, &line) || !line.FileName)
            return false;

        static const VectoredSourceWhitelistEntry whitelist[] = {
            { "MirageAgent\\BreakPoint.h", 42, 60 },
            { "MirageAgent\\Utils\\Hooking.h", 9, 87 },
            { "MirageAgent\\Utils\\LuaCommands.h", 233, 237 },
            { "MirageAgent\\Utils\\NetPackets.h", 221, 225 },
            { "MirageAgent\\Utils\\NetPackets.h", 346, 364 },
            { "MirageAgent\\Utils\\NetPackets.h", 376, 382 },
            { "MirageAgent\\LuaInjector.h", 445, 468 },
            { "MirageAgent\\dllmain.cpp", 54, 66 },
            { "MirageAgent\\Utils\\ImGuiIntegration.h", 706, 775 },
        };

        for (const auto& entry : whitelist)
        {
            if (!PathEndsWithInsensitive(line.FileName, entry.fileSuffix))
                continue;
            if (line.LineNumber >= entry.lineStart && line.LineNumber <= entry.lineEnd)
                return true;
        }

        return false;
    }

    inline bool ShouldLogVectoredMirageException(PEXCEPTION_POINTERS ep)
    {
        if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
            return false;

        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        if (!IsFatalException(code))
            return false;

        const uintptr_t pc = static_cast<uintptr_t>(ep->ContextRecord->Eip);
        const uintptr_t faultAddr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
        if (!IsMirageAddress(pc) && !IsMirageAddress(faultAddr))
            return false;

        if (IsMirageVectoredWhitelistHit(pc))
            return false;

        const DWORD now = GetTickCount();
        if (s_lastVectoredCode == code && s_lastVectoredPc == pc)
        {
            const DWORD delta = now - s_lastVectoredTick;
            if (delta < 750)
                return false;
        }

        s_lastVectoredCode = code;
        s_lastVectoredPc = pc;
        s_lastVectoredTick = now;
        return true;
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
            EnsureMirageSymbols();
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
        IMAGEHLP_LINE64 sourceLine{};
        sourceLine.SizeOfStruct = sizeof(sourceLine);
        DWORD lineDisplacement = 0;
        const bool hasLine = SymGetLineFromAddr64(s_process, static_cast<DWORD64>(addr), &lineDisplacement, &sourceLine) == TRUE && sourceLine.FileName != nullptr;
        char lineSuffix[512]{};
        if (hasLine)
            sprintf_s(lineSuffix, sizeof(lineSuffix), " [File %s:%lu]", sourceLine.FileName, static_cast<unsigned long>(sourceLine.LineNumber));

        if (space == AddressSpace::MainModule)
        {
            const uint32_t rva = RuntimeToMainRva(addr);
            const uint32_t idaAddr = RuntimeToIda(addr);
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX [RVA 0x%08X] [IDA 0x%08X]%s", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), rva, idaAddr, lineSuffix);
            else
                sprintf_s(out, outSize, "%s+0x%llX [RVA 0x%08X] [IDA 0x%08X]%s", moduleName, static_cast<unsigned long long>(addr - static_cast<uintptr_t>(moduleBase)), rva, idaAddr, lineSuffix);
        }
        else if (space == AddressSpace::MirageImage)
        {
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX [RVA 0x%08X]%s", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), RuntimeToMirageRva(addr), lineSuffix);
            else
                sprintf_s(out, outSize, "%s+0x%llX [RVA 0x%08X]%s", moduleName, static_cast<unsigned long long>(addr - s_mirageBase), RuntimeToMirageRva(addr), lineSuffix);
        }
        else
        {
            if (hasSymbol)
                sprintf_s(out, outSize, "%s!%s+0x%llX%s", moduleName, symbol->Name, static_cast<unsigned long long>(displacement), lineSuffix);
            else if (moduleBase)
                sprintf_s(out, outSize, "%s+0x%llX%s", moduleName, static_cast<unsigned long long>(addr - static_cast<uintptr_t>(moduleBase)), lineSuffix);
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

        RefreshMainModuleInfo();

        if (!faultAddr)
            faultAddr = static_cast<uintptr_t>(ctx.Eip);

        SYSTEMTIME st{};
        GetLocalTime(&st);
        AppendLog("\n============ CRASH MINIMAL ============\n");
        AppendLog("Time: %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        if (reason && *reason)
            AppendLog("Reason: %s\n", reason);
        AppendLog("Code: 0x%08X\n", code);
        AppendLog("Fault VA: 0x%08X\n", static_cast<unsigned int>(faultAddr));
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

        __try
        {
            EnsureSymbols();

            const AddressSpace faultSpace = GetAddressSpace(faultAddr);
            char faultDesc[1024]{};
            DescribeAddress(faultAddr, faultDesc, sizeof(faultDesc));

            AppendLog("============== CRASH FULL =============\n");
            AppendLog("Address space: %s\n", AddressSpaceName(faultSpace));
            if (faultSpace == AddressSpace::Unknown)
                AppendLog("Address: VA=0x%08X RVA=n/a\n", static_cast<unsigned int>(faultAddr));
            else
                AppendLog("Address: VA=0x%08X RVA=0x%08X\n", static_cast<unsigned int>(faultAddr), RuntimeToRva(faultAddr));
            AppendLog("Address detail: %s\n", faultDesc);
            AppendLog("Main module: base=0x%08X size=0x%08X\n", static_cast<unsigned int>(s_mainBase), s_mainSize);
            if (s_mirageBase && s_mirageSize)
                AppendLog("Mirage image: base=0x%08X size=0x%08X\n", static_cast<unsigned int>(s_mirageBase), s_mirageSize);
            AppendLog("Recent calls (top 16):\n");
            DumpStackTrace(ctx, StackTraceFilter::All, 16);
            AppendLog("GTA SA main-module stack trace:\n");
            DumpStackTrace(ctx, StackTraceFilter::MainModule, 64);
            AppendLog("Mirage image stack trace:\n");
            DumpStackTrace(ctx, StackTraceFilter::MirageImage, 64);
            AppendLog("Full stack trace:\n");
            DumpStackTrace(ctx, StackTraceFilter::All, 64);
            DumpRawStack(ctx);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            AppendLog("[CrashHandler] Secondary fault while building extended crash report. code=0x%08X\n", GetExceptionCode());
        }

        AppendLog("========================================\n");

        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline LONG WriteCrashReportRecoverable(const char* reason, DWORD code, uintptr_t faultAddr, const CONTEXT& ctx)
    {
        LONG result = WriteCrashReport(reason, code, faultAddr, ctx);
        InterlockedExchange(&s_inCrashHandler, 0);
        return result;
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

        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        const uintptr_t faultAddr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
        if (IsSilentCrashException(code))
        {
            WriteCrashReportRecoverable("Vectored fatal exit", code, faultAddr, *ep->ContextRecord);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (!ShouldLogVectoredMirageException(ep))
            return EXCEPTION_CONTINUE_SEARCH;

        WriteCrashReportRecoverable("Vectored fatal exception", code, faultAddr, *ep->ContextRecord);
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

    inline void LogStatus()
    {
        EnsureSymbols();
        EnsureMirageSymbols();

        char modulePath[MAX_PATH]{};
        BuildMirageModulePath(modulePath, sizeof(modulePath));

        char pdbPath[MAX_PATH]{};
        if (modulePath[0])
        {
            strcpy_s(pdbPath, modulePath);
            char* dot = std::strrchr(pdbPath, '.');
            if (dot)
                strcpy_s(dot, MAX_PATH - static_cast<size_t>(dot - pdbPath), ".pdb");
        }

        bool pdbPresent = false;
        if (pdbPath[0])
            pdbPresent = GetFileAttributesA(pdbPath) != INVALID_FILE_ATTRIBUTES;

        char loadedPdb[MAX_PATH]{};
        IMAGEHLP_MODULE64 moduleInfo{};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        bool hasModuleInfo = false;
        if (s_process && s_mirageBase)
        {
            hasModuleInfo = SymGetModuleInfo64(s_process, static_cast<DWORD64>(s_mirageBase), &moduleInfo) == TRUE;
            if (hasModuleInfo && moduleInfo.LoadedPdbName[0])
                strcpy_s(loadedPdb, moduleInfo.LoadedPdbName);
        }

        IMAGEHLP_LINE64 probeLine{};
        probeLine.SizeOfStruct = sizeof(probeLine);
        DWORD probeDisp = 0;
        bool pdbProbeOk = false;
        if (s_process)
        {
            pdbProbeOk = SymGetLineFromAddr64(s_process, static_cast<DWORD64>(reinterpret_cast<uintptr_t>(&LogStatus)), &probeDisp, &probeLine) == TRUE &&
                probeLine.FileName != nullptr;
        }

        AppendLog("[CrashHandler] Status: installed=%d symbols=%d mirage_base=0x%08X mirage_size=0x%08X.\n",
            s_installed.load() ? 1 : 0,
            s_symbolsInitialized ? 1 : 0,
            static_cast<unsigned int>(s_mirageBase),
            s_mirageSize);
        AppendLog("[CrashHandler] Mirage module path: %s\n", modulePath[0] ? modulePath : "<empty>");
        AppendLog("[CrashHandler] Mirage pdb present: %s\n", pdbPresent ? "yes" : "no");
        AppendLog("[CrashHandler] Mirage symbols loaded: %s\n", s_mirageSymbolsLoaded ? "yes" : "no");
        AppendLog("[CrashHandler] Loaded pdb: %s\n", hasModuleInfo && loadedPdb[0] ? loadedPdb : "<none>");
        AppendLog("[CrashHandler] PDB probe: %s%s%s\n",
            pdbProbeOk ? "ok " : "failed",
            pdbProbeOk ? probeLine.FileName : "",
            pdbProbeOk ? ":" : "");
        if (pdbProbeOk)
            AppendLog("[CrashHandler] PDB probe line: %lu\n", static_cast<unsigned long>(probeLine.LineNumber));
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
        if (PatchSetUnhandledExceptionFilter())
            AppendLog("[CrashHandler] SetUnhandledExceptionFilter protected.\n");
        else
            AppendLog("[CrashHandler] Failed to protect SetUnhandledExceptionFilter.\n");
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
        UnpatchSetUnhandledExceptionFilter();
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
