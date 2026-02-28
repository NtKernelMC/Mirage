#include "ShadowTrace.h"
#include <unordered_set>

namespace ModernBypass
{
	typedef int(__thiscall* ptrInitializePrivateSerialGen)(void* ECX, void* parent);
	ptrInitializePrivateSerialGen callInitializePrivateSerialGen = nullptr;

	typedef bool(__thiscall* ptrSendPacket)(void* ECX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
	ptrSendPacket callSendPacket = nullptr;
	bool __fastcall SendPacket(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering)
	{
		CNetAPI = ECX;
		g_pNet = (CNet*)ECX;
		RestorePrologue((DWORD)callSendPacket, packet_prologue, sizeof(packet_prologue));
		/*static bool init_priv_gen = false;
		if (!init_priv_gen)
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] Initializing Private Serial generator ...\n"));
			if (CNetAPI != nullptr && callInitializePrivateSerialGen != nullptr)
			{
				auto priv_obj = (void*)((uintptr_t)CNetAPI + 0x98);
				callInitializePrivateSerialGen(priv_obj, CNetAPI);
				LogInFile(LOG_NAME, xorstr_("[LOG] Called InitializePrivateSerialGen!\n"));
			}
			else LogInFile(LOG_NAME, xorstr_("[ERROR] Failed to call InitializePrivateSerialGen.\n"));
			init_priv_gen = true;
		}*/
		//auto color_name = magic_enum::enum_name((ePacketID)ucPacketID);
		//LogInFile(LOG_NAME, xorstr_("PacketID: %d | PacketName: %s\n"), ucPacketID, color_name.data());
		/*if ((ucPacketID >= 91 && ucPacketID <= 94 && ucPacketID != 93) || ucPacketID == 34 || ucPacketID == 25)
		{
			MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
			return true;
		}*/
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

#pragma pack(push, 2)
	struct RakNet_InternalPacket // sizeof=0x60
	{
		int32_t messageNumber;
		int32_t sendSequenceId;
		uint8_t flags0;
		uint8_t _pad09[3];
		uint32_t _unk0C;
		uint32_t priority;
		uint32_t reliability;
		uint8_t orderingChannel;
		uint8_t _pad19[3];
		uint32_t orderingIndex;
		uint16_t splitPacketId;
		uint16_t _pad22;
		uint32_t splitPacketIndex;
		uint32_t splitPacketCount;
		uint32_t _unk2C;
		uint64_t creationTime;
		uint8_t _unk38[20];
		uint32_t dataBitLength;
		void* data;
		uint32_t _unk54;
		void* poolPage;
		uint32_t _unk5C;
	};
#pragma pack(pop)

	using ptrRakPeer_QueueBufferedPacket = int(__thiscall*)(void *ECX, void* payload, int bitLength, int priority,
	int reliability, char orderingChannel, int targetIp, __int16 targetPort, char broadcast, int receiptNumber);
	static ptrRakPeer_QueueBufferedPacket callRakPeer_QueueBufferedPacket = nullptr;

	__forceinline void ApplyD4Cipher(BYTE* data, size_t len)
	{
		for (size_t i = 0; i < len; ++i)
		{
			data[i] = (BYTE)(data[i] ^ (BYTE)i ^ 0xD4 ^ (1 << (i & 7)));
		}
	}

	__forceinline bool IsSerialV15Char(BYTE c)
	{
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
	}

	__forceinline BYTE D1Encode(BYTE v15, unsigned int i)
	{
		return (BYTE)(v15 ^ i ^ 0xD1 ^ (1 << (i & 7)));
	}

	__forceinline BYTE D1Decode(BYTE b, unsigned int i)
	{
		return (BYTE)(b ^ i ^ 0xD1 ^ (1 << (i & 7)));
	}

	bool EncodePublicSerialV15(const std::string& v15, BYTE out[32])
	{
		if (v15.size() < 32) return false;

		for (unsigned int i = 0; i < 32; ++i)
		{
			BYTE c = (BYTE)v15[i];
			if (!IsSerialV15Char(c))
				return false;

			out[i] = D1Encode(c, i);
		}
		return true;
	}

	void DecodePublicSerialV15(const BYTE* enc, char out_v15[33])
	{
		for (unsigned int i = 0; i < 32; ++i)
			out_v15[i] = (char)D1Decode(enc[i], i);
		out_v15[32] = 0;
	}

	static std::string BytesToEscaped(const BYTE* p, size_t len)
	{
		std::ostringstream oss;
		for (size_t i = 0; i < len; ++i)
		{
			unsigned char c = (unsigned char)p[i];
			if (c >= 32 && c <= 126) // printable ASCII
			{
				if (c == '\\') oss << "\\\\";
				else oss << (char)c;
			}
			else
			{
				oss << "\\x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
					<< (int)c << std::nouppercase << std::dec;
			}
		}
		return oss.str();
	}

	static std::string BytesToSerialString(const BYTE* p, size_t len)
	{
		std::string s; s.reserve(len);
		for (size_t i = 0; i < len; ++i)
		{
			BYTE c = p[i];
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) s.push_back((char)c);
		}
		return s;
	}

	static void PrintPrivEncryptedDecryptedAsString(const BYTE* packet, size_t private_off, size_t private_len)
	{
		const BYTE* enc = packet + private_off;
		BYTE tmp[64];
		std::memcpy(tmp, enc, private_len);
		ApplyD4Cipher(tmp, private_len); // XOR decrypt == encrypt
		LogInFile(LOG_NAME, xorstr_("[RAKNET] Private Serial: %s\n"), BytesToEscaped(tmp, private_len));
	}

	static bool ShouldIgnoreCommandPacket(const BYTE* payload, int bitLength)
	{
		if (!payload || bitLength <= 0) return false;
		const size_t totalBytes = (static_cast<size_t>(bitLength) + 7) >> 3;
		if (totalBytes <= 1) return false;

		std::string cmdLine(reinterpret_cast<const char*>(payload + 1), totalBytes - 1);
		std::string cmdName = NormalizeCommandName(cmdLine);
		if (cmdName.empty()) return false;

		std::lock_guard<std::mutex> lock(command_ignore_mutex);
		return command_ignore_set.find(cmdName) != command_ignore_set.end();
	}

	int __fastcall RakPeer_QueueBufferedPacket(void* ECX, void* EDX, void* payload, int bitLength, int priority,
	int reliability, char orderingChannel, int targetIp, __int16 targetPort, char broadcast, int receiptNumber)
	{
		unsigned int packetId = *(BYTE*)payload;
		const unsigned int PACKET_RAK_ADDED_TO_ID = 99;

		if (!packetId || packetId < PACKET_RAK_ADDED_TO_ID) return 1;
		packetId -= PACKET_RAK_ADDED_TO_ID;

		if (packetId == PACKET_ID_COMMAND)
		{
			if (ShouldIgnoreCommandPacket(reinterpret_cast<const BYTE*>(payload), bitLength))
				return 1;
		}
		if (packetId == PACKET_ID_PLAYER_SCREENSHOT && block_screen_packet)
		{
			return 1;
		}
		
		if (packetId == 4 || packetId == 19)
		{
			std::string public_serial = mirage.public_serial;
			const bool should_set_public = mirage.set_public && !public_serial.empty();
			std::string private_serial = xorstr_("7E4A9C0F2D8B6E13A5F1C97D04B8E6A3F5C2D9B147E0A86F4C1D35E92");

			const size_t totalBytes = (bitLength + 7) >> 3;
			if (totalBytes < 3 + 32 + 2)
			{
				LogInFile(LOG_NAME, xorstr_("[ERROR] QueueBufferedPacket - Invalid packet size.\n"));
				return 0;
			}

			BYTE* packet = (BYTE*)payload;
			const size_t public_off = 3;
			const size_t public_len = 32;

			if (should_set_public)
			{
				BYTE public_enc[32] = {};
				if (!EncodePublicSerialV15(public_serial, public_enc))
				{
					LogInFile(LOG_NAME, xorstr_("[ERROR] QueueBufferedPacket - Invalid public serial (v15 mode).\n"));
					return 0;
				}
				memcpy(packet + public_off, public_enc, 32);
			}

			const size_t private_len = 64;
			size_t private_off = public_off + public_len + 2;

			BYTE priv_enc[private_len];
			memcpy(priv_enc, private_serial.data(), private_len);
			ApplyD4Cipher(priv_enc, private_len);

			PrintPrivEncryptedDecryptedAsString(packet, private_off, private_len);
			//memcpy(packet + private_off, priv_enc, private_len);
		}

		if ((packetId >= 91 && packetId <= 94) || (packetId == 34 || packetId == 25)) return 1;
		return callRakPeer_QueueBufferedPacket(ECX, payload, bitLength, priority, reliability, orderingChannel, targetIp, targetPort, broadcast, receiptNumber);
	}
	using ptr_moris_hook_scanner = uint8_t * (__stdcall*)(uint8_t* a1);
	static ptr_moris_hook_scanner call_moris_hook_scanner = nullptr;

	static uint8_t* __stdcall moris_hook_scanner(uint8_t* a1)
	{
		return a1;
	}

	typedef int(__thiscall* ptrEncryptWmiBuffer)(void* ECX, char* buff, size_t size);
	ptrEncryptWmiBuffer callEncryptWmiBuffer = nullptr;
	int __fastcall EncryptWmiBuffer(void* ECX, void* EDX, char* buff, size_t size)
	{
		if (!buff || size == 0 || size > 0x4000) return callEncryptWmiBuffer(ECX, buff, size);

		size_t n = 0;
		while (n < size && buff[n] != '\0') ++n;
		std::string original(buff, buff + n);

		ShadowTrace::WmiFieldInfo info;
		std::string fake;
		if (ShadowTrace::IsUniqueWmiValue(original, &info))
		{
			if (info.className.empty()) info.className = "Unknown";
			if (info.propertyName.empty()) info.propertyName = "Unknown";

			fake = ShadowTrace::GetOrGenerateFake(original);
		}
		else if (ShadowTrace::TryGenerateNonUnique(original, info, fake))
		{
			if (info.className.empty()) info.className = "Unknown";
			if (info.propertyName.empty()) info.propertyName = "Unknown";
		}

		if (!fake.empty())
		{
			fake = ShadowTrace::PreserveTrailingSpaces(original, fake);
			const size_t replLen = fake.size();
			memset(buff, 0, size);
			const size_t toCopy = (replLen < (size - 1)) ? replLen : (size - 1);
			memcpy(buff, fake.data(), toCopy);
			buff[toCopy] = '\0';

			//LogInFile(LOG_NAME, xorstr_("[LOG] WMI Field (spoofed %s.%s): %s\n"), info.className.c_str(), info.propertyName.c_str(), fake.c_str());
		}
		else
		{
			//LogInFile(LOG_NAME, xorstr_("[LOG] WMI Field: %s\n"), original.c_str());
		}

		return callEncryptWmiBuffer(ECX, buff, size);
	}

	typedef ShadowTrace::NetcEncodedString* (__cdecl* ptrCollectAdapterMacHexByName)(ShadowTrace::NetcEncodedString* a1, int a2);
	ptrCollectAdapterMacHexByName callCollectAdapterMacHexByName = nullptr;

	ShadowTrace::NetcEncodedString* __cdecl CollectAdapterMacHexByName(ShadowTrace::NetcEncodedString* a1, int a2)
	{
		ShadowTrace::NetcEncodedString* ret = callCollectAdapterMacHexByName(a1, a2);
		
		if (!a1) return ret;

		std::string decoded;
		if (!NetcDecodeString(a1, decoded)) return ret;

		if (!ShadowTrace::LooksLikeHexMac(decoded, nullptr, nullptr)) return ret;

		const std::string orig = decoded;
		const std::string fake = ShadowTrace::GetOrGenerateMacFake(orig);
		if (fake.empty()) return ret;

		if (!NetcEncodeStringInPlace(a1, fake)) return ret;

		//LogInFile(LOG_NAME, xorstr_("[LOG] MAC spoofed: %s -> %s\n"), orig.c_str(), fake.c_str());
		return ret;
	}

	void EvadeAnticheat()
	{
		SigScan scan; MessageBeep(MB_ICONASTERISK);
		//ShadowTrace::InitWmiCacheOnce();
		
		/*callInitializePrivateSerialGen = (ptrInitializePrivateSerialGen)scan.FindCallPattern(xorstr_("netc.dll"),
			xorstr_("E8 ? ? ? ? 33 C0 66 A3"));
		if (callInitializePrivateSerialGen != nullptr)
		{
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to InitializePrivateSerialGen!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for InitializePrivateSerialGen.\n"));
		callCollectAdapterMacHexByName = (ptrCollectAdapterMacHexByName)scan.FindCallPattern(xorstr_("netc.dll"),
			xorstr_("E8 ? ? ? ? 8D 45 ? C6 45 ? ? 50 E8 ? ? ? ? 8D 45 ? C6 45"));
		if (callCollectAdapterMacHexByName != nullptr)
		{
			MH_RemoveHook(callCollectAdapterMacHexByName);
			MH_CreateHook(callCollectAdapterMacHexByName, &CollectAdapterMacHexByName, reinterpret_cast<LPVOID*>(&callCollectAdapterMacHexByName));
			MH_EnableHook(MH_ALL_HOOKS);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to CollectAdapterMacHexByName!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for CollectAdapterMacHexByName.\n"));
		callEncryptWmiBuffer = (ptrEncryptWmiBuffer)scan.FindCallPattern(xorstr_("netc.dll"),
			xorstr_("E8 ? ? ? ? C6 45 ? ? 83 C8"));
		if (callEncryptWmiBuffer != nullptr)
		{
			MH_RemoveHook(callEncryptWmiBuffer);
			MH_CreateHook(callEncryptWmiBuffer, &EncryptWmiBuffer, reinterpret_cast<LPVOID*>(&callEncryptWmiBuffer));
			MH_EnableHook(MH_ALL_HOOKS);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to EncryptWmiBuffer!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for EncryptWmiBuffer.\n"));*/
		/*call_moris_hook_scanner = (ptr_moris_hook_scanner)scan.FindPatternIDA(xorstr_("core.dll"),
			xorstr_("55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC ? A1 ? ? ? ? 33 C5 89 45 ? 56 50 8D 45 ? 64 A3 ? ? ? ? 8B 75 ? 89 4D"));
		if (call_moris_hook_scanner)
		{
			MH_RemoveHook(call_moris_hook_scanner);
			MH_CreateHook(call_moris_hook_scanner, &moris_hook_scanner, reinterpret_cast<LPVOID*>(&call_moris_hook_scanner));
			MH_EnableHook(MH_ALL_HOOKS);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to AC_SCAN!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for AC_SCAN.\n"));*/
		
		callRakPeer_QueueBufferedPacket = (ptrRakPeer_QueueBufferedPacket)scan.FindPatternIDA(xorstr_("netc.dll"),
		xorstr_("55 8B EC 53 56 8B F1 57 8D 8E"));
		if (callRakPeer_QueueBufferedPacket != nullptr)
		{
			MH_RemoveHook(callRakPeer_QueueBufferedPacket);
			MH_CreateHook(callRakPeer_QueueBufferedPacket, &RakPeer_QueueBufferedPacket, reinterpret_cast<LPVOID*>(&callRakPeer_QueueBufferedPacket));
			MH_EnableHook(MH_ALL_HOOKS);
			LogInFile(LOG_NAME, xorstr_("[LOG] Found address from signature to RakPeer_QueueBufferedPacket!\n"));
		}
		else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for RakPeer_QueueBufferedPacket.\n"));
		
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
		MH_EnableHook(MH_ALL_HOOKS);
	}
};
namespace LegacyBypass
{
	typedef bool(__thiscall* ptrSendPacket)(void* ECX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
	ptrSendPacket callSendPacket = nullptr;
	bool __fastcall SendPacket(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering)
	{
		CNetAPI = ECX;
		g_pNet = (CNet*)ECX;
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
			else
			{
				LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for SendPacket.\n"));
				ModernBypass::EvadeAnticheat();
			}
		}
	}
};

