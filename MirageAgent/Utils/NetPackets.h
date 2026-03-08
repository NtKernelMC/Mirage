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

static bool ReadLuaFloatNumber(void* luaVM, int idx, float& out)
{
	if (!call_tostring)
		return false;

	unsigned int strLen = 0;
	const char* text = call_tostring(luaVM, idx, &strLen);
	if (!text || strLen == 0)
		return false;

	float value = 0.0f;
	if (sscanf_s(text, "%f", &value) != 1)
		return false;

	out = value;
	return true;
}

static bool ReadLuaIntNumber(void* luaVM, int idx, int& out)
{
	if (!call_tostring)
		return false;

	unsigned int strLen = 0;
	const char* text = call_tostring(luaVM, idx, &strLen);
	if (!text || strLen == 0)
		return false;

	int value = 0;
	if (sscanf_s(text, "%d", &value) != 1)
		return false;

	out = value;
	return true;
}

static bool ReadLuaElementId(void* luaVM, int idx, ElementID& out, bool allowNil = false)
{
	out = static_cast<ElementID>(INVALID_ELEMENT_ID);
	if (!call_touserdata)
		return false;

	void* rawUserData = call_touserdata(luaVM, idx);
	if (!rawUserData)
		return allowNil;

	void* elementPtr = nullptr;
	__try
	{
		elementPtr = *(void**)rawUserData;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		elementPtr = nullptr;
	}

	if (!elementPtr)
		return allowNil;

	out = TO_ELEMENTID(elementPtr);
	return true;
}

static unsigned short ClampAmmoValue(int value)
{
	if (value < 0)
		return 0;
	if (value > 0xFFFF)
		return 0xFFFF;

	return static_cast<unsigned short>(value);
}

static bool HasVehiclePureSyncTurret(unsigned short model)
{
	switch (model)
	{
	case 407:
	case 432:
	case 601:
		return true;
	default:
		return false;
	}
}

static bool HasVehiclePureSyncAdjustableProperty(unsigned short model)
{
	switch (model)
	{
	case 406:
	case 443:
	case 486:
	case 520:
	case 524:
	case 525:
	case 530:
	case 531:
	case 592:
		return true;
	default:
		return false;
	}
}

static bool HasVehiclePureSyncDoors(unsigned short model)
{
	if (model < 400 || model > 611)
		return false;

	switch (model)
	{
	case 424:
	case 430:
	case 441:
	case 446:
	case 448:
	case 449:
	case 452:
	case 453:
	case 454:
	case 457:
	case 461:
	case 462:
	case 463:
	case 464:
	case 468:
	case 472:
	case 473:
	case 481:
	case 484:
	case 485:
	case 486:
	case 493:
	case 504:
	case 509:
	case 510:
	case 521:
	case 522:
	case 523:
	case 531:
	case 537:
	case 538:
	case 539:
	case 568:
	case 569:
	case 570:
	case 571:
	case 581:
	case 586:
	case 590:
	case 594:
	case 595:
	case 606:
	case 607:
	case 610:
		return false;
	default:
		return true;
	}
}

