#include "AesCryptor.h"
#include "Utils/ScriptDump.h"
// Глобальные хранилища загруженных скриптов
// Держим скрипты глобально, чтобы буферы не освобождались после выхода из хука
static std::unordered_map<std::string, std::string> loaded_custom_scripts;
static std::mutex scripts_mutex; // Защита shared-хранилища скриптов
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
    // Снимаем перехват перед вызовом оригинала (для INLINE/HWBP режимов)
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        RestorePrologue((DWORD)callLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK)
        HWBP::DeleteHWBP((DWORD)callLuaLoadBuffer);

    static size_t injected_count = 0;

    // Дампим входящий скрипт при включенном дампере
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

    // Флаг: был ли найден и подменен целевой скрипт
    bool script_replaced = false;

    // Ищем совпадение имени загружаемого скрипта со списком lvm-подмен
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

                // Читаем файл нашего скрипта в временный буфер
                std::string temp_script(custom_size, '\0');

                if (!script_file.read(&temp_script[0], custom_size))
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %ls\n"), lvm.our_script.c_str());
                    script_file.close();
                    continue;
                }

                script_file.close();

                // Декодируем шифрованный скрипт (если требуется)
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
                // Сохраняем буфер скрипта в глобальном кэше
                {
                    std::lock_guard<std::mutex> lock(scripts_mutex);
                    loaded_custom_scripts[script_name] = std::move(temp_script);
                }

                // Берем стабильную ссылку на сохраненный буфер
                auto& stored_script = loaded_custom_scripts[script_name];

                // Вызываем оригинальный luaL_loadbuffer уже с подмененным скриптом
                injected_count++;
                if (injected_count >= lua_injection_list.size())
                    hwbp_end1 = true;

                LogInFile(LOG_NAME, xorstr_("[LOG] Replacing script for: %s!\n"), name);

                // Возвращаем перехват обратно перед выходом из хука
                if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1)
                    HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
                if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
                    MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));

                // Передаем в оригинал буфер подмененного скрипта
                return callLuaLoadBuffer(L, (char*)stored_script.data(), stored_script.size(), name);
            }
        }
    }

    // Возвращаем перехват, если подмена не произошла
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end1)
        HWBP::InstallHWBP((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        MakeJump((DWORD)callLuaLoadBuffer, (DWORD)&hkLuaLoadBuffer, loadbuff_prologue, sizeof(loadbuff_prologue));

    // Прозрачный passthrough: вызываем оригинал без изменений
    return callLuaLoadBuffer(L, buff, sz, name);
}

// Более низкоуровневый хук для lua_load
int __cdecl lua_load(void* L, lua_Reader reader, void* data, const char* chunkname)
{
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        RestorePrologue((DWORD)call_lua_load, load_prologue, sizeof(load_prologue));
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK)
        HWBP::DeleteHWBP((DWORD)call_lua_load);

    static size_t injected_count = 0;

    // Дампим chunk из lua_load, если включен дампер
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

    // Проверяем, нужно ли подменять текущий chunk
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

                // Читаем файл replacement-скрипта в буфер
                std::string temp_script(custom_size, '\0');

                if (!script_file.read(&temp_script[0], custom_size))
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] Error: can't read our lua script: %ls\n"), lvm.our_script.c_str());
                    script_file.close();
                    continue;
                }

                script_file.close();

                // Декодируем replacement-скрипт при необходимости
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
                // Кэшируем replacement-скрипт в глобальном хранилище
                {
                    std::lock_guard<std::mutex> lock(scripts_mutex);
                    loaded_custom_scripts[chunk_name] = std::move(temp_script);
                }

                // Берем ссылку на кэшированный replacement-буфер
                auto& stored_script = loaded_custom_scripts[chunk_name];

                // Формируем структуру LoadS, указывающую на наш буфер
                LoadS custom_load_s;
                custom_load_s.s = const_cast<char*>(stored_script.c_str());
                custom_load_s.size = stored_script.size();

                injected_count++;
                if (injected_count >= lua_injection_list.size())
                    hwbp_end2 = true;

                LogInFile(LOG_NAME, xorstr_("[LOG] Replacing script for: %s!\n"), chunkname);

                // Возвращаем перехват перед вызовом оригинала
                if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2)
                    HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
                if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
                    MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));

                // Вызываем оригинальный lua_load с подмененным LoadS
                return call_lua_load(L, reader, &custom_load_s, chunkname);
            }
        }
    }

    // Возвращаем перехват, если подмена не применялась
    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK && !hwbp_end2)
        HWBP::InstallHWBP((DWORD)call_lua_load, (DWORD)&lua_load);
    if (mirage.hwbp_hooking == HookingType::INLINE_JUMP)
        MakeJump((DWORD)call_lua_load, (DWORD)&lua_load, load_prologue, sizeof(load_prologue));

    // Без подмены: обычный вызов оригинального lua_load
    return call_lua_load(L, reader, data, chunkname);
}

