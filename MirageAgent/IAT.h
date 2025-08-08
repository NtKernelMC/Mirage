#include <imagehlp.h>
#pragma comment(lib, "ImageHlp.lib")
/**
 * @brief  Overwrite 4 bytes at destAddr by given newOffset
 * @param  destAddr: An address where overwritten
 * @param  newValue: A new value
 * @return TRUE on success, otherwise FALSE
 */
int forceWrite4(DWORD* destAddr, DWORD newValue)
{
    DWORD oldProtect;
    if (VirtualProtect(destAddr, sizeof(DWORD), PAGE_READWRITE, &oldProtect)) {
        *destAddr = newValue;
        VirtualProtect(destAddr, sizeof(DWORD), oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), destAddr, sizeof(DWORD));
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief  Replace target function by hook function by writing IAT.
 * @param  modName: A module name. If NULL, hook calling process(.exe file)
 * @param  targetName: A function name to be replaced
 * @param  hookFunc: An address of replacement function
 * @param  origFunc: __out An original functions address to be set
 * @return TRUE on success, otherwise FALSE
 */
int hookIATwithName(const char* modName, const char* targetName, DWORD hookFunc, DWORD* origFunc)
{
    HMODULE hMod = NULL;
    ULONG   size = 0;
    PIMAGE_IMPORT_DESCRIPTOR  pImportDesc = NULL;
    PIMAGE_THUNK_DATA         pNameTable = NULL;
    PIMAGE_THUNK_DATA         pAddressTable = NULL;
    PIMAGE_IMPORT_BY_NAME     pImportByName = NULL;
    DWORD* pImportedAddr = 0;
    char* importedName = NULL;
    int found = FALSE;

    hMod = GetModuleHandleA(modName);
    pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData((PVOID)hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);

    if (pImportDesc == NULL) {
        //LogInFile(LOG_NAME, "ERROR: Import Directory not found\n");
        return FALSE;
    }

    // Repeat a count of DLLs
    while (pImportDesc->Name && !found) {
        char* dllName = (char*)((DWORD)hMod + pImportDesc->Name);
        //LogInFile(LOG_NAME, "%s: \n", dllName);

        pNameTable = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImportDesc->OriginalFirstThunk);
        pAddressTable = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImportDesc->FirstThunk);

        // Repeat a count of functions
        while (pNameTable->u1.Function && !found) {
            pImportedAddr = &(pAddressTable->u1.Function);
            //LogInFile(LOG_NAME, "imported: 0x%x, IAT addr: 0x%x, func first4: 0x%x \n",
            //        *pImportedAddr, pImportedAddr, *((DWORD *) *pImportedAddr));

            if (pNameTable->u1.AddressOfData & 0x80000000) {
                // シンボルが序数情報の場合
                DWORD dwOrd = pNameTable->u1.AddressOfData ^ 0x80000000;
                //LogInFile(LOG_NAME, "Ordinal %d (0x%x)\n", dwOrd, dwOrd);
            }
            else {
                // シンボルが名前情報の場合
                pImportByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hMod + pNameTable->u1.AddressOfData);
                importedName = (char*)(pImportByName->Name);
                //LogInFile(LOG_NAME, "[%s]\n", importedName);

                if (_stricmp(targetName, importedName) == 0) {
                    //LogInFile(LOG_NAME, "%s: imported: 0x%x, IAT addr: 0x%x, func first4: 0x%x \n",
                        //importedName, *pImportedAddr, pImportedAddr, *((DWORD*)*pImportedAddr));
                    found = TRUE;
                    break;
                }
            }
            pNameTable++;
            pAddressTable++;
        }
        if (!found) {
            pImportDesc++;
        }
    }

    if (!found || pImportedAddr == 0 || hookFunc == 0) {
        return FALSE;
    }

    *origFunc = *pImportedAddr;
    forceWrite4(pImportedAddr, hookFunc);
    return TRUE;
}

/**
 * @brief  Replace target function by hook function by writing IAT.
 * @param  modName: A module name. If NULL, hook calling process(.exe file)
 * @param  targetFunc: An address of function to be replaced
 * @param  hookFunc: An address of replacement function
 * @param  origFunc: __out An original functions address to be set
 * @return TRUE on success, otherwise FALSE
 */
int hookIATwithAddress(const char* modName, DWORD targetFunc, DWORD hookFunc, DWORD* origFunc)
{
    HMODULE hMod = NULL;
    ULONG   size = 0;
    PIMAGE_IMPORT_DESCRIPTOR  pImportDesc = NULL;
    PIMAGE_THUNK_DATA         pNameTable = NULL;
    PIMAGE_THUNK_DATA         pAddressTable = NULL;
    PIMAGE_IMPORT_BY_NAME     pImportByName = NULL;
    DWORD* pImportedAddr = 0;
    char* importedName = NULL;
    int found = FALSE;

    hMod = GetModuleHandleA(modName);
    pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData((PVOID)hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);

    if (pImportDesc == NULL) {
        //LogInFile(LOG_NAME, "ERROR: Import Directory not found\n");
        return FALSE;
    }

    // Repeat a count of DLLs
    while (pImportDesc->Name && !found) {
        char* dllName = (char*)((DWORD)hMod + pImportDesc->Name);
        //LogInFile(LOG_NAME, "%s: \n", dllName);

        pNameTable = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImportDesc->OriginalFirstThunk);
        pAddressTable = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImportDesc->FirstThunk);

        // Repeat a count of functions
        while (pAddressTable->u1.Function && !found) {
            pImportedAddr = &(pAddressTable->u1.Function);
            //LogInFile(LOG_NAME, "imported: 0x%x, IAT addr: 0x%x, func first4: 0x%x \n",
            //        *pImportedAddr, pImportedAddr, *((DWORD *) *pImportedAddr));

            if (*pImportedAddr == targetFunc) {
                //LogInFile(LOG_NAME, "--: imported: 0x%x, IAT addr: 0x%x, func first4: 0x%x \n",
                    //*pImportedAddr, pImportedAddr, *((DWORD*)*pImportedAddr));
                found = TRUE;
                break;
            }
            pNameTable++;
            pAddressTable++;
        }
        if (!found) {
            pImportDesc++;
        }
    }

    if (!found || pImportedAddr == 0 || hookFunc == 0) {
        return FALSE;
    }

    *origFunc = *pImportedAddr;
    forceWrite4(pImportedAddr, hookFunc);
    return TRUE;
}

/**
 * @brief  Return an address of given apiName.
 * @param  modName: A module name
 * @param  apiName: An api name
 * @return An address of apiName
 */
DWORD getApiAddress(const char* modName, const char* apiName)
{
    HMODULE hMod;
    hMod = GetModuleHandleA(modName);
    FARPROC fproc = GetProcAddress(hMod, apiName);

    //LogInFile(LOG_NAME, "%s: get proc: 0x%x, hMod: 0x%x \n", apiName, fproc, hMod);
    return (DWORD)fproc;
}
