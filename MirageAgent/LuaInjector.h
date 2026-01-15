#include "AesCryptor.h"
// Глобальные переменные для хранения загруженных скриптов
// Важно: храним скрипты глобально, чтобы они не удалялись после выхода из функции
static std::unordered_map<std::string, std::string> loaded_custom_scripts;
static std::mutex scripts_mutex; // Для потокобезопасности
std::vector<std::string> mirage_resource_list;
std::mutex mirage_resources_mutex;

static std::string ExtractResourceName(const std::string& script_name)
{
    if (script_name.empty())
        return {};

    size_t start = 0;
    if (script_name[0] == '@')
        start = 1;

    size_t sep = script_name.find('/', start);
    if (sep == std::string::npos)
        sep = script_name.find('\\', start);

    if (sep == std::string::npos)
        return script_name.substr(start);

    return script_name.substr(start, sep - start);
}

static void AddMirageResource(const std::string& script_name)
{
    std::string res = ExtractResourceName(script_name);
    if (res.empty())
        return;

    std::lock_guard<std::mutex> lock(mirage_resources_mutex);
    if (std::find(mirage_resource_list.begin(), mirage_resource_list.end(), res) == mirage_resource_list.end())
        mirage_resource_list.push_back(res);
}
static void ReplaceMirageFunc(std::string& script)
{
    const std::string from = xorstr_("mirageFunc");
    const std::string to = xorstr_("getPedVoice");
    size_t pos = 0;
    while ((pos = script.find(from, pos)) != std::string::npos)
    {
        script.replace(pos, from.size(), to);
        pos += to.size();
    }
}

int __cdecl hkLuaLoadBuffer(void* L, char* buff, size_t sz, const char* name)
{
    // Восстанавливаем пролог/удаляем HWBP, если необходимо
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        RestorePrologue((DWORD)callLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK)
        HWBP::DeleteHWBP((DWORD)callLuaLoadBuffer);

    static size_t injected_count = 0;

    // Обработка дампера
    if (mirage.Dumper)
    {
        if (name == nullptr)
        {
            std::string scr_name_dmp = xorstr_("chunk.lua");
            DumpIfNotDuplicate(xorstr_("Chunks"), scr_name_dmp.c_str(), buff, sz);
        }
        else if (findStringIC(name, mirage.dump_resource_name) || mirage.DumpAllScripts)
        {
            DumpScript(xorstr_("DumpedScripts"), name, buff, sz);
            LogInFile(LOG_NAME, xorstr_("[LOG] Dumped script: %s | Size: %d\n"), name, sz);
        }
    }

    // Флаг для определения, была ли произведена замена
    bool script_replaced = false;

    // Если имя скрипта задано и есть записи для инъекции
    if (name && !lua_injection_list.empty())
    {
        std::string script_name(name);

        for (const auto& lvm : lua_injection_list)
        {
            if (w_findStringIC(std::wstring(script_name.begin(), script_name.end()),
                std::wstring(lvm.target_script.begin(), lvm.target_script.end())))
            {
                AddMirageResource(script_name);
                std::ifstream script_file(lvm.our_script, std::ios::binary | std::ios::ate);
                if (!script_file.is_open())
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open our lua script: %ls\n"), lvm.our_script.c_str());
                    continue;
                }

                std::streamsize custom_size = script_file.tellg();
                script_file.seekg(0, std::ios::beg);

                // Создаем временный буфер для чтения
                std::string temp_script(custom_size, '\0');

                if (!script_file.read(&temp_script[0], custom_size))
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %ls\n"), lvm.our_script.c_str());
                    script_file.close();
                    continue;
                }

                script_file.close();

                // Декодируем скрипт
                bool decrypted = false;
                if (IsEncryptedScript(temp_script))
                {
                    temp_script = DecryptBuffer(temp_script, &decrypted);
                    if (!decrypted)
                    {
                        LogInFile(LOG_NAME, xorstr_("[WARN] Decrypt failed for script: %ls. Loading raw data; code leak risk.\n"),
                            lvm.our_script.c_str());
                    }
                }
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[WARN] Loading plaintext script: %ls. Code leak risk.\n"),
                        lvm.our_script.c_str());
                }
                ReplaceMirageFunc(temp_script);
                // Конвертируем в UTF-8 если необходимо
                if (!IsUtf8(temp_script))
                {
                    temp_script = cp1251_to_utf8(temp_script.c_str());
                }

                // Сохраняем скрипт в глобальном хранилище
                {
                    std::lock_guard<std::mutex> lock(scripts_mutex);
                    loaded_custom_scripts[script_name] = std::move(temp_script);
                }

                // Получаем ссылку на сохраненный скрипт
                auto& stored_script = loaded_custom_scripts[script_name];

                // Вызываем оригинальную функцию с нашим скриптом
                injected_count++;
                if (injected_count >= lua_injection_list.size())
                    hwbp_end1 = true;

                LogInFile(LOG_NAME, xorstr_("[LOG] Replacing script for: %s!\n"), name);

                // Восстанавливаем хук перед вызовом
                if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1)
                    HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
                if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
                    MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));

                // Вызываем с нашим скриптом
                return callLuaLoadBuffer(L, (char*)stored_script.data(), stored_script.size(), name);
            }
        }
    }

    // Восстанавливаем хук
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1)
        HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));

    // Вызываем оригинальную функцию с исходными параметрами
    return callLuaLoadBuffer(L, buff, sz, name);
}

