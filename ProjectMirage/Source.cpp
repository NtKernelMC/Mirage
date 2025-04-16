#include "Mirage.h"
BOOL InjectDLL(HANDLE hProcess, DWORD processPID, std::wstring filePath)
{
	LogInFile(LOG_NAME, xorstr_("Инжектим DLL-ку %ls ...\n"), filePath.c_str());
	std::string filePathStr = CvWideToAnsi(filePath);
    std::ifstream file(filePathStr, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LogInFile(LOG_NAME, xorstr_("Failed to open DLL file: %ls\n"), filePath.c_str());
        return FALSE;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> buffer(static_cast<size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        LogInFile(LOG_NAME, xorstr_("Failed to read DLL file: %ls\n"), filePath.c_str());
        return FALSE;
    }
    file.close();

    if (mirage.dll_injection_type == DllInjectionType::MMAP)
    {
        if (!ManualMapDll(hProcess, buffer.data(), static_cast<SIZE_T>(fileSize), false, false, false, false))
        {
            LogInFile(LOG_NAME, xorstr_("Failed to inject %ls\n"), filePath.c_str());
            return FALSE;
        }
		else LogInFile(LOG_NAME, xorstr_("DLL %ls injected successfully!\n"), filePath.c_str());
    }
    else if (mirage.dll_injection_type == DllInjectionType::SET_WINDOWS_HOOK)
    {
        // Новый способ поиска потока с окном: повторяем вызов до успешного нахождения, опрашивая каждые 100 мс
        DWORD threadID = 0;
        int retryCount = 0;
        while ((threadID = GetThreadWithWindow(processPID)) == 0)
        {
            Sleep(100);
            retryCount++;
            // Каждые 1 секунду (10 попыток) выводим сообщение в лог
            if (retryCount % 10 == 0)
            {
                LogInFile(LOG_NAME, xorstr_("Ожидание окна для процесса PID: %d...\n"), processPID);
            }
        }

        if (threadID == 0)  // хотя цикл гарантирует, что здесь будет ненулевое значение, добавляем проверку
        {
            LogInFile(LOG_NAME, xorstr_("Не удалось найти окно для процесса PID: %d\n"), processPID);
            return FALSE;
        }

        // Загружаем DLL по указанному пути с флагом DONT_RESOLVE_DLL_REFERENCES
        HMODULE hModule = LoadLibraryExA(filePathStr.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
        if (!hModule)
        {
            LogInFile(LOG_NAME, xorstr_("Не удалось загрузить DLL. Code: %d, Path: %ls\n"),
                GetLastError(), filePath.c_str());
            return FALSE;
        }

        // Получаем адрес экспортированной функции "NextHook"
        HOOKPROC hookProc = (HOOKPROC)GetProcAddress(hModule, xorstr_("NextHook"));
        if (!hookProc)
        {
            LogInFile(LOG_NAME, xorstr_("Не удалось найти экспорт NextHook. Code: %d, Path: %ls\n"),
                GetLastError(), filePath.c_str());
            return FALSE;
        }

        // Устанавливаем хук на поток найденного окна с помощью WH_GETMESSAGE
        HHOOK hook = SetWindowsHookExA(WH_GETMESSAGE, hookProc, hModule, threadID);
        if (!hook)
        {
            LogInFile(LOG_NAME, xorstr_("Не удалось установить хук. Code: %d\n"), GetLastError());
            return FALSE;
        }

        // Отправляем сообщение WM_NULL, чтобы инициировать срабатывание хукового кода
        PostThreadMessageA(threadID, WM_NULL, 0, 0);

        LogInFile(LOG_NAME, xorstr_("DLL %ls injected successfully via Windows Hook!\n"), filePath.c_str());
        LogInFile(LOG_NAME, xorstr_("Не закрывайте консоль DLL инжектора, иначе игру крашнит!\n"));
    }
	else LogInFile(LOG_NAME, xorstr_("Unknown DLL_INJECTION_TYPE\n"));
    return TRUE;
}
void SetMirageDirs()
{
    std::wstring currentDir = std::filesystem::current_path().wstring();
    std::wstring luaScriptsSubDir = currentDir + xorstr_(L"\\LuaScripts");
    namespace fs = std::filesystem;

    if (!fs::exists(luaScriptsSubDir))
    {
        if (!fs::create_directory(luaScriptsSubDir))
        {
            throw std::runtime_error(xorstr_("Не удалось создать директорию: LuaScripts"));
        }
    }

    lua_scripts_dir = luaScriptsSubDir;
    mapped_image_dir = currentDir;
    mirage_config_dir = currentDir + xorstr_(L"\\Mirage.cfg");

    CEasyRegistry* reg = new CEasyRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\MicrosoftUpdate_8246G", true);
    if (reg != nullptr)
    {
        reg->WriteString(xorstr_(L"MicrosoftDir"), lua_scripts_dir.c_str());
        reg->WriteString(xorstr_(L"WindowsDir"), mapped_image_dir.c_str());
        reg->WriteString(xorstr_(L"UpdateDir"), mirage_config_dir.c_str());
        delete reg;
    }
}
int main()
{
	SetConsoleTitleA(xorstr_("Mirage Injector by DroidZero"));
	setlocale(LC_ALL, xorstr_("Russian"));
	system(xorstr_("color 09"));
	SetMirageDirs();
    RemoveOldLog();
    ParseMirageConfig();
	LogInFile(LOG_NAME, xorstr_("Запускайте игру! Ожидаю запуск...\n"));
    int proc_id = GetGtaProc();
    if (proc_id == -1)
    {
        while ((proc_id = GetGtaProc()) == -1)
        {
            Sleep(60);
        }
    }
    LogInFile(LOG_NAME, xorstr_("Запущен процесс GTA:SA! PID: %d\n"), proc_id);
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_id);
    if (hProc != nullptr)
    {
		LogInFile(LOG_NAME, xorstr_("Открыт хендл процесса GTA:SA! PID: %d\n"), proc_id);
        InjectDLL(hProc, proc_id, mapped_image_dir + xorstr_(L"\\MirageAgent.dll"));
        CloseHandle(hProc);
    }
	else LogInFile(LOG_NAME, xorstr_("Не удалось открыть хендл процесса GTA:SA! PID: %d\n"), proc_id);
    while (true) Sleep(1000);
	return 1;
}