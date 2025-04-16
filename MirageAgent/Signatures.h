typedef bool(__thiscall* ptrStartGame)(void* ECX, const char* szNick, const char* szPassword, int Type, const char* szSecret);
ptrStartGame callStartGame = nullptr;
bool __fastcall StartGame(void* ECX, void* EDX, const char* szNick, const char* szPassword, int Type, const char* szSecret)
{
    RestorePrologue((DWORD)callStartGame, gamestart_prologue, sizeof(gamestart_prologue)); // восстанавливаем пролог функции
    OneClientLoad = false; // разрешаем луа инжект только при коннекте к серверу (защита от перезагрузки client.dll с мта)
    bool rslt = callStartGame(ECX, szNick, szPassword, Type, szSecret);
    MakeJump((DWORD)callStartGame, (DWORD)&StartGame, gamestart_prologue, sizeof(gamestart_prologue));
    return rslt;
}
typedef bool(__thiscall* ptrProcessMessage)(void* ECX, HWND__* hwnd, unsigned int uMsg, unsigned int wParam, int lParam);
ptrProcessMessage callProcessMessage = nullptr;
bool __fastcall ProcessMessage(void* ECX, void* EDX, HWND__* hwnd, unsigned int uMsg, unsigned int wParam, int lParam)
{
	RestorePrologue((DWORD)callProcessMessage, pmsg_prologue, sizeof(pmsg_prologue)); // восстанавливаем пролог функции
    CLocalGUI = ECX;
    LogInFile(LOG_NAME, xorstr_("[LOG] Снят указатель CLocalGUI из хука ProcessMessage, хук удален!\n"));
    return callProcessMessage(ECX, hwnd, uMsg, wParam, lParam);
}
typedef BOOL(__stdcall* ptrGetThreadContext)(HANDLE hThread, LPCONTEXT lpContext);
ptrGetThreadContext callGetThreadContext = nullptr;
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
                call_settop(L, -2); // Удаляем элемент с вершины стека
            }
        }
    }
    int rslt = callAddDebugHook(L);
    HWBP::InstallHWBP((DWORD)callAddDebugHook, (DWORD)&AddDebugHook);
    call_pushboolean(L, rslt);
    return rslt;
}
void SignatureScanner()
{
    SigScan scan;
	if (mirage.fork_version == ForkVersion::FORK_VERSION_1_5) LegacyBypass::EvadeAnticheat();
    if (callProcessMessage == nullptr)
    {
        callProcessMessage = (ptrProcessMessage)scan.FindPattern(xorstr_("core.dll"),
            xorstr_("\x55\x8B\xEC\x81\xEC\x14\x02"), xorstr_("xxxxxxx"));
        if (callProcessMessage != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ProcessMessage!\n"));
            MakeJump((DWORD)callProcessMessage, (DWORD)&ProcessMessage, pmsg_prologue, sizeof(pmsg_prologue));
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for ProcessMessage.\n"));
    }
    callAddDebugHook = (ptrAddDebugHook)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\xF4\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\x75"),
        xorstr_("xxxxxx????xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxx"));
    if (callAddDebugHook != NULL)
    {
       LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AddDebugHook!\n"));
	   if (!DbgHook) HWBP::InstallHWBP((DWORD)callAddDebugHook, (DWORD)&AddDebugHook);
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
    callStartGame = (ptrStartGame)scan.FindPattern(xorstr_("client.dll"),
        xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\xA0\x01"),
        xorstr_("xxxxxx????xxxxxxxxxxx"));
    if (callStartGame != nullptr)
    {
		MakeJump((DWORD)callStartGame, (DWORD)&StartGame, gamestart_prologue, sizeof(gamestart_prologue));
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
    call_pushcclosure = (lua_pushcclosure)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_pushcclosure"));
    if (call_pushcclosure != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushcclosure!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushcclosure.\n"));
    call_setfield = (lua_setfield)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_setfield"));
    if (call_setfield != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_setfield!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_setfield.\n"));
    call_pushboolean = (lua_pushboolean)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_pushboolean"));
    if (call_pushboolean != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushboolean!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushboolean.\n"));
    call_toboolean = (lua_toboolean)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_toboolean"));
    if (call_toboolean != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_toboolean!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for toboolean.\n"));
    call_pushnumber = (lua_pushnumber)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_pushnumber"));
    if (call_pushnumber != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushnumber!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushnumber.\n"));
    call_tostring = (lua_tostring)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_tolstring"));
    if (call_tostring != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_tostring!\n"));
    call_pushstring = (lua_pushstring)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_pushstring"));
    if (call_pushstring != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_pushstring!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_pushstring.\n"));
    call_lua_remove = (ptr_lua_remove)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_remove"));
    if (call_lua_remove != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_remove!\n"));
    else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_remove.\n"));
	call_objlen = (lua_objlen)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_objlen"));
	if (call_objlen != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_objlen!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_objlen.\n"));
	call_rawgeti = (lua_rawgeti)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_rawgeti"));
	if (call_rawgeti != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_rawgeti!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_rawgeti.\n"));
	call_rawseti = (lua_rawseti)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_rawseti"));
	if (call_rawseti != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_rawseti!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_rawseti.\n"));
	call_settop = (lua_settop)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_settop"));
	if (call_settop != nullptr) LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to lua_settop!\n"));
	else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for lua_settop.\n"));
    if (mirage.injection_type == LuaInjectionType::METHOD_LUA_L_LOADBUFFER)
    {
        callLuaLoadBuffer = (t_LuaLoadBuffer)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("luaL_loadbuffer"));
        if (callLuaLoadBuffer != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to Lua Engine!\n"));
            if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
            if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
        }
        else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for Lua Engine. 0x%X\n"));
    }
    if (mirage.injection_type == LuaInjectionType::METHOD_LUA_L_LOAD)
    {
        call_lua_load = (ptr_lua_load)GetProcAddress(GetModuleHandleA(xorstr_("lua5.1c.dll")), xorstr_("lua_load"));
        if (call_lua_load != nullptr)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to Lua Engine!\n"));
            if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));
            if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
        }
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for Lua Engine.\n"));
    }
}