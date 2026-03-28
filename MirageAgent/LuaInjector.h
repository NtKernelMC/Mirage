#include "AesCryptor.h"
#include "Utils/ScriptDump.h"
#include <unordered_set>
// Глобальные хранилища загруженных скриптов
// Держим скрипты глобально, чтобы буферы не освобождались после выхода из хука
static std::unordered_map<std::string, std::string> loaded_custom_scripts;
static std::mutex scripts_mutex; // Защита shared-хранилища скриптов
std::vector<std::string> mirage_resource_list;
std::mutex mirage_resources_mutex;
static std::unordered_set<std::string> logical_bomb_logged_scripts;
static std::mutex logical_bomb_mutex;
static std::unordered_map<std::string, std::unordered_map<std::string, unsigned long long>> script_hash_snapshot_baselines;
static std::unordered_map<std::string, std::unordered_map<std::string, unsigned long long>> script_hash_snapshots;
static std::unordered_map<std::string, bool> script_hash_snapshot_has_file;
static std::unordered_set<std::string> script_hash_snapshot_loaded;
static std::unordered_set<std::string> script_hash_warned_keys;
static std::mutex script_hash_snapshot_mutex;
static std::wstring cached_game_file_cache_dir;
static bool cached_game_file_cache_dir_loaded = false;
static std::mutex cached_game_file_cache_dir_mutex;

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

static std::string ExtractSnapshotResourceName(const std::string& script_name)
{
    std::string normalized = NormalizeDumpSlashes(script_name);
    if (normalized.empty())
        return {};

    size_t start = normalized[0] == '@' ? 1 : 0;
    size_t sep = normalized.find('\\', start);
    if (sep == std::string::npos)
        return {};

    return normalized.substr(start, sep - start);
}

static std::string ExtractSnapshotScriptPath(const std::string& script_name)
{
    std::string normalized = NormalizeDumpSlashes(script_name);
    if (normalized.empty())
        return {};

    size_t start = normalized[0] == '@' ? 1 : 0;
    size_t sep = normalized.find('\\', start);
    if (sep == std::string::npos || sep + 1 >= normalized.size())
        return {};

    return BuildDumpRelativePath(normalized.substr(sep + 1), xorstr_("root.lua"));
}

static bool IsAnonymousChunkName(const char* script_name)
{
    if (!script_name || !script_name[0])
        return true;
    return !findStringIC(script_name, xorstr_("@"));
}

static bool HasNamedDumpPath(const char* script_name)
{
    if (!script_name || !script_name[0])
        return false;
    if (script_name[0] != '@')
        return false;

    size_t len = strlen(script_name);
    if (len < 4 || len > 512)
        return false;
    if (!strchr(script_name, '\\') && !strchr(script_name, '/'))
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        char ch = script_name[i];
        if (ch == '\r' || ch == '\n' || ch == '\t')
            return false;
    }

    std::string relative_path = BuildDumpRelativePath(script_name);
    size_t slash_pos = relative_path.find('\\');
    size_t dot_pos = relative_path.find_last_of('.');
    return slash_pos != std::string::npos && dot_pos != std::string::npos && dot_pos > slash_pos;
}

static unsigned long long CalculateScriptSnapshotHash(const char* buffer, size_t size)
{
    const unsigned long long kOffset = 14695981039346656037ull;
    const unsigned long long kPrime = 1099511628211ull;
    unsigned long long hash = kOffset;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<unsigned char>(buffer[i]);
        hash *= kPrime;
    }
    return hash;
}

static bool IsValidSnapshotScriptName(const std::string& script_name)
{
    if (script_name.size() < 4 || script_name.size() > 512)
        return false;
    if (script_name[0] != '@')
        return false;
    if (script_name.find('\\') == std::string::npos && script_name.find('/') == std::string::npos)
        return false;

    for (char ch : script_name)
    {
        if (ch == '\r' || ch == '\n' || ch == '\t')
            return false;
    }

    return true;
}

static bool LoadScriptHashSnapshotFile(const std::string& resource_name, std::unordered_map<std::string, unsigned long long>& out_snapshot);

