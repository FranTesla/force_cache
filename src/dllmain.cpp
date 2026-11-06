#include <windows.h>

HMODULE hSystemVersionDll = NULL;

typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t fpCreateFileW_original = nullptr;

HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile
) {
    DWORD dwOriginalFlags = dwFlagsAndAttributes;

    // Only modify flags for READ operations
    if ((dwDesiredAccess & GENERIC_WRITE) == 0 && (dwDesiredAccess & GENERIC_READ) != 0) {
        // Force cache for read operations - remove any bypass/cacheless flags
        if (dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) {
            dwFlagsAndAttributes &= ~FILE_FLAG_NO_BUFFERING;
        }
    }

    return fpCreateFileW_original(lpFileName, dwDesiredAccess, dwShareMode,
                                  lpSecurityAttributes, dwCreationDisposition,
                                  dwFlagsAndAttributes, hTemplateFile);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        wchar_t sysPath[MAX_PATH];
        if (!GetSystemDirectoryW(sysPath, MAX_PATH)) return FALSE;
        wcscat_s(sysPath, L"\\version.dll");

        hSystemVersionDll = LoadLibraryExW(sysPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!hSystemVersionDll) return FALSE;

        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        if (hKernel32) {
            void* targetAddress = GetProcAddress(hKernel32, "CreateFileW");
            if (targetAddress) {
                fpCreateFileW_original = (CreateFileW_t)targetAddress;
            }
        }
        break;
    }
    case DLL_PROCESS_DETACH: {
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        break;
    }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) HANDLE WINAPI CreateFileW_Proxy(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile
) {
    if (fpCreateFileW_original) {
        return fpCreateFileW_original(lpFileName, dwDesiredAccess, dwShareMode,
                                      lpSecurityAttributes, dwCreationDisposition,
                                      dwFlagsAndAttributes, hTemplateFile);
    }
    return INVALID_HANDLE_VALUE;
}