// Очистка кэша подмененных скриптов при выгрузке
void ClearLoadedScripts()
{
    std::lock_guard<std::mutex> lock(scripts_mutex);
    loaded_custom_scripts.clear();
    std::lock_guard<std::mutex> res_lock(mirage_resources_mutex);
    mirage_resource_list.clear();
}

struct ExoticTString
{
    void* next;
    unsigned char tt;
    unsigned char marked;
    unsigned char reserved;
    unsigned int hash;
    size_t len;
};

struct ExoticZio
{
    size_t n;
    const char* p;
    lua_Reader reader;
    void* data;
    void* L;
    int eoz;
};

struct ExoticMbuffer
{
    char* buffer;
    size_t n;
    size_t buffsize;
};

struct ExoticSParser
{
    ExoticZio* z;
    ExoticMbuffer buff;
    const char* name;
};

struct ExoticScriptEntry
{
    std::wstring target_script;
    std::string script;
};

struct ExoticZioState
{
    std::string original_data;
    std::string chunk_name;
    const ExoticScriptEntry* replacement;
    std::string override_script;
    size_t override_offset;
    bool override_is_original;
    bool chunk_name_known;
    bool compiled;
    bool compiled_checked;
    bool dump_done;
    bool override_ready;
    bool drain_done;
    bool log_replace;
    bool log_compiled;
    bool log_bad_name;
    bool log_dump_disabled;
    bool log_no_data;
    bool log_dump_skipped;
};

static std::mutex exotic_mutex;
static std::unordered_map<ExoticZio*, ExoticZioState> exotic_states;
static std::vector<ExoticScriptEntry> exotic_scripts;
static bool exotic_scripts_loaded = false;
static bool exotic_hooks_ready = false;
static bool exotic_hooks_failed = false;
static bool exotic_logged_fallback = false;
static bool exotic_logged_not_found = false;

static constexpr int kExoticEoz = -1;
static constexpr size_t kExoticChunkSize = 4096;
typedef int(__cdecl* ptr_luaZ_fill)(ExoticZio* z);
typedef void(__cdecl* ptr_luaX_setinput)(void* L, void* ls, ExoticZio* z, ExoticTString* source);
typedef int(__cdecl* ptr_luaD_protectedparser)(void* L, ExoticZio* z, const char* name);
typedef void(__cdecl* ptr_f_parser)(void* L, void* ud);
ptr_luaZ_fill call_luaZ_fill = nullptr;
ptr_luaX_setinput call_luaX_setinput = nullptr;
ptr_luaD_protectedparser call_luaD_protectedparser = nullptr;
ptr_f_parser call_f_parser = nullptr;

static size_t ExoticStrnlen(const char* s, size_t max_len)
{
    if (!s) return 0;
    size_t n = 0;
    while (n < max_len && s[n] != '\0')
    {
        ++n;
    }
    return n;
}

