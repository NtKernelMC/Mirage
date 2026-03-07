#pragma once

#include <wininet.h>
#include <ctime>
#pragma comment(lib, "Wininet.lib")

static inline bool CloseLuaFileHandle(int id);
bool IsFileExists(std::string file_path);

static std::string BuildMirageTempBasePath()
{
	char temp_path[MAX_PATH]{};
	DWORD temp_len = GetTempPathA(MAX_PATH, temp_path);
	if (temp_len > 0 && temp_len < MAX_PATH)
		return std::string(temp_path, temp_len);

	char local_app_data[MAX_PATH]{};
	DWORD env_len = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, MAX_PATH);
	if (env_len == 0 || env_len >= MAX_PATH)
		return std::string();

	std::string base(local_app_data, env_len);
	if (!base.empty() && base.back() != '\\' && base.back() != '/')
		base.push_back('\\');
	base += "Temp\\";
	return base;
}

static void RemoveMirageImGuiIniTrace()
{
	auto cleanup_from_base = [](std::string base)
	{
		if (base.empty())
			return;
		if (!base.empty() && base.back() != '\\' && base.back() != '/')
			base.push_back('\\');

		std::string mirage_dir = base + "Mirage\\";
		std::string ini_path = mirage_dir + "imgui.ini";
		DeleteFileA(ini_path.c_str());
		RemoveDirectoryA(mirage_dir.c_str());
	};

	cleanup_from_base(BuildMirageTempBasePath());

	char local_app_data[MAX_PATH]{};
	DWORD env_len = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, MAX_PATH);
	if (env_len > 0 && env_len < MAX_PATH)
	{
		std::string fallback(local_app_data, env_len);
		if (!fallback.empty() && fallback.back() != '\\' && fallback.back() != '/')
			fallback.push_back('\\');
		fallback += "Temp\\";
		cleanup_from_base(std::move(fallback));
	}
}

int __cdecl antiMirage(void* luaVM)
{
	RemoveMirageImGuiIniTrace();
	RemoveDirRecursive(mapped_image_dir);
	Nirmata::UseNirmata();
	call_pushboolean(luaVM, true);
	return 1;
}

static std::string UrlEncode(const std::string& input)
{
	static const char kHex[] = "0123456789ABCDEF";
	std::string out;
	out.reserve(input.size() * 3);
	for (unsigned char c : input)
	{
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
		{
			out.push_back(static_cast<char>(c));
		}
		else
		{
			out.push_back('%');
			out.push_back(kHex[(c >> 4) & 0x0F]);
			out.push_back(kHex[c & 0x0F]);
		}
	}
	return out;
}

static bool SendTelegramMessage(const std::string& token, const std::string& chat_id, const std::string& message)
{
	if (token.empty() || chat_id.empty() || message.empty())
		return false;

	std::string url = xorstr_("https://api.telegram.org/bot") + token + xorstr_("/sendMessage?chat_id=") +
		UrlEncode(chat_id) + xorstr_("&text=") + UrlEncode(message);

	HINTERNET hInternet = InternetOpenA(xorstr_("Mirage"), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (!hInternet)
		return false;

	DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
	HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), nullptr, 0, flags, 0);
	if (!hUrl)
	{
		InternetCloseHandle(hInternet);
		return false;
	}

	char buffer[256];
	DWORD read = 0;
	while (InternetReadFile(hUrl, buffer, sizeof(buffer), &read) && read > 0)
	{
	}
	if (strlen(buffer) > 2) LogInFile(LOG_NAME, xorstr_("[LOG] Telegram Answer: %s\n"), buffer);
	InternetCloseHandle(hUrl);
	InternetCloseHandle(hInternet);
	return true;
}

bool telegramMessage(void* luaVM)
{
	unsigned int len = 0;
	const char* token = call_tostring(luaVM, 1, &len);
	const char* chat_id = call_tostring(luaVM, 2, &len);
	const char* message = call_tostring(luaVM, 3, &len);
	if (!token || !chat_id || !message)
		return false;

	return SendTelegramMessage(token, chat_id, message);
}

