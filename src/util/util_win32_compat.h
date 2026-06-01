#pragma once

#if defined(__unix__) || defined(__APPLE__)

#include <windows.h>
#include <dlfcn.h>
#include <unistd.h>

#include "log/log.h"

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <mutex>
#include <condition_variable>
#endif

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

// Semaphore handle.
// On Apple platforms we use Grand Central Dispatch (dispatch_semaphore_t).
// Counts are capped at maxCount to match Windows semantics.
struct NativeSemaphoreHandle {
  NativeHandleHeader header;
  LONG               maxCount;
#ifdef __APPLE__
  dispatch_semaphore_t dsema;
#else
  // Fallback for non-Apple unix: mutex + condition variable
  std::mutex              mtx;
  std::condition_variable cv;
  LONG                    count;
#endif
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

#ifdef __APPLE__
  s->dsema = dispatch_semaphore_create(lInitialCount);
  if (!s->dsema) {
    delete s;
    return nullptr;
  }
#else
  s->count = lInitialCount;
#endif

  return static_cast<HANDLE>(s);
}
#define CreateSemaphore CreateSemaphoreA

inline BOOL ReleaseSemaphore(
        HANDLE hSemaphore,
        LONG   lReleaseCount,
        LONG*  lpPreviousCount) {
  if (!hSemaphore || hSemaphore == INVALID_HANDLE_VALUE || lReleaseCount < 1)
    return FALSE;

  auto* s = static_cast<NativeSemaphoreHandle*>(hSemaphore);
  if (s->header.kind != NativeHandleKind::Semaphore)
    return FALSE;

#ifdef __APPLE__
  // GCD does not expose the current count, so we cannot check maxCount.
  // Signal lReleaseCount times.
  if (lpPreviousCount)
    *lpPreviousCount = 0;  // count not retrievable from GCD semaphore

  for (LONG i = 0; i < lReleaseCount; ++i)
    dispatch_semaphore_signal(s->dsema);
#else
  std::unique_lock<std::mutex> lock(s->mtx);
  if (s->count + lReleaseCount > s->maxCount)
    return FALSE;

  if (lpPreviousCount)
    *lpPreviousCount = s->count;

  s->count += lReleaseCount;
  lock.unlock();
  for (LONG i = 0; i < lReleaseCount; ++i)
    s->cv.notify_one();
#endif

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
  if (!hObject || hObject == INVALID_HANDLE_VALUE)
    return FALSE;

  auto* header = static_cast<NativeHandleHeader*>(hObject);
  switch (header->kind) {
    case NativeHandleKind::Semaphore: {
      auto* s = static_cast<NativeSemaphoreHandle*>(hObject);
#ifdef __APPLE__
      dispatch_release(s->dsema);
#endif
      delete s;
      return TRUE;
    }
    default:
      dxvk::Logger::warn("CloseHandle: unknown handle type.");
      return FALSE;
  }
}

// ---------------------------------------------------------------------------
// Process identity
// ---------------------------------------------------------------------------

inline HANDLE GetCurrentProcess() {
  // Windows returns (HANDLE)-1 as the pseudo-handle for the current process.
  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

inline DWORD GetCurrentProcessId() {
  return static_cast<DWORD>(getpid());
}

// ---------------------------------------------------------------------------
// Session management — macOS has no Win32 session concept; return session 0
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
