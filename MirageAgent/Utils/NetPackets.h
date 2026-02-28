#pragma once

int GetCurrentWeaponID()
{
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;

	BYTE byteCurrentSlot = *(BYTE*)(dwPlayerPed + 0x718);
	DWORD dwWeaponSlot = dwPlayerPed + 0x5A0 + (byteCurrentSlot * 0x1C);

	return *(int*)(dwWeaponSlot);
}

bool IsHoldingFirearm()
{
	int weaponID = GetCurrentWeaponID();

	if (weaponID >= 22 && weaponID <= 34)
		return true;
	if (weaponID >= 35 && weaponID <= 38)
		return true;

	return false;
}

int sendBulletSync(CVector fake_vecStart, CVector fake_vecEnd)
{
	if (CNetAPI != nullptr)
	{
		if (!IsHoldingFirearm()) return 0;

		static int m_ucBulletSyncOrderCounter = 1;
		NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();

		if (pBitStream)
		{
			pBitStream->Write((char)GetCurrentWeaponID());

			pBitStream->Write((const char*)&fake_vecStart, sizeof(CVector));
			pBitStream->Write((const char*)&fake_vecEnd, sizeof(CVector));

			pBitStream->Write(m_ucBulletSyncOrderCounter++);

			pBitStream->WriteBit(false);

			g_pNet->SendPacket(PACKET_ID_PLAYER_BULLETSYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_RELIABLE);
			g_pNet->DeallocateNetBitStream(pBitStream);
			return 1;
		}
	}
	return 0;
}

void WriteCameraOrientation(const CVector& vecPositionBase, NetBitStreamInterface& BitStream)
{
	// Stub: write default zeros without calculations.
	const float fPI = 3.14159265f;
	SFloatAsBitsSync<8> rotation(-fPI, fPI, false);

	// Write rotations as 0.
	rotation.data.fValue = 0.0f;
	BitStream.Write(&rotation);  // Z rotation
	rotation.data.fValue = 0.0f;
	BitStream.Write(&rotation);  // X rotation

	// Stub position: relative (false), idx=0 (3 bits per component, range +-4).
	bool bUseAbsolutePosition = false;
	uint idx = 0;
	uint uiNumBits = 3;
	float fRange = 4.0f;

	BitStream.WriteBit(bUseAbsolutePosition);
	BitStream.WriteBits(&idx, 2);

	SFloatAsBitsSyncBase position(uiNumBits, -fRange, fRange, false);
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // X
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // Y
	position.data.fValue = 0.0f;
	BitStream.Write(&position);  // Z
}

class CControllerState
{
public:
	signed short LeftStickX;             // move/steer left (-128?)/right (+128)
	signed short LeftStickY;             // move back(+128)/forwards(-128?)
	signed short RightStickX;            // numpad 6(+128)/numpad 4(-128?)
	signed short RightStickY;

	signed short LeftShoulder1;
	signed short LeftShoulder2;
	signed short RightShoulder1;            // target / hand brake
	signed short RightShoulder2;

	signed short DPadUp;              // radio change up
	signed short DPadDown;            // radio change down
	signed short DPadLeft;
	signed short DPadRight;

	signed short Start;
	signed short Select;

	signed short ButtonSquare;              // jump / reverse
	signed short ButtonTriangle;            // get in/out
	signed short ButtonCross;               // sprint / accelerate
	signed short ButtonCircle;              // fire

	signed short ShockButtonL;
	signed short ShockButtonR;            // look behind

	signed short m_bChatIndicated;
	signed short m_bPedWalk;
	signed short m_bVehicleMouseLook;
	signed short m_bRadioTrackSkip;

	CControllerState() { memset(this, 0, sizeof(CControllerState)); }
};

void WriteFullKeysync(const CControllerState& ControllerState, NetBitStreamInterface& BitStream)
{
	// Put the controllerstate bools into a key byte.
	SFullKeysyncSync keys;
	keys.data.bLeftShoulder1 = (ControllerState.LeftShoulder1 != 0);
	keys.data.bRightShoulder1 = (ControllerState.RightShoulder1 != 0);
	keys.data.bButtonSquare = (ControllerState.ButtonSquare != 0);
	keys.data.bButtonCross = (ControllerState.ButtonCross != 0);
	keys.data.bButtonCircle = (ControllerState.ButtonCircle != 0);
	keys.data.bButtonTriangle = (ControllerState.ButtonTriangle != 0);
	keys.data.bShockButtonL = (ControllerState.ShockButtonL != 0);
	keys.data.bPedWalk = (ControllerState.m_bPedWalk != 0);
	keys.data.ucButtonSquare = (unsigned char)ControllerState.ButtonSquare;
	keys.data.ucButtonCross = (unsigned char)ControllerState.ButtonCross;
	keys.data.sLeftStickX = ControllerState.LeftStickX;
	keys.data.sLeftStickY = ControllerState.LeftStickY;

	// Write it.
	BitStream.Write(&keys);
}