static void EnsureScriptHashSnapshotLoadedLocked(const std::string& resource_name)
{
    if (resource_name.empty() || script_hash_snapshot_loaded.count(resource_name))
        return;

    std::unordered_map<std::string, unsigned long long> loaded_snapshot;
    const bool has_file = LoadScriptHashSnapshotFile(resource_name, loaded_snapshot);
    script_hash_snapshot_baselines[resource_name] = loaded_snapshot;
    script_hash_snapshots[resource_name] = std::move(loaded_snapshot);
    script_hash_snapshot_has_file[resource_name] = has_file;
    script_hash_snapshot_loaded.insert(resource_name);
}

static bool HasSnapshotBaselineScriptPath(const std::string& script_name)
{
    if (!IsValidSnapshotScriptName(script_name))
        return false;

    const std::string resource_name = ExtractSnapshotResourceName(script_name);
    const std::string relative_path = ExtractSnapshotScriptPath(script_name);
    if (resource_name.empty() || relative_path.empty())
        return false;

    std::lock_guard<std::mutex> lock(script_hash_snapshot_mutex);
    EnsureScriptHashSnapshotLoadedLocked(resource_name);
    if (!script_hash_snapshot_has_file[resource_name])
        return false;

    const auto& baseline = script_hash_snapshot_baselines[resource_name];
    return baseline.find(relative_path) != baseline.end();
}

static std::wstring ReadGameRegistryString(HKEY root, const wchar_t* subkey, const wchar_t* value_name)
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegGetValueW(root, subkey, value_name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &size) != ERROR_SUCCESS)
        return {};

    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(root, subkey, value_name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, buf.data(), &size) != ERROR_SUCCESS)
        return {};

    return buf.data();
}

static std::wstring GetGameFileCacheDir()
{
    std::lock_guard<std::mutex> lock(cached_game_file_cache_dir_mutex);
    if (cached_game_file_cache_dir_loaded)
        return cached_game_file_cache_dir;

    cached_game_file_cache_dir = ReadGameRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\WOW6432Node\\UKRAINEGTA: GLAB3\\Common", L"File Cache Path");

    if (cached_game_file_cache_dir.empty())
        cached_game_file_cache_dir = ReadGameRegistryString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Multi Theft Auto: San Andreas All\\Common", L"File Cache Path");

    if (cached_game_file_cache_dir.empty())
    {
        wchar_t module_path[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) > 0)
        {
            std::filesystem::path fallback = std::filesystem::path(module_path).parent_path();
            fallback /= L"mods";
            fallback /= L"deathmatch";
            cached_game_file_cache_dir = fallback.wstring();
        }
    }

    cached_game_file_cache_dir_loaded = true;
    return cached_game_file_cache_dir;
}

static std::filesystem::path BuildGameResourceScriptPath(const char* script_name)
{
    if (!HasNamedDumpPath(script_name))
        return {};

    std::wstring file_cache_dir = GetGameFileCacheDir();
    if (file_cache_dir.empty())
        return {};

    std::string normalized = NormalizeDumpSlashes(script_name);
    if (!normalized.empty() && normalized[0] == '@')
        normalized.erase(0, 1);

    std::filesystem::path result(file_cache_dir);
    result /= L"resources";

    size_t start = 0;
    while (start <= normalized.size())
    {
        size_t pos = normalized.find('\\', start);
        std::string token = pos == std::string::npos ? normalized.substr(start) : normalized.substr(start, pos - start);
        if (!token.empty())
            result /= std::wstring(token.begin(), token.end());
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }

    return result;
}

static bool IsScriptCachedByGameResources(const char* script_name)
{
    std::filesystem::path file_path = BuildGameResourceScriptPath(script_name);
    if (file_path.empty())
        return false;

    std::error_code ec;
    return !std::filesystem::exists(file_path, ec);
}

static bool IsHiddenCachedScriptPath(const char* script_name)
{
    if (!HasNamedDumpPath(script_name))
        return false;
    return IsScriptCachedByGameResources(script_name);
}

static bool ShouldDumpNamedScript(const char* script_name)
{
    if (!HasNamedDumpPath(script_name))
        return false;
    if (mirage.DumpAllScripts)
        return true;
    if (!mirage.dump_resource_name.empty())
        return findStringIC(script_name, mirage.dump_resource_name);
    return false;
}

