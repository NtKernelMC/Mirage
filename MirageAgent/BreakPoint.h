#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <map>
#include <cstdio>

class VEH_HWBP
{
    enum : DWORD_PTR { HOOK_DISABLED = 0, HOOK_RAK_SPLIT = 1 };

    static inline volatile LONG g_deleteScheduled = 0;
    static inline DWORD_PTR g_deleteTarget = 0;

    static DWORD WINAPI DeleteWorker(LPVOID)
    {
        DWORD_PTR t = g_deleteTarget;
        DeleteHWBP(t);                 // твоя функция, чистит DRx по всем потокам
        g_deleteTarget = 0;
        InterlockedExchange(&g_deleteScheduled, 0);
        return 0;
    }

    static inline std::map<DWORD_PTR, DWORD_PTR> hooks; // target -> kind
    static inline PVOID vehHandle = nullptr;

    static __forceinline DWORD_PTR GetIP(PEXCEPTION_POINTERS ei)
    {
#ifdef _WIN64
        return (DWORD_PTR)ei->ContextRecord->Rip;
#else
        return (DWORD_PTR)ei->ContextRecord->Eip;
#endif
    }

    static __forceinline void SetRF(PEXCEPTION_POINTERS ei)
    {
        ei->ContextRecord->EFlags |= 0x10000; // RF
        ei->ContextRecord->Dr6 = 0;
    }
    static bool WriteU32Fast(uintptr_t addr, uint32_t v)
    {
        __try { *(volatile uint32_t*)addr = v; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    static __forceinline bool ReadPtrSafe(DWORD_PTR addr, DWORD_PTR& out)
    {
        __try { out = *(DWORD_PTR*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
    }

    static __forceinline bool ReadU32Safe(DWORD_PTR addr, uint32_t& out)
    {
        __try { out = *(uint32_t*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
    }

    static __forceinline bool ReadU16Safe(DWORD_PTR addr, uint16_t& out)
    {
        __try { out = *(uint16_t*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
    }
    static void __stdcall hwLogInFile(std::string log_name, const char* log, ...)
    {
        char new_dir[600];
        memset(new_dir, 0, sizeof(new_dir));
        std::wstring tmp_dir = xorstr_(L"C:\\Users\\RAID\\source\\repos\\Mirage\\ProjectMirage\\Release");
        sprintf(new_dir, xorstr_("%ls\\%s"), tmp_dir.c_str(), log_name.c_str());
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
    }
    static void LogRakSplitAtPoint(PEXCEPTION_POINTERS ei)
    {
#ifndef _WIN64
        auto* ctx = ei->ContextRecord;
        DWORD_PTR ebp = (DWORD_PTR)ctx->Ebp;
        DWORD_PTR thisPtr = (DWORD_PTR)ctx->Ecx;     // RakPeer*
        DWORD_PTR pkt = 0;

        if (!ReadPtrSafe(ebp + 8, pkt) || !pkt)
            return;
        uint32_t splitCount = 0, splitIndex = 0;
        uint16_t splitId = 0, ctrNow = 0;

        // RakNet_InternalPacket offsets (как в твоём IDA разборе)
        WriteU32Fast(pkt + 0x28, 8); // faking 0xFFFFFFFF
        ReadU32Safe(pkt + 0x28, splitCount);
        ReadU32Safe(pkt + 0x24, splitIndex);
        //ReadU16Safe(pkt + 0x20, splitId);

        // RakPeer::splitPacketIdCounter @ +0x90E
        //ReadU16Safe(thisPtr + 0x90E, ctrNow);
        const int isSplit = (splitCount > 1);

        hwLogInFile("RakBit.log", "[RakSplit] ip=%08X this=%08X pkt=%08X isSplit=%d count=%u idx=%u id=%u ctrNow=%u\n",
            (unsigned)GetIP(ei), (unsigned)thisPtr, (unsigned)pkt,
            isSplit, splitCount, splitIndex, splitId, ctrNow);
        
#endif
    }

protected:
    static LONG __stdcall hwbpHandler(PEXCEPTION_POINTERS ei)
    {
        if (ei->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP &&
            ei->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD_PTR ip = GetIP(ei);

        auto it = hooks.find(ip);
        if (it == hooks.end() || it->second == HOOK_DISABLED)
            return EXCEPTION_CONTINUE_SEARCH;

        if (it->second == HOOK_RAK_SPLIT)
        {
            LogRakSplitAtPoint(ei); // твоя функция чтения/printf

            // 1) локально снять BP у текущего потока (из Context)
            DWORD_PTR* dr = (DWORD_PTR*)&ei->ContextRecord->Dr0;
            for (int i = 0; i < 4; ++i)
            {
                if (dr[i] == ip)
                {
                    dr[i] = 0;
                    ei->ContextRecord->Dr7 &= ~(1ull << (i * 2));        // disable local
                    ei->ContextRecord->Dr7 &= ~(0xFull << (16 + i * 4)); // clear RW/LEN
                }
            }
            ei->ContextRecord->Dr6 = 0;

            // 2) пометить хук выключенным (без erase в хендлере)
            it->second = HOOK_DISABLED;

            // 3) отложенно снять по всем потокам
            g_deleteTarget = ip;
            if (InterlockedCompareExchange(&g_deleteScheduled, 1, 0) == 0)
                CreateThread(nullptr, 0, DeleteWorker, nullptr, 0, nullptr);
        }

        SetRF(ei); // RF обязательно, чтобы не зациклиться на той же инструкции
        return EXCEPTION_CONTINUE_EXECUTION;
    }
public:
    static int GetFreeIndex(size_t dr7)
    {
        if (!(dr7 & 1)) return 0;
        else if (!(dr7 & 4)) return 1;
        else if (!(dr7 & 16)) return 2;
        else if (!(dr7 & 64)) return 3;
        return -1;
    }

private:
    struct PRM_THREAD { DWORD_PTR target; DWORD_PTR kind; };

    static bool SetThreadExecBP(HANDLE hThread, DWORD_PTR target)
    {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(hThread, &ctx)) return false;

        int index = GetFreeIndex((size_t)ctx.Dr7);
        if (index < 0) return false;

        // local enable for DRx
        ctx.Dr7 |= (1ull << (index * 2));
        // clear RW/LEN for this bp (exec)
        ctx.Dr7 &= ~(0xFull << (16 + index * 4));

        // set address
        DWORD_PTR* dr = (DWORD_PTR*)&ctx.Dr0;
        dr[index] = target;

        return !!SetThreadContext(hThread, &ctx);
    }

    static bool installHWBP(LPVOID p)
    {
        auto prm = *(PRM_THREAD*)p;
        delete (PRM_THREAD*)p;

        if (!vehHandle)
            vehHandle = AddVectoredExceptionHandler(1, hwbpHandler);

        hooks.insert({ prm.target, prm.kind });

        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        const DWORD pid = GetCurrentProcessId();

        if (Thread32First(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID != pid) continue;
                if (te.th32ThreadID == GetCurrentThreadId()) continue; // не трогаем поток-инсталлер

                HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                if (!th) continue;

                SuspendThread(th);
                SetThreadExecBP(th, prm.target);
                ResumeThread(th);
                CloseHandle(th);

            } while (Thread32Next(snap, &te));
        }

        CloseHandle(snap);
        return true;
    }

public:
    // Хук-логгер для split: target = netcBase + (0x1020C213-0x10000000)
    static bool InstallRakSplitLogger(DWORD_PTR target)
    {
        if (!target) return false;
        if (hooks.find(target) != hooks.end()) return false;
        if (hooks.size() == 4) return false;

        auto* prm = new PRM_THREAD{ target, HOOK_RAK_SPLIT };
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)installHWBP, prm, 0, nullptr);
        if (!hThread) { delete prm; return false; }
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        return true;
    }

    static bool DeleteHWBP(DWORD_PTR target)
    {
        auto it = hooks.find(target);
        if (it == hooks.end()) return false;
        hooks.erase(it);

        // снять DRx только там где совпадает target
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        const DWORD pid = GetCurrentProcessId();

        if (Thread32First(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID != pid) continue;
                if (te.th32ThreadID == GetCurrentThreadId()) continue;

                HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                if (!th) continue;

                SuspendThread(th);

                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(th, &ctx))
                {
                    DWORD_PTR* dr = (DWORD_PTR*)&ctx.Dr0;
                    for (int i = 0; i < 4; ++i)
                    {
                        if (dr[i] == target)
                        {
                            dr[i] = 0;
                            ctx.Dr7 &= ~(1ull << (i * 2));                 // disable local
                            ctx.Dr7 &= ~(0xFull << (16 + i * 4));          // clear RW/LEN
                        }
                    }
                    ctx.Dr6 = 0;
                    SetThreadContext(th, &ctx);
                }

                ResumeThread(th);
                CloseHandle(th);

            } while (Thread32Next(snap, &te));
        }

        CloseHandle(snap);

        if (hooks.empty() && vehHandle)
        {
            RemoveVectoredExceptionHandler(vehHandle);
            vehHandle = nullptr;
        }
        return true;
    }
};
