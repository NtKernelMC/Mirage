#pragma once
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <windows.h>
#include <assert.h>
#include <process.h>

typedef BYTE byte;

class CRegistry
{
protected:
    HKEY _hKey;
    bool error_success;
    bool GetRegister(LPCWSTR lpName, DWORD dwType, void* outBuf, DWORD size);
    bool SetRegister(LPCWSTR lpName, DWORD dwType, void* inBuf, DWORD size);
    bool GetRegisterDefault(LPWSTR outBuf, LONG maxSize);
    bool SetRegisterDefault(LPCWSTR inBuf, bool long_string = false);
    bool DeleteRegister(LPCWSTR lpName);
    bool AutoSizeWrite(DWORD dwType, void* inBuf, DWORD &size);
    bool AutoSizeRead(DWORD dwType, void* outBuf, DWORD &size);
    CRegistry(HKEY hKey, LPCWSTR lpSubKey, bool mode);
    ~CRegistry();
};

class CEasyRegistry : CRegistry
{
public:
    bool no_error;
    bool ErrorSuccess();
    void WriteString(LPCWSTR lpName, LPCWSTR lpString, bool multi_sz = false, ...);
    std::wstring ReadString(LPCWSTR lpName, bool multi_sz = false);
    void WriteInteger(LPCWSTR lpName, int value);
    int ReadInteger(LPCWSTR lpName);
    void WriteFloat(LPCWSTR lpName, float value);
    float ReadFloat(LPCWSTR lpName);
    void WriteLongLong(LPCWSTR lpName, long long value);
    long long ReadLongLong(LPCWSTR lpName);
    void WriteDouble(LPCWSTR lpName, double value);
    double ReadDouble(LPCWSTR lpName);
    void DeleteKey(LPCWSTR lpName);
    bool IsError();
    CEasyRegistry(HKEY hKey, LPCWSTR lpSubKey, bool mode);
    ~CEasyRegistry();
};