static bool ExoticTryGetParserInfo(void* ud, ExoticZio*& out_z, const char*& out_name)
{
    if (!ud) return false;
    __try
    {
        ExoticSParser* parser = reinterpret_cast<ExoticSParser*>(ud);
        out_z = parser->z;
        out_name = parser->name;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out_z = nullptr;
        out_name = nullptr;
        return false;
    }
    return out_z != nullptr && out_name != nullptr;
}

static bool ExoticTryGetChunkName(const char* name, std::string& out_name)
{
    if (!name) return false;
    size_t len = 0;
    __try
    {
        len = ExoticStrnlen(name, 4096);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (len == 0 || len >= 4096) return false;
    out_name.assign(name, len);
    return true;
}

static bool ExoticGetChunkName(ExoticTString* source, std::string& out_name)
{
    if (!source) return false;
    const char* data = reinterpret_cast<const char*>(source + 1);
    if (!data) return false;
    size_t len = source->len;
    if (len == 0 || len > 4096)
    {
        len = ExoticStrnlen(data, 4096);
        if (len == 0 || len >= 4096) return false;
    }
    out_name.assign(data, len);
    return true;
}

static bool ExoticIsBytecode(const std::string& data)
{
    if (data.size() < 4) return false;
    return (static_cast<unsigned char>(data[0]) == 0x1B &&
        data[1] == 'L' && data[2] == 'u' && data[3] == 'a');
}

static std::string ExoticNormalizeName(const std::string& name)
{
    std::string out = name;
    for (auto& ch : out)
    {
        if (ch == '/') ch = '\\';
    }
    return out;
}

static bool ExoticEndsWithIC(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size()) return false;
    size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i)
    {
        char a = value[offset + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
        if (a != b) return false;
    }
    return true;
}

static std::string ExoticBuildDumpName(const ExoticZioState& state)
{
    std::string name = ExoticNormalizeName(state.chunk_name);
    if (name.empty())
    {
        name = state.compiled ? xorstr_("chunk.luac") : xorstr_("chunk.lua");
    }
    if (state.compiled)
    {
        if (!ExoticEndsWithIC(name, xorstr_(".luac")))
        {
            if (ExoticEndsWithIC(name, xorstr_(".lua")))
                name = name.substr(0, name.size() - 4) + xorstr_(".luac");
            else
                name += xorstr_(".luac");
        }
    }
    return name;
}

static void ExoticLoadScripts()
{
    if (exotic_scripts_loaded) return;
    std::vector<ExoticScriptEntry> local_scripts;
    local_scripts.reserve(lua_injection_list.size());
    for (const auto& lvm : lua_injection_list)
    {
        std::ifstream script_file(lvm.our_script, std::ios::binary | std::ios::ate);
        if (!script_file.is_open())
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: can't open script: %ls\n"), lvm.our_script.c_str());
            continue;
        }
        std::streamsize custom_size = script_file.tellg();
        if (custom_size <= 0)
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: empty script: %ls\n"), lvm.our_script.c_str());
            script_file.close();
            continue;
        }
        script_file.seekg(0, std::ios::beg);
        std::string temp_script(static_cast<size_t>(custom_size), '\0');
        if (!script_file.read(&temp_script[0], custom_size))
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: can't read script: %ls\n"), lvm.our_script.c_str());
            script_file.close();
            continue;
        }
        script_file.close();
        bool decrypted = false;
        if (IsEncryptedScript(temp_script))
        {
            temp_script = DecryptBuffer(temp_script, &decrypted);
            if (!decrypted)
            {
                LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC: decrypt failed: %ls\n"), lvm.our_script.c_str());
            }
        }
        ReplaceMirageFunc(temp_script);
        ExoticScriptEntry entry;
        entry.target_script = lvm.target_script;
        entry.script = std::move(temp_script);
        local_scripts.push_back(std::move(entry));
    }
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        if (!exotic_scripts_loaded)
        {
            exotic_scripts = std::move(local_scripts);
            exotic_scripts_loaded = true;
        }
    }
}