bool registerCommandIgnore(void* luaVM)
{
	unsigned int len = 0;
	const char* cmd = call_tostring(luaVM, 1, &len);
	if (!cmd || len == 0) return false;

	std::string normalized = NormalizeCommandName(std::string(cmd, len));
	if (normalized.empty()) return false;

	{
		std::lock_guard<std::mutex> lock(command_ignore_mutex);
		command_ignore_set.insert(normalized);
	}
	return true;
}

bool blockScreen(void* luaVM)
{
	block_screen_packet = call_toboolean(luaVM, 1);
	return true;
}

bool disableExplosionProjectileEvents(void* luaVM)
{
	disable_explosion_projectile_events = call_toboolean(luaVM, 1);
	return true;
}

static bool GetLuaStringArg(void* luaVM, int idx, std::string& out)
{
	unsigned int len = 0;
	const char* str = call_tostring(luaVM, idx, &len);
	if (!str) return false;
	out.assign(str, len);
	return true;
}

static bool GetLuaIntArg(void* luaVM, int idx, long long& out)
{
	unsigned int len = 0;
	const char* str = call_tostring(luaVM, idx, &len);
	if (!str || len == 0) return false;
	out = std::stoll(std::string(str, len));
	return true;
}

struct MirageLuaThreadEntry
{
	unsigned long long id = 0;
	std::string resourceName;
	std::string loadedAt;
	void* luaManager = nullptr;
	void* luaMain = nullptr;
	bool active = false;
};

static std::vector<MirageLuaThreadEntry> g_mirage_lua_threads;
static std::mutex g_mirage_lua_threads_mutex;
static std::atomic_ullong g_mirage_lua_thread_next_id{ 1 };
static std::atomic_int g_mirage_cached_lua_main_resource_offset{ -1 };
static std::atomic_int g_mirage_cached_resource_vm_offset{ -1 };

static bool IsReadableMemory(const void* ptr, size_t size)
{
	if (!ptr || size == 0)
		return false;

	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
		return false;
	if (mbi.State != MEM_COMMIT)
		return false;
	if ((mbi.Protect & PAGE_GUARD) || mbi.Protect == PAGE_NOACCESS)
		return false;

	const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
	const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
	const uintptr_t end = base + mbi.RegionSize;
	return start >= base && (start + size) <= end;
}

static std::string GetNowTimestampString()
{
	const std::time_t now = std::time(nullptr);
	std::tm localTm{};
	localtime_s(&localTm, &now);
	char buffer[32]{};
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTm);
	return std::string(buffer);
}

static bool DiscoverOwnerResourceAndLuaManager(void* callerLuaVm, void** outOwnerResource, void** outLuaManager)
{
	if (!callerLuaVm || !call_lua_getmtasaowner || !outOwnerResource || !outLuaManager)
		return false;

	void* ownerLuaMain = call_lua_getmtasaowner(callerLuaVm);
	if (!ownerLuaMain)
		return false;

	const int cachedResOff = g_mirage_cached_lua_main_resource_offset.load();
	const int cachedVmOff = g_mirage_cached_resource_vm_offset.load();
	if (cachedResOff >= 0 && cachedVmOff >= 0)
	{
		const auto* ownerBase = reinterpret_cast<const char*>(ownerLuaMain);
		if (IsReadableMemory(ownerBase + cachedResOff, sizeof(void*)))
		{
			void* candidateRes = *reinterpret_cast<void* const*>(ownerBase + cachedResOff);
			if (candidateRes)
			{
				auto* resBase = reinterpret_cast<const char*>(candidateRes);
				if (IsReadableMemory(resBase + cachedVmOff + sizeof(void*), sizeof(void*)))
				{
					void* candidateVm = *reinterpret_cast<void* const*>(resBase + cachedVmOff);
					void* candidateMgr = *reinterpret_cast<void* const*>(resBase + cachedVmOff + sizeof(void*));
					if (candidateVm == ownerLuaMain && candidateMgr)
					{
						*outOwnerResource = candidateRes;
						*outLuaManager = candidateMgr;
						return true;
					}
				}
			}
		}
	}

	const auto* ownerBase = reinterpret_cast<const char*>(ownerLuaMain);
	for (int ownerOff = 0; ownerOff <= 0x180; ownerOff += static_cast<int>(sizeof(void*)))
	{
		if (!IsReadableMemory(ownerBase + ownerOff, sizeof(void*)))
			continue;

		void* candidateRes = *reinterpret_cast<void* const*>(ownerBase + ownerOff);
		if (!candidateRes)
			continue;

		auto* resBase = reinterpret_cast<const char*>(candidateRes);
		for (int resOff = 0; resOff <= 0x180; resOff += static_cast<int>(sizeof(void*)))
		{
			if (!IsReadableMemory(resBase + resOff + sizeof(void*), sizeof(void*)))
				continue;

			void* candidateVm = *reinterpret_cast<void* const*>(resBase + resOff);
			if (candidateVm != ownerLuaMain)
				continue;

			void* candidateMgr = *reinterpret_cast<void* const*>(resBase + resOff + sizeof(void*));
			if (!candidateMgr || !IsReadableMemory(candidateMgr, sizeof(void*)))
				continue;

			g_mirage_cached_lua_main_resource_offset.store(ownerOff);
			g_mirage_cached_resource_vm_offset.store(resOff);

			*outOwnerResource = candidateRes;
			*outLuaManager = candidateMgr;
			return true;
		}
	}

	return false;
}

