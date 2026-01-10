#include "AnsiRegistry.h"
#include <windows.h>
#pragma comment (lib, "advapi32")
CAnsiRegistry::CAnsiRegistry(HKEY hKey, LPCSTR lpSubKey, bool mode)
{
	_hKey = 0;
	if (RegOpenKeyExA(hKey, lpSubKey, 0, KEY_ALL_ACCESS, &_hKey) != ERROR_SUCCESS)
	{ 
		if (mode == true) RegCreateKeyExA(hKey, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &_hKey, NULL);
	}
	else error_success = true;
}
CAnsiRegistry::~CAnsiRegistry()
{
	if (_hKey != NULL)
	RegCloseKey(_hKey);
}
bool CAnsiRegistry::GetRegister(LPCSTR lpName, DWORD dwType, void* outBuf, DWORD size)
{
	if (outBuf == nullptr || _hKey == NULL)
		return false;
	if (!strlen(lpName))
		return false;
	if (!AutoSizeRead(dwType, outBuf, size))
		return false;

	return !RegQueryValueExA(_hKey, lpName, NULL, &dwType, (byte*)outBuf, &size);
}
bool CAnsiRegistry::SetRegister(LPCSTR lpName, DWORD dwType, void* inBuf, DWORD size)
{
	if (inBuf == nullptr || _hKey == NULL)
		return false;
	if (!strlen(lpName))
		return false;
	if (!AutoSizeWrite(dwType, inBuf, size))
		return false;

	return !RegSetValueExA(_hKey, lpName, 0, dwType, (byte*)inBuf, size);
}

bool CAnsiRegistry::GetRegisterDefault(LPSTR outBuf, LONG maxSize)
{
	if (outBuf == nullptr || _hKey == NULL)
		return false;

	return !RegQueryValueA(_hKey, NULL, (LPSTR)outBuf, &maxSize);
}
bool CAnsiRegistry::SetRegisterDefault(LPCSTR inBuf, bool long_string)
{
	if (inBuf == nullptr || _hKey == NULL)
		return false;
	bool rslt = false;
	if (!long_string) rslt = RegSetValueA(_hKey, NULL, REG_SZ, inBuf, strlen(inBuf));
	else rslt = RegSetValueA(_hKey, NULL, REG_MULTI_SZ, inBuf, strlen(inBuf));
	return rslt;
}
bool CAnsiRegistry::DeleteRegister(LPCSTR lpName)
{
	if (_hKey == NULL)
		return false;
	if (!strlen(lpName))
		return false;
	return !RegDeleteKeyEx(_hKey, lpName, KEY_WOW64_32KEY, NULL);
}

bool CAnsiRegistry::AutoSizeWrite(DWORD dwType, void* inBuf, DWORD &size)
{
	if (!size){
		switch (dwType)
		{
		case REG_BINARY:
			size = 1;
			break;
		case REG_DWORD:
			size = 4;
			break;
		case REG_DWORD_BIG_ENDIAN:
			size = 4;
			break;
		case REG_QWORD:
			size = 8;
			break;
		case REG_EXPAND_SZ:
			break;
		case REG_LINK:
			break;
		case REG_MULTI_SZ:
			size = strlen((LPCSTR)inBuf);
			break;
		case REG_SZ:
			size = strlen((LPCSTR)inBuf);
			break;
		case REG_NONE:
		default:
			return false;
		}
	}
	return true;
}
bool CAnsiRegistry::AutoSizeRead(DWORD dwType, void* outBuf, DWORD &size)
{
	if (!size){
		switch (dwType)
		{
		case REG_BINARY:
			size = 1;
			break;
		case REG_DWORD:
			size = 4;
			break;
		case REG_DWORD_BIG_ENDIAN:
			size = 4;
			break;
		case REG_QWORD:
			size = 8;
			break;
		case REG_EXPAND_SZ:
			break;
		case REG_LINK:
			break;
		case REG_MULTI_SZ:
			break;
		case REG_SZ:
			break;
		case REG_NONE:
			break;
		default:
			return false;
		}
	}
	return true;
}
CEasyRegistryAnsi::CEasyRegistryAnsi(HKEY hKey, LPCSTR lpSubKey, bool mode) : CAnsiRegistry(hKey, lpSubKey, mode)
{
	no_error = true;
}
CEasyRegistryAnsi::~CEasyRegistryAnsi()
{

}
void CEasyRegistryAnsi::DeleteKey(LPCSTR lpName)
{
	no_error = DeleteRegister(lpName);
}
void CEasyRegistryAnsi::WriteString(LPCSTR lpName, LPCSTR lpString, bool multi_sz, ...)
{
	va_list ap;
	char    *szStr = new char[strlen(lpString) * 2 + 1024];
	va_start(ap, lpString);
	vsprintf(szStr, lpString, ap);
	va_end(ap);

	if (!multi_sz) no_error = SetRegister(lpName, REG_SZ, szStr, strlen(szStr));
	else no_error = SetRegister(lpName, REG_MULTI_SZ, szStr, strlen(szStr));
	delete[] szStr;
}
std::string CEasyRegistryAnsi::ReadString(LPCSTR lpName, bool multi_sz)
{
	char szStr[0x1000];
	if (!multi_sz) no_error = GetRegister(lpName, REG_SZ, szStr, 0x1000);
	else no_error = GetRegister(lpName, REG_MULTI_SZ, szStr, 0x1000);
	return szStr;
}

void CEasyRegistryAnsi::WriteInteger(LPCSTR lpName, int value)
{
	no_error = SetRegister(lpName, REG_DWORD, &value, 0);
}
int CEasyRegistryAnsi::ReadInteger(LPCSTR lpName)
{
	int value;
	no_error = GetRegister(lpName, REG_DWORD, &value, 0);
	return value;
}

void CEasyRegistryAnsi::WriteFloat(LPCSTR lpName, float value)
{
	no_error = SetRegister(lpName, REG_DWORD, &value, 0);
}
float CEasyRegistryAnsi::ReadFloat(LPCSTR lpName)
{
	float value;
	no_error = GetRegister(lpName, REG_DWORD, &value, 0);
	return value;
}

void CEasyRegistryAnsi::WriteLongLong(LPCSTR lpName, long long value)
{
	no_error = SetRegister(lpName, REG_QWORD, &value, 0);
}
long long CEasyRegistryAnsi::ReadLongLong(LPCSTR lpName)
{
	long long value;
	no_error = GetRegister(lpName, REG_QWORD, &value, 0);
	return value;
}

void CEasyRegistryAnsi::WriteDouble(LPCSTR lpName, double value)
{
	no_error = SetRegister(lpName, REG_QWORD, &value, 0);
}
double CEasyRegistryAnsi::ReadDouble(LPCSTR lpName)
{
	double value;
	no_error = GetRegister(lpName, REG_QWORD, &value, 0);
	return value;
}

bool CEasyRegistryAnsi::IsError()
{
	return !no_error;
}

bool CEasyRegistryAnsi::ErrorSuccess()
{
	if (error_success == true) return true;
	return false;
}