void SendRPC(eServerRPCFunctions ID, NetBitStreamInterface* pBitStream)
{
	NetBitStreamInterface* pRPCBitStream = g_pNet->AllocateNetBitStream();
	if (pRPCBitStream)
	{
		// Write the rpc ID.
		pRPCBitStream->Write((unsigned char)ID);

		if (pBitStream)
		{
			// Copy each byte from the bitstream we have to this one.
			unsigned char ucTemp;
			int           iLength = pBitStream->GetNumberOfBitsUsed();
			while (iLength > 8)
			{
				pBitStream->Read(ucTemp);
				pRPCBitStream->Write(ucTemp);
				iLength -= 8;
			}
			if (iLength > 0)
			{
				pBitStream->ReadBits(&ucTemp, iLength);
				pRPCBitStream->WriteBits(&ucTemp, iLength);
			}
			pBitStream->ResetReadPointer();
		}

		g_pNet->SendPacket(PACKET_ID_RPC, pRPCBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
		g_pNet->DeallocateNetBitStream(pRPCBitStream);
	}
}

bool sendWeaponSlot5()
{
	if (!g_pNet) return false;
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;

	NetBitStreamInterface* bs = g_pNet->AllocateNetBitStream();
	if (!bs) return false;

	// Always write the "zero-ammo" bit for v>=0x44 (server reads it regardless).
	if (bs->Version() >= 0x44) bs->WriteBit(false);

	// Slot.
	SWeaponSlotSync slot{};
	slot.data.uiSlot = 5;
	bs->Write(&slot);

	// Ammo (slot 5 requires ammo).
	SWeaponAmmoSync ammo(WEAPONTYPE_M4, true, true);
	ammo.data.usAmmoInClip = static_cast<unsigned short>(30);
	ammo.data.usTotalAmmo = static_cast<unsigned short>(50);
	bs->Write(&ammo);

	// Send RPC.
	SendRPC(PLAYER_WEAPON, bs);
	g_pNet->DeallocateNetBitStream(bs);
	return true;
}

int sendPlayerSync(CVector position)
{
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed) return 0;
	NetBitStreamInterface* bitStream = g_pNet->AllocateNetBitStream();
	if (bitStream)
	{
		CControllerState cs{};
		SPlayerPuresyncFlags flags;
		SPositionSync        positionSync(false);
		SPedRotationSync     rotationSync;
		SVelocitySync        velocitySync;
		SPlayerHealthSync    healthSync;
		SPlayerArmorSync     armorSync;
		SCameraRotationSync  camRotationSync;

		flags.data.bAkimboTargetUp = false;
		flags.data.bHasAWeapon = false;
		flags.data.bHasContact = false;
		flags.data.bHasJetPack = false;
		flags.data.bIsChoking = false;
		flags.data.bIsDucked = false;
		//
		flags.data.bIsInWater = false;
		flags.data.bSyncingVelocity = true;
		flags.data.bIsOnGround = false;
		//
		flags.data.bIsOnFire = false;
		flags.data.bStealthAiming = false;
		flags.data.bWearsGoogles = false;

		positionSync.data.vecPosition = position;
		rotationSync.data.fRotation = 0.0f;
		velocitySync.data.vecVelocity = CVector(0.0f, 0.0f, 0.0f); //CVector(NAN, NAN, NAN);

		healthSync.data.fValue = *(float*)(dwPlayerPed + 0x540);
		armorSync.data.fValue = *(float*)(dwPlayerPed + 0x548);

		camRotationSync.data.fRotation = 0.0f;

		unsigned char ctx = 0;
		bitStream->Write(ctx);
		WriteFullKeysync(cs, *bitStream);
		bitStream->Write(&flags);
		bitStream->Write(&positionSync);
		bitStream->Write(&rotationSync);
		bitStream->Write(&velocitySync);
		bitStream->Write(&healthSync);
		bitStream->Write(&armorSync);
		bitStream->Write(&camRotationSync);
		WriteCameraOrientation(position, *bitStream);
		//sendWeaponSlot5(); // RPC
		if (flags.data.bHasAWeapon)
		{
			bitStream->Write(WEAPONTYPE_M4);
			SWeaponSlotSync slot{};
			slot.data.uiSlot = 5;
			bitStream->Write(&slot);
			SWeaponAmmoSync ammo(WEAPONTYPE_M4, true, true);
			ammo.data.usAmmoInClip = 30;
			ammo.data.usTotalAmmo = 50;
			bitStream->Write(&ammo);
			SWeaponAimSync aim(0.0f, false);
			aim.data.fArm = 0.0f;
			bitStream->Write(&aim);
		}
		bitStream->WriteBit(false);
		g_pNet->SendPacket(PACKET_ID_PLAYER_PURESYNC, bitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_UNRELIABLE_SEQUENCED);
		g_pNet->DeallocateNetBitStream(bitStream);
		return 1;
	}
	return 0;
}

