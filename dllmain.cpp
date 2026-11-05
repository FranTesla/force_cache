#include <windows.h>
#include "MinHook.h"

#pragma comment(lib, "libMinHook.x64.lib")

// Original function pointer for CreateFileW
typedef HANDLE(WINAPI* CreateFileW_t)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);

CreateFileW_t fpCreateFileW = NULL;
HMODULE hSystemVersionDll = NULL;

// Function pointer definitions for version.dll exports (ANSI & Wide)
typedef BOOL(APIENTRY* GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL(APIENTRY* GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL(APIENTRY* VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL(APIENTRY* VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

GetFileVersionInfoA_t pGetFileVersionInfoA = NULL;
GetFileVersionInfoW_t pGetFileVersionInfoW = NULL;
GetFileVersionInfoSizeA_t pGetFileVersionInfoSizeA = NULL;
GetFileVersionInfoSizeW_t pGetFileVersionInfoSizeW = NULL;
VerQueryValueA_t pVerQueryValueA = NULL;
VerQueryValueW_t pVerQueryValueW = NULL;

// Our hooked CreateFileW forcing cache usage
HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    // Strip FILE_FLAG_NO_BUFFERING (0x20000000) to force Windows caching
    if (dwFlagsAndAttributes & 0x20000000) {
        dwFlagsAndAttributes &= ~0x20000000;
    }
    // Strip FILE_FLAG_WRITE_THROUGH (0x80000000) if present
    if (dwFlagsAndAttributes & 0x80000000) {
        dwFlagsAndAttributes &= ~0x80000000;
    }

    return fpCreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );
}

void InitializeProxyAndHooks() {
    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat_s(sysPath, L"\\version.dll");

    // Use LoadLibraryExW with LOAD_WITH_ALTERED_SEARCH_PATH to safely load the real system DLL from System32.
    hSystemVersionDll = LoadLibraryExW(sysPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hSystemVersionDll) {
        OutputDebugString(L"Failed to load system version.dll\n");
        return;
    }

    pGetFileVersionInfoA = (GetFileVersionInfoA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoA");
    pGetFileVersionInfoW = (GetFileVersionInfoW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoW");
    pGetFileVersionInfoSizeA = (GetFileVersionInfoSizeA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeA");
    pGetFileVersionInfoSizeW = (GetFileVersionInfoSizeW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeW");
    pVerQueryValueA = (VerQueryValueA_t)GetProcAddress(hSystemVersionDll, "VerQueryValueA");
    pVerQueryValueW = (VerQueryValueW_t)GetProcAddress(hSystemVersionDll, "VerQueryValueW");

    // Ensure at least the core Wide functions are valid
    if (!pGetFileVersionInfoW || !pGetFileVersionInfoSizeW || !pVerQueryValueW) {
        OutputDebugString(L"Failed to get essential function addresses from version.dll\n");
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    // Initialize MinHook and hook CreateFileW
    if (MH_Initialize() != MH_OK) {
        OutputDebugString(L"Failed to initialize MinHook\n");
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    if (MH_CreateHook(&CreateFileW, &Hooked_CreateFileW, reinterpret_cast<LPVOID*>(&fpCreateFileW)) != MH_OK) {
        OutputDebugString(L"Failed to create hook for CreateFileW\n");
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    if (MH_EnableHook(&CreateFileW) != MH_OK) {
        OutputDebugString(L"Failed to enable hook for CreateFileW\n");
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }
}

void UninitializeProxyAndHooks() {
    MH_DisableHook(&CreateFileW);
    MH_Uninitialize();
    if (hSystemVersionDll) {
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
    }
}

// Standard exported wrapper functions (both ANSI and Wide variants)
extern "C" {
    __declspec(dllexport) BOOL APIENTRY GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoA) return pGetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    __declspec(dllexport) BOOL APIENTRY GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoW) return pGetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    __declspec(dllexport) DWORD APIENTRY GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeA) return pGetFileVersionInfoSizeA(lptstrFilename, lpdwHandle);
        return 0;
    }

    __declspec(dllexport) DWORD APIENTRY GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeW) return pGetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
        return 0;
    }

    __declspec(dllexport) BOOL APIENTRY VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueA) return pVerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }

    __declspec(dllexport) BOOL APIENTRY VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueW) return pVerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InitializeProxyAndHooks();
        break;
    case DLL_PROCESS_DETACH:
        UninitializeProxyAndHooks();
        break;
    }
    return TRUE;
}