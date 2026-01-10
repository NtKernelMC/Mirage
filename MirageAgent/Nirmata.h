#pragma once
#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdio.h>
#include <direct.h>
#include <string>
#include <thread>
#include <iostream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <cwctype>
#include "AnsiRegistry.h"
#include <winevt.h>
#pragma comment(lib, "wevtapi.lib")
#include <filesystem>
namespace fs = std::filesystem;

namespace Nirmata
{
    using TimeCache = std::unordered_map<std::wstring, fs::file_time_type>;
    TimeCache g_originalTimes;

    std::wstring ToLowerWide(const std::wstring& input)
    {
        std::wstring lowered = input;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) { return std::towlower(ch); });
        return lowered;
    }

    std::wstring ToWide(const std::string& input)
    {
        return std::wstring(input.begin(), input.end());
    }

    fs::path NormalizePath(const fs::path& path)
    {
        std::error_code ec;
        fs::path normalized = fs::weakly_canonical(path, ec);
        if (ec)
        {
            normalized = fs::absolute(path, ec);
        }
        if (ec)
        {
            normalized = path;
        }
        return normalized;
    }

    bool IsSameOrSubPath(const std::wstring& parent, const std::wstring& child)
    {
        const std::wstring lowerParent = ToLowerWide(parent);
        const std::wstring lowerChild = ToLowerWide(child);
        if (lowerChild.size() < lowerParent.size())
        {
            return false;
        }
        if (lowerChild.compare(0, lowerParent.size(), lowerParent) != 0)
        {
            return false;
        }
        if (lowerChild.size() == lowerParent.size())
        {
            return true;
        }
        const wchar_t separator = lowerChild[lowerParent.size()];
        return separator == L'\\' || separator == L'/';
    }

    void SnapshotDirectoryTimes(const fs::path& folderPath, TimeCache& cache)
    {
        if (folderPath.empty())
        {
            return;
        }
        std::error_code ec;
        if (!fs::exists(folderPath, ec) || !fs::is_directory(folderPath, ec))
        {
            return;
        }

        auto recordTime = [&](const fs::path& target)
            {
                std::error_code tsErr;
                auto ts = fs::last_write_time(target, tsErr);
                if (!tsErr)
                {
                    cache[NormalizePath(target).wstring()] = ts;
                }
            };

        const fs::path normalized = NormalizePath(folderPath);
        recordTime(normalized);

        fs::recursive_directory_iterator it(normalized, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            if (it->is_directory())
            {
                recordTime(it->path());
            }
        }

        if (ec)
        {
            std::wcerr << L"Не удалось сохранить время изменения для " << normalized.wstring() << L": " << ToWide(ec.message()) << std::endl;
        }
    }

    void RestoreDirectoryTimes(const fs::path& folderPath, const TimeCache& cache)
    {
        if (folderPath.empty())
        {
            return;
        }
        const std::wstring normalizedRoot = NormalizePath(folderPath).wstring();
        if (normalizedRoot.empty())
        {
            return;
        }

        for (const auto& entry : cache)
        {
            if (!IsSameOrSubPath(normalizedRoot, entry.first))
            {
                continue;
            }
            std::error_code ec;
            if (fs::exists(entry.first, ec))
            {
                fs::last_write_time(entry.first, entry.second, ec);
                if (ec)
                {
                    std::wcerr << L"Не удалось вернуть время для " << entry.first << L": " << ToWide(ec.message()) << std::endl;
                }
            }
        }
    }

    void ClearDirectoryPreserveRoot(const fs::path& dirPath)
    {
        if (dirPath.empty())
        {
            return;
        }
        std::error_code ec;
        if (!fs::exists(dirPath, ec))
        {
            fs::create_directories(dirPath, ec);
            if (ec)
            {
                std::wcerr << L"Не удалось создать директорию " << dirPath.wstring() << L": " << ToWide(ec.message()) << std::endl;
                return;
            }
        }

        fs::directory_iterator it(dirPath, fs::directory_options::skip_permission_denied, ec);
        fs::directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            std::error_code removeErr;
            fs::remove_all(it->path(), removeErr);
            if (removeErr)
            {
                std::wcerr << L"Не удалось очистить " << it->path().wstring() << L": " << removeErr.message().c_str() << std::endl;
            }
        }

        if (ec)
        {
            std::wcerr << L"Не удалось перечислить содержимое " << dirPath.wstring() << L": " << ToWide(ec.message()) << std::endl;
        }
    }

    bool findStringIC(const std::string& str, const std::string& search)
    {
        auto it = std::search(
            str.begin(), str.end(),
            search.begin(), search.end(),
            [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
        );
        return (it != str.end());
    }
    void deleteAllFilesInFolder(const std::string& folderPath)
    {
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
        {
            std::cerr << "Ошибка: путь не существует или не является директорией" << std::endl;
            return;
        }
        try
        {
            for (const auto& entry : fs::directory_iterator(folderPath))
            {
                if (fs::is_regular_file(entry))
                { // Проверяем, является ли объект файлом
                    fs::remove(entry.path());
                    std::cout << "Удален файл: " << entry.path() << std::endl;
                }
                else
                {
                    // Если нужно, здесь можно добавить логику для обработки папок
                    std::cout << "Пропущена папка: " << entry.path() << std::endl;
                }
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Ошибка файловой системы: " << e.what() << std::endl;
        }
    }

    // Получаем имя пользователя
    std::string getUserName()
    {
        char userName[256];
        DWORD userNameSize = sizeof(userName);
        if (GetUserNameA(userName, &userNameSize))
        {
            return std::string(userName);
        }
        else
        {
            //std::cerr << "Ошибка при получении имени пользователя." << std::endl;
            return "";
        }
    }

    LONG DeleteSubkeys(HKEY hKeyParent, LPCSTR lpszSubkey)
    {
        HKEY hKey;
        LONG lResult = RegOpenKeyExA(hKeyParent, lpszSubkey, 0, KEY_READ | KEY_WRITE, &hKey);
        if (lResult != ERROR_SUCCESS) {
            return lResult;
        }

        FILETIME time;
        CHAR szBuffer[256];
        DWORD dwSize = sizeof(szBuffer) / sizeof(szBuffer[0]);
        while (RegEnumKeyExA(hKey, 0, szBuffer, &dwSize, NULL, NULL, NULL, &time) == ERROR_SUCCESS) {
            lResult = DeleteSubkeys(hKey, szBuffer);
            if (lResult != ERROR_SUCCESS) {
                // Не удалось удалить один из подключей
                RegCloseKey(hKey);
                return lResult;
            }
            dwSize = sizeof(szBuffer) / sizeof(szBuffer[0]);
        }

        RegCloseKey(hKey);

        return RegDeleteKeyA(hKeyParent, lpszSubkey);
    }

    void ClearOpenSavePidlMRU()
    {
        LPCSTR subkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
        }
    }

    void ClearLastVisitedPidlMRU()
    {
        LPCSTR subkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
        }
    }

    void ClearLastVisitedPidlMRULegacy()
    {
        LPCSTR subkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRULegacy";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
        }
    }

    void ClearBagMRU()
    {
        LPCSTR subkey = "SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\BagMRU";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
        }
    }
    bool ClearRecentDocs()
    {
        // Путь к ключу RecentDocs
        LPCSTR subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
            return false;
        }
        return true;
    }
    void ClearRunMRU()
    {
        LPCSTR subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU";
        LONG lResult = DeleteSubkeys(HKEY_CURRENT_USER, subkey);
        if (lResult == ERROR_SUCCESS)
        {
            std::wcout << L"Ключ реестра успешно очищен. " << subkey << std::endl;
        }
        else
        {
            std::wcout << L"Ошибка при очистке ключа реестра: " << lResult << " | " << subkey << std::endl;
        }
    }
    bool ClearEventsLog(LPCWSTR logName)
    {
        // Очищаем журнал событий
        if (!EvtClearLog(NULL, logName, NULL, 0))
        {
            wprintf(L"Не удалось очистить журнал событий '%s'. Ошибка: %lu\n", logName, GetLastError());
            return false;
        }
        else
        {
            wprintf(L"Журнал событий '%s' успешно очищен.\n", logName);
            return true;
        }
    }

    bool GetEventRecordId(EVT_HANDLE eventHandle, ULONGLONG& recordId)
    {
        EVT_VARIANT eventProperty = { 0 };
        DWORD bufferUsed = 0;

        const EVT_EVENT_PROPERTY_ID recordIdProp =
#ifdef EvtEventRecordId
            EvtEventRecordId
#else
            static_cast<EVT_EVENT_PROPERTY_ID>(2) // Fallback for older headers
#endif
            ;

        if (!EvtGetEventInfo(eventHandle, recordIdProp, sizeof(EVT_VARIANT), &eventProperty, &bufferUsed))
        {
            return false;
        }

        recordId = eventProperty.UInt64Val;
        return true;
    }

    bool RenderEventXml(EVT_HANDLE eventHandle, std::wstring& xmlOut)
    {
        DWORD bufferUsed = 0;
        DWORD propertyCount = 0;
        if (!EvtRender(NULL, eventHandle, EvtRenderEventXml, 0, NULL, &bufferUsed, &propertyCount))
        {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                return false;
            }
        }

        std::vector<wchar_t> buffer(bufferUsed / sizeof(wchar_t) + 1);
        if (!EvtRender(NULL, eventHandle, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &bufferUsed, &propertyCount))
        {
            return false;
        }

        buffer[bufferUsed / sizeof(wchar_t)] = L'\0';
        xmlOut.assign(buffer.data());
        return true;
    }

    bool ShouldRemoveExecutableEvent(const std::wstring& xmlContent, const std::vector<std::wstring>& allowedNamesLower)
    {
        const std::wstring lowered = ToLowerWide(xmlContent);
        if (lowered.find(L".exe") == std::wstring::npos)
        {
            return false;
        }

        for (const auto& allowed : allowedNamesLower)
        {
            if (lowered.find(allowed) != std::wstring::npos)
            {
                return false;
            }
        }
        return true;
    }

    bool CollectRecordIdsToKeep(const std::vector<std::wstring>& allowedNamesLower, std::vector<ULONGLONG>& idsToKeep)
    {
        EVT_HANDLE query = EvtQuery(NULL, L"Application", L"*", EvtQueryChannelPath);
        if (!query)
        {
            std::wcerr << L"Не удалось открыть журнал Application. Ошибка: " << GetLastError() << std::endl;
            return false;
        }

        EVT_HANDLE events[32];
        DWORD returned = 0;
        bool success = true;

        while (EvtNext(query, 32, events, 0, 0, &returned))
        {
            for (DWORD i = 0; i < returned; ++i)
            {
                std::wstring xml;
                bool dropEvent = false;
                if (RenderEventXml(events[i], xml))
                {
                    dropEvent = ShouldRemoveExecutableEvent(xml, allowedNamesLower);
                }

                ULONGLONG recordId = 0;
                if (GetEventRecordId(events[i], recordId) && !dropEvent)
                {
                    idsToKeep.push_back(recordId);
                }

                EvtClose(events[i]);
            }
        }

        const DWORD lastErr = GetLastError();
        if (lastErr != ERROR_NO_MORE_ITEMS)
        {
            std::wcerr << L"Ошибка при чтении журнала Application: " << lastErr << std::endl;
            success = false;
        }

        EvtClose(query);

        if (success)
        {
            std::sort(idsToKeep.begin(), idsToKeep.end());
            idsToKeep.erase(std::unique(idsToKeep.begin(), idsToKeep.end()), idsToKeep.end());
        }

        return success;
    }

    std::vector<std::pair<ULONGLONG, ULONGLONG>> BuildRecordRanges(const std::vector<ULONGLONG>& ids)
    {
        std::vector<std::pair<ULONGLONG, ULONGLONG>> ranges;
        if (ids.empty())
        {
            return ranges;
        }

        ULONGLONG start = ids.front();
        ULONGLONG prev = ids.front();

        for (size_t i = 1; i < ids.size(); ++i)
        {
            if (ids[i] == prev + 1)
            {
                prev = ids[i];
                continue;
            }
            ranges.emplace_back(start, prev);
            start = prev = ids[i];
        }

        ranges.emplace_back(start, prev);
        return ranges;
    }

    std::wstring BuildRangeQuery(const std::vector<std::pair<ULONGLONG, ULONGLONG>>& ranges)
    {
        if (ranges.empty())
        {
            return L"";
        }

        std::wstring query = L"*[System[";
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            const auto& range = ranges[i];
            if (range.first == range.second)
            {
                query += L"(EventRecordID=" + std::to_wstring(range.first) + L")";
            }
            else
            {
                query += L"(EventRecordID>=" + std::to_wstring(range.first) + L" and EventRecordID<=" + std::to_wstring(range.second) + L")";
            }

            if (i + 1 != ranges.size())
            {
                query += L" or ";
            }
        }

        query += L"]]";
        return query;
    }

    std::wstring CreateTemporaryEvtxPath()
    {
        wchar_t tempPath[MAX_PATH] = { 0 };
        if (GetTempPathW(MAX_PATH, tempPath) == 0)
        {
            return L"";
        }

        wchar_t tempFile[MAX_PATH] = { 0 };
        if (GetTempFileNameW(tempPath, L"nrm", 0, tempFile) == 0)
        {
            return L"";
        }

        std::wstring tempFilePath = tempFile;
        DeleteFileW(tempFilePath.c_str());
        tempFilePath += L".evtx";
        return tempFilePath;
    }

    std::wstring GetLogPhysicalPath(const std::wstring& logName)
    {
        EVT_HANDLE channel = EvtOpenChannelConfig(NULL, logName.c_str(), 0);
        if (!channel)
        {
            return L"";
        }

        DWORD bufferUsed = 0;
        if (!EvtGetChannelConfigProperty(channel, EvtChannelLoggingConfigLogFilePath, 0, 0, NULL, &bufferUsed))
        {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                EvtClose(channel);
                return L"";
            }
        }

        std::vector<BYTE> buffer(bufferUsed);
        if (!EvtGetChannelConfigProperty(channel, EvtChannelLoggingConfigLogFilePath, 0, static_cast<DWORD>(buffer.size()), reinterpret_cast<PEVT_VARIANT>(buffer.data()), &bufferUsed))
        {
            EvtClose(channel);
            return L"";
        }

        auto value = reinterpret_cast<EVT_VARIANT*>(buffer.data());
        std::wstring path = value->StringVal ? value->StringVal : L"";

        EvtClose(channel);
        return path;
    }

    bool TryCopyFilteredLog(const std::wstring& logName, const std::wstring& sourcePath)
    {
        const std::wstring destination = GetLogPhysicalPath(logName);
        if (destination.empty())
        {
            std::wcerr << L"Не удалось определить путь к файлу журнала " << logName << std::endl;
            return false;
        }

        std::error_code ec;
        fs::copy_file(sourcePath, destination, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::wcerr << L"Не удалось заменить журнал " << logName << L": " << ToWide(ec.message()) << std::endl;
            return false;
        }

        return true;
    }

    bool FilterExecutableStartsFromApplication(const std::vector<std::wstring>& allowedNames)
    {
        std::vector<std::wstring> allowedLower;
        allowedLower.reserve(allowedNames.size());
        for (const auto& name : allowedNames)
        {
            allowedLower.push_back(ToLowerWide(name));
        }

        std::vector<ULONGLONG> idsToKeep;
        if (!CollectRecordIdsToKeep(allowedLower, idsToKeep))
        {
            return false;
        }

        if (idsToKeep.empty())
        {
            std::wcerr << L"В журнале Application нет записей для сохранения. Выполняю полную очистку." << std::endl;
            return ClearEventsLog(L"Application");
        }

        const auto ranges = BuildRecordRanges(idsToKeep);
        const std::wstring query = BuildRangeQuery(ranges);
        if (query.empty())
        {
            return false;
        }

        const std::wstring tempPath = CreateTemporaryEvtxPath();
        if (tempPath.empty())
        {
            std::wcerr << L"Не удалось создать временный файл для журнала." << std::endl;
            return false;
        }

        if (!EvtExportLog(NULL, L"Application", query.c_str(), tempPath.c_str(), EvtExportLogChannelPath | EvtExportLogTolerateQueryErrors))
        {
            std::wcerr << L"Не удалось экспортировать отфильтрованный журнал Application. Ошибка: " << GetLastError() << std::endl;
            DeleteFileW(tempPath.c_str());
            return false;
        }

        bool copied = TryCopyFilteredLog(L"Application", tempPath);
        if (!copied)
        {
            // Попытка освободить файл журнала и повторить перезапись
            ClearEventsLog(L"Application");
            copied = TryCopyFilteredLog(L"Application", tempPath);
        }

        DeleteFileW(tempPath.c_str());

        if (copied)
        {
            std::wcout << L"Application журнал отфильтрован без удаления разрешенных событий." << std::endl;
        }
        else
        {
            std::wcerr << L"Не удалось заменить журнал Application, выполнена только очистка." << std::endl;
        }

        return copied;
    }
    bool deleteRegistryKey(HKEY hKeyRoot, const std::string& subKey)
    {
        HKEY hKey;
        if (RegOpenKeyExA(hKeyRoot, subKey.c_str(), 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            std::cerr << "Не удалось открыть ключ реестра: " << subKey << std::endl;
            return false;
        }

        // Рекурсивное удаление всех подключей
        char buffer[256];
        DWORD bufferSize = sizeof(buffer);
        while (RegEnumKeyExA(hKey, 0, buffer, &bufferSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            std::string currentSubKey = subKey + "\\" + buffer;
            if (!deleteRegistryKey(hKeyRoot, currentSubKey)) {
                RegCloseKey(hKey);
                return false;
            }
            bufferSize = sizeof(buffer);
        }

        RegCloseKey(hKey);

        // Удаление самого ключа
        if (RegDeleteKeyA(hKeyRoot, subKey.c_str()) != ERROR_SUCCESS) {
            std::cerr << "Не удалось удалить ключ реестра: " << subKey << std::endl;
            return false;
        }

        std::cout << "Ключ реестра успешно удален: " << subKey << std::endl;
        return true;
    }

    void CleanUserAssistArtifact()
    {
        deleteRegistryKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{CEBFF5CD-ACE2-4F4F-9178-9926F41749EA}\\Count");
        deleteRegistryKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\{F4E57C4B-2036-45F0-A9AB-443BCFE33D9F}\\Count");
    }
    void DeleteRecentAppsLegacy()
    {
        deleteRegistryKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Search\\RecentApps");
    }
    void CleanExplorerTracks()
    {
        if (!deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs")) {
            std::cerr << "Ошибка при удалении RecentDocs" << std::endl;
        }

        if (!deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\WordWheelQuery")) {
            std::cerr << "Ошибка при удалении WordWheelQuery" << std::endl;
        }

        if (!deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths")) {
            std::cerr << "Ошибка при удалении TypedPaths" << std::endl;
        }
    }
    void SetFolderModificationTime(const char* folderPath)
    {
        if (folderPath == nullptr || folderPath[0] == '\0')
        {
            return;
        }

        try
        {
            RestoreDirectoryTimes(folderPath, g_originalTimes);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Ошибка восстановления времени для " << folderPath << ": " << ex.what() << std::endl;
        }
    }
    int UseNirmata()
    {
        std::string removed = "";
        auto removeSubstring = [](const std::string& original, const std::string& toRemove)
            {
                std::string result = original;
                size_t pos = result.find(toRemove);
                if (pos != std::string::npos)
                {
                    result.erase(pos, toRemove.length());
                }
                return result;
            };
        std::string launcher_dir = "";
        std::string game_dir = "";
        CEasyRegistryAnsi* adreg = new CEasyRegistryAnsi(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\UKRAINEGTA: GLAB3\\Common", false);
        if (adreg != nullptr)
        {
            std::string lpath = adreg->ReadString("GTA:SA Path");
            game_dir = removeSubstring(lpath, "\\bin");
            if (lpath.length() > 5) removed = removeSubstring(lpath, "\\game");
            launcher_dir = removed;
            delete adreg;
        }

        const fs::path gamePath = game_dir;
        const fs::path launcherPath = launcher_dir;
        const fs::path mtaLogs = gamePath / "mta" / "logs";
        const fs::path mtaDumpsPrivate = gamePath / "mta" / "dumps" / "private";
        const fs::path mtaDumpsPublic = gamePath / "mta" / "dumps" / "public";

        SnapshotDirectoryTimes(gamePath, g_originalTimes);
        SnapshotDirectoryTimes(launcherPath, g_originalTimes);
        SnapshotDirectoryTimes(mtaLogs, g_originalTimes);
        SnapshotDirectoryTimes(mtaDumpsPrivate, g_originalTimes);
        SnapshotDirectoryTimes(mtaDumpsPublic, g_originalTimes);

        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\LUA3NG1N3");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\SU53RS500F3R");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\C3RB3RUS");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA222FAB");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA222FAR");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA222FAX");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA3CVV");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA9FAB");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA9RAB");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA9RRB");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CFA9UUH");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\CfxData");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\DRAM9283SB");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\FB0TM0D3");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\FctpEXE");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\JBEG914H12FA");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\JBEG914H12FD");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\LgbtEXE");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\MPK8372GBR3Q");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\SFWUM0D");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\srvGH0ST");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Stage6EXE");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\TeslaData");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\TTUEXE");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\ugta_spf");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Z3R0HVCK");
        deleteRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\ZbkData");
        deleteRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\MicrosoftUpdate_8246G");
        // Путь к папке Prefetch
        std::string prefetchPath = "C:\\Windows\\Prefetch\\";
        deleteAllFilesInFolder(prefetchPath);

        // Получаем имя пользователя и строим путь к папке Recent
        std::string userName = getUserName();
        std::string recentPath = "C:\\Users\\" + userName + "\\AppData\\Roaming\\Microsoft\\Windows\\Recent\\";
        deleteAllFilesInFolder(recentPath);

        // очистка истории недавних действий в винде
        DeleteRecentAppsLegacy();
        CleanUserAssistArtifact();
        ClearLastVisitedPidlMRU();
        ClearLastVisitedPidlMRULegacy();
        ClearOpenSavePidlMRU();
        ClearBagMRU();
        ClearRunMRU();
        CleanExplorerTracks();

        std::vector<std::wstring> applicationExeAllowList = { L"UKRAINE GTA.exe", L"UKRAINEGTA.exe", L"gta_sa.exe", L"proxy_sa.exe", L"updater.exe" };
        if (!FilterExecutableStartsFromApplication(applicationExeAllowList))
        {
            std::wcerr << L"Фильтрация журнала Application не удалась, выполняется полная очистка." << std::endl;
            ClearEventsLog(L"Application");
        }
        ClearEventsLog(L"System");
        ClearEventsLog(L"Security");

        CEasyRegistryAnsi* dreg = new CEasyRegistryAnsi(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\UKRAINEGTA: GLAB3\\Common", false);
        if (dreg != nullptr)
        {
            ClearDirectoryPreserveRoot(mtaLogs);
            ClearDirectoryPreserveRoot(mtaDumpsPrivate);
            ClearDirectoryPreserveRoot(mtaDumpsPublic);
            printf("Очищены мта логи и крашдампы!\n");
            delete dreg;
        }
        if (ClearRecentDocs())
        {
            std::cout << "RecentDocs успешно очищен." << std::endl;
        }
        else
        {
            std::cout << "Возникла ошибка при очистке RecentDocs." << std::endl;
        }
        SetFolderModificationTime(game_dir.c_str());
        SetFolderModificationTime(launcher_dir.c_str());
        printf("\n\nВсё! Хвосты подчищены :)\n\n");
        return 1;
    }
}