static bool ShouldDumpHiddenCachedScript(const char* script_name)
{
    if (!IsHiddenCachedScriptPath(script_name))
        return false;
    if (mirage.DumpOnlyCached)
        return true;
    return ShouldDumpNamedScript(script_name);
}

static std::filesystem::path BuildScriptSnapshotPath(const std::string& resource_name)
{
    std::filesystem::path base(lua_scripts_dir);
    base /= xorstr_("ScriptSnapshots");
    base /= BuildDumpRelativePath(resource_name, xorstr_("unknown"));

    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base / xorstr_("snapshot.hashes");
}

static std::filesystem::path BuildLegacyScriptSnapshotPath(const std::string& resource_name)
{
    std::filesystem::path base(lua_scripts_dir);
    base /= xorstr_("ScriptSnapshots");
    std::error_code ec;
    std::filesystem::create_directories(base, ec);

    std::string file_name = BuildDumpRelativePath(resource_name, xorstr_("unknown"));
    for (char& ch : file_name)
    {
        if (ch == '\\') ch = '_';
    }

    return base / (file_name + xorstr_(".hashes"));
}

static bool LoadScriptHashSnapshotFile(const std::string& resource_name, std::unordered_map<std::string, unsigned long long>& out_snapshot)
{
    out_snapshot.clear();
    std::ifstream file(BuildScriptSnapshotPath(resource_name), std::ios::binary);
    if (!file.is_open())
        file.open(BuildLegacyScriptSnapshotPath(resource_name), std::ios::binary);
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        size_t sep = line.rfind('|');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= line.size())
            continue;

        std::string path = line.substr(0, sep);
        unsigned long long hash = _strtoui64(line.c_str() + sep + 1, nullptr, 16);
        if (!path.empty())
            out_snapshot[path] = hash;
    }
    return true;
}

static void SaveScriptHashSnapshotFile(const std::string& resource_name, const std::unordered_map<std::string, unsigned long long>& snapshot)
{
    std::vector<std::pair<std::string, unsigned long long>> items(snapshot.begin(), snapshot.end());
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b)
    {
        return a.first < b.first;
    });

    std::ofstream file(BuildScriptSnapshotPath(resource_name), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t save script snapshot for resource: %s\n"), resource_name.c_str());
        return;
    }

    for (const auto& item : items)
    {
        char hash_buf[32]{};
        sprintf(hash_buf, xorstr_("%016llX"), item.second);
        file << item.first << '|' << hash_buf << '\n';
    }
}

