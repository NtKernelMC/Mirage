namespace LegacyBypass
{
    typedef bool(__thiscall* ptrSendPacket)(void* ECX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
    ptrSendPacket callSendPacket = nullptr;
    bool __fastcall SendPacket(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering)
    {
		CNetAPI = ECX;
		g_pNet = (CNet*)ECX;
		if (ucPacketID == 91) return true;
		if (ucPacketID == PACKET_ID_VOICE_DATA && cursed_voice)
		{
			NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();

			if (pBitStream)
			{
				unsigned short uiBytesWritten = 34900;
				static char bufTempOutput[34900];
				static bool once_writer = false;
				
				if (!once_writer)
				{
					for (int i = 0; i < 34900; i++)
					{
						bufTempOutput[i] = 120;
					}

					once_writer = true;
				}
				pBitStream->Write(uiBytesWritten);
				pBitStream->Write((char*)bufTempOutput, uiBytesWritten);

				callSendPacket(ECX, PACKET_ID_VOICE_DATA, pBitStream, PACKET_PRIORITY_LOW, PACKET_RELIABILITY_RELIABLE_ORDERED, PACKET_ORDERING_VOICE);
				g_pNet->DeallocateNetBitStream(pBitStream);

			}
			return true;
		}
        bool rslt = callSendPacket(ECX, ucPacketID, bitStream, packetPriority, packetReliability, packetOrdering);
        return rslt;
    }
    void EvadeAnticheat()
    {
        SigScan scan; 
        if (callSendPacket == nullptr)
        {
            callSendPacket = (ptrSendPacket)scan.FindPattern(xorstr_("netc.dll"),
                xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\xF1\x89\xB5\x00\x00\x00\x00\x8B\x7D\x0C"),
                xorstr_("xxxxxx????xx????xxx????x????xxxxxxxxxxxxx????xxxx????xxx")); // 1.5.9
            if (callSendPacket != nullptr)
            {
                LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to SendPacket!\n"));
				MH_RemoveHook(callSendPacket);
				MH_CreateHook(callSendPacket, &SendPacket, reinterpret_cast<LPVOID*>(&callSendPacket));
				MH_EnableHook(MH_ALL_HOOKS);
			}
			else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for SendPacket.\n"));
        }
    }
};
namespace ModernBypass
{
	typedef bool(__thiscall* ptrSendPacket)(void* ECX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
	ptrSendPacket callSendPacket = nullptr;
	bool __fastcall SendPacket(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering)
	{
		CNetAPI = ECX;
		g_pNet = (CNet*)ECX;
		RestorePrologue((DWORD)callSendPacket, packet_prologue, sizeof(packet_prologue));
		auto color_name = magic_enum::enum_name((ePacketID)ucPacketID);
		//LogInFile(LOG_NAME, xorstr_("PacketID: %d | PacketName: %s\n"), ucPacketID, color_name.data());
		if ((ucPacketID >= 91 && ucPacketID <= 94 && ucPacketID != 93)) // || ucPacketID == 34
		{
			MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
			return true;
		}
		bool rslt = callSendPacket(ECX, ucPacketID, bitStream, packetPriority, packetReliability, packetOrdering);
		MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
		return rslt;
	}

	// Netc hooks
	typedef void(__thiscall* vpnbypass_t)(DWORD*, char);
	vpnbypass_t vpnbypass = nullptr;
	void __fastcall hkvpnbypass(DWORD* a1, DWORD EDX, char a2) {}
	bool __cdecl hkIsNetworkConnected(DWORD* a1, DWORD* a2) { return false; }
	bool __fastcall hkIsNotViolationCode(void* ECX, void*, unsigned int a2) { return true; }
	void __fastcall hkAcSecurityViolationKick(void* ECX, void*) {}
	int __fastcall hkSendClientKick(void* ECX, void*, char arg) { return 1; }
	typedef void(__cdecl* ptrSendReport)(int reportId, std::string* text, int size, int a2, int a3);
	ptrSendReport callSendReport = nullptr;
	void __cdecl hkSendReport(int reportId, std::string* text, int size, int a2, int a3) { return; }
	void __fastcall hkSetClientKick(void* ECX, void*, void** args, void** reason, bool a4, int a5) { return; }
	void __fastcall hkVfB00_Z00Scanner(int ECX, int, int a3) { return; }
	void __fastcall hkSetClientKickNew(int ECX, int, char a2) { return; }
	void __fastcall hkScanModuleIntegrity(int ECX, int, DWORD* a3) { return; }
	void EvadeAnticheat()
	{
		SigScan scan; MessageBeep(MB_ICONASTERISK);
		if (callSendPacket == nullptr)
		{
			callSendPacket = (ptrSendPacket)scan.FindPattern(xorstr_("netc.dll"),
				xorstr_("\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\xF1\x89\xB5\x00\x00\x00\x00\x8B\x7D\x0C"),
				xorstr_("xxxxxx????xx????xxx????x????xxxxxxxxxxxxx????xxxx????xxx")); // 1.5.9
			if (callSendPacket != nullptr)
			{
				LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to SendPacket!\n"));
				MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
			}
			else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for SendPacket.\n"));
		}

		vpnbypass = (vpnbypass_t)scan.FindPatternIDA(xorstr_("netc.dll"), xorstr_("55 8B EC 53 56 57 8B F1 50 B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? B8 ?? ?? ?? ?? 58 E8"));

		if (vpnbypass != nullptr)
		{
			MH_RemoveHook(vpnbypass);
			MH_CreateHook(vpnbypass, &hkvpnbypass, reinterpret_cast<LPVOID*>(&vpnbypass));
			MH_EnableHook(MH_ALL_HOOKS);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #0!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for ByPass Component #0.\n"));

		LPVOID target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 8B 45 0C 8B D0 83 78 14 0F 76 02 8B 10 8B 4D 08 56 8B F1 83 79 14 0F 76 02 8B 31 FF 70 10 52 FF 71 10 56 E8 ? ? ? ? 83 C4 10 34")));
		if (target)
		{
			MH_CreateHook(target, &hkIsNetworkConnected, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #1!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #1.\n"));

		target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"), xorstr_("55 8B EC 8B 45 08 33 D2 56")));
		if (target)
		{
			MH_CreateHook(target, &hkIsNotViolationCode, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #2!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #2.\n"));

		/*target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 83 EC 64 A1 ? ? ? ? 33 C5 89 45 F0 56 57 50 8D 45 F4 64 A3 00 00 00 00 8B F1 50 B8 FE 14 71 4B B8 DE 2C EB 61 B8 8E 82 D9 85 B8 C2 CC B3 92 B8 3E A4 D9")));
		if (target)
		{
			MH_CreateHook(target, &hkAcSecurityViolationKick, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #3!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #3.\n"));*/

		target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 F0 56 57 50 8D 45 F4 64 A3 ? ? ? ? 8B F9 89 7D 84 50")));
		if (target)
		{
			MH_CreateHook(target, &hkSendClientKick, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #4!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #4.\n"));

		callSendReport = (ptrSendReport)(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 51 56 A1 ? ? ? ? 33 C5 50 8D 45 ? 64 A3 ? ? ? ? A1 ? ? ? ? 85 C0 75 ? 68 ? ? ? ? E8 ? ? ? ? 8B F0 33 C0 68 ? ? ? ? 83 FE ? 8B CE 6A ? 0F 44 C8 51 E8 ? ? ? ? 83 C4 ? 89 75 ? C7 45 ? ? ? ? ? 85 F6 74 ? 8B CE E8 ? ? ? ? EB ? 33 C0 C7 45 ? ? ? ? ? A3 ? ? ? ? FF 75 ? 8B C8 FF 75 ? FF 75 ? FF 75 ? FF 75 ? E8 ? ? ? ? FF 05")));
		if (callSendReport)
		{
			MH_CreateHook(callSendReport, &hkSendReport, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #5!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #5.\n"));

		/*target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 83 EC 34 A1 ? ? ? ? 33 C5 89 45 F0 56 57 50 8D 45 F4 64 A3 00 00 00 00 8B F1 50 B8 1E 22 84 8E B8 22 59 21 B2 B8 FE")));
		if (target)
		{
			MH_CreateHook(target, &hkSetClientKick, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #6!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #6.\n"));*/

		target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("53 8B DC 83 EC ? 83 E4 ? 83 C4 ? 55 8B 6B ? 89 6C 24 ? 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 53 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 ? 56 57 50 8D 45 ? 64 A3 ? ? ? ? 8B F9 89 BD ? ? ? ? 50 B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? B8 ? ? ? ? 58 83 7B")));
		if (target)
		{
			MH_CreateHook(target, &hkVfB00_Z00Scanner, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #7!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #7.\n"));

		target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("53 8B DC 83 EC ? 83 E4 ? 83 C4 ? 55 8B 6B ? 89 6C 24 ? 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 53 B8 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 33 C5 89 45 ? 56 57 50 8D 45 ? 64 A3 ? ? ? ? 8B F9 89 BD ? ? ? ? C7 85")));
		if (target)
		{
			MH_CreateHook(target, &hkSetClientKickNew, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #8!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #8.\n"));

		target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 F0 56 57 50 8D 45 F4 64 A3 ? ? ? ? 8B F9 89 7D 84 50")));
		if (target)
		{
			MH_CreateHook(target, &hkSendClientKick, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #9!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #9.\n"));

		/*target = reinterpret_cast<LPVOID>(scan.FindPatternIDA(xorstr_("netc.dll"),
			xorstr_("53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 53 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 EC 56 57 50 8D 45 F4 64 A3 ? ? ? ? 89 8D ? ? ? ? 8B 7B 08 33 C0")));
		if (target)
		{
			MH_CreateHook(target, &hkScanModuleIntegrity, nullptr);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to ByPass Component #10!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find address for signature to ByPass Component #10.\n"));*/
	}
};