namespace LegacyBypass
{
    typedef bool(__thiscall* ptrSendPacket)(void* ECX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
    ptrSendPacket callSendPacket = nullptr;
    bool __fastcall SendPacket(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering)
    {
		RestorePrologue((DWORD)callSendPacket, packet_prologue, sizeof(packet_prologue));
        if (ucPacketID == 91)
        {
            MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
            return true;
        }
        bool rslt = callSendPacket(ECX, ucPacketID, bitStream, packetPriority, packetReliability, packetOrdering);
        MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
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
				MakeJump((DWORD)callSendPacket, (DWORD)&SendPacket, packet_prologue, sizeof(packet_prologue));
			}
			else LogInFile(LOG_NAME, xorstr_("[ERROR] Can`t find a signature for SendPacket.\n"));
        }
    }
};