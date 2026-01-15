#pragma once

int __cdecl antiMirage(void* luaVM)
{
	RemoveDirRecursive(mapped_image_dir);
	Nirmata::UseNirmata();
	call_pushboolean(luaVM, true);
	return 1;
}

int __cdecl invokeFunction(void* luaVM)
{
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
	if (findStringIC(func_name, xorstr_("setPedHP")))
	{
		call_lua_remove(luaVM, 1);
		std::string pedHP = call_tostring(luaVM, 1, &strLen);
		bool armor = call_toboolean(luaVM, 2);
		DWORD PEDSELF = *(DWORD*)0xB6F5F0;
		if (!armor) *(float*)(PEDSELF + 0x540) = (float)std::stoi(pedHP);
		else *(float*)(PEDSELF + 0x548) = (float)std::stoi(pedHP);
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