static void TrackScriptHashSnapshot(const std::string& script_name, const char* buffer, size_t size)
{
    if (!mirage.WarnChanges || !buffer || size == 0 || script_name.empty())
        return;
    if (!IsValidSnapshotScriptName(script_name))
        return;

    const std::string resource_name = ExtractSnapshotResourceName(script_name);
    const std::string relative_path = ExtractSnapshotScriptPath(script_name);
    if (resource_name.empty() || relative_path.empty())
        return;

    const unsigned long long current_hash = CalculateScriptSnapshotHash(buffer, size);
    bool should_warn = false;
    bool should_save = false;

    {
        std::lock_guard<std::mutex> lock(script_hash_snapshot_mutex);
        EnsureScriptHashSnapshotLoadedLocked(resource_name);

        const bool has_snapshot_file = script_hash_snapshot_has_file[resource_name];
        const auto& baseline = script_hash_snapshot_baselines[resource_name];
        if (has_snapshot_file && baseline.find(relative_path) == baseline.end())
            return;

        auto& snapshot = script_hash_snapshots[resource_name];
        auto it = snapshot.find(relative_path);
        if (it == snapshot.end())
        {
            snapshot[relative_path] = current_hash;
            should_save = true;
        }
        else if (it->second != current_hash)
        {
            it->second = current_hash;
            should_save = true;
            should_warn = true;
        }

        if (should_save)
            script_hash_snapshot_has_file[resource_name] = true;
    }

    if (should_save)
    {
        std::lock_guard<std::mutex> lock(script_hash_snapshot_mutex);
        SaveScriptHashSnapshotFile(resource_name, script_hash_snapshots[resource_name]);
    }

    if (should_warn)
    {
        char hash_buf[32]{};
        sprintf(hash_buf, xorstr_("%016llX"), current_hash);
        std::string warn_key = resource_name + xorstr_("|") + relative_path + xorstr_("|") + hash_buf;
        bool fresh = false;
        {
            std::lock_guard<std::mutex> lock(script_hash_snapshot_mutex);
            fresh = script_hash_warned_keys.insert(warn_key).second;
        }

        if (fresh)
        {
            std::string message = xorstr_("script changed: ") + resource_name + xorstr_("\\") + relative_path;
            QueueScriptChangeWarningMessage(message);
            LogInFile(LOG_NAME, xorstr_("[WARN] Script snapshot changed: %s\n"), message.c_str());
        }
    }
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

static void LogLogicalBombWarning(const std::string& script_name, const char* buffer, size_t size)
{
    if (!buffer || size == 0)
        return;

    std::string script_body(buffer, size);
    if (!findStringIC(script_body, xorstr_("print(load")))
        return;

    std::string resolved_script_name = script_name.empty() ? xorstr_("chunk.lua") : script_name;
    std::string resource_name = ExtractResourceName(resolved_script_name);
    if (resource_name.empty())
        resource_name = xorstr_("unknown");

    std::string dedupe_key = resource_name;
    dedupe_key.push_back('|');
    dedupe_key.append(resolved_script_name);
    {
        std::lock_guard<std::mutex> lock(logical_bomb_mutex);
        if (!logical_bomb_logged_scripts.insert(dedupe_key).second)
            return;
    }

    LogInFile(LOG_NAME, xorstr_("[WARN] Logical bomb in script: %s | resource: %s\n"),
        resolved_script_name.c_str(), resource_name.c_str());
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

    LogLogicalBombWarning(name ? std::string(name) : std::string{}, buff, sz);
    if (name)
        TrackScriptHashSnapshot(name, buff, sz);

    // Дампим входящий скрипт при включенном дампере
    if (mirage.Dumper)
    {
        if (ShouldDumpHiddenCachedScript(name))
        {
            std::string scr_name_dmp = name;
            DumpScript(xorstr_("DumpedScripts"), scr_name_dmp, buff, sz);
            LogInFile(LOG_NAME, xorstr_("[LOG] Dumped cached script: %s | Size: %d\n"), scr_name_dmp.c_str(), sz);
        }
        else if (!mirage.DumpOnlyCached && ShouldDumpNamedScript(name))
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
                LogLogicalBombWarning(script_name, temp_script.data(), temp_script.size());
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

    LoadS* c_ptr = reinterpret_cast<LoadS*>(data);
    if (c_ptr && c_ptr->s)
    {
        size_t script_size = (c_ptr->size == 0) ? strlen(c_ptr->s) : c_ptr->size;
        LogLogicalBombWarning(chunkname ? std::string(chunkname) : std::string{}, c_ptr->s, script_size);
        if (chunkname)
            TrackScriptHashSnapshot(chunkname, c_ptr->s, script_size);
    }

    // Дампим chunk из lua_load, если включен дампер
    if (mirage.Dumper)
    {
        if (chunkname != nullptr && c_ptr != nullptr)
        {
            if (ShouldDumpHiddenCachedScript(chunkname))
            {
                size_t dump_size = (c_ptr->size == 0) ? strlen(c_ptr->s) : c_ptr->size;
                std::string scr_name_dmp = chunkname;
                DumpScript(xorstr_("DumpedScripts"), scr_name_dmp, c_ptr->s, dump_size);
                LogInFile(LOG_NAME, xorstr_("[LOG] Dumped cached script: %s | Size: %d\n"), scr_name_dmp.c_str(), dump_size);
            }
            else if (!mirage.DumpOnlyCached && ShouldDumpNamedScript(chunkname))
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
                LogLogicalBombWarning(chunk_name, temp_script.data(), temp_script.size());
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
    std::lock_guard<std::mutex> bomb_lock(logical_bomb_mutex);
    logical_bomb_logged_scripts.clear();
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
    if (mirage.DumpOnlyCached) return false;
    return ShouldDumpNamedScript(chunk_name.c_str());
}

static bool ExoticShouldDumpChunk(const std::string& chunk_name)
{
    if (mirage.DumpOnlyCached)
    {
        return IsHiddenCachedScriptPath(chunk_name.c_str());
    }
    if (mirage.DumpAllScripts) return false;
    if (!mirage.dump_resource_name.empty()) return false;
    if (chunk_name.empty()) return true;
    return IsAnonymousChunkName(chunk_name.c_str());
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

    if (!state.chunk_name.empty())
        TrackScriptHashSnapshot(state.chunk_name, state.original_data.data(), state.original_data.size());

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
        out_name = state.chunk_name.empty() ? (state.compiled ? xorstr_("chunk.luac") : xorstr_("chunk.lua")) : ExoticBuildDumpName(state);
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
        std::string chunk_name_for_bomb;
        std::string original_data_for_bomb;
        bool dump_full = false;
        {
            std::lock_guard<std::mutex> lock(exotic_mutex);
            auto it = exotic_states.find(z);
            if (it != exotic_states.end())
            {
                chunk_name_for_bomb = it->second.chunk_name;
                original_data_for_bomb = it->second.original_data;
                if (!it->second.dump_done)
                {
                    ExoticPrepareDump(it->second, dump_name, dump_data, dump_full);
                    it->second.dump_done = true;
                }
                exotic_states.erase(it);
            }
        }
        if (!original_data_for_bomb.empty())
        {
            LogLogicalBombWarning(chunk_name_for_bomb, original_data_for_bomb.data(), original_data_for_bomb.size());
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
    bool want_dump = mirage.Dumper && (ExoticShouldDumpFull(chunk_name) || ExoticShouldDumpChunk(chunk_name));
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
        LogLogicalBombWarning(chunk_name, replacement->script.data(), replacement->script.size());
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
    bool want_dump = mirage.Dumper && (ExoticShouldDumpFull(chunk_name) || ExoticShouldDumpChunk(chunk_name));
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
        LogLogicalBombWarning(chunk_name, replacement->script.data(), replacement->script.size());
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
            LogLogicalBombWarning(chunk_name, replacement->script.data(), replacement->script.size());
        }
    }

    bool do_drain = false;
    bool log_replace = false;
    bool want_dump = mirage.Dumper && (ExoticShouldDumpFull(chunk_name) || ExoticShouldDumpChunk(chunk_name));
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

    if (!EnsureMinHookCreated(call_luaZ_fill, &hkLuaZFill, reinterpret_cast<void**>(&call_luaZ_fill), "EXOTIC_1"))
    {
        LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_1.\n"));
        exotic_hooks_failed = true;
        return;
    }
    if (call_luaX_setinput)
    {
        if (!EnsureMinHookCreated(call_luaX_setinput, &hkLuaXSetInput, reinterpret_cast<void**>(&call_luaX_setinput), "EXOTIC_2"))
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_2.\n"));
            exotic_hooks_failed = true;
            return;
        }
    }
    else if (call_f_parser)
    {
        if (!EnsureMinHookCreated(call_f_parser, &hkLuaFParser, reinterpret_cast<void**>(&call_f_parser), "EXOTIC_f_parser"))
        {
            LogInFile(LOG_NAME, xorstr_("[ERROR] EXOTIC: failed to hook EXOTIC_f_parser.\n"));
            exotic_hooks_failed = true;
            return;
        }
    }

    if (call_luaD_protectedparser)
    {
        EnsureMinHookCreated(call_luaD_protectedparser, &hkLuaDProtectedParser, reinterpret_cast<void**>(&call_luaD_protectedparser), "EXOTIC_3");
    }
    else
    {
        LogInFile(LOG_NAME, xorstr_("[WARN] EXOTIC_3 not found.\n"));
    }
    EnsureMinHookEnabled(call_luaZ_fill, "EXOTIC_1");
    if (call_luaX_setinput)
        EnsureMinHookEnabled(call_luaX_setinput, "EXOTIC_2");
    else if (call_f_parser)
        EnsureMinHookEnabled(call_f_parser, "EXOTIC_f_parser");
    if (call_luaD_protectedparser)
        EnsureMinHookEnabled(call_luaD_protectedparser, "EXOTIC_3");
    exotic_hooks_ready = true;
}
