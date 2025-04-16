int __cdecl hkLuaLoadBuffer(void* L, char* buff, size_t sz, const char* name)
{
    // Восстанавливаем пролог/удаляем HWBP, если необходимо
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) RestorePrologue((DWORD)callLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::DeleteHWBP((DWORD)callLuaLoadBuffer);

    static size_t injected_count = 0;

    // Вызов оригинальной функции с исходным буфером
    int original_result = callLuaLoadBuffer(L, buff, sz, name);

    if (mirage.Dumper)
    {
        if (name == nullptr)
        {
            std::string scr_name_dmp = xorstr_("chunk.lua");
            DumpIfNotDuplicate(xorstr_("Chunks"), scr_name_dmp.c_str(), buff, sz);
        }
        if (name != nullptr)
        {
            if (findStringIC(name, mirage.dump_resource_name) || mirage.DumpAllScripts)
            {
                DumpScript(xorstr_("DumpedScripts"), name, buff, sz);
                LogInFile(LOG_NAME, xorstr_("[LOG] Dumped script: %s | Size: %d\n"), name, sz);
            }
        }
    }

    // Если имя скрипта задано и есть записи для инъекции
    if (name && !lua_injection_list.empty())
    {
        for (const auto& lvm : lua_injection_list)
        {
            if (w_findStringIC(std::wstring(name, name + strlen(name)), std::wstring(lvm.target_script.begin(), lvm.target_script.end())))
            {
                std::ifstream script_file(lvm.our_script, std::ios::binary | std::ios::ate);
                if (script_file.is_open())
                {
                    std::streamsize custom_size = script_file.tellg();
                    script_file.seekg(0, std::ios::beg);
                    std::string custom_script(custom_size, '\0');

                    if (script_file.read(&custom_script[0], custom_size))
                    {
                        // Если скрипт наш, декодируем (дешифруем) его с помощью XOR алгоритма
                        custom_script = DecryptBuffer(custom_script);
                        if (!IsUtf8(custom_script))
                        {
                            custom_script = cp1251_to_utf8(custom_script.c_str());
                        }

                        // ЗАМЕНА скрипта: используем только кастомный скрипт
                        
                        buff = const_cast<char*>(custom_script.data());
                        sz = custom_script.size();

                        injected_count++;
                        if (injected_count >= lua_injection_list.size()) hwbp_end1 = true;
						lua_register(L, xorstr_("invokeFunction"), invokeFunction);
                        int rslt = callLuaLoadBuffer(L, buff, sz, name);
                        LogInFile(LOG_NAME, xorstr_("[LOG] Replaced script for: %s!\n"), name);
                        if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1) HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
                        if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
                        return rslt;
                    }
                    else
                    {
                        LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %s\n"), lvm.our_script);
                    }
                }
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open our lua script: %s\n"), lvm.our_script);
                }
                break;
            }
        }
    }
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1) HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
    return original_result;
}
// более низкоуровневый хук луашки
int __cdecl lua_load(void* L, lua_Reader reader, void* data, const char* chunkname)
{
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) RestorePrologue((DWORD)call_lua_load, load_prologue, sizeof(load_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK) HWBP::DeleteHWBP((DWORD)call_lua_load);

    static size_t injected_count = 0;

    // Вызов оригинальной функции с исходными данными
    int original_result = call_lua_load(L, reader, data, chunkname);

    if (mirage.Dumper)
    {
        LoadS* c_ptr = reinterpret_cast<LoadS*>(data);
        if (chunkname != nullptr && c_ptr != nullptr && !findStringIC(chunkname, xorstr_("@")))
        {
            std::string scr_name_dmp = xorstr_("chunk.lua");
			if (c_ptr->size == 0) c_ptr->size = strlen(c_ptr->s);
            DumpIfNotDuplicate(xorstr_("Chunks"), scr_name_dmp.c_str(), c_ptr->s, c_ptr->size);
        }
        if (chunkname != nullptr && c_ptr != nullptr)
        {
            if (findStringIC(chunkname, xorstr_("@")))
            {
                if (findStringIC(chunkname, mirage.dump_resource_name) || mirage.DumpAllScripts)
                {
                    if (c_ptr->size == 0) c_ptr->size = strlen(c_ptr->s);
                    DumpScript(xorstr_("DumpedScripts"), chunkname, c_ptr->s, c_ptr->size);
                    LogInFile(LOG_NAME, xorstr_("[LOG] Dumped script: %s | Size: %d\n"), chunkname, c_ptr->size);
                }
            }
        }
    }

    if (chunkname && !lua_injection_list.empty())
    {
        for (const auto& lvm : lua_injection_list)
        {
            if (w_findStringIC(std::wstring(chunkname, chunkname + strlen(chunkname)), std::wstring(lvm.target_script.begin(), lvm.target_script.end())))
            {
                LoadS* s_ptr = reinterpret_cast<LoadS*>(data);
                std::ifstream script_file(lvm.our_script, std::ios::binary | std::ios::ate);
                if (script_file.is_open())
                {
                    std::streamsize custom_size = script_file.tellg();
                    script_file.seekg(0, std::ios::beg);
                    std::string custom_script(custom_size, '\0');

                    if (script_file.read(&custom_script[0], custom_size))
                    {
                        // Если скрипт наш, декодируем (дешифруем) его с помощью XOR алгоритма
                        custom_script = DecryptBuffer(custom_script);
                        if (!IsUtf8(custom_script))
                        {
                            custom_script = cp1251_to_utf8(custom_script.c_str());
                        }
                        lua_register(L, xorstr_("invokeFunction"), invokeFunction);
                        // ЗАМЕНА скрипта: подставляем кастомный скрипт вместо исходного
                        
                        s_ptr->s = (char*)custom_script.c_str();
                        s_ptr->size = custom_script.size();

                        injected_count++;
                        if (injected_count >= lua_injection_list.size()) hwbp_end2 = true;

                        LogInFile(LOG_NAME, xorstr_("[LOG] Replaced script for: %s!\n"), chunkname);
                        int rslt = call_lua_load(L, reader, data, chunkname);
                        if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2) HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
                        if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));
                        return rslt;
                    }
                    else
                    {
                        LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %s\n"), lvm.our_script);
                    }
                }
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open our lua script: %s\n"), lvm.our_script);
                }
                break;
            }
        }
    }
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2) HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP) MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));
    return original_result;
}