void completeInOut(ElementID pedID, ElementID vehID)
{
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream || g_pNet == nullptr) return;

	if (static_cast<eBitStreamVersion>(g_pNet->GetServerBitStreamVersion()) >= eBitStreamVersion::PedEnterExit)
	{
		pBitStream->Write(pedID);
	}

	pBitStream->Write(vehID);
	unsigned char ucAction = static_cast<unsigned char>(1); // enter completed
	pBitStream->WriteBits(&ucAction, 4);

	g_pNet->SendPacket(PACKET_ID_VEHICLE_INOUT, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
}

bool sendInOutRequest(void* luaVM)
{
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream || g_pNet == nullptr) return false;

	void* user_data = *(void**)call_touserdata(luaVM, 1);
	void* user_data2 = *(void**)call_touserdata(luaVM, 2);

	ElementID pedID = TO_ELEMENTID(user_data);
	ElementID vehID = TO_ELEMENTID(user_data2);

	if (static_cast<eBitStreamVersion>(g_pNet->GetServerBitStreamVersion()) >= eBitStreamVersion::PedEnterExit)
	{
		pBitStream->Write(pedID);
	}

	pBitStream->Write(vehID);
	unsigned char ucAction = static_cast<unsigned char>(0); // enter attempt
	unsigned char ucSeat = static_cast<unsigned char>(0); // driver
	bool          bIsOnWater = false;
	unsigned char ucDoor = 0; // driver door
	pBitStream->WriteBits(&ucAction, 4);
	pBitStream->WriteBits(&ucSeat, 4);
	pBitStream->WriteBit(bIsOnWater);
	pBitStream->WriteBits(&ucDoor, 3);

	g_pNet->SendPacket(PACKET_ID_VEHICLE_INOUT, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	completeInOut(pedID, vehID);
	return true;
}

bool sendCameraSync(void* luaVM)
{
	if (!g_pNet) return false;
	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream) return false;

	unsigned int strLen = 256;
	float x = std::stof(call_tostring(luaVM, 1, &strLen));
	float y = std::stof(call_tostring(luaVM, 2, &strLen));
	float z = std::stof(call_tostring(luaVM, 3, &strLen));

	CVector camPos(x, y, z);
	CVector lookAt = camPos;
	lookAt.fY += 1.0f; // look direction

	// In v0x5E+ camera sync-time context is required.
	if (pBitStream->Version() >= 0x5E)
	{
		unsigned char ctx = 0;
		pBitStream->Write(ctx);
	}

	// Fixed mode.
	pBitStream->WriteBit(true);

	SPositionSync pos(false);
	pos.data.vecPosition = camPos;
	pBitStream->Write(&pos);

	SPositionSync look(false);
	look.data.vecPosition = lookAt;
	pBitStream->Write(&look);

	g_pNet->SendPacket(PACKET_ID_CAMERA_SYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_UNRELIABLE_SEQUENCED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}

bool sendSrvEvent(void* luaVM)
{
	if (!g_pNet) return false;

	unsigned int eventNameLen = 0;
	const char* eventName = call_tostring(luaVM, 1, &eventNameLen);
	if (!eventName || eventNameLen == 0 || eventNameLen >= MAX_EVENT_NAME_LENGTH) return false;

	unsigned int countLen = 0;
	const char* countStr = call_tostring(luaVM, 2, &countLen);
	if (!countStr || countLen == 0) return false;

	int tableCount = 0;
	try
	{
		tableCount = std::stoi(std::string(countStr, countLen));
	}
	catch (...)
	{
		return false;
	}
	if (tableCount < 0) return false;

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream) return false;

	const unsigned short usNameLength = static_cast<unsigned short>(eventNameLen);
	const unsigned int uiNumArgs = static_cast<unsigned int>(tableCount);
	const ElementID sourceElement = ElementID(0);
	constexpr unsigned char LUA_TNIL_TYPE = 0;
	constexpr unsigned char LUA_TTABLE_TYPE = 5;

	pBitStream->WriteCompressed(usNameLength);
	pBitStream->Write(eventName, usNameLength);
	pBitStream->Write(sourceElement);

	// CLuaArguments::uiNumArgs
	pBitStream->WriteCompressed(uiNumArgs);

	// Each argument is a table that contains only TNIL.
	for (unsigned int i = 0; i < uiNumArgs; ++i)
	{
		SLuaTypeSync typeTable{};
		typeTable.data.ucType = LUA_TTABLE_TYPE;
		pBitStream->Write(&typeTable);

		pBitStream->WriteCompressed(1u);

		SLuaTypeSync typeNil{};
		typeNil.data.ucType = LUA_TNIL_TYPE;
		pBitStream->Write(&typeNil);
	}

	g_pNet->SendPacket(PACKET_ID_LUA_EVENT, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}