static const ExoticScriptEntry* ExoticFindScript(const std::string& chunk_name)
{
    if (chunk_name.empty()) return nullptr;
    std::wstring wname(chunk_name.begin(), chunk_name.end());
    std::lock_guard<std::mutex> lock(exotic_mutex);
    for (const auto& entry : exotic_scripts)
    {
        if (w_findStringIC(wname, entry.target_script))
        {
            return &entry;
        }
    }
    return nullptr;
}

static bool ExoticShouldDumpFull(const std::string& chunk_name)
{
    if (chunk_name.empty()) return false;
    if (mirage.DumpAllScripts) return true;
    if (!mirage.dump_resource_name.empty())
    {
        return findStringIC(chunk_name, mirage.dump_resource_name);
    }
    return false;
}

static bool ExoticShouldDumpChunk(const std::string& chunk_name)
{
    if (mirage.DumpAllScripts) return false;
    if (!mirage.dump_resource_name.empty()) return false;
    if (chunk_name.empty()) return true;
    return !findStringIC(chunk_name, xorstr_("@"));
}

static void ExoticDrainOriginal(ExoticZio* z)
{
    if (!z || !call_luaZ_fill) return;
    z->eoz = 0;
    z->n = 0;
    z->p = nullptr;
    while (true)
    {
        int ret = call_luaZ_fill(z);
        if (ret == kExoticEoz)
        {
            break;
        }
        size_t size = z->n + 1;
        const char* buf = z->p - 1;
        if (buf && size > 0)
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto it = exotic_states.find(z);
            if (it != exotic_states.end())
            {
                it->second.original_data.append(buf, size);
            }
        }
        z->n = 0;
        z->p = nullptr;
    }
    z->eoz = 0;
    z->n = 0;
    z->p = nullptr;
}

static void ExoticPrepareDump(ExoticZioState& state, std::string& out_name, std::string& out_data, bool& out_full)
{
    out_full = false;
    if (!mirage.Dumper)
    {
        if (!state.log_dump_disabled)
        {
            state.log_dump_disabled = true;
            //LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC: dumper disabled, skipping.\n"));
        }
        return;
    }
    if (state.original_data.empty())
    {
        bool should_dump = mirage.DumpAllScripts || ExoticShouldDumpFull(state.chunk_name) || ExoticShouldDumpChunk(state.chunk_name);
        if (should_dump && !state.log_no_data)
        {
            state.log_no_data = true;
            LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC: no data collected for dump. name=%s compiled=%d override=%d drain=%d\n"), state.chunk_name.c_str(), state.compiled ? 1 : 0, state.override_ready ? 1 : 0, state.drain_done ? 1 : 0);
        }
        return;
    }
    if (ExoticShouldDumpFull(state.chunk_name))
    {
        out_name = ExoticBuildDumpName(state);
        out_data = state.original_data;
        out_full = true;
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dump match full name=%s size=%d compiled=%d\n"), out_name.c_str(), out_data.size(), state.compiled ? 1 : 0);
        return;
    }
    if (ExoticShouldDumpChunk(state.chunk_name))
    {
        out_name = state.compiled ? xorstr_("chunk.luac") : xorstr_("chunk.lua");
        out_data = state.original_data;
        out_full = false;
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dump fallback chunk name=%s size=%d compiled=%d\n"), out_name.c_str(), out_data.size(), state.compiled ? 1 : 0);
    }
    else
    {
        if (!state.log_dump_skipped)
        {
            state.log_dump_skipped = true;
            LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC: dump skipped, no match for resource=%s chunk=%s\n"), mirage.dump_resource_name.c_str(), state.chunk_name.c_str());
        }
    }
}

