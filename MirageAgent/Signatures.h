typedef bool(__thiscall* ptrStartGame)(void* ECX, const char* szNick, const char* szPassword, int Type, const char* szSecret);
ptrStartGame callStartGame = nullptr;
bool __fastcall StartGame(void* ECX, void* EDX, const char* szNick, const char* szPassword, int Type, const char* szSecret)
{
    RestorePrologue((DWORD)callStartGame, gamestart_prologue, sizeof(gamestart_prologue)); // восстанавливаем пролог функции
    bool rslt = callStartGame(ECX, szNick, szPassword, Type, szSecret);
    MakeJump((DWORD)callStartGame, (DWORD)&StartGame, gamestart_prologue, sizeof(gamestart_prologue));
    return rslt;
}
typedef bool(__thiscall* ptrProcessMessage)(void* ECX, HWND__* hwnd, unsigned int uMsg, unsigned int wParam, int lParam);
ptrProcessMessage callProcessMessage = nullptr;
bool __fastcall ProcessMessage(void* ECX, void* EDX, HWND__* hwnd, unsigned int uMsg, unsigned int wParam, int lParam)
{
	CLocalGUI = ECX;
    return callProcessMessage(ECX, hwnd, uMsg, wParam, lParam);
}
BOOL __stdcall hookGetThreadContext(HANDLE hThread, LPCONTEXT lpContext)
{
	RestorePrologue((DWORD)callGetThreadContext, thread_prologue, sizeof(thread_prologue)); // восстанавливаем пролог функции
    BOOL rslt = callGetThreadContext(hThread, lpContext);
    if ((lpContext->Dr0 != NULL || lpContext->Dr1 != NULL || lpContext->Dr2 != NULL || lpContext->Dr3 != NULL) || lpContext->Dr7 != NULL)
    {
        lpContext->Dr0 = NULL;
        lpContext->Dr1 = NULL;
        lpContext->Dr2 = NULL;
        lpContext->Dr3 = NULL;
        lpContext->Dr7 = NULL;
    }
    MakeJump((DWORD)callGetThreadContext, (DWORD)hookGetThreadContext, thread_prologue, sizeof(thread_prologue));
    return rslt;
}
int __cdecl AddDebugHook(void* L)
{
    HWBP::DeleteHWBP((DWORD)callAddDebugHook);
    if (!DbgHook)
    {
        if (mirage.fuck_dbg_hooks == FuckDbgHooksMode::FUCK_DBG_HOOKS)
        {
            HWBP::InstallHWBP((DWORD)callAddDebugHook, (DWORD)&AddDebugHook);
            call_pushboolean(L, 0x1);
            return 0x1;
        }
        if (mirage.fuck_dbg_hooks == FuckDbgHooksMode::PROTECTED_MODE)
        {
            unsigned int strLen = 500;
            int n = call_objlen(L, 3);
            for (int i = 1; i <= n; ++i)
            {
                call_rawgeti(L, 3, i);
                const char* s = call_tostring(L, -1, &strLen);
                if (strcmp(s, xorstr_("loadstring")) == 0)
                {
                    call_pushstring(L, xorstr_("fuck_toffy"));
                    call_rawseti(L, 3, i);
                }
                else if (strcmp(s, xorstr_("pcall")) == 0)
                {
                    call_pushstring(L, xorstr_("toffy_loh"));
                    call_rawseti(L, 3, i);
                }
                else if (strcmp(s, xorstr_("addDebugHook")) == 0)
                {
                    call_pushstring(L, xorstr_("fuckThisGay"));
                    call_rawseti(L, 3, i);
                }
                else if (strcmp(s, xorstr_("triggerLatentServerEvent")) == 0)
                {
                    call_pushstring(L, xorstr_("hookMeBitch"));
                    call_rawseti(L, 3, i);
                }
                call_settop(L, -2); // Удаляем элемент с вершины стека
            }
        }
    }
    int rslt = callAddDebugHook(L);
    HWBP::InstallHWBP((DWORD)callAddDebugHook, (DWORD)&AddDebugHook);
    call_pushboolean(L, rslt);
    return rslt;
}
typedef void(__thiscall* ptrSendBulletSyncFire)(void* ECX, eWeaponType weaponType, CVector& vecStart, CVector& vecEnd, float fDamage, BYTE ucHitZone, void* pRemoteDamagedPlayer);
ptrSendBulletSyncFire callSendBulletSyncFire = nullptr;
void __fastcall SendBulletSyncFire(void* ECX, void* EDX, eWeaponType weaponType, CVector& vecStart, CVector& vecEnd, float fDamage, BYTE ucHitZone, void* pRemoteDamagedPlayer)
{
    callSendBulletSyncFire(ECX, weaponType, vecStart, vecEnd, fDamage, ucHitZone, pRemoteDamagedPlayer);
}
std::string utf8_to_cp1251_safe(const char* data, size_t len)
{
    std::string out;
    size_t      offset = 0;

    while (offset < len)
    {
        // ищем следующий 0x00
        size_t next0 = offset;
        while (next0 < len && data[next0] != '\0')
            ++next0;

        const char* chunk = data + offset;
        size_t      clen = next0 - offset;           // длина куска без 0

        if (clen)
        {
            // пробуем обычную конвертацию
            int wlen = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS,
                chunk, static_cast<int>(clen),
                nullptr, 0);

            if (wlen > 0)
            {
                std::vector<wchar_t> wbuf(static_cast<size_t>(wlen));
                MultiByteToWideChar(CP_UTF8, 0,
                    chunk, static_cast<int>(clen),
                    wbuf.data(), wlen);

                int cplen = WideCharToMultiByte(
                    1251, 0,
                    wbuf.data(), wlen,
                    nullptr, 0, nullptr, nullptr);

                if (cplen > 0)
                {
                    size_t old = out.size();
                    out.resize(old + static_cast<size_t>(cplen));
                    WideCharToMultiByte(
                        1251, 0,
                        wbuf.data(), wlen,
                        out.data() + old, cplen,
                        nullptr, nullptr);
                }
                else
                {
                    // CP-1251 не смог — копируем «как есть»
                    out.append(chunk, clen);
                }
            }
            else
            {
                // битый UTF-8 — копируем «как есть»
                out.append(chunk, clen);
            }
        }

        // если встретили 0x00 в середине буфера — сохраняем его тоже
        if (next0 < len)
            out.push_back('\0');

        offset = next0 + 1;   // переходим за найденный 0
    }

    return out;
}
static std::string GenerateDumpPath()
{
    // Время в мс от эпохи Unix
    const uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();

    // Потокобезопасный RNG
    thread_local std::mt19937 rng{
        static_cast<std::mt19937::result_type>(
            std::chrono::steady_clock::now().time_since_epoch().count()) };

    std::uniform_int_distribution<int> dist(10000, 99999);

    std::ostringstream oss;
    oss << "mem_" << now_ms << '_' << dist(rng) << ".dmp";

    return (std::filesystem::path(kDumpDir) / oss.str()).string();
}