static bool GetLocalDrivenVehicle(DWORD& outPlayerPed, unsigned short& outVehicleModel, float& outVehicleHealth)
{
	outPlayerPed = 0;
	outVehicleModel = 0;
	outVehicleHealth = 0.0f;

	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed)
		return false;

	__try
	{
		const DWORD pedFlags = *(DWORD*)(dwPlayerPed + 0x46C);
		if ((pedFlags & (1u << 8)) == 0)
			return false;

		DWORD dwVehicle = *(DWORD*)(dwPlayerPed + 0x58C);
		if (!dwVehicle)
			return false;

		if (*(DWORD*)(dwVehicle + 0x460) != dwPlayerPed)
			return false;

		outPlayerPed = dwPlayerPed;
		outVehicleModel = static_cast<unsigned short>(*(WORD*)(dwVehicle + 0x22));
		outVehicleHealth = *(float*)(dwVehicle + 0x4C0);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static CVector GetEntityPosition(DWORD dwEntity)
{
	CVector position(0.0f, 0.0f, 0.0f);
	if (!dwEntity)
		return position;

	__try
	{
		position.fX = *(float*)(dwEntity + 0x4);
		position.fY = *(float*)(dwEntity + 0x8);
		position.fZ = *(float*)(dwEntity + 0xC);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		position = CVector(0.0f, 0.0f, 0.0f);
	}

	return position;
}

static unsigned short GetCurrentWeaponTotalAmmo()
{
	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed)
		return 0;

	BYTE byteCurrentSlot = *(BYTE*)(dwPlayerPed + 0x718);
	if (byteCurrentSlot >= 13)
		return 0;

	DWORD dwWeaponSlot = dwPlayerPed + 0x5A0 + (byteCurrentSlot * 0x1C);
	DWORD totalAmmo = *(DWORD*)(dwWeaponSlot + 0xC);
	if (totalAmmo > 0xFFFF)
		totalAmmo = 0xFFFF;

	return static_cast<unsigned short>(totalAmmo);
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
		g_pNet->SendPacket(PACKET_ID_PLAYER_PURESYNC, bitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
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

bool sendVehiclePushSync(void* luaVM)
{
	if (!g_pNet)
		return false;

	ElementID vehicleID;
	if (!ReadLuaElementId(luaVM, 1, vehicleID))
		return false;

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream)
		return false;

	SUnoccupiedPushSync pushSync;
	pushSync.data.vehicleID = vehicleID;
	pBitStream->Write(&pushSync);

	g_pNet->SendPacket(PACKET_ID_VEHICLE_PUSH_SYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}

bool sendUnoccupiedVehicleSync(void* luaVM)
{
	if (!g_pNet)
		return false;

	ElementID vehicleID;
	ElementID trailerID;
	float     x = 0.0f;
	float     y = 0.0f;
	float     z = 0.0f;

	if (!ReadLuaElementId(luaVM, 1, vehicleID))
		return false;
	if (!ReadLuaFloatNumber(luaVM, 2, x) || !ReadLuaFloatNumber(luaVM, 3, y) || !ReadLuaFloatNumber(luaVM, 4, z))
		return false;
	if (!ReadLuaElementId(luaVM, 5, trailerID, true))
		return false;

	SUnoccupiedVehicleSync sync;
	sync.data.vehicleID = vehicleID;
	sync.data.ucTimeContext = 0;
	sync.data.bSyncPosition = true;
	sync.data.vecPosition = CVector(x, y, z);
	sync.data.bSyncTrailer = true;
	sync.data.trailer = trailerID;
	if (trailerID != INVALID_ELEMENT_ID)
	{
		sync.data.bSyncVelocity = true;
		sync.data.vecVelocity = CVector(0.0f, 30.0f, 0.0f);
	}

	const int argCount = call_lua_gettop ? call_lua_gettop(luaVM) : 0;
	if (argCount >= 6)
		sync.data.bEngineOn = call_toboolean(luaVM, 6);
	if (argCount >= 7)
		sync.data.bDerailed = call_toboolean(luaVM, 7);
	if (argCount >= 8)
		sync.data.bIsInWater = call_toboolean(luaVM, 8);
	if (argCount >= 9)
	{
		int timeContext = 0;
		if (!ReadLuaIntNumber(luaVM, 9, timeContext) || timeContext < 0 || timeContext > 255)
			return false;
		sync.data.ucTimeContext = static_cast<unsigned char>(timeContext);
	}

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream)
		return false;

	pBitStream->Write(&sync);
	g_pNet->SendPacket(PACKET_ID_UNOCCUPIED_VEHICLE_SYNC, pBitStream, PACKET_PRIORITY_MEDIUM, PACKET_RELIABILITY_UNRELIABLE_SEQUENCED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}

bool sendVehicleDamageSync(void* luaVM)
{
	if (!g_pNet)
		return false;

	constexpr int kWheelStateMin = 0;
	constexpr int kWheelStateMax = 3;

	ElementID vehicleID;
	if (!ReadLuaElementId(luaVM, 1, vehicleID))
		return false;

	int wheelStates[MAX_WHEELS]{};
	for (int i = 0; i < MAX_WHEELS; ++i)
	{
		if (!ReadLuaIntNumber(luaVM, 2 + i, wheelStates[i]))
			return false;
		if (wheelStates[i] < kWheelStateMin || wheelStates[i] > kWheelStateMax)
			return false;
	}

	SVehicleDamageSync damage(true, true, true, true, true);
	memset(&damage.data, 0, sizeof(damage.data));
	for (unsigned int i = 0; i < MAX_WHEELS; ++i)
	{
		damage.data.bWheelStatesChanged[i] = true;
		damage.data.ucWheelStates[i] = static_cast<unsigned char>(wheelStates[i]);
	}

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream)
		return false;

	pBitStream->Write(vehicleID);
	pBitStream->Write(&damage);
	g_pNet->SendPacket(PACKET_ID_VEHICLE_DAMAGE_SYNC, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}

bool sendVehiclePureSync(void* luaVM)
{
	if (!g_pNet)
		return false;

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float rotX = 0.0f;
	float rotY = 0.0f;
	float rotZ = 0.0f;
	float velX = 0.0f;
	float velY = 0.0f;
	float velZ = 0.0f;

	if (!ReadLuaFloatNumber(luaVM, 1, x) || !ReadLuaFloatNumber(luaVM, 2, y) || !ReadLuaFloatNumber(luaVM, 3, z))
		return false;
	if (!ReadLuaFloatNumber(luaVM, 4, rotX) || !ReadLuaFloatNumber(luaVM, 5, rotY) || !ReadLuaFloatNumber(luaVM, 6, rotZ))
		return false;
	if (!ReadLuaFloatNumber(luaVM, 7, velX) || !ReadLuaFloatNumber(luaVM, 8, velY) || !ReadLuaFloatNumber(luaVM, 9, velZ))
		return false;

	DWORD dwPlayerPed = 0;
	unsigned short vehicleModel = 0;
	float vehicleHealth = 0.0f;
	if (!GetLocalDrivenVehicle(dwPlayerPed, vehicleModel, vehicleHealth))
		return false;

	const int argCount = call_lua_gettop ? call_lua_gettop(luaVM) : 0;
	if (argCount >= 10 && !ReadLuaFloatNumber(luaVM, 10, vehicleHealth))
		return false;

	if (vehicleHealth < 0.0f)
		vehicleHealth = 0.0f;
	if (vehicleHealth > 2047.5f)
		vehicleHealth = 2047.5f;

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream)
		return false;

	CControllerState controllerState{};
	SPositionSync positionSync(false);
	SRotationDegreesSync rotationSync;
	SVelocitySync velocitySync;
	SVelocitySync turnSpeedSync;
	SVehicleHealthSync vehicleHealthSync;
	SOccupiedSeatSync seatSync;
	SPlayerHealthSync playerHealthSync;
	SPlayerArmorSync playerArmorSync;
	SVehiclePuresyncFlags flags{};
	SVehicleTurretSync turretSync;
	SDoorOpenRatioSync doorSync;

	memset(&flags.data, 0, sizeof(flags.data));

	positionSync.data.vecPosition = CVector(x, y, z);
	rotationSync.data.vecRotation = CVector(rotX, rotY, rotZ);
	velocitySync.data.vecVelocity = CVector(velX, velY, velZ);
	turnSpeedSync.data.vecVelocity = CVector(0.0f, 0.0f, 0.0f);
	vehicleHealthSync.data.fValue = vehicleHealth;
	seatSync.data.ucSeat = 0;
	playerHealthSync.data.fValue = *(float*)(dwPlayerPed + 0x540);
	playerArmorSync.data.fValue = *(float*)(dwPlayerPed + 0x548);
	doorSync.data.fRatio = 0.0f;

	unsigned char ctx = 0;
	pBitStream->Write(ctx);
	WriteFullKeysync(controllerState, *pBitStream);
	if (pBitStream->Version() >= 0x05F)
		pBitStream->Write(static_cast<int>(vehicleModel));
	pBitStream->Write(&positionSync);
	WriteCameraOrientation(positionSync.data.vecPosition, *pBitStream);
	pBitStream->Write(&seatSync);
	pBitStream->Write(&rotationSync);
	pBitStream->Write(&velocitySync);
	pBitStream->Write(&turnSpeedSync);
	pBitStream->Write(&vehicleHealthSync);
	pBitStream->WriteBit(false);
	if (pBitStream->Version() >= 0x047)
	{
		pBitStream->WriteBit(false);
	}
	pBitStream->Write(&playerHealthSync);
	pBitStream->Write(&playerArmorSync);
	pBitStream->Write(&flags);
	if (HasVehiclePureSyncTurret(vehicleModel))
	{
		turretSync.data.fTurretX = 0.0f;
		turretSync.data.fTurretY = 0.0f;
		pBitStream->Write(&turretSync);
	}
	if (HasVehiclePureSyncAdjustableProperty(vehicleModel))
	{
		unsigned short adjustableProperty = 0;
		pBitStream->Write(adjustableProperty);
	}
	if (HasVehiclePureSyncDoors(vehicleModel))
	{
		for (unsigned char i = 2; i < 6; ++i)
		{
			pBitStream->Write(&doorSync);
		}
	}

	g_pNet->SendPacket(PACKET_ID_PLAYER_VEHICLE_PURESYNC, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
	g_pNet->DeallocateNetBitStream(pBitStream);
	return true;
}

bool sendPlayerWasted(void* luaVM)
{
	if (!g_pNet)
		return false;

	ElementID damagerId;
	int weapon = 0;
	int bodypart = 0;
	int animGroup = 0;
	int animID = 0;

	if (!ReadLuaElementId(luaVM, 1, damagerId, true))
		return false;
	if (!ReadLuaIntNumber(luaVM, 2, weapon) || !ReadLuaIntNumber(luaVM, 3, bodypart))
		return false;
	if (!ReadLuaIntNumber(luaVM, 4, animGroup) || !ReadLuaIntNumber(luaVM, 5, animID))
		return false;

	if (weapon < 0 || weapon > 255)
		return false;
	if (bodypart < 0 || bodypart > 255)
		return false;
	if (animGroup < 0 || animGroup > 0xFFFF)
		return false;
	if (animID < 0 || animID > 0xFFFF)
		return false;

	DWORD dwPlayerPed = *(DWORD*)0xB6F5F0;
	if (!dwPlayerPed)
		return false;

	CVector position = GetEntityPosition(dwPlayerPed);
	unsigned short totalAmmo = GetCurrentWeaponTotalAmmo();

	const int argCount = call_lua_gettop ? call_lua_gettop(luaVM) : 0;
	if (argCount >= 8)
	{
		if (!ReadLuaFloatNumber(luaVM, 6, position.fX) || !ReadLuaFloatNumber(luaVM, 7, position.fY) || !ReadLuaFloatNumber(luaVM, 8, position.fZ))
			return false;
	}
	else if (argCount >= 6)
	{
		return false;
	}

	if (argCount >= 9)
	{
		int ammo = 0;
		if (!ReadLuaIntNumber(luaVM, 9, ammo))
			return false;

		totalAmmo = ClampAmmoValue(ammo);
	}

	NetBitStreamInterface* pBitStream = g_pNet->AllocateNetBitStream();
	if (!pBitStream)
		return false;

	SWeaponTypeSync weaponSync;
	SBodypartSync bodypartSync;
	SPositionSync positionSync(false);
	SWeaponAmmoSync ammoSync(static_cast<unsigned char>(weapon), true, false);

	weaponSync.data.ucWeaponType = static_cast<unsigned char>(weapon);
	bodypartSync.data.uiBodypart = static_cast<unsigned int>(bodypart);
	positionSync.data.vecPosition = position;
	ammoSync.data.usTotalAmmo = totalAmmo;

	pBitStream->WriteCompressed(static_cast<unsigned short>(animGroup));
	pBitStream->WriteCompressed(static_cast<unsigned short>(animID));
	pBitStream->Write(damagerId);
	pBitStream->Write(&weaponSync);
	pBitStream->Write(&bodypartSync);
	pBitStream->Write(&positionSync);
	pBitStream->Write(&ammoSync);

	g_pNet->SendPacket(PACKET_ID_PLAYER_WASTED, pBitStream, PACKET_PRIORITY_HIGH, PACKET_RELIABILITY_RELIABLE_ORDERED);
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