int __cdecl hkLuaZFill(ExoticZio* z)
{
    if (!z || !call_luaZ_fill)
    {
        return kExoticEoz;
    }

    const std::string* override_script = nullptr;
    size_t offset = 0;
    bool use_override = false;
    bool do_drain = false;
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        if (state.override_ready)
        {
            if (state.override_is_original)
            {
                if (!state.override_script.empty())
                {
                    use_override = true;
                    override_script = &state.override_script;
                    offset = state.override_offset;
                }
            }
            else if (state.replacement)
            {
                use_override = true;
                override_script = &state.replacement->script;
                offset = state.override_offset;
                if (mirage.Dumper && !state.drain_done)
                {
                    state.drain_done = true;
                    do_drain = true;
                }
            }
        }
    }

    if (do_drain)
    {
        ExoticDrainOriginal(z);
    }

    if (use_override && override_script)
    {
        size_t total = override_script->size();
        if (offset >= total)
        {
            std::string dump_name;
            std::string dump_data;
            bool dump_full = false;
            {
                std::lock_guard<std::mutex> lock(exotic_mutex);
                auto it = exotic_states.find(z);
                if (it != exotic_states.end())
                {
                    if (!it->second.dump_done)
                    {
                        ExoticPrepareDump(it->second, dump_name, dump_data, dump_full);
                        it->second.dump_done = true;
                    }
                    exotic_states.erase(it);
                }
            }
            if (!dump_data.empty())
            {
                if (dump_full)
                {
                    std::string dumped_path = DumpScriptEx(xorstr_("DumpedScripts"), dump_name, const_cast<char*>(dump_data.data()), dump_data.size());
                    if (!dumped_path.empty())
                    {
                        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dumped full to %s | Size: %d\n"), dumped_path.c_str(), dump_data.size());
                    }
                }
                else
                {
                    std::string dumped_path = DumpIfNotDuplicateEx(xorstr_("Chunks"), dump_name.c_str(), const_cast<char*>(dump_data.data()), dump_data.size());
                    if (!dumped_path.empty())
                    {
                        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dumped chunk to %s | Size: %d\n"), dumped_path.c_str(), dump_data.size());
                    }
                }
            }
            z->eoz = 1;
            z->n = 0;
            z->p = nullptr;
            return kExoticEoz;
        }

        size_t remain = total - offset;
        size_t size = remain > kExoticChunkSize ? kExoticChunkSize : remain;
        const char* buf = override_script->data() + offset;
        z->p = buf;
        z->n = size > 0 ? size - 1 : 0;
        z->eoz = 0;
        int ret = static_cast<unsigned char>(*(z->p++));
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto it = exotic_states.find(z);
            if (it != exotic_states.end())
            {
                it->second.override_offset += size;
            }
        }
        return ret;
    }

    int ret = call_luaZ_fill(z);
    if (ret == kExoticEoz)
    {
        std::string dump_name;
        std::string dump_data;
        bool dump_full = false;
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto it = exotic_states.find(z);
            if (it != exotic_states.end())
            {
                if (!it->second.dump_done)
                {
                    ExoticPrepareDump(it->second, dump_name, dump_data, dump_full);
                    it->second.dump_done = true;
                }
                exotic_states.erase(it);
            }
        }
        if (!dump_data.empty())
        {
            if (dump_full)
            {
                std::string dumped_path = DumpScriptEx(xorstr_("DumpedScripts"), dump_name, const_cast<char*>(dump_data.data()), dump_data.size());
                if (!dumped_path.empty())
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dumped full to %s | Size: %d\n"), dumped_path.c_str(), dump_data.size());
                }
            }
            else
            {
                std::string dumped_path = DumpIfNotDuplicateEx(xorstr_("Chunks"), dump_name.c_str(), const_cast<char*>(dump_data.data()), dump_data.size());
                if (!dumped_path.empty())
                {
                    LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: dumped chunk to %s | Size: %d\n"), dumped_path.c_str(), dump_data.size());
                }
            }
        }
        return ret;
    }

    bool log_compiled = false;
    {
        size_t size = z->n + 1;
        const char* buf = z->p - 1;
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        if (buf && size > 0)
        {
            state.original_data.append(buf, size);
        }
        if (!state.compiled_checked && state.original_data.size() >= 4)
        {
            state.compiled_checked = true;
            if (ExoticIsBytecode(state.original_data))
            {
                state.compiled = true;
                if (!state.log_compiled)
                {
                    state.log_compiled = true;
                    log_compiled = true;
                }
            }
        }
    }
    if (log_compiled)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: bytecode detected, lexer will be skipped.\n"));
    }
    return ret;
}

