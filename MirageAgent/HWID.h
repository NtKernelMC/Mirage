#pragma once
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include "MD5.h"
#include "xorstr.h"
typedef struct _SMBIOSHEADER {
	BYTE Type;     // Тип структуры
	BYTE Length;   // Длина структуры в байтах
	WORD Handle;   // Уникальный идентификатор (дескриптор) структуры
} SMBIOSHEADER, *PSMBIOSHEADER;
typedef struct _RawSMBIOSData {
	BYTE    Used20CallingMethod;
	BYTE    SMBIOSMajorVersion;
	BYTE    SMBIOSMinorVersion;
    BYTE    DmiRevision;
    DWORD   Length;
	PBYTE   SMBIOSTableData;
} RawSMBIOSData, *PRawSMBIOSData;
typedef struct _BoardInfo {
    SMBIOSHEADER Header; // Общий заголовок для всех типов структур SMBIOS
    UCHAR Manufacturer;  // Индекс строки, содержащей имя производителя базовой платы
    UCHAR Product;       // Индекс строки, содержащей название продукта (модели базовой платы)
    UCHAR Version;       // Индекс строки, содержащей версию базовой платы
    UCHAR SN;            // Индекс строки, содержащей серийный номер базовой платы
    UCHAR AssetTag;      // Индекс строки, содержащей тег актива базовой платы
    UCHAR FeatureFlags;  // Флаги особенностей базовой платы
    UCHAR LocationInChassis; // Индекс строки, содержащей местоположение базовой платы в шасси
    UINT16 ChassisHandle;    // Дескриптор шасси
    UCHAR Type;              // Тип базовой платы
    UCHAR NumObjHandle;      // Количество объектных дескрипторов
    UINT16* pObjHandle;      // Указатель на объектные дескрипторы
} BoardInfo, *PBoardInfo;
typedef struct _BIOSInfo {
    SMBIOSHEADER Header; // Общий заголовок для всех типов структур SMBIOS
    UCHAR Vendor;        // Индекс строки, содержащей имя производителя BIOS
    UCHAR Version;       // Индекс строки, содержащей версию BIOS
    UINT16 StartingAddrSeg;
    UCHAR ReleaseDate;   // Индекс строки, содержащей дату выпуска BIOS
    UCHAR ROMSize;
    ULONG64 Characteristics;
    // Дополнительные поля, зависящие от версии спецификации SMBIOS
    UCHAR Extension[2]; // Для спецификации 2.3
    UCHAR MajorRelease;
    UCHAR MinorRelease;
    UCHAR ECFirmwareMajor;
    UCHAR ECFirmwareMinor;
} BIOSInfo, *PBIOSInfo;
std::string getStringByNumber(const char* str, BYTE number);
PSMBIOSHEADER getNextHeader(PSMBIOSHEADER currentHeader);
using PC_DATA = struct {
    std::string baseboard_manufacturer;
    std::string baseboard_product;
    std::string bios_vendor;
    std::string bios_version;
    std::string bios_release_date;
};

using BiosInfo = struct {
    std::string Vendor;
    std::string Version;
    std::string ReleaseDate;
};

using BaseBoardInfo = struct {
    std::string Manufacturer;
    std::string Product;
};

// Вспомогательная функция для получения информации о BIOS
BiosInfo GetBiosInfo()
{
	const DWORD Signature = 'RSMB';
    DWORD bufferSize = GetSystemFirmwareTable(Signature, 0, NULL, 0);
    if (bufferSize == 0) {
        // Не удалось получить размер данных
        return {};
    }

    auto buffer = std::make_unique<BYTE[]>(bufferSize);
    if (GetSystemFirmwareTable(Signature, 0, buffer.get(), bufferSize) != bufferSize) {
        // Не удалось получить данные
        return {};
    }

    auto pDMIData = reinterpret_cast<PRawSMBIOSData>(buffer.get());
    auto pHeader = reinterpret_cast<PSMBIOSHEADER>(&pDMIData->SMBIOSTableData);
    if (pHeader->Type != 0) {
        // Не тот тип данных
        return {};
    }

    auto pBIOS = reinterpret_cast<PBIOSInfo>(pHeader);
    auto str = reinterpret_cast<const char*>(pHeader) + pHeader->Length;

    BiosInfo biosInfo;
    // Используем безопасные функции для работы со строками
    biosInfo.Vendor = getStringByNumber(str, pBIOS->Vendor);
    biosInfo.Version = getStringByNumber(str, pBIOS->Version);
    biosInfo.ReleaseDate = getStringByNumber(str, pBIOS->ReleaseDate);
	return biosInfo;
}

// Похожая функция для получения информации о базовой плате
BaseBoardInfo GetBaseBoardInfo()
{
	const DWORD Signature = 'RSMB';
    DWORD bufferSize = GetSystemFirmwareTable(Signature, 0, NULL, 0);
    if (bufferSize == 0) {
        // Не удалось получить размер данных
        return {};
    }

    auto buffer = std::make_unique<BYTE[]>(bufferSize);
    if (GetSystemFirmwareTable(Signature, 0, buffer.get(), bufferSize) != bufferSize) {
        // Не удалось получить данные
        return {};
    }

    auto pDMIData = reinterpret_cast<PRawSMBIOSData>(buffer.get());
    auto pHeader = reinterpret_cast<PSMBIOSHEADER>(&pDMIData->SMBIOSTableData);
    BaseBoardInfo baseBoardInfo;

    while (reinterpret_cast<char*>(pHeader) < reinterpret_cast<char*>(buffer.get()) + bufferSize) {
        if (pHeader->Type == 2) {
            auto pBaseBoard = reinterpret_cast<PBoardInfo>(pHeader);
            auto str = reinterpret_cast<const char*>(pHeader) + pHeader->Length;

            baseBoardInfo.Manufacturer = getStringByNumber(str, pBaseBoard->Manufacturer);
            baseBoardInfo.Product = getStringByNumber(str, pBaseBoard->Product);
            break;
        }

        // Правильно перемещаем указатель к следующей структуре
        pHeader = getNextHeader(pHeader);
    }
	return baseBoardInfo;
}