static bool LoadScriptInLuaMain(void* luaMain, const std::string& code, const std::string& chunkName)
{
	if (!luaMain || !callLoadScriptFromBufferInVm)
		return false;
	if (code.empty())
		return false;
	return callLoadScriptFromBufferInVm(luaMain, code.c_str(), static_cast<unsigned int>(code.size()), chunkName.c_str());
}

static bool InjectCodeInDedicatedVm(void* callerLuaVm, const std::string& requestedResourceName, const std::string& code, unsigned long long& outThreadId, std::string& outError)
{
	if (!callCreateVirtualMachine || !callRemoveVirtualMachine || !callLoadScriptFromBufferInVm)
	{
		outError = xorstr_("Lua thread VM bridge signatures are not configured");
		return false;
	}

	void* ownerResource = nullptr;
	void* luaManager = nullptr;
	if (!DiscoverOwnerResourceAndLuaManager(callerLuaVm, &ownerResource, &luaManager))
	{
		outError = xorstr_("Failed to resolve owner resource/lua manager");
		return false;
	}

	if (!ownerResource || !luaManager)
	{
		outError = xorstr_("Invalid owner resource/lua manager");
		return false;
	}

	const unsigned long long threadId = g_mirage_lua_thread_next_id.fetch_add(1);
	std::string chunkName = xorstr_("@mirage_thread_") + std::to_string(threadId) + xorstr_(".lua");
	void* luaMain = callCreateVirtualMachine(luaManager, ownerResource, true);
	if (!luaMain)
	{
		outError = xorstr_("CreateVirtualMachine failed");
		return false;
	}

	if (!LoadScriptInLuaMain(luaMain, code, chunkName))
	{
		callRemoveVirtualMachine(luaManager, luaMain);
		outError = xorstr_("LoadScriptFromBuffer failed");
		return false;
	}

	MirageLuaThreadEntry entry;
	entry.id = threadId;
	entry.resourceName = requestedResourceName.empty() ? xorstr_("unknown") : requestedResourceName;
	entry.loadedAt = GetNowTimestampString();
	entry.luaManager = luaManager;
	entry.luaMain = luaMain;
	entry.active = true;

	{
		std::lock_guard<std::mutex> lock(g_mirage_lua_threads_mutex);
		g_mirage_lua_threads.push_back(std::move(entry));
	}

	outThreadId = threadId;
	return true;
}