int __cdecl hkLuaDProtectedParser(void* L, ExoticZio* z, const char* name)
{
    if (!call_luaD_protectedparser)
    {
        return 0;
    }
    if (!z || !name)
    {
        return call_luaD_protectedparser(L, z, name);
    }

    std::string chunk_name(name);
    if (!exotic_scripts_loaded)
    {
        ExoticLoadScripts();
    }

    const ExoticScriptEntry* replacement = ExoticFindScript(chunk_name);
    bool do_drain = false;
    bool log_replace = false;
    bool want_dump = mirage.Dumper && (mirage.DumpAllScripts || ExoticShouldDumpFull(chunk_name));
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        state.chunk_name = chunk_name;
        state.chunk_name_known = !chunk_name.empty();
        if (replacement)
        {
            state.replacement = replacement;
            state.override_ready = true;
            state.override_is_original = false;
            state.override_offset = 0;
            if (!state.log_replace)
            {
                state.log_replace = true;
                log_replace = true;
            }
            if (mirage.Dumper && !state.drain_done)
            {
                state.drain_done = true;
                do_drain = true;
            }
        }
        else if (want_dump && !state.drain_done)
        {
            state.drain_done = true;
            state.override_is_original = true;
            do_drain = true;
        }
    }

    if (replacement)
    {
        AddMirageResource(chunk_name);
    }

    if (log_replace)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: replacing script for: %s\n"), chunk_name.c_str());
    }

    if (do_drain)
    {
        ExoticDrainOriginal(z);
        if (!replacement)
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto& state = exotic_states[z];
            if (state.override_is_original && !state.original_data.empty())
            {
                state.override_script = state.original_data;
                state.override_ready = true;
                state.override_offset = 0;
                if (!state.compiled_checked && state.original_data.size() >= 4)
                {
                    state.compiled_checked = true;
                    if (ExoticIsBytecode(state.original_data))
                    {
                        state.compiled = true;
                    }
                }
            }
            else
            {
                state.override_is_original = false;
            }
        }
    }

    if (replacement)
    {
        z->n = 0;
        z->p = nullptr;
        z->eoz = 0;
    }

    return call_luaD_protectedparser(L, z, name);
}

void __cdecl hkLuaFParser(void* L, void* ud)
{
    if (!call_f_parser)
    {
        return;
    }

    ExoticZio* z = nullptr;
    const char* name = nullptr;
    if (!ExoticTryGetParserInfo(ud, z, name))
    {
        call_f_parser(L, ud);
        return;
    }

    std::string chunk_name;
    bool name_ok = ExoticTryGetChunkName(name, chunk_name);
    if (!name_ok)
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        if (!state.log_bad_name)
        {
            state.log_bad_name = true;
        }
    }

    if (!exotic_scripts_loaded)
    {
        ExoticLoadScripts();
    }

    const ExoticScriptEntry* replacement = ExoticFindScript(chunk_name);
    bool do_drain = false;
    bool log_replace = false;
    bool want_dump = mirage.Dumper && (mirage.DumpAllScripts || ExoticShouldDumpFull(chunk_name));
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        state.chunk_name = chunk_name;
        state.chunk_name_known = !chunk_name.empty();
        if (replacement)
        {
            state.replacement = replacement;
            state.override_ready = true;
            state.override_is_original = false;
            state.override_offset = 0;
            if (!state.log_replace)
            {
                state.log_replace = true;
                log_replace = true;
            }
            if (mirage.Dumper && !state.drain_done)
            {
                state.drain_done = true;
                do_drain = true;
            }
        }
        else if (want_dump && !state.drain_done)
        {
            state.drain_done = true;
            state.override_is_original = true;
            do_drain = true;
        }
    }

    if (replacement)
    {
        AddMirageResource(chunk_name);
    }

    if (log_replace)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: replacing script for: %s\n"), chunk_name.c_str());
    }

    if (do_drain)
    {
        ExoticDrainOriginal(z);
        if (!replacement)
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto& state = exotic_states[z];
            if (state.override_is_original && !state.original_data.empty())
            {
                state.override_script = state.original_data;
                state.override_ready = true;
                state.override_offset = 0;
                if (!state.compiled_checked && state.original_data.size() >= 4)
                {
                    state.compiled_checked = true;
                    if (ExoticIsBytecode(state.original_data))
                    {
                        state.compiled = true;
                    }
                }
            }
            else
            {
                state.override_is_original = false;
            }
        }
    }

    if (replacement)
    {
        z->n = 0;
        z->p = nullptr;
        z->eoz = 0;
    }

    call_f_parser(L, ud);
}

