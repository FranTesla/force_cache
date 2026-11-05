#include <windows.h>
#include <string>
#include "MinHook.h"

// Handle to the original system version.dll
HMODULE hSystemVersionDll = NULL;

// Define function pointer types for the functions we are proxying
extern "C" {
    typedef BOOL(APIENTRY* GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
    typedef BOOL(APIENTRY* GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
    typedef DWORD(APIENTRY* GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
    typedef DWORD(APIENTRY* GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
    typedef BOOL(APIENTRY* VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    typedef BOOL(APIENTRY* VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
}

// Function pointers to the real system functions
GetFileVersionInfoA_t pGetFileVersionInfoA = NULL;
GetFileVersionInfoW_t pGetFileVersionInfoW = NULL;
GetFileVersionInfoSizeA_t pGetFileVersionInfoSizeA = NULL;
GetFileVersionInfoSizeW_t pGetFileVersionInfoSizeW = NULL;
VerQueryValueA_t pVerQueryValueA = NULL;
VerQueryValueW_t pVerQueryValueW = NULL;

// Target function pointer for your hook (e.g., CreateFileW example)
typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t fpCreateFileW = NULL;

HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    // Add your hook logic here if needed
    return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

std::string ConvertWideToMultiByte(const std::wstring& wideStr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void InitializeProxyAndHooks() {
    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat_s(sysPath, L"\\version.dll");

    hSystemVersionDll = LoadLibraryExW(sysPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hSystemVersionDll) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to load system version.dll\n").c_str());
        return;
    }

    pGetFileVersionInfoA = (GetFileVersionInfoA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoA");
    pGetFileVersionInfoW = (GetFileVersionInfoW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoW");
    pGetFileVersionInfoSizeA = (GetFileVersionInfoSizeA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeA");
    pGetFileVersionInfoSizeW = (GetFileVersionInfoSizeW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeW");
    pVerQueryValueA = (VerQueryValueA_t)GetProcAddress(hSystemVersionDll, "VerQueryValueA");
    pVerQueryValueW = (VerQueryValueW_t)GetProcAddress(hSystemVersionDll, "VerQueryValueW");

    if (!pGetFileVersionInfoW || !pGetFileVersionInfoSizeW || !pVerQueryValueW) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get essential function addresses from version.dll\n").c_str());
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    if (MH_Initialize() != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to initialize MinHook\n").c_str());
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    if (MH_CreateHook(&CreateFileW, &Hooked_CreateFileW, reinterpret_cast<LPVOID*>(&fpCreateFileW)) != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to create hook for CreateFileW\n").c_str());
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }

    if (MH_EnableHook(&CreateFileW) != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to enable hook for CreateFileW\n").c_str());
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        return;
    }
}

void UnloadProxyAndHooks() {
    MH_DisableHook(&CreateFileW);
    MH_Uninitialize();
    if (hSystemVersionDll) {
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InitializeProxyAndHooks();
        break;
    case DLL_PROCESS_DETACH:
        UnloadProxyAndHooks();
        break;
    }
    return TRUE;
}

// Proxied Exports Implementation
extern "C" {
    BOOL APIENTRY GetFileVersionInfoA_Proxy(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoA) return pGetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    BOOL APIENTRY GetFileVersionInfoW_Proxy(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoW) return pGetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    DWORD APIENTRY GetFileVersionInfoSizeA_Proxy(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeA) return pGetFileVersionInfoSizeA(lptstrFilename, lpdwHandle);
        return 0;
    }

    DWORD APIENTRY GetFileVersionInfoSizeW_Proxy(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeW) return pGetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
        return 0;
    }

    BOOL APIENTRY VerQueryValueA_Proxy(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueA) return pVerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }

    BOOL APIENTRY VerQueryValueW_Proxy(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueW) return pVerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }
}