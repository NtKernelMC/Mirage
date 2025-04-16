#include "Registry.h"
#include <windows.h>
#include <stdarg.h>
#include <wchar.h>
#pragma comment (lib, "advapi32")

CRegistry::CRegistry(HKEY hKey, LPCWSTR lpSubKey, bool mode)
{
    _hKey = 0;
    if (RegOpenKeyExW(hKey, lpSubKey, 0, KEY_ALL_ACCESS, &_hKey) != ERROR_SUCCESS)
    {
        if (mode == true)
            RegCreateKeyExW(hKey, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &_hKey, NULL);
    }
    else {
        error_success = true;
    }
}

CRegistry::~CRegistry()
{
    if (_hKey != NULL)
        RegCloseKey(_hKey);
}

bool CRegistry::GetRegister(LPCWSTR lpName, DWORD dwType, void* outBuf, DWORD size)
{
    if (outBuf == nullptr || _hKey == NULL)
        return false;
    if (!wcslen(lpName))
        return false;
    if (!AutoSizeRead(dwType, outBuf, size))
        return false;

    return (RegQueryValueExW(_hKey, lpName, NULL, &dwType, reinterpret_cast<BYTE*>(outBuf), &size) == ERROR_SUCCESS);
}

bool CRegistry::SetRegister(LPCWSTR lpName, DWORD dwType, void* inBuf, DWORD size)
{
    if (inBuf == nullptr || _hKey == NULL)
        return false;
    if (!wcslen(lpName))
        return false;
    if (!AutoSizeWrite(dwType, inBuf, size))
        return false;

    return (RegSetValueExW(_hKey, lpName, 0, dwType, reinterpret_cast<BYTE*>(inBuf), size) == ERROR_SUCCESS);
}

bool CRegistry::GetRegisterDefault(LPWSTR outBuf, LONG maxSize)
{
    if (outBuf == nullptr || _hKey == NULL)
        return false;

    return (RegQueryValueW(_hKey, NULL, outBuf, &maxSize) == ERROR_SUCCESS);
}

bool CRegistry::SetRegisterDefault(LPCWSTR inBuf, bool long_string)
{
    if (inBuf == nullptr || _hKey == NULL)
        return false;
    bool rslt = false;
    if (!long_string)
        rslt = (RegSetValueW(_hKey, NULL, REG_SZ, inBuf, static_cast<DWORD>(wcslen(inBuf) * sizeof(wchar_t))) == ERROR_SUCCESS);
    else
        rslt = (RegSetValueW(_hKey, NULL, REG_MULTI_SZ, inBuf, static_cast<DWORD>(wcslen(inBuf) * sizeof(wchar_t))) == ERROR_SUCCESS);
    return rslt;
}

bool CRegistry::DeleteRegister(LPCWSTR lpName)
{
    if (_hKey == NULL)
        return false;
    if (!wcslen(lpName))
        return false;
    return (RegDeleteKeyExW(_hKey, lpName, KEY_WOW64_32KEY, 0) == ERROR_SUCCESS);
}

bool CRegistry::AutoSizeWrite(DWORD dwType, void* inBuf, DWORD &size)
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
            size = static_cast<DWORD>(wcslen(reinterpret_cast<LPCWSTR>(inBuf)) * sizeof(wchar_t));
            break;
        case REG_SZ:
            size = static_cast<DWORD>(wcslen(reinterpret_cast<LPCWSTR>(inBuf)) * sizeof(wchar_t));
            break;
        case REG_NONE:
        default:
            return false;
        }
    }
    return true;
}

bool CRegistry::AutoSizeRead(DWORD dwType, void* outBuf, DWORD &size)
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

CEasyRegistry::CEasyRegistry(HKEY hKey, LPCWSTR lpSubKey, bool mode) : CRegistry(hKey, lpSubKey, mode)
{
    no_error = true;
}

CEasyRegistry::~CEasyRegistry()
{
}

void CEasyRegistry::DeleteKey(LPCWSTR lpName)
{
    no_error = DeleteRegister(lpName);
}

void CEasyRegistry::WriteString(LPCWSTR lpName, LPCWSTR lpString, bool multi_sz, ...)
{
	va_list ap;
	size_t len = wcslen(lpString);
	// Выделяем буфер для wide строки
	wchar_t* szStr = new wchar_t[len * 2 + 1024];
	va_start(ap, lpString);
	vswprintf_s(szStr, len * 2 + 1024, lpString, ap);
	va_end(ap);

	if (!multi_sz)
		no_error = SetRegister(lpName, REG_SZ, szStr, static_cast<DWORD>(wcslen(szStr) * sizeof(wchar_t)));
	else
		no_error = SetRegister(lpName, REG_MULTI_SZ, szStr, static_cast<DWORD>(wcslen(szStr) * sizeof(wchar_t)));
	delete[] szStr;
}

std::wstring CEasyRegistry::ReadString(LPCWSTR lpName, bool multi_sz)
{
    wchar_t szStr[0x1000] = { 0 };
    if (!multi_sz)
        no_error = GetRegister(lpName, REG_SZ, szStr, sizeof(szStr));
    else
        no_error = GetRegister(lpName, REG_MULTI_SZ, szStr, sizeof(szStr));
    return szStr;
}

void CEasyRegistry::WriteInteger(LPCWSTR lpName, int value)
{
    no_error = SetRegister(lpName, REG_DWORD, &value, sizeof(value));
}

int CEasyRegistry::ReadInteger(LPCWSTR lpName)
{
    int value = 0;
    no_error = GetRegister(lpName, REG_DWORD, &value, sizeof(value));
    return value;
}

void CEasyRegistry::WriteFloat(LPCWSTR lpName, float value)
{
    no_error = SetRegister(lpName, REG_DWORD, &value, sizeof(value));
}

float CEasyRegistry::ReadFloat(LPCWSTR lpName)
{
    float value = 0;
    no_error = GetRegister(lpName, REG_DWORD, &value, sizeof(value));
    return value;
}

void CEasyRegistry::WriteLongLong(LPCWSTR lpName, long long value)
{
    no_error = SetRegister(lpName, REG_QWORD, &value, sizeof(value));
}

long long CEasyRegistry::ReadLongLong(LPCWSTR lpName)
{
    long long value = 0;
    no_error = GetRegister(lpName, REG_QWORD, &value, sizeof(value));
    return value;
}

void CEasyRegistry::WriteDouble(LPCWSTR lpName, double value)
{
    no_error = SetRegister(lpName, REG_QWORD, &value, sizeof(value));
}

double CEasyRegistry::ReadDouble(LPCWSTR lpName)
{
    double value = 0;
    no_error = GetRegister(lpName, REG_QWORD, &value, sizeof(value));
    return value;
}

bool CEasyRegistry::IsError()
{
    return !no_error;
}

bool CEasyRegistry::ErrorSuccess()
{
    return error_success;
}