void __cdecl hkLuaXSetInput(void* L, void* ls, ExoticZio* z, ExoticTString* source)
{
    if (!z || !call_luaX_setinput)
    {
        if (call_luaX_setinput)
        {
            call_luaX_setinput(L, ls, z, source);
        }
        return;
    }
    std::string chunk_name;
    bool name_ok = ExoticGetChunkName(source, chunk_name);
    if (!name_ok)
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        if (!state.log_bad_name)
        {
            state.log_bad_name = true;
        }
    }

    if (!exotic_scripts_loaded)
    {
        ExoticLoadScripts();
    }

    const ExoticScriptEntry* replacement = nullptr;
    if (!chunk_name.empty())
    {
        replacement = ExoticFindScript(chunk_name);
        if (replacement)
        {
            AddMirageResource(chunk_name);
        }
    }

    bool do_drain = false;
    bool log_replace = false;
    bool want_dump = mirage.Dumper && (mirage.DumpAllScripts || ExoticShouldDumpFull(chunk_name));
    {
        std::lock_guard<std::mutex> lock(exotic_mutex);
        auto& state = exotic_states[z];
        state.chunk_name = chunk_name;
        state.chunk_name_known = !chunk_name.empty();
        if (replacement)
        {
            state.replacement = replacement;
            state.override_ready = true;
            state.override_is_original = false;
            state.override_offset = 0;
            if (!state.log_replace)
            {
                state.log_replace = true;
                log_replace = true;
            }
            if (mirage.Dumper && !state.drain_done)
            {
                state.drain_done = true;
                do_drain = true;
            }
        }
        else if (want_dump && !state.drain_done)
        {
            state.drain_done = true;
            state.override_is_original = true;
            do_drain = true;
        }
    }

    if (log_replace)
    {
        LogInFile(LOG_NAME, xorstr_("[LOG] EXOTIC: replacing script for: %s\n"), chunk_name.c_str());
    }

    if (do_drain)
    {
        ExoticDrainOriginal(z);
        if (!replacement)
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto& state = exotic_states[z];
            if (state.override_is_original && !state.original_data.empty())
            {
                state.override_script = state.original_data;
                state.override_ready = true;
                state.override_offset = 0;
                if (!state.compiled_checked && state.original_data.size() >= 4)
                {
                    state.compiled_checked = true;
                    if (ExoticIsBytecode(state.original_data))
                    {
                        state.compiled = true;
                    }
                }
            }
            else
            {
                state.override_is_original = false;
            }
        }
    }

    if (replacement)
    {
        z->n = 0;
        z->p = nullptr;
        z->eoz = 0;
    }

    call_luaX_setinput(L, ls, z, source);
}

