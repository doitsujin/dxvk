#pragma once

#if defined(__unix__) || defined(__APPLE__)

#include <windows.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

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

// ---------------------------------------------------------------------------
// Native handle infrastructure
//
// HANDLE is void* on non-Windows.  We allocate small tagged structs on the
// heap and cast their pointers to HANDLE so that CloseHandle can dispatch to
// the right destructor.
// ---------------------------------------------------------------------------

enum class NativeHandleKind : uint32_t {
  Semaphore = 0x534D5048u,  // 'SMPH'
};

struct NativeHandleHeader {
  NativeHandleKind kind;
};

// Semaphore handle backed by a mutex + condition variable.
// Windows HANDLE is void*; we cast the pointer to HANDLE and back.
struct NativeSemaphoreHandle {
  NativeHandleHeader      header;
  LONG                    maxCount;
  std::mutex              mtx;
  std::condition_variable cv;
  LONG                    count;
};

// ---------------------------------------------------------------------------
// CreateSemaphoreA / ReleaseSemaphore
// ---------------------------------------------------------------------------

inline HANDLE CreateSemaphoreA(
        SECURITY_ATTRIBUTES*  /* lpSemaphoreAttributes */,
        LONG                  lInitialCount,
        LONG                  lMaximumCount,
        LPCSTR                /* lpName */) {
  if (lInitialCount < 0 || lMaximumCount < 1 || lInitialCount > lMaximumCount)
    return nullptr;

  auto* s = new NativeSemaphoreHandle();
  s->header.kind = NativeHandleKind::Semaphore;
  s->maxCount    = lMaximumCount;
  s->count       = lInitialCount;
  return static_cast<HANDLE>(s);
}
#define CreateSemaphore CreateSemaphoreA

inline BOOL ReleaseSemaphore(
        HANDLE hSemaphore,
        LONG   lReleaseCount,
        LONG*  lpPreviousCount) {
  if (!hSemaphore || hSemaphore == INVALID_HANDLE_VALUE || lReleaseCount < 1)
    return FALSE;

  // Check the kind tag through the base header before casting to the full type.
  auto* header = static_cast<NativeHandleHeader*>(hSemaphore);
  if (header->kind != NativeHandleKind::Semaphore)
    return FALSE;

  auto* s = static_cast<NativeSemaphoreHandle*>(hSemaphore);
  std::unique_lock<std::mutex> lock(s->mtx);

  // Guard against overflow and against exceeding the declared maximum count.
  if (lReleaseCount > s->maxCount - s->count)
    return FALSE;

  if (lpPreviousCount)
    *lpPreviousCount = s->count;

  s->count += lReleaseCount;
  lock.unlock();

  for (LONG i = 0; i < lReleaseCount; ++i)
    s->cv.notify_one();

  return TRUE;
}

// ---------------------------------------------------------------------------
// SetEvent — stub: event objects are not yet implemented
// ---------------------------------------------------------------------------

inline BOOL SetEvent(HANDLE hEvent) {
  dxvk::Logger::warn("SetEvent: event objects not implemented on this platform.");
  return FALSE;
}

// ---------------------------------------------------------------------------
// DuplicateHandle — stub
// ---------------------------------------------------------------------------

inline BOOL DuplicateHandle(
        HANDLE  /* hSourceProcessHandle */,
        HANDLE  /* hSourceHandle */,
        HANDLE* /* hTargetProcessHandle */,
        HANDLE* lpTargetHandle,
        DWORD   /* dwDesiredAccess */,
        BOOL    /* bInheritHandle */,
        DWORD   /* dwOptions */) {
  dxvk::Logger::warn("DuplicateHandle: not implemented on this platform.");
  if (lpTargetHandle)
    *lpTargetHandle = nullptr;
  return FALSE;
}

// ---------------------------------------------------------------------------
// CloseHandle — dispatches on NativeHandleKind
// ---------------------------------------------------------------------------

inline BOOL CloseHandle(HANDLE hObject) {
  // Reject null and INVALID_HANDLE_VALUE (-1), which also covers the
  // GetCurrentProcess() pseudo-handle that returns (HANDLE)-1.
  if (!hObject || hObject == INVALID_HANDLE_VALUE)
    return FALSE;

  // Inspect the kind tag through the base header before casting.
  auto* header = static_cast<NativeHandleHeader*>(hObject);
  switch (header->kind) {
    case NativeHandleKind::Semaphore:
      delete static_cast<NativeSemaphoreHandle*>(hObject);
      return TRUE;
    default:
      dxvk::Logger::warn("CloseHandle: unknown handle type.");
      return FALSE;
  }
}

// ---------------------------------------------------------------------------
// Process identity
// ---------------------------------------------------------------------------

inline HANDLE GetCurrentProcess() {
  // Windows convention: the pseudo-handle for the current process is -1,
  // which is the same value as INVALID_HANDLE_VALUE.  CloseHandle on this
  // value is a no-op on Windows, and our CloseHandle does the same.
  return INVALID_HANDLE_VALUE;
}

inline DWORD GetCurrentProcessId() {
  return static_cast<DWORD>(getpid());
}

// ---------------------------------------------------------------------------
// Session management — macOS/Linux have no Win32 session concept.
// Return session 0 for any existing process.
// ---------------------------------------------------------------------------

inline BOOL ProcessIdToSessionId(DWORD /* pid */, DWORD* id) {
  if (id)
    *id = 0;
  return TRUE;
}

// ---------------------------------------------------------------------------
// GDI DC stubs — GDI device contexts are Windows-only
// ---------------------------------------------------------------------------

inline HDC CreateCompatibleDC(HDC /* hdc */) {
  return nullptr;
}

inline BOOL DeleteDC(HDC /* hdc */) {
  return FALSE;
}

#endif
