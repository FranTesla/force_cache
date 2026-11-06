// src/dllmain.cpp
#include <windows.h>
#include <string>
#include <cstring>  // Added for WideCharToMultiByte
#include "MinHook.h"

// Handle to the original system version.dll
HMODULE hSystemVersionDll = NULL;

// Function pointer types for the functions we are proxying
typedef BOOL(APIENTRY* GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL(APIENTRY* GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD(APIENTRY* GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL(APIENTRY* VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL(APIENTRY* VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

// Function pointers to the real system functions (thread-safe static initialization)
static GetFileVersionInfoA_t pGetFileVersionInfoA = nullptr;
static GetFileVersionInfoW_t pGetFileVersionInfoW = nullptr;
static GetFileVersionInfoSizeA_t pGetFileVersionInfoSizeA = nullptr;
static GetFileVersionInfoSizeW_t pGetFileVersionInfoSizeW = nullptr;
static VerQueryValueA_t pVerQueryValueA = nullptr;
static VerQueryValueW_t pVerQueryValueW = nullptr;

// Target function pointer for your hook (use separate variable)
typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t fpCreateFileW_original = nullptr;  // Original address stored here

// Hooked function
HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    // Force caching by using FILE_FLAG_WRITE_THROUGH if appropriate
    // or modifying flags to force cache usage
    return fpCreateFileW_original(lpFileName, dwDesiredAccess, dwShareMode, 
                                  lpSecurityAttributes, dwCreationDisposition,
                                  dwFlagsAndAttributes, hTemplateFile);
}

// Convert wide string to multi-byte string
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

// Simple mutex for thread safety
CRITICAL_SECTION g_csInitializeProxyAndHooks;
static bool g_initialized = false;

// Initialize proxy and hooks
void InitializeProxyAndHooks() {
    // Thread-safe initialization check with atomic-style check
    if (!g_initialized) {
        EnterCriticalSection(&g_csInitializeProxyAndHooks);
        
        if (!g_initialized) {
            wchar_t sysPath[MAX_PATH];
            if (GetSystemDirectoryW(sysPath, MAX_PATH)) {
                wcscat_s(sysPath, L"\\version.dll");

                hSystemVersionDll = LoadLibraryExW(sysPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (!hSystemVersionDll) {
                    OutputDebugStringA(ConvertWideToMultiByte(L"Failed to load system version.dll\n").c_str());
                } else {
                    pGetFileVersionInfoA = (GetFileVersionInfoA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoA");
                    pGetFileVersionInfoW = (GetFileVersionInfoW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoW");
                    pGetFileVersionInfoSizeA = (GetFileVersionInfoSizeA_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeA");
                    pGetFileVersionInfoSizeW = (GetFileVersionInfoSizeW_t)GetProcAddress(hSystemVersionDll, "GetFileVersionInfoSizeW");
                    pVerQueryValueA = (VerQueryValueA_t)GetProcAddress(hSystemVersionDll, "VerQueryValueA");
                    pVerQueryValueW = (VerQueryValueW_t)GetProcAddress(hSystemVersionDll, "VerQueryValueW");

                    if (pGetFileVersionInfoA && pGetFileVersionInfoW && 
                        pGetFileVersionInfoSizeA && pGetFileVersionInfoSizeW && 
                        pVerQueryValueA && pVerQueryValueW) {
                        
                        if (MH_Initialize() == MH_OK) {
                            HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
                            void* targetAddress = reinterpret_cast<void*>(GetProcAddress(hKernel32, "CreateFileW"));

                            if (targetAddress) {
                                LPVOID pOriginalPointer = NULL;  // Store original function pointer
                                
                                if (MH_CreateHook(targetAddress, 
                                                &Hooked_CreateFileW, 
                                                &pOriginalPointer) == MH_OK) {
                                    fpCreateFileW_original = CreateFileW_t(pOriginalPointer);
                                    
                                    if (MH_EnableHook(targetAddress) == MH_OK) {
                                        g_initialized = true;
                                    } else {
                                        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to enable hook\n").c_str());
                                    }
                                }
                            } else {
                                OutputDebugStringA(ConvertWideToMultiByte(L"CreateFileW not found in kernel32.dll\n").c_str());
                            }

                        } else {
                            OutputDebugStringA(ConvertWideToMultiByte(L"MinHook initialization failed\n").c_str());
                        }
                    } else {
                        OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get essential function addresses from version.dll\n").c_str());
                        FreeLibrary(hSystemVersionDll);
                        hSystemVersionDll = NULL;
                    }
                }
            } else {
                OutputDebugStringA(ConvertWideToMultiByte(L"Failed to get system directory\n").c_str());
            }

        }
        LeaveCriticalSection(&g_csInitializeProxyAndHooks);
    }
}

// Unload proxy and hooks
void UnloadProxyAndHooks() {
    if (g_initialized) {
        MH_DisableHookAll();
        MH_Uninitialize();
    }
    EnterCriticalSection(&g_csInitializeProxyAndHooks);
    if (hSystemVersionDll) {
        FreeLibrary(hSystemVersionDll);
        hSystemVersionDll = NULL;
    }
    g_initialized = false;
    LeaveCriticalSection(&g_csInitializeProxyAndHooks);
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_csInitializeProxyAndHooks);
        InitializeProxyAndHooks();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // No action needed for thread events
        break;
    case DLL_PROCESS_DETACH:
        UnloadProxyAndHooks();
        DeleteCriticalSection(&g_csInitializeProxyAndHooks);
        break;
    }
    return TRUE;
}

// Proxied Exports Implementation (forward to real functions) - Export as version.dll exports
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

    // Add CreateFileW export as well if needed
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