void SetupExoticLuaHooks()
{
    if (exotic_hooks_ready || exotic_hooks_failed) return;
    SigScan scan;
   
    if (!call_luaZ_fill)
    {
        call_luaZ_fill = (ptr_luaZ_fill)scan.FindPatternIDA(xorstr_("lua5.1c.dll"), xorstr_("55 8B EC 56 8B 75 ? 83 7E ? ? 75"));
        if (call_luaZ_fill == nullptr) call_luaZ_fill = (ptr_luaZ_fill)scan.FindPatternIDA(xorstr_("client.dll"), xorstr_("55 8B EC 51 56 68"));
        if (call_luaZ_fill)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to EXOTIC_1!\n"));
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for EXOTIC_1.\n"));
        }
    }
    if (!call_luaX_setinput)
    {
        call_luaX_setinput = (ptr_luaX_setinput)scan.FindPatternIDA(xorstr_("lua5.1c.dll"), xorstr_("55 8B EC 8B 45 ? 8B 4D ? 56 8B 75"));
        if (call_luaX_setinput)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to EXOTIC_2!\n"));
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for EXOTIC_2.\n"));
        }
    }
    if (!call_luaD_protectedparser)
    {
        call_luaD_protectedparser = (ptr_luaD_protectedparser)scan.FindPatternIDA(xorstr_("lua5.1c.dll"), xorstr_("55 8B EC 83 EC ? 8B 45 ? 53 56"));
        if (call_luaD_protectedparser)
        {
            LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to EXOTIC_3!\n"));
        }
        else
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a sig for EXOTIC_3.\n"));
        }
    }

    if (!call_luaX_setinput && !call_luaD_protectedparser && !call_f_parser)
    {
        call_f_parser = (ptr_f_parser)scan.FindPatternIDA(xorstr_("client.dll"), xorstr_("55 8B EC 53 56 57 68 ? ? ? ? E9 ? ? ? ? 33 D3"));
        if (!exotic_logged_fallback)
        {
            exotic_logged_fallback = true;
            if (call_f_parser)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to EXOTIC_4 fallback.\n"));
            }
            else
            {
                LogInFile(LOG_NAME, xorstr_("[WARN] Can`t find a sig for EXOTIC_4 fallback.\n"));
            }
        }
    }

    if (!call_luaZ_fill || (!call_luaX_setinput && !call_f_parser))
    {
        if (!exotic_logged_not_found)
        {
            exotic_logged_not_found = true;
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC_1 or EXOTIC_2/EXOTIC_4 not found.\n"));
        }
        return;
    }

    if (mirage.hwbp_hooking == HookingType::HWBP_HOOK)
    {
        HWBP::InstallHWBP((DWORD)call_luaZ_fill, (DWORD)&hkLuaZFill);
        if (call_luaX_setinput)
        {
            HWBP::InstallHWBP((DWORD)call_luaX_setinput, (DWORD)&hkLuaXSetInput);
        }
        else if (call_f_parser)
        {
            HWBP::InstallHWBP((DWORD)call_f_parser, (DWORD)&hkLuaFParser);
        }
        if (call_luaD_protectedparser)
        {
            HWBP::InstallHWBP((DWORD)call_luaD_protectedparser, (DWORD)&hkLuaDProtectedParser);
        }
        exotic_hooks_ready = true;
        return;
    }

    if (MH_CreateHook(call_luaZ_fill, &hkLuaZFill, reinterpret_cast<LPVOID*>(&call_luaZ_fill)) != MH_OK)
    {
        LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_1.\n"));
        exotic_hooks_failed = true;
        return;
    }
    if (call_luaX_setinput)
    {
        if (MH_CreateHook(call_luaX_setinput, &hkLuaXSetInput, reinterpret_cast<LPVOID*>(&call_luaX_setinput)) != MH_OK)
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_2.\n"));
            exotic_hooks_failed = true;
            return;
        }
    }
    else if (call_f_parser)
    {
        if (MH_CreateHook(call_f_parser, &hkLuaFParser, reinterpret_cast<LPVOID*>(&call_f_parser)) != MH_OK)
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_f_parser.\n"));
            exotic_hooks_failed = true;
            return;
        }
    }

    if (call_luaD_protectedparser)
    {
        MH_CreateHook(call_luaD_protectedparser, &hkLuaDProtectedParser, reinterpret_cast<LPVOID*>(&call_luaD_protectedparser));
    }
    else
    {
        LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC_3 not found.\n"));
    }
    MH_EnableHook(MH_ALL_HOOKS);
    exotic_hooks_ready = true;
}
