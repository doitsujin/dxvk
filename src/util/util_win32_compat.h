#pragma once

#if defined(__unix__)

#include <windows.h>
#include <dlfcn.h>

#include "log/log.h"

inline HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
  return dlopen(lpLibFileName, RTLD_NOW);
}

inline void FreeLibrary(HMODULE module) {
  dlclose(module);
}

inline void* GetProcAddress(HMODULE module, LPCSTR lpProcName) {
  if (!module)
    return nullptr;

  return dlsym(module, lpProcName);
}

inline HANDLE CreateSemaphoreA(
        SECURITY_ATTRIBUTES*  lpSemaphoreAttributes,
        LONG                  lInitialCount,
        LONG                  lMaximumCount,
        LPCSTR                lpName) {
  dxvk::Logger::warn("CreateSemaphoreA not implemented.");
  return nullptr;
}
#define CreateSemaphore CreateSemaphoreA

inline BOOL ReleaseSemaphore(
        HANDLE hSemaphore,
        LONG   lReleaseCount,
        LONG*  lpPreviousCount) {
  dxvk::Logger::warn("ReleaseSemaphore not implemented.");
  return FALSE;
}

inline BOOL SetEvent(HANDLE hEvent) {
  dxvk::Logger::warn("SetEvent not implemented.");
  return FALSE;
}

inline BOOL DuplicateHandle(
        HANDLE hSourceProcessHandle,
        HANDLE hSourceHandle,
        HANDLE hTargetProcessHandle,
        HANDLE* lpTargetHandle,
        DWORD dwDesiredAccess,
        BOOL bInheritHandle,
        DWORD dwOptions) {
  dxvk::Logger::warn("DuplicateHandle not implemented.");
  return FALSE;
}

inline BOOL CloseHandle(HANDLE hObject) {
  dxvk::Logger::warn("CloseHandle not implemented.");
  return FALSE;
}

inline HANDLE GetCurrentProcess() {
  dxvk::Logger::warn("GetCurrentProcess not implemented.");
  return nullptr;
}

inline DWORD GetCurrentProcessId() {
  dxvk::Logger::warn("GetCurrentProcessId not implemented.");
  return 0;
}

inline BOOL ProcessIdToSessionId(DWORD pid, DWORD *id) {
  dxvk::Logger::warn("ProcessIdToSessionId not implemented.");
  *id = 0;
  return FALSE;
}

inline HDC CreateCompatibleDC(HDC hdc) {
  dxvk::Logger::warn("CreateCompatibleDC not implemented.");
  return nullptr;
}

inline BOOL DeleteDC(HDC hdc) {
  return FALSE;
}

#endif
