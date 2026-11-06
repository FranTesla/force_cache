#include <windows.h>
#include <string>
#include <cstring>
#include "MinHook.h"

HMODULE hSystemVersionDll = NULL;

typedef BOOL(APIENTRY* GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL(APIENTRY* GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL(APIENTRY* VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL(APIENTRY* VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

static GetFileVersionInfoA_t pGetFileVersionInfoA = nullptr;
static GetFileVersionInfoW_t pGetFileVersionInfoW = nullptr;
static GetFileVersionInfoSizeA_t pGetFileVersionInfoSizeA = nullptr;
static GetFileVersionInfoSizeW_t pGetFileVersionInfoSizeW = nullptr;
static VerQueryValueA_t pVerQueryValueA = nullptr;
static VerQueryValueW_t pVerQueryValueW = nullptr;

typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t fpCreateFileW_original = nullptr;

HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    return fpCreateFileW_original(lpFileName, dwDesiredAccess, dwShareMode, 
                                  lpSecurityAttributes, dwCreationDisposition,
                                  dwFlagsAndAttributes, hTemplateFile);
}

std::string ConvertWideToMultiByte(const std::wstring& wideStr) {
    if (wideStr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], 
                                          static_cast<int>(wideStr.size()), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    if (size_needed > 0) {
        WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], static_cast<int>(wideStr.size()), 
                            &strTo[0], size_needed, NULL, NULL);
    }
    return strTo;
}

CRITICAL_SECTION g_csInitializeProxyAndHooks;
static bool g_initialized = false;

void InitializeProxyAndHooks() {
    EnterCriticalSection(&g_csInitializeProxyAndHooks);
    if (g_initialized) {
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }
    
    wchar_t sysPath[MAX_PATH];
    if (!GetSystemDirectoryW(sysPath, MAX_PATH)) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get system directory\n").c_str());
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }
    
    wcscat_s(sysPath, L"\\version.dll");

    hSystemVersionDll = LoadLibraryExW(sysPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hSystemVersionDll) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to load system version.dll\n").c_str());
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    pGetFileVersionInfoA = (GetFileVersionInfoA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoA");
    pGetFileVersionInfoW = (GetFileVersionInfoW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoW");
    pGetFileVersionInfoSizeA = (GetFileVersionInfoSizeA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeA");
    pGetFileVersionInfoSizeW = (GetFileVersionInfoSizeW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeW");
    pVerQueryValueA = (VerQueryValueA_t)GetProcAddress(hSystemVersionDll, "VerQueryValueA");
    pVerQueryValueW = (VerQueryValueW_t)GetProcAddress(hSystemVersionDll, "VerQueryValueW");

    if (!(pGetFileVersionInfoA && pGetFileVersionInfoW && 
          pGetFileVersionInfoSizeA && pGetFileVersionInfoSizeW && 
          pVerQueryValueA && pVerQueryValueW)) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get essential function addresses from version.dll\n").c_str());
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }
    
    if (MH_Initialize() != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"MinHook initialization failed\n").c_str());
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get kernel32.dll handle\n").c_str());
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    void* targetAddress = GetProcAddress(hKernel32, "CreateFileW");
    if (!targetAddress) {
        OutputDebugStringA(ConvertWideToMultiByte(L"CreateFileW not found in kernel32.dll\n").c_str());
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    PMH_HOOK pHook = NULL;
    if (MH_CreateHook(targetAddress, &Hooked_CreateFileW, reinterpret_cast<void**>(&pHook)) != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to create hook\n").c_str());
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    if (MH_EnableHook(pHook) != MH_OK) {
        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to enable hook\n").c_str());
        MH_RemoveHook(pHook);
        MH_Uninitialize();
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
        return;
    }

    fpCreateFileW_original = (CreateFileW_t)pHook;
    g_initialized = true;
    LeaveCriticalSection(&g_csInitializeProxyAndHooks);
}

void UnloadProxyAndHooks() {
    EnterCriticalSection(&g_csInitializeProxyAndHooks);
    
    if (g_initialized && fpCreateFileW_original) {
        MH_RemoveHook(reinterpret_cast<PMH_FUNCTION>(fpCreateFileW_original));
    }
    MH_Uninitialize();
    g_initialized = false;
    
    if (hSystemVersionDll) {
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
    }
    
    LeaveCriticalSection(&g_csInitializeProxyAndHooks);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_csInitializeProxyAndHooks);
        InitializeProxyAndHooks();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        UnloadProxyAndHooks();
        DeleteCriticalSection(&g_csInitializeProxyAndHooks);
        break;
    }
    return TRUE;
}

extern "C" {
    BOOL APIENTRY __declspec(dllexport) GetFileVersionInfoA_Proxy(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoA) return pGetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    BOOL APIENTRY __declspec(dllexport) GetFileVersionInfoW_Proxy(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
        if (pGetFileVersionInfoW) return pGetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
        return FALSE;
    }

    DWORD APIENTRY __declspec(dllexport) GetFileVersionInfoSizeA_Proxy(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeA) return pGetFileVersionInfoSizeA(lptstrFilename, lpdwHandle);
        return 0;
    }

    DWORD APIENTRY __declspec(dllexport) GetFileVersionInfoSizeW_Proxy(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
        if (pGetFileVersionInfoSizeW) return pGetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
        return 0;
    }

    BOOL APIENTRY __declspec(dllexport) VerQueryValueA_Proxy(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueA) return pVerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }

    BOOL APIENTRY __declspec(dllexport) VerQueryValueW_Proxy(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
        if (pVerQueryValueW) return pVerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
        return FALSE;
    }

    HANDLE APIENTRY __declspec(dllexport) CreateFileW_Proxy(LPCWSTR lpFileName, DWORD dwDesiredAccess, 
                                DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
                                DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, 
                                HANDLE hTemplateFile) {
        if (fpCreateFileW_original) {
            return fpCreateFileW_original(lpFileName, dwDesiredAccess, dwShareMode, 
                                          lpSecurityAttributes, dwCreationDisposition,
                                          dwFlagsAndAttributes, hTemplateFile);
        }
        return INVALID_HANDLE_VALUE;
    }
}