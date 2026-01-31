#pragma once

#include <wininet.h>
#pragma comment(lib, "Wininet.lib")

static inline bool CloseLuaFileHandle(int id);
bool IsFileExists(std::string file_path);

int __cdecl antiMirage(void* luaVM)
{
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
	if (findStringIC(func_name, xorstr_("sendInOutRequest")))
	{
		call_lua_remove(luaVM, 1);
		bool rslm = sendInOutRequest(luaVM);
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
