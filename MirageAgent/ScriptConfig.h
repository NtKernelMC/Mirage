void ParseLuaConfig()
{
    // Очистка списка конфигураций
    lua_injection_list.clear();

    for (const auto& entry : fs::directory_iterator(lua_scripts_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == xorstr_(L".lvm"))
        {
            std::wifstream file(entry.path());
            if (!file.is_open())
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to open .lvm file!\n"));
                continue;
            }

            LVM current_lvm;

            // Читаем target_script
            if (!std::getline(file, current_lvm.target_script))
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to read target_script!\n"));
                continue;
            }
            // Читаем our_script
            if (!std::getline(file, current_lvm.our_script))
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to read our_script!\n"));
                continue;
            }
            current_lvm.our_script = lua_scripts_dir + xorstr_(L"\\") + current_lvm.our_script;
            lua_injection_list.push_back(current_lvm);
        }
    }
}
// Вспомогательная функция для обрезки пробелов с начала и конца строки
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

void ParseMirageConfig()
{
    std::ifstream file(mirage_config_dir);
    if (!file.is_open())
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] Error: failed to open config file!\n"));
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Удаляем пробелы по краям строки
        line = trim(line);
        if (line.empty() || line[0] == '#') // Пропуск пустых строк или комментариев
            continue;

        // Разбиваем строку по символу '='
        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid config line (no '=')!\n"));
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (key == xorstr_("LUA_INJECTION_TYPE"))
        {
            if (value == xorstr_("METHOD_LUA_L_LOAD"))
                mirage.injection_type = LuaInjectionType::METHOD_LUA_L_LOAD;
            else if (value == xorstr_("METHOD_LUA_L_LOADBUFFER"))
                mirage.injection_type = LuaInjectionType::METHOD_LUA_L_LOADBUFFER;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid LUA_INJECTION_TYPE value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("FORK_VERSION"))
        {
            if (value == xorstr_("FORK_VERSION_1_6"))
            {
                mirage.fork_version = ForkVersion::FORK_VERSION_1_6;
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: MTA:SA 1.6 is not supported yet!\n"));
            }
            else if (value == xorstr_("FORK_VERSION_1_5"))
                mirage.fork_version = ForkVersion::FORK_VERSION_1_5;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid FORK_VERSION value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("FUCK_DBG_HOOKS"))
        {
            // Теперь значение читается как текст вместо числа.
            if (value == xorstr_("ALLOW_DBG_HOOKS"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::ALLOW_DBG_HOOKS;
                DbgHook = true;
            }
            else if (value == xorstr_("FUCK_DBG_HOOKS"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::FUCK_DBG_HOOKS;
                DbgHook = false;
            }
            else if (value == xorstr_("PROTECTED_MODE"))
            {
                mirage.fuck_dbg_hooks = FuckDbgHooksMode::PROTECTED_MODE;
                DbgHook = false;
            }
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid FUCK_DBG_HOOKS value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("HOOKING_METHOD"))
        {
            if (value == xorstr_("HWBP_HOOK"))
                mirage.hwbp_hooking = HookingType::HWBP_HOOK;
            else if (value == xorstr_("INLINE_JUMP"))
                mirage.hwbp_hooking = HookingType::INLINE_JUMP;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid HOOKING_METHOD value!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DUMPER"))
        {
            try {
                int flag = std::stoi(value);
                if (flag == 0 || flag == 1)
                    mirage.Dumper = static_cast<bool>(flag);
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DUMPER value!\n"));
                    continue;
                }
            }
            catch (...)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: exception parsing DUMPER!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DUMP_RESOURCE_NAME"))
        {
            mirage.dump_resource_name = value;
        }
        else if (key == xorstr_("DUMP_ALL_SCRIPTS"))
        {
            try {
                int flag = std::stoi(value);
                if (flag == 0 || flag == 1)
                    mirage.DumpAllScripts = static_cast<bool>(flag);
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DUMP_ALL_SCRIPTS value!\n"));
                    continue;
                }
            }
            catch (...)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: exception parsing DUMP_ALL_SCRIPTS!\n"));
                continue;
            }
        }
        else if (key == xorstr_("DLL_INJECTION_TYPE"))
        {
            if (value == xorstr_("MMAP"))
                mirage.dll_injection_type = DllInjectionType::MMAP;
            else if (value == xorstr_("SET_WINDOWS_HOOK"))
                mirage.dll_injection_type = DllInjectionType::SET_WINDOWS_HOOK;
            else
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid DLL_INJECTION_TYPE value!\n"));
                continue;
            }
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Warning: Unknown key in config: %s\n"), key.c_str());
        }
    }
}