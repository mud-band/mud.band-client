#include <windows.h>
#include <tlhelp32.h>
#include <msi.h>
#include <msiquery.h>

#ifdef __cplusplus
extern "C" {
#endif

UINT __stdcall KillMudBandProcesses(MSIHANDLE hInstall)
{
    HANDLE hSnapshot;
    PROCESSENTRY32W pe32;
    BOOL bContinue;
    UINT result = ERROR_SUCCESS;

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return ERROR_INSTALL_FAILURE;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32W);
    bContinue = Process32FirstW(hSnapshot, &pe32);

    while (bContinue) {
        if (_wcsicmp(pe32.szExeFile, L"mudband_ui.exe") == 0 ||
            _wcsicmp(pe32.szExeFile, L"mudband.exe") == 0) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
            if (hProcess != NULL) {
                if (!TerminateProcess(hProcess, 0)) {
                    result = ERROR_INSTALL_FAILURE;
                }
                CloseHandle(hProcess);
            }
        }
        bContinue = Process32NextW(hSnapshot, &pe32);
    }

    CloseHandle(hSnapshot);
    return result;
}

#ifdef __cplusplus
}
#endif 