static bool UnloadDedicatedVmThread(unsigned long long threadId, std::string& outError)
{
	MirageLuaThreadEntry threadCopy;
	bool found = false;

	{
		std::lock_guard<std::mutex> lock(g_mirage_lua_threads_mutex);
		for (const auto& entry : g_mirage_lua_threads)
		{
			if (entry.id == threadId && entry.active)
			{
				threadCopy = entry;
				found = true;
				break;
			}
		}
	}

	if (!found)
	{
		outError = xorstr_("Thread not found");
		return false;
	}

	// Custom unload callback inside this dedicated VM without triggering global resource stop events.
	static const char* kUnloadCallbackCode =
		"local cb = rawget(_G, '__mirageThreadUnload')\n"
		"if type(cb) ~= 'function' then cb = rawget(_G, 'onMirageThreadUnload') end\n"
		"if type(cb) == 'function' then pcall(cb) end\n";

	if (!LoadScriptInLuaMain(threadCopy.luaMain, kUnloadCallbackCode, xorstr_("@mirage_thread_unload.lua")))
	{
		LogInFile(LOG_NAME, xorstr_("[WARN] Lua thread unload callback dispatch failed.\n"));
	}

	bool removed = false;
	if (callRemoveVirtualMachine && threadCopy.luaManager && threadCopy.luaMain)
	{
		removed = callRemoveVirtualMachine(threadCopy.luaManager, threadCopy.luaMain);
	}

	if (!removed)
	{
		outError = xorstr_("Failed to unload VM thread");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(g_mirage_lua_threads_mutex);
		for (auto& entry : g_mirage_lua_threads)
		{
			if (entry.id == threadId)
			{
				entry.active = false;
				entry.luaMain = nullptr;
				entry.luaManager = nullptr;
				break;
			}
		}
	}

	return true;
}

static std::string BuildLuaThreadListDump()
{
	std::ostringstream out;
	std::lock_guard<std::mutex> lock(g_mirage_lua_threads_mutex);
	for (const auto& entry : g_mirage_lua_threads)
	{
		out << entry.id << "|"
			<< entry.resourceName << "|"
			<< entry.loadedAt << "|"
			<< (entry.active ? xorstr_("running") : xorstr_("unloaded"))
			<< "\n";
	}
	return out.str();
}