// Более низкоуровневый хук луашки
int __cdecl lua_load(void* L, lua_Reader reader, void* data, const char* chunkname)
{
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        RestorePrologue((DWORD)call_lua_load, load_prologue, sizeof(load_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK)
        HWBP::DeleteHWBP((DWORD)call_lua_load);

    static size_t injected_count = 0;

    // Обработка дампера
    if (mirage.Dumper)
    {
        LoadS* c_ptr = reinterpret_cast<LoadS*>(data);
        if (chunkname != nullptr && c_ptr != nullptr)
        {
            if (!findStringIC(chunkname, xorstr_("@")))
            {
                std::string scr_name_dmp = xorstr_("chunk.lua");
                size_t dump_size = (c_ptr->size == 0) ? strlen(c_ptr->s) : c_ptr->size;
                DumpIfNotDuplicate(xorstr_("Chunks"), scr_name_dmp.c_str(), c_ptr->s, dump_size);
            }
            else if (findStringIC(chunkname, mirage.dump_resource_name) || mirage.DumpAllScripts)
            {
                size_t dump_size = (c_ptr->size == 0) ? strlen(c_ptr->s) : c_ptr->size;
                DumpScript(xorstr_("DumpedScripts"), chunkname, c_ptr->s, dump_size);
                LogInFile(LOG_NAME, xorstr_("[LOG] Dumped script: %s | Size: %d\n"), chunkname, dump_size);
            }
        }
    }

    // Проверяем необходимость инъекции
    if (chunkname && !lua_injection_list.empty())
    {
        std::string chunk_name(chunkname);

        for (const auto& lvm : lua_injection_list)
        {
            if (w_findStringIC(std::wstring(chunk_name.begin(), chunk_name.end()),
                std::wstring(lvm.target_script.begin(), lvm.target_script.end())))
            {
                AddMirageResource(chunk_name);
                LoadS* s_ptr = reinterpret_cast<LoadS*>(data);
                if (!s_ptr)
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: invalid data pointer for script: %s\n"), chunkname);
                    continue;
                }

                std::ifstream script_file(lvm.our_script, std::ios::binary | std::ios::ate);
                if (!script_file.is_open())
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't open our lua script: %ls\n"), lvm.our_script.c_str());
                    continue;
                }

                std::streamsize custom_size = script_file.tellg();
                script_file.seekg(0, std::ios::beg);

                // Создаем временный буфер
                std::string temp_script(custom_size, '\0');

                if (!script_file.read(&temp_script[0], custom_size))
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %ls\n"), lvm.our_script.c_str());
                    script_file.close();
                    continue;
                }

                script_file.close();

                // Декодируем скрипт
                bool decrypted = false;
                if (IsEncryptedScript(temp_script))
                {
                    temp_script = DecryptBuffer(temp_script, &decrypted);
                    if (!decrypted)
                    {
                        LogInFile(LOG_NAME, xorstr_("[WARN] Decrypt failed for script: %ls. Loading raw data; code leak risk.\n"),
                            lvm.our_script.c_str());
                    }
                }
                else
                {
                    LogInFile(LOG_NAME, xorstr_("[WARN] Loading plaintext script: %ls. Code leak risk.\n"),
                        lvm.our_script.c_str());
                }
                ReplaceMirageFunc(temp_script);
                // Конвертируем в UTF-8 если необходимо
                if (!IsUtf8(temp_script))
                {
                    temp_script = cp1251_to_utf8(temp_script.c_str());
                }

                // Сохраняем скрипт в глобальном хранилище
                {
                    std::lock_guard<std::mutex> lock(scripts_mutex);
                    loaded_custom_scripts[chunk_name] = std::move(temp_script);
                }

                // Получаем ссылку на сохраненный скрипт
                auto& stored_script = loaded_custom_scripts[chunk_name];

                // Создаем новую структуру LoadS с нашими данными
                LoadS custom_load_s;
                custom_load_s.s = const_cast<char*>(stored_script.c_str());
                custom_load_s.size = stored_script.size();

                injected_count++;
                if (injected_count >= lua_injection_list.size())
                    hwbp_end2 = true;

                LogInFile(LOG_NAME, xorstr_("[LOG] Replacing script for: %s!\n"), chunkname);

                // Восстанавливаем хук
                if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2)
                    HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
                if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
                    MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));

                // Вызываем с новой структурой
                return call_lua_load(L, reader, &custom_load_s, chunkname);
            }
        }
    }

    // Восстанавливаем хук
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2)
        HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));

    // Вызываем оригинальную функцию
    return call_lua_load(L, reader, data, chunkname);
}

// Функция для очистки загруженных скриптов (вызывать при выгрузке)
void ClearLoadedScripts()
{
    std::lock_guard<std::mutex> lock(scripts_mutex);
    loaded_custom_scripts.clear();
    std::lock_guard<std::mutex> res_lock(mirage_resources_mutex);
    mirage_resource_list.clear();
}