std::string getStringByNumber(const char* str, BYTE number) {
    while (number > 0 && *str != '\0') {
        if (--number == 0) {
            return std::string(str);
        }
        str += strlen(str) + 1; // Переход к следующей строке
    }
    return {}; // Возвращает пустую строку, если не найдено
}

PSMBIOSHEADER getNextHeader(PSMBIOSHEADER currentHeader) {
    // Переходим к концу текущей структуры
    auto endOfStruct = reinterpret_cast<char*>(currentHeader) + currentHeader->Length;

    // Ищем двойной нулевой байт, который означает конец структуры
    while (*reinterpret_cast<USHORT*>(endOfStruct) != 0) {
        endOfStruct++;
    }

	// Возвращаем указатель на следующую структуру, следующую за двойным нулевым байтом
	return reinterpret_cast<PSMBIOSHEADER>(endOfStruct + 2);
}

// Функция для генерации HWID
std::string GenHWID()
{
	PC_DATA pc;
	auto baseBoardInfo = GetBaseBoardInfo();
	pc.baseboard_manufacturer = std::move(baseBoardInfo.Manufacturer);
	pc.baseboard_product = std::move(baseBoardInfo.Product);

	auto biosInfo = GetBiosInfo();
    pc.bios_vendor = std::move(biosInfo.Vendor);
	pc.bios_version = std::move(biosInfo.Version);
	pc.bios_release_date = std::move(biosInfo.ReleaseDate);

	unsigned int InfoType = 0; unsigned int a, b, c, d;
    std::string CpuVendor, CpuBrand;
    __asm
	{
        pushfd
        pushad
		mov eax, InfoType
        cpuid
        mov a, EAX;
		mov b, EBX;
        mov c, ECX;
        mov d, EDX;
		popad
            popfd
    }
	CpuVendor = std::string((const char*)&b, 4);
    CpuVendor += std::string((const char*)&d, 4);
    CpuVendor += std::string((const char*)&c, 4);
	for (int i = 0x80000002; i < 0x80000005; ++i)
    {
		__asm
        {
			pushfd
			pushad
			mov EAX, i;
			cpuid;
			mov a, EAX;
			mov b, EBX;
            mov c, ECX;
            mov d, EDX;
            popad
                popfd
        }
        CpuBrand += std::string((const char*)&a, 4);
        CpuBrand += std::string((const char*)&b, 4);
        CpuBrand += std::string((const char*)&c, 4);
        CpuBrand += std::string((const char*)&d, 4);
    }
	std::string cpu_hwid = md5(CpuVendor + CpuBrand);
	std::string gHWID = md5(pc.baseboard_manufacturer + pc.baseboard_product + pc.bios_vendor + cpu_hwid);
	return gHWID;
}

std::string GenRawHWID()
{
	PC_DATA pc;
	auto baseBoardInfo = GetBaseBoardInfo();
	pc.baseboard_manufacturer = std::move(baseBoardInfo.Manufacturer);
	pc.baseboard_product = std::move(baseBoardInfo.Product);

	auto biosInfo = GetBiosInfo();
    pc.bios_vendor = std::move(biosInfo.Vendor);
	pc.bios_version = std::move(biosInfo.Version);
	pc.bios_release_date = std::move(biosInfo.ReleaseDate);

	unsigned int InfoType = 0; unsigned int a, b, c, d;
    std::string CpuVendor, CpuBrand;
    __asm
	{
        pushfd
        pushad
		mov eax, InfoType
        cpuid
        mov a, EAX;
		mov b, EBX;
        mov c, ECX;
        mov d, EDX;
		popad
            popfd
    }
	CpuVendor = std::string((const char*)&b, 4);
    CpuVendor += std::string((const char*)&d, 4);
    CpuVendor += std::string((const char*)&c, 4);
	for (int i = 0x80000002; i < 0x80000005; ++i)
    {
		__asm
        {
			pushfd
			pushad
			mov EAX, i;
			cpuid;
			mov a, EAX;
			mov b, EBX;
            mov c, ECX;
            mov d, EDX;
            popad
                popfd
        }
        CpuBrand += std::string((const char*)&a, 4);
        CpuBrand += std::string((const char*)&b, 4);
        CpuBrand += std::string((const char*)&c, 4);
        CpuBrand += std::string((const char*)&d, 4);
    }
	std::string raw_hwid = xorstr_("BaseBoardManufacterer: ") + pc.baseboard_manufacturer + xorstr_(" | BaseBoardProduct: ") + pc.baseboard_product + xorstr_(" | BiosVendor: ") + pc.bios_vendor + xorstr_(" | CpuVendor: ") + CpuVendor + xorstr_(" | CpuBrand: ") + CpuBrand;
	return raw_hwid;
}