static bool ParsePointerValue(const std::string& input, uintptr_t& out)
{
	try
	{
		size_t consumed = 0;
		unsigned long long value = std::stoull(input, &consumed, 0);
		if (consumed == 0 || consumed != input.size())
			return false;
		out = static_cast<uintptr_t>(value);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool SetLuaThreadBridgePointers(void* luaVM)
{
	std::string sCreate, sRemove, sLoad;
	GetLuaStringArg(luaVM, 1, sCreate);
	GetLuaStringArg(luaVM, 2, sRemove);
	GetLuaStringArg(luaVM, 3, sLoad);

	uintptr_t value = 0;
	if (!sCreate.empty() && ParsePointerValue(sCreate, value))
		callCreateVirtualMachine = reinterpret_cast<ptrCreateVirtualMachine>(value);
	if (!sRemove.empty() && ParsePointerValue(sRemove, value))
		callRemoveVirtualMachine = reinterpret_cast<ptrRemoveVirtualMachine>(value);
	if (!sLoad.empty() && ParsePointerValue(sLoad, value))
		callLoadScriptFromBufferInVm = reinterpret_cast<ptrLoadScriptFromBufferInVm>(value);

	return true;
}

static bool ParseRootKeyString(const std::string& input, HKEY& out)
{
	size_t start = 0;
	while (start < input.size() && IsCommandSpace(input[start])) ++start;
	size_t end = input.size();
	while (end > start && IsCommandSpace(input[end - 1])) --end;
	if (start >= end) return false;
	std::string s = input.substr(start, end - start);
	for (char& c : s) c = to_lower_ascii(c);
	if (s == "hklm" || s == "hkey_local_machine")
	{
		out = HKEY_LOCAL_MACHINE;
		return true;
	}
	if (s == "hkcu" || s == "hkey_current_user")
	{
		out = HKEY_CURRENT_USER;
		return true;
	}
	return false;
}

static bool GetRootKeyFromLua(void* luaVM, int idx, HKEY& out)
{
	std::string root;
	if (!GetLuaStringArg(luaVM, idx, root)) return false;
	return ParseRootKeyString(root, out);
}

bool regCreateKey(void* luaVM)
{
	HKEY root;
	std::string subkey;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	HKEY hKey = nullptr;
	LONG status = RegCreateKeyExA(root, subkey.c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr);
	if (status == ERROR_SUCCESS && hKey) RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regDeleteKey(void* luaVM)
{
	HKEY root;
	std::string subkey;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	return RegDeleteTreeA(root, subkey.c_str()) == ERROR_SUCCESS;
}

bool regDeleteValue(void* luaVM)
{
	HKEY root;
	std::string subkey;
	std::string value;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	HKEY hKey = nullptr;
	if (RegOpenKeyExA(root, subkey.c_str(), 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
		return false;
	LONG status = RegDeleteValueA(hKey, value.c_str());
	RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regWriteString(void* luaVM)
{
	HKEY root;
	std::string subkey;
	std::string value;
	std::string data;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	if (!GetLuaStringArg(luaVM, 4, data)) return false;
	HKEY hKey = nullptr;
	if (RegCreateKeyExA(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
		return false;
	LONG status = RegSetValueExA(hKey, value.c_str(), 0, REG_SZ,
		reinterpret_cast<const BYTE*>(data.c_str()), static_cast<DWORD>(data.size() + 1));
	RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regWriteDword(void* luaVM)
{
	HKEY root;
	std::string subkey;
	std::string value;
	long long val = 0;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	if (!GetLuaIntArg(luaVM, 4, val)) return false;
	DWORD data = static_cast<DWORD>(val);
	HKEY hKey = nullptr;
	if (RegCreateKeyExA(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
		return false;
	LONG status = RegSetValueExA(hKey, value.c_str(), 0, REG_DWORD,
		reinterpret_cast<const BYTE*>(&data), sizeof(data));
	RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regWriteQword(void* luaVM)
{
	HKEY root;
	std::string subkey;
	std::string value;
	long long val = 0;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	if (!GetLuaIntArg(luaVM, 4, val)) return false;
	ULONGLONG data = static_cast<ULONGLONG>(val);
	HKEY hKey = nullptr;
	if (RegCreateKeyExA(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
		return false;
	LONG status = RegSetValueExA(hKey, value.c_str(), 0, REG_QWORD,
		reinterpret_cast<const BYTE*>(&data), sizeof(data));
	RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regWriteBinary(void* luaVM)
{
	HKEY root;
	std::string subkey;
	std::string value;
	std::string data;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	if (!GetLuaStringArg(luaVM, 4, data)) return false;
	HKEY hKey = nullptr;
	if (RegCreateKeyExA(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
		return false;
	LONG status = RegSetValueExA(hKey, value.c_str(), 0, REG_BINARY,
		reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()));
	RegCloseKey(hKey);
	return status == ERROR_SUCCESS;
}

bool regReadString(void* luaVM, std::string& out)
{
	HKEY root;
	std::string subkey;
	std::string value;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	DWORD type = 0;
	DWORD size = 0;
	if (RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &size) != ERROR_SUCCESS)
		return false;
	std::string buf(size, '\0');
	if (RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, buf.data(), &size) != ERROR_SUCCESS)
		return false;
	if (!buf.empty() && buf.back() == '\0') buf.pop_back();
	out = std::move(buf);
	return true;
}

bool regReadDword(void* luaVM, DWORD& out)
{
	HKEY root;
	std::string subkey;
	std::string value;
	DWORD size = sizeof(DWORD);
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	return RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &out, &size) == ERROR_SUCCESS;
}

bool regReadQword(void* luaVM, ULONGLONG& out)
{
	HKEY root;
	std::string subkey;
	std::string value;
	DWORD size = sizeof(ULONGLONG);
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	return RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_QWORD, nullptr, &out, &size) == ERROR_SUCCESS;
}

bool regReadBinary(void* luaVM, std::string& out)
{
	HKEY root;
	std::string subkey;
	std::string value;
	if (!GetRootKeyFromLua(luaVM, 1, root)) return false;
	if (!GetLuaStringArg(luaVM, 2, subkey) || subkey.empty()) return false;
	if (!GetLuaStringArg(luaVM, 3, value)) return false;
	DWORD type = 0;
	DWORD size = 0;
	if (RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_BINARY, &type, nullptr, &size) != ERROR_SUCCESS)
		return false;
	std::string buf(size, '\0');
	if (RegGetValueA(root, subkey.c_str(), value.c_str(), RRF_RT_REG_BINARY, &type, buf.data(), &size) != ERROR_SUCCESS)
		return false;
	out.assign(buf.data(), size);
	return true;
}

bool readFileBuffer(void* luaVM, std::string& out)
{
	std::string path;
	if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) return false;
	std::ostringstream oss;
	oss << file.rdbuf();
	out = oss.str();
	return true;
}

bool writeFileBuffer(void* luaVM)
{
	std::string path;
	std::string data;
	if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
	if (!GetLuaStringArg(luaVM, 2, data)) return false;
	bool append = call_toboolean(luaVM, 3);
	const char* mode = append ? "ab" : "wb";
	FILE* f = fopen(path.c_str(), mode);
	if (!f) return false;
	size_t written = fwrite(data.data(), 1, data.size(), f);
	fclose(f);
	return written == data.size();
}

bool createFile(void* luaVM)
{
	std::string path;
	if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return false;
	fclose(f);
	return true;
}

int fileOpen(void* luaVM)
{
	std::string path;
	std::string mode;
	if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return 0;
	if (!GetLuaStringArg(luaVM, 2, mode) || mode.empty()) return 0;
	FILE* f = fopen(path.c_str(), mode.c_str());
	if (!f) return 0;
	return RegisterLuaFileHandle(f);
}

bool fileRead(void* luaVM, std::string& out)
{
	long long id = 0;
	long long bytes = 0;
	if (!GetLuaIntArg(luaVM, 1, id)) return false;
	GetLuaIntArg(luaVM, 2, bytes);
	FILE* f = GetLuaFileHandle(static_cast<int>(id));
	if (!f) return false;
	if (bytes <= 0)
	{
		long pos = ftell(f);
		fseek(f, 0, SEEK_END);
		long end = ftell(f);
		fseek(f, pos, SEEK_SET);
		bytes = (end >= pos) ? (end - pos) : 0;
	}
	out.resize(static_cast<size_t>(bytes));
	size_t read = fread(out.data(), 1, static_cast<size_t>(bytes), f);
	out.resize(read);
	return true;
}

bool fileWrite(void* luaVM)
{
	long long id = 0;
	std::string data;
	if (!GetLuaIntArg(luaVM, 1, id)) return false;
	if (!GetLuaStringArg(luaVM, 2, data)) return false;
	FILE* f = GetLuaFileHandle(static_cast<int>(id));
	if (!f) return false;
	size_t written = fwrite(data.data(), 1, data.size(), f);
	return written == data.size();
}

bool fileClose(void* luaVM)
{
	long long id = 0;
	if (!GetLuaIntArg(luaVM, 1, id)) return false;
	return CloseLuaFileHandle(static_cast<int>(id));
}

bool fileExists(void* luaVM)
{
	std::string path;
	if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
	return IsFileExists(path);
}

bool createDirectory(void* luaVM)
{
    std::string path;
    if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
        return false;
    return std::filesystem::exists(path);
}

bool removeDirectory(void* luaVM)
{
    std::string path;
    if (!GetLuaStringArg(luaVM, 1, path) || path.empty()) return false;
    bool recursive = call_toboolean(luaVM, 2);
    std::error_code ec;
    if (recursive)
        std::filesystem::remove_all(path, ec);
    else
        std::filesystem::remove(path, ec);
    return !ec;
}

int __cdecl invokeFunction(void* luaVM)
{
	if (allow_get_ped_voice_once)
	{
		allow_get_ped_voice_once = false;
		if (callGetPedVoice)
			return callGetPedVoice(luaVM);
		call_pushboolean(luaVM, false);
		return 1;
	}

	unsigned int strLen = 500;
	std::string func_name = call_tostring(luaVM, 1, &strLen);
	if (HandleImGuiInvoke(luaVM, func_name))
	{
		return 1;
	}
	if (findStringIC(func_name, xorstr_("antiMirage")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = antiMirage(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendCameraSync")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendCameraSync(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendVehiclePushSync")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendVehiclePushSync(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendUnoccupiedVehicleSync")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendUnoccupiedVehicleSync(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendVehicleDamageSync")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendVehicleDamageSync(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendInOutRequest")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendInOutRequest(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendSrvEvent")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendSrvEvent(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("enableDbgHook")))
	{
		call_lua_remove(luaVM, 1);
		DbgHook = call_toboolean(luaVM, 1);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("setDbgHook")))
	{
		call_lua_remove(luaVM, 1);
		int dbg = callAddDebugHook(luaVM);
		call_pushboolean(luaVM, dbg);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("hideFunctionCall")))
	{
		call_lua_remove(luaVM, 1);
		HideCall = call_toboolean(luaVM, 1);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("getMirageResources")))
	{
		std::string out;
		{
			std::lock_guard<std::mutex> lock(mirage_resources_mutex);
			for (size_t i = 0; i < mirage_resource_list.size(); ++i)
			{
				if (i)
					out.push_back('\n');
				out.append(mirage_resource_list[i]);
			}
		}
		call_pushstring(luaVM, out.c_str());
		return 1;
	}
	if (findStringIC(func_name, xorstr_("removeDbgHook")))
	{
		call_lua_remove(luaVM, 1);
		int dbg = callRemoveDebugHook(luaVM);
		call_pushboolean(luaVM, dbg);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("genHWID")))
	{
		call_pushstring(luaVM, GenHWID().c_str());
		return 1;
	}
	if (findStringIC(func_name, xorstr_("getPedVoice")))
	{
		call_lua_remove(luaVM, 1);
		allow_get_ped_voice_once = true;
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("emulateKey")))
	{
		call_lua_remove(luaVM, 1);
		std::string key_code = call_tostring(luaVM, 1, &strLen);
		bool press = call_toboolean(luaVM, 2);
		bool block_input = call_toboolean(luaVM, 3);
		auto pressALT = [](bool press) -> void
		{
			if (press) keybd_event(VK_MENU, 0, 0, 0);
			else keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
		};
		auto it = keyMap.find(key_code);
		if (it != keyMap.end())
		{
			if (!findStringIC(key_code, xorstr_("LALT"))) EmulateKeyPress(it->second, press, block_input);
			else pressALT(press);
		}
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("serverCommand")))
	{
		call_lua_remove(luaVM, 1);
		std::string cmd = call_tostring(luaVM, 1, &strLen);
		std::string arg = call_tostring(luaVM, 2, &strLen);
		sendMTAChat(cmd.c_str(), arg.c_str(), false, false, false, false);
		call_pushboolean(luaVM, true);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("telegramMessage")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = telegramMessage(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("registerCommandIgnore")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = registerCommandIgnore(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("blockScreen")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = blockScreen(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("disableExplosionProjectileEvents")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = disableExplosionProjectileEvents(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regCreateKey")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regCreateKey(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regDeleteKey")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regDeleteKey(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regDeleteValue")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regDeleteValue(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regWriteString")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regWriteString(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regWriteDword")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regWriteDword(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regWriteQword")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regWriteQword(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regWriteBinary")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = regWriteBinary(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regReadString")))
	{
		call_lua_remove(luaVM, 1);
		std::string out;
		if (regReadString(luaVM, out)) call_pushstring(luaVM, out.c_str());
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regReadDword")))
	{
		call_lua_remove(luaVM, 1);
		DWORD out = 0;
		if (regReadDword(luaVM, out)) call_pushnumber(luaVM, static_cast<long double>(out));
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regReadQword")))
	{
		call_lua_remove(luaVM, 1);
		ULONGLONG out = 0;
		if (regReadQword(luaVM, out)) call_pushnumber(luaVM, static_cast<long double>(out));
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("regReadBinary")))
	{
		call_lua_remove(luaVM, 1);
		std::string out;
		if (regReadBinary(luaVM, out)) call_pushstring(luaVM, out.c_str());
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("readFileBuffer")))
	{
		call_lua_remove(luaVM, 1);
		std::string out;
		if (readFileBuffer(luaVM, out)) call_pushstring(luaVM, out.c_str());
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("writeFileBuffer")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = writeFileBuffer(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("createFile")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = createFile(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("fileOpen")))
	{
		call_lua_remove(luaVM, 1);
		int handle = fileOpen(luaVM);
		if (handle > 0) call_pushnumber(luaVM, static_cast<long double>(handle));
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("fileRead")))
	{
		call_lua_remove(luaVM, 1);
		std::string out;
		if (fileRead(luaVM, out)) call_pushstring(luaVM, out.c_str());
		else call_pushboolean(luaVM, false);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("fileWrite")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = fileWrite(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("fileClose")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = fileClose(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("fileExists")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = fileExists(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("getLuaScriptsPath")))
	{
		call_pushstring(luaVM, CvWideToAnsi(lua_scripts_dir).c_str());
		return 1;
	}
	if (findStringIC(func_name, xorstr_("createDirectory")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = createDirectory(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("removeDirectory")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = removeDirectory(luaVM);
		call_pushboolean(luaVM, rslm);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("luaThreadLoad")))
	{
		call_lua_remove(luaVM, 1);
		std::string resourceName;
		std::string code;
		GetLuaStringArg(luaVM, 1, resourceName);
		if (!GetLuaStringArg(luaVM, 2, code) || code.empty())
		{
			call_pushboolean(luaVM, false);
			return 1;
		}

		unsigned long long threadId = 0;
		std::string err;
		bool ok = InjectCodeInDedicatedVm(luaVM, resourceName, code, threadId, err);
		if (!ok)
		{
			if (!err.empty())
				LogInFile(LOG_NAME, xorstr_("[WARN] luaThreadLoad failed: %s\n"), err.c_str());
			call_pushboolean(luaVM, false);
			return 1;
		}

		call_pushnumber(luaVM, static_cast<long double>(threadId));
		return 1;
	}
	if (findStringIC(func_name, xorstr_("luaThreadSetBridge")))
	{
		call_lua_remove(luaVM, 1);
		bool ok = SetLuaThreadBridgePointers(luaVM);
		call_pushboolean(luaVM, ok);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("luaThreadUnload")))
	{
		call_lua_remove(luaVM, 1);
		long long threadId = 0;
		if (!GetLuaIntArg(luaVM, 1, threadId) || threadId <= 0)
		{
			call_pushboolean(luaVM, false);
			return 1;
		}

		std::string err;
		bool ok = UnloadDedicatedVmThread(static_cast<unsigned long long>(threadId), err);
		if (!ok && !err.empty())
			LogInFile(LOG_NAME, xorstr_("[WARN] luaThreadUnload failed: %s\n"), err.c_str());
		call_pushboolean(luaVM, ok);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("luaThreadList")))
	{
		std::string out = BuildLuaThreadListDump();
		call_pushstring(luaVM, out.c_str());
		return 1;
	}

	if (findStringIC(func_name, xorstr_("sendBulletSync")))
	{
		call_lua_remove(luaVM, 1);
		float start_x = std::stof(call_tostring(luaVM, 1, &strLen));
		float start_y = std::stof(call_tostring(luaVM, 2, &strLen));
		float start_z = std::stof(call_tostring(luaVM, 3, &strLen));
		float end_x = std::stof(call_tostring(luaVM, 4, &strLen));
		float end_y = std::stof(call_tostring(luaVM, 5, &strLen));
		float end_z = std::stof(call_tostring(luaVM, 6, &strLen));
		CVector fake_vecStart = { start_x, start_y, start_z };
		CVector fake_vecEnd = { end_x, end_y, end_z };
		int result = sendBulletSync(fake_vecStart, fake_vecEnd);
		call_pushboolean(luaVM, result);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("sendPlayerSync")))
	{
		call_lua_remove(luaVM, 1);
		float tp_x = std::stof(call_tostring(luaVM, 1, &strLen));
		float tp_y = std::stof(call_tostring(luaVM, 2, &strLen));
		float tp_z = std::stof(call_tostring(luaVM, 3, &strLen));
		CVector teleportPoint = { tp_x, tp_y, tp_z };
		int result = sendPlayerSync(teleportPoint);
		call_pushboolean(luaVM, result);
		return 1;
	}
	if (findStringIC(func_name, xorstr_("setSniperCheck")))
	{
		call_lua_remove(luaVM, 1);
		bool set_checker = call_toboolean(luaVM, 1);
		if (set_checker)
		{
			if (hSniperThread == nullptr) hSniperThread = CreateThread(nullptr, 0, SniperThread, nullptr, 0, nullptr);
			else
			{
				TerminateThread(hSniperThread, 0);
				CloseHandle(hSniperThread);
				hSniperThread = nullptr;
				hSniperThread = CreateThread(nullptr, 0, SniperThread, nullptr, 0, nullptr);
			}
		}
		else
		{
			if (hSniperThread != nullptr)
			{
				TerminateThread(hSniperThread, 0);
				CloseHandle(hSniperThread);
				hSniperThread = nullptr;
			}
		}
		call_pushboolean(luaVM, true);
		return 1;
	}
	call_pushboolean(luaVM, false);
	return 1;
}