typedef void(__thiscall* ptrDecodeAndBuffer)(void* ECX, char* pBuffer, unsigned int bytesWritten);
ptrDecodeAndBuffer callDecodeAndBuffer = nullptr;
// Хук-обёртка оригинальной функции
void __fastcall DecodeAndBuffer(void* ECX, void* EDX, char* pBuffer, unsigned int bytesWritten)
{
    if (pBuffer && bytesWritten > 10000 && cursed_voice)
    {
        std::string filePath = GenerateDumpPath();
		if (filePath.empty())
		{
			LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to generate dump file path.\n"));
			callDecodeAndBuffer(ECX, pBuffer, bytesWritten);
			return;
		}
		LogInFile(LOG_NAME, xorstr_("[PLUGIN] Dumping memory to file: %s\n"), filePath.c_str());
        std::ofstream dump(filePath, std::ios::binary);
        if (!dump.is_open())
        {
			LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to open dump file: %s\n"), filePath.c_str());
            callDecodeAndBuffer(ECX, pBuffer, bytesWritten);
            return;
        }
        dump.write(pBuffer, bytesWritten);
        dump.close();
		LogInFile(LOG_NAME, xorstr_("[PLUGIN] Memory dump saved to: %s\n"), filePath.c_str());
    }
    callDecodeAndBuffer(ECX, pBuffer, bytesWritten);
}
static std::vector<std::string> vecHooks =
{
    xorstr_("addDebugHook"),
    xorstr_("removeDbgHook"),
    xorstr_("triggerServerEvent"),
    xorstr_("triggerLatentServerEvent"),
    xorstr_("addEventHandler"),
    xorstr_("loadstring"),
    xorstr_("load"),
    xorstr_("setPlayerNametagText"),
    xorstr_("setElementData"),
    xorstr_("blowVehicle")
};
typedef bool(__thiscall* ptrIsNameAllowed)(void* ECX, const char* szName, void* eventHookList, bool bNameMustBeExplicitlyAllowed);
ptrIsNameAllowed callIsNameAllowed = nullptr;
bool __fastcall IsNameAllowed(void* ECX, void* EDX, const char* szName, void* eventHookList, bool bNameMustBeExplicitlyAllowed)
{
    for (const auto& hook : vecHooks)
    {
        if (szName != nullptr && !strcmp(szName, hook.c_str()) && HideCall)
        {
            //LogInFile(LOG_NAME, xorstr_("[LOG] Hook of %s is skipped!\n"), szName);
            return false; // Блокируем хук
        }
    }
    return callIsNameAllowed(ECX, szName, eventHookList, bNameMustBeExplicitlyAllowed);
}
void SignatureScanner()
{
    SigScan scan;
	if (mirage.fork_version == ForkVersion::FORK_VERSION_1_5) LegacyBypass::EvadeAnticheat();
    if (mirage.fork_version == ForkVersion::FORK_VERSION_1_6)
    {
        ModernBypass::EvadeAnticheat(); DWORD oldProtect = 0x0;
        DWORD patch_addr = (DWORD)scan.FindPatternIDA(xorstr_("client.dll"), xorstr_("E8 ? ? ? ? 83 C4 ? 46 3B F7 0F 8C"));
        if (patch_addr != NULL)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AC_CheckDlls!\n"));
            BYTE nop[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
            VirtualProtect((void*)patch_addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy((void*)patch_addr, nop, 5);
            VirtualProtect((void*)patch_addr, 5, oldProtect, &oldProtect);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for AC_CheckDlls.\n"));
        patch_addr = (DWORD)scan.FindPatternIDA(xorstr_("client.dll"), xorstr_("E8 ? ? ? ? 33 FF 39 BD"));
        if (patch_addr != NULL)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AC_AntiMirage!\n"));
            BYTE nop[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
            VirtualProtect((void*)patch_addr, sizeof(nop), PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy((void*)patch_addr, nop, sizeof(nop));
            VirtualProtect((void*)patch_addr, sizeof(nop), oldProtect, &oldProtect);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for AC_AntiMirage.\n"));
        patch_addr = (DWORD)scan.FindPatternIDA(xorstr_("client.dll"), xorstr_("E8 ? ? ? ? 83 C4 ? 47 3B BD"));
        if (patch_addr != NULL)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AC_ScanProcesses!\n"));
            BYTE nop[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
            VirtualProtect((void*)patch_addr, sizeof(nop), PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy((void*)patch_addr, nop, sizeof(nop));
            VirtualProtect((void*)patch_addr, sizeof(nop), oldProtect, &oldProtect);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for AC_ScanProcesses.\n"));
    }
	
    if (callProcessMessage == nullptr)
    {
        callProcessMessage = (ptrProcessMessage)scan.FindPattern(xorstr_("core.dll"),
            xorstr_("\x55\x8B\xEC\x81\xEC\x14\x02"), xorstr_("xxxxxxx"));
        if (callProcessMessage != nullptr)
        {
            //LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ProcessMessage!\n"));
            //MH_RemoveHook(callProcessMessage);
            //MH_CreateHook(callProcessMessage, &ProcessMessage, reinterpret_cast<LPVOID*>(&callProcessMessage));
            //MH_EnableHook(MH_ALL_HOOKS);
        }
        //else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for ProcessMessage.\n"));
    }
    PVOID getPedVoice = (PVOID)scan.FindPatternIDA(xorstr_("client.dll"),
        xorstr_("55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 ? 56 50 8D 45 ? 64 A3 ? ? ? ? 8B 75 ? 8D 8D ? ? ? ? 56 C7 85 ? ? ? ? ? ? ? ? E8 ? ? ? ? 6A ? 6A ? 8D 85 ? ? ? ? C7 45 ? ? ? ? ? 50 6A ? 8D 8D ? ? ? ? E8 ? ? ? ? 83 BD ? ? ? ? ? 74 ? 83 BD ? ? ? ? ? 74 ? C7 05 ? ? ? ? ? ? ? ? 8A 85 ? ? ? ? 84 C0 0F 85 ? ? ? ? 83 7D ? ? 74 ? 83 7D ? ? 8D 45 ? 0F 47 45 ? 50 FF B5 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 ? C7 45 ? ? ? ? ? 83 7D ? ? 8D 45 ? 0F 47 45 ? ? ? ? 8A 85 ? ? ? ? 84 C0 0F 85 ? ? ? ? 8B 8D ? ? ? ? E8 ? ? ? ? 84 C0"));
    if (getPedVoice != nullptr)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to getPedVoice!\n"));
        MH_RemoveHook(getPedVoice);
        MH_CreateHook(getPedVoice, invokeFunction, reinterpret_cast<LPVOID*>(&getPedVoice));
        MH_EnableHook(MH_ALL_HOOKS);
    }
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for getPedVoice.\n"));
    callDecodeAndBuffer = (ptrDecodeAndBuffer)scan.FindPatternIDA(xorstr_("client.dll"),
        xorstr_("55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 81 EC 34 08"));
    if (callDecodeAndBuffer != nullptr)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to DecodeAndBuffer!\n"));
        MH_RemoveHook(callDecodeAndBuffer);
        MH_CreateHook(callDecodeAndBuffer, DecodeAndBuffer, reinterpret_cast<LPVOID*>(&callDecodeAndBuffer));
        MH_EnableHook(MH_ALL_HOOKS);
    }
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for DecodeAndBuffer.\n"));
    /*callSendBulletSyncFire = (ptrSendBulletSyncFire)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x56\x8B\xF1\x8B\x00\x00\x00\x00\x00\x57"),
        xorstr_("xxxxxxx?????x")); // SendBulletSyncFire
    if (callSendBulletSyncFire != nullptr)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to SendBulletSyncFire!\n"));
        MH_RemoveHook(callSendBulletSyncFire);
        MH_CreateHook(callSendBulletSyncFire, &SendBulletSyncFire, reinterpret_cast<LPVOID*>(&callSendBulletSyncFire));
        MH_EnableHook(MH_ALL_HOOKS);
    }
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for SendBulletSyncFire.\n"));*/
    callAddDebugHook = (ptrAddDebugHook)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\xF4\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\x75"),
        xorstr_("xxxxxx????xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxx"));
    if (callAddDebugHook != NULL)
    {
       if (!DbgHook && mirage.hwbp_hooking != HookingType::IAT)
       {
           HWBP::InstallHWBP((DWORD)callAddDebugHook, (DWORD)&AddDebugHook);
           LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AddDebugHook!\n"));
       }
    }
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to find address from signature to AddDebugHook!\n"));
    callRemoveDebugHook = (ptrRemoveDebugHook)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\xE8\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\x75\x08\x8D\x8D\x0C"),
        xorstr_("xxxxxx????xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxx"));
    if (callRemoveDebugHook != NULL)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to RemoveDebugHook!\n"));
    }
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to find address from signature to RemoveDebugHook!\n"));
    sendMTAChat = (f_SendChat)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x9C\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x80"),
        xorstr_("xxxxxx????xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxx"));
    if (sendMTAChat) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ExecCommand!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to find address from signature to ExecCommand!\n"));
    callStartGame = (ptrStartGame)scan.FindCallPattern(xorstr_("client.dll"),
        xorstr_("E8 ? ? ? ? EB ? 8B 0D ? ? ? ? ? ? 8B 40 ? FF D0 68"));
	if (mirage.fork_version == ForkVersion::FORK_VERSION_1_5)
	{
		callStartGame = (ptrStartGame)scan.FindCallPattern(xorstr_("client.dll"),
			xorstr_("E8 ? ? ? ? 8D 4D A8 E8 ? ? ? ? EB 29"));
	}
    if (callStartGame != nullptr)
    {
		//MakeJump((DWORD)callStartGame, (DWORD)&StartGame, gamestart_prologue, sizeof(gamestart_prologue));
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to StartGame!\n"));
    }
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for StartGame.\n"));
    if (callIsChatBoxInputEnabled == nullptr)
    {
        callIsChatBoxInputEnabled = (ptrIsChatBoxInputEnabled)scan.FindPattern(xorstr_("core.dll"),
            xorstr_("\x8B\x41\x08\x85\xC0\x74"), xorstr_("xxxxxx"));
        if (callIsChatBoxInputEnabled != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to IsChatBoxInputEnabled!\n"));
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for IsChatBoxInputEnabled.\n"));
    }
    callWriteCameraOrientation = (ptrWriteCameraOrientation)scan.FindPatternIDA(xorstr_("client.dll"),
        xorstr_("53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 81 EC C0"));
    if (callWriteCameraOrientation != nullptr)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to WriteCameraOrientation!\n"));
    }
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for WriteCameraOrientation.\n"));
    call_pushcclosure = (lua_pushcclosure)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_pushcclosure"));
    if (call_pushcclosure != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushcclosure!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushcclosure.\n"));
    call_setfield = (lua_setfield)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_setfield"));
    if (call_setfield != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_setfield!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_setfield.\n"));
    call_pushboolean = (lua_pushboolean)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_pushboolean"));
    if (call_pushboolean != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushboolean!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushboolean.\n"));
    call_toboolean = (lua_toboolean)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_toboolean"));
    if (call_toboolean != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_toboolean!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for toboolean.\n"));
    call_pushnumber = (lua_pushnumber)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_pushnumber"));
    if (call_pushnumber != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushnumber!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushnumber.\n"));
    call_tostring = (lua_tostring)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_tolstring"));
    if (call_tostring != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_tostring!\n"));
    call_pushstring = (lua_pushstring)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_pushstring"));
    if (call_pushstring != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushstring!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushstring.\n"));
    call_lua_remove = (ptr_lua_remove)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_remove"));
    if (call_lua_remove != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_remove!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_remove.\n"));
	call_objlen = (lua_objlen)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_objlen"));
	if (call_objlen != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_objlen!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_objlen.\n"));
	call_rawgeti = (lua_rawgeti)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_rawgeti"));
	if (call_rawgeti != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_rawgeti!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_rawgeti.\n"));
	call_rawseti = (lua_rawseti)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_rawseti"));
	if (call_rawseti != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_rawseti!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_rawseti.\n"));
	call_settop = (lua_settop)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_settop"));
	if (call_settop != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_settop!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_settop.\n"));
    if (mirage.injection_type == LuaInjectionType::METHOD_LUA_L_LOADBUFFER)
    {
        callLuaLoadBuffer = (t_LuaLoadBuffer)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("luaL_loadbuffer"));
        if (callLuaLoadBuffer != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to Lua Engine!\n"));
            if (mirage.hwbp_hooking == HookingType::IAT) hookIATwithName(xorstr_("client.dll"), xorstr_("luaL_loadbuffer"), (DWORD)&hkLuaLoadBuffer, (DWORD*)&callLuaLoadBuffer);
            if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
            if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for Lua Engine. 0x%X\n"));
    }
    if (mirage.injection_type == LuaInjectionType::METHOD_LUA_L_LOAD)
    {
        call_lua_load = (ptr_lua_load)GetProcedure(xorstr_("lua5.1c.dll"), xorstr_("lua_load"));
        if (call_lua_load != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to Lua Engine!\n"));
            if (mirage.hwbp_hooking == HookingType::IAT) hookIATwithName(xorstr_("client.dll"), xorstr_("luaL_load"), (DWORD)&lua_load, (DWORD*)&call_lua_load);
            if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));
            if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
        }
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for Lua Engine.\n"));
    }
    if (mirage.fuck_dbg_hooks == FuckDbgHooksMode::STEALTH_MODE)
    {
        callIsNameAllowed = (ptrIsNameAllowed)scan.FindCallPattern(xorstr_("client.dll"),
        xorstr_("E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? C7 45 ? ? ? ? ? C7 45 ? ? ? ? ? C7 45 ? ? ? ? ? 6A"));
        if (callIsNameAllowed != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to IsNameAllowed!\n"));
            MH_RemoveHook(callIsNameAllowed);
            MH_CreateHook(callIsNameAllowed, IsNameAllowed, reinterpret_cast<LPVOID*>(&callIsNameAllowed));
            MH_EnableHook(MH_ALL_HOOKS);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for IsNameAllowed!\n"));
    }
}