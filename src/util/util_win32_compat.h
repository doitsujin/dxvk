#pragma once

#if defined(__unix__) || defined(__APPLE__)

#include <windows.h>
#include <dlfcn.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
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

// INFINITE is not declared by the native <windows.h> shim; the real Win32
// header defines it as 0xFFFFFFFF.  WAIT_OBJECT_0 / WAIT_TIMEOUT / WAIT_FAILED
// already come from windows_base.h.
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif

// ---------------------------------------------------------------------------
// Native handle infrastructure
//
// HANDLE is void* on non-Windows.  We allocate small tagged structs on the
// heap and cast their pointers to HANDLE.  Every handle carries a kind tag so
// CloseHandle / WaitForSingleObject / DuplicateHandle can dispatch on type, and
// a reference count so DuplicateHandle and CloseHandle can share ownership of a
// single underlying object (matching Win32 handle semantics).
// ---------------------------------------------------------------------------

enum class NativeHandleKind : uint32_t {
  Semaphore = 0x534D5048u,  // 'SMPH'
  Event     = 0x45564E54u,  // 'EVNT'
};

struct NativeHandleHeader {
  NativeHandleKind      kind;
  std::atomic<uint32_t> refCount;
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

// Event handle backed by a mutex + condition variable.  Supports both
// manual-reset events (stay signalled until ResetEvent) and auto-reset events
// (a single successful wait clears the signalled state).
struct NativeEventHandle {
  NativeHandleHeader      header;
  std::mutex              mtx;
  std::condition_variable cv;
  bool                    manualReset;
  bool                    signaled;
};

// Validate a HANDLE and return its header if it is one of our tagged native
// objects, or nullptr otherwise.  The kind check guards against null,
// INVALID_HANDLE_VALUE (the GetCurrentProcess pseudo-handle), and foreign
// pointers before any type-specific cast.
inline NativeHandleHeader* GetNativeHandleHeader(HANDLE hObject) {
  if (!hObject || hObject == INVALID_HANDLE_VALUE)
    return nullptr;

  auto* header = static_cast<NativeHandleHeader*>(hObject);
  switch (header->kind) {
    case NativeHandleKind::Semaphore:
    case NativeHandleKind::Event:
      return header;
    default:
      return nullptr;
  }
}

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
  s->header.refCount.store(1, std::memory_order_relaxed);
  s->maxCount    = lMaximumCount;
  s->count       = lInitialCount;
  return static_cast<HANDLE>(s);
}
#define CreateSemaphore CreateSemaphoreA

inline BOOL ReleaseSemaphore(
        HANDLE hSemaphore,
        LONG   lReleaseCount,
        LONG*  lpPreviousCount) {
  if (lReleaseCount < 1)
    return FALSE;

  // Check the kind tag through the base header before casting to the full type.
  auto* header = GetNativeHandleHeader(hSemaphore);
  if (!header || header->kind != NativeHandleKind::Semaphore)
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
// CreateEventA / CreateEventW / SetEvent / ResetEvent
// ---------------------------------------------------------------------------

inline HANDLE CreateEventA(
        SECURITY_ATTRIBUTES*  /* lpEventAttributes */,
        BOOL                  bManualReset,
        BOOL                  bInitialState,
        LPCSTR                /* lpName */) {
  auto* e = new NativeEventHandle();
  e->header.kind = NativeHandleKind::Event;
  e->header.refCount.store(1, std::memory_order_relaxed);
  e->manualReset = bManualReset != FALSE;
  e->signaled    = bInitialState != FALSE;
  return static_cast<HANDLE>(e);
}

inline HANDLE CreateEventW(
        SECURITY_ATTRIBUTES*  lpEventAttributes,
        BOOL                  bManualReset,
        BOOL                  bInitialState,
        LPCWSTR               /* lpName */) {
  return CreateEventA(lpEventAttributes, bManualReset, bInitialState, nullptr);
}
#define CreateEvent CreateEventA

inline BOOL SetEvent(HANDLE hEvent) {
  auto* header = GetNativeHandleHeader(hEvent);
  if (!header || header->kind != NativeHandleKind::Event)
    return FALSE;

  auto* e = static_cast<NativeEventHandle*>(hEvent);
  std::lock_guard<std::mutex> lock(e->mtx);
  e->signaled = true;

  // Manual-reset events release every waiter; auto-reset events release one,
  // and the woken waiter consumes the signal.
  if (e->manualReset)
    e->cv.notify_all();
  else
    e->cv.notify_one();

  return TRUE;
}

inline BOOL ResetEvent(HANDLE hEvent) {
  auto* header = GetNativeHandleHeader(hEvent);
  if (!header || header->kind != NativeHandleKind::Event)
    return FALSE;

  auto* e = static_cast<NativeEventHandle*>(hEvent);
  std::lock_guard<std::mutex> lock(e->mtx);
  e->signaled = false;
  return TRUE;
}

// ---------------------------------------------------------------------------
// WaitForSingleObject / WaitForSingleObjectEx
//
// Waits on a semaphore (decrementing its count) or an event (clearing the
// signal for auto-reset events).  A timeout of INFINITE blocks indefinitely;
// any other value is treated as a millisecond timeout, with 0 acting as a poll.
// ---------------------------------------------------------------------------

inline DWORD WaitForSingleObjectEx(
        HANDLE hHandle,
        DWORD  dwMilliseconds,
        BOOL   /* bAlertable */) {
  auto* header = GetNativeHandleHeader(hHandle);
  if (!header)
    return WAIT_FAILED;

  const auto timeout = std::chrono::milliseconds(dwMilliseconds);

  switch (header->kind) {
    case NativeHandleKind::Semaphore: {
      auto* s = static_cast<NativeSemaphoreHandle*>(hHandle);
      std::unique_lock<std::mutex> lock(s->mtx);
      auto ready = [&] { return s->count > 0; };

      if (dwMilliseconds == INFINITE)
        s->cv.wait(lock, ready);
      else if (!s->cv.wait_for(lock, timeout, ready))
        return WAIT_TIMEOUT;

      s->count -= 1;
      return WAIT_OBJECT_0;
    }

    case NativeHandleKind::Event: {
      auto* e = static_cast<NativeEventHandle*>(hHandle);
      std::unique_lock<std::mutex> lock(e->mtx);
      auto ready = [&] { return e->signaled; };

      if (dwMilliseconds == INFINITE)
        e->cv.wait(lock, ready);
      else if (!e->cv.wait_for(lock, timeout, ready))
        return WAIT_TIMEOUT;

      // Auto-reset events consume the signal on a successful wait.
      if (!e->manualReset)
        e->signaled = false;

      return WAIT_OBJECT_0;
    }

    default:
      return WAIT_FAILED;
  }
}

inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
  return WaitForSingleObjectEx(hHandle, dwMilliseconds, FALSE);
}

// ---------------------------------------------------------------------------
// DuplicateHandle
//
// Win32 hands out a second handle that refers to the same kernel object.  We
// model this by sharing the underlying object through its reference count and
// returning the same pointer; each handle must be closed exactly once.
// ---------------------------------------------------------------------------

inline BOOL DuplicateHandle(
        HANDLE  /* hSourceProcessHandle */,
        HANDLE  hSourceHandle,
        HANDLE  /* hTargetProcessHandle */,
        HANDLE* lpTargetHandle,
        DWORD   /* dwDesiredAccess */,
        BOOL    /* bInheritHandle */,
        DWORD   dwOptions) {
  if (lpTargetHandle)
    *lpTargetHandle = nullptr;

  auto* header = GetNativeHandleHeader(hSourceHandle);
  if (!header)
    return FALSE;

  // DUPLICATE_CLOSE_SOURCE closes the source as part of the operation, so the
  // net reference count is unchanged (one handle consumed, one produced).
  // Otherwise we add a reference for the freshly minted target handle.
  if (!(dwOptions & DUPLICATE_CLOSE_SOURCE))
    header->refCount.fetch_add(1, std::memory_order_relaxed);

  if (lpTargetHandle)
    *lpTargetHandle = hSourceHandle;

  return TRUE;
}

// ---------------------------------------------------------------------------
// CloseHandle — drops a reference and destroys the object when the last
// reference goes away, dispatching the destructor on NativeHandleKind.
// ---------------------------------------------------------------------------

inline BOOL CloseHandle(HANDLE hObject) {
  // Reject null and INVALID_HANDLE_VALUE (-1), which also covers the
  // GetCurrentProcess() pseudo-handle that returns (HANDLE)-1.
  auto* header = GetNativeHandleHeader(hObject);
  if (!header) {
    if (hObject && hObject != INVALID_HANDLE_VALUE)
      dxvk::Logger::warn("CloseHandle: unknown handle type.");
    return FALSE;
  }

  // Only the thread that observes the count drop to zero destroys the object.
  if (header->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
    return TRUE;

  switch (header->kind) {
    case NativeHandleKind::Semaphore:
      delete static_cast<NativeSemaphoreHandle*>(hObject);
      return TRUE;
    case NativeHandleKind::Event:
      delete static_cast<NativeEventHandle*>(hObject);
      return TRUE;
    default:
      // Unreachable: GetNativeHandleHeader already validated the kind.
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
