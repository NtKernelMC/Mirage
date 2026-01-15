#pragma once

#include <wininet.h>
#pragma comment(lib, "Wininet.lib")

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
