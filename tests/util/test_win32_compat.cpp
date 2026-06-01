// Unit tests for the native Win32 compatibility shims in
// src/util/util_win32_compat.h (Milestone E).
//
// These cover the handle objects SpockD3D9 emulates on macOS/Linux:
// semaphores, events (auto- and manual-reset), WaitForSingleObject(Ex)
// timeouts, DuplicateHandle reference sharing, and CloseHandle teardown.
//
// The test is intentionally hermetic: it includes the real shim header and
// stubs the single Logger symbol the header references, so it builds with a
// bare C++ toolchain (no Vulkan/SDL/meson required). Build from the repo root:
//
//   g++ -std=c++17 -pthread -Isrc/util -Iinclude/native/windows
//       tests/util/test_win32_compat.cpp -o test_win32_compat
//   ./test_win32_compat

#include "util_win32_compat.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

// The shim only ODR-uses dxvk::Logger::warn (on the unknown-handle path).
// Provide a stub so the test links without the rest of the util library.
namespace dxvk {
  void Logger::warn(const std::string& message) {
    std::fprintf(stderr, "[warn] %s\n", message.c_str());
  }
}

static int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::printf("FAIL: %s (line %d)\n", #cond, __LINE__);              \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

static void test_semaphore_counting() {
  HANDLE s = CreateSemaphoreA(nullptr, 1, 2, nullptr);
  CHECK(s != nullptr);

  // Initial count 1 -> one immediate acquire succeeds, the next polls out.
  CHECK(WaitForSingleObject(s, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(s, 0) == WAIT_TIMEOUT);

  LONG previous = -1;
  CHECK(ReleaseSemaphore(s, 2, &previous) == TRUE);
  CHECK(previous == 0);

  // Releasing past the maximum count must fail and not change the count.
  CHECK(ReleaseSemaphore(s, 1, nullptr) == FALSE);
  CHECK(WaitForSingleObject(s, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(s, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(s, 0) == WAIT_TIMEOUT);

  CHECK(CloseHandle(s) == TRUE);
}

static void test_semaphore_blocking() {
  HANDLE s = CreateSemaphoreA(nullptr, 0, 1, nullptr);
  std::atomic<bool> woke{false};

  std::thread waiter([&] {
    CHECK(WaitForSingleObject(s, INFINITE) == WAIT_OBJECT_0);
    woke = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  CHECK(!woke.load());
  CHECK(ReleaseSemaphore(s, 1, nullptr) == TRUE);
  waiter.join();
  CHECK(woke.load());

  CHECK(CloseHandle(s) == TRUE);
}

static void test_auto_reset_event() {
  HANDLE e = CreateEventA(nullptr, FALSE /* auto-reset */, FALSE, nullptr);
  CHECK(WaitForSingleObject(e, 0) == WAIT_TIMEOUT);

  CHECK(SetEvent(e) == TRUE);
  CHECK(WaitForSingleObject(e, 0) == WAIT_OBJECT_0); // consumes the signal
  CHECK(WaitForSingleObject(e, 0) == WAIT_TIMEOUT);  // auto-reset cleared it

  // A blocked waiter is released by SetEvent.
  std::atomic<bool> woke{false};
  std::thread waiter([&] {
    CHECK(WaitForSingleObject(e, INFINITE) == WAIT_OBJECT_0);
    woke = true;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  CHECK(!woke.load());
  CHECK(SetEvent(e) == TRUE);
  waiter.join();
  CHECK(woke.load());

  CHECK(CloseHandle(e) == TRUE);
}

static void test_manual_reset_event() {
  HANDLE e = CreateEventA(nullptr, TRUE /* manual-reset */, TRUE /* signaled */, nullptr);

  // Stays signaled across multiple waits until ResetEvent.
  CHECK(WaitForSingleObject(e, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(e, 0) == WAIT_OBJECT_0);
  CHECK(ResetEvent(e) == TRUE);
  CHECK(WaitForSingleObject(e, 0) == WAIT_TIMEOUT);
  CHECK(SetEvent(e) == TRUE);
  CHECK(WaitForSingleObject(e, 0) == WAIT_OBJECT_0);

  CHECK(CloseHandle(e) == TRUE);
}

static void test_duplicate_handle() {
  // Shared ownership: the object survives closing the source while a duplicate
  // still references it. This is the d3d11 frame-latency waitable-object path.
  HANDLE s = CreateSemaphoreA(nullptr, 1, 1, nullptr);
  HANDLE dup = nullptr;
  HANDLE proc = GetCurrentProcess();

  CHECK(DuplicateHandle(proc, s, proc, &dup, 0, FALSE, DUPLICATE_SAME_ACCESS) == TRUE);
  CHECK(dup == s); // same underlying object
  CHECK(CloseHandle(s) == TRUE);
  CHECK(WaitForSingleObject(dup, 0) == WAIT_OBJECT_0); // still usable
  CHECK(CloseHandle(dup) == TRUE);                     // last reference frees it

  // DUPLICATE_CLOSE_SOURCE leaves the net reference count unchanged.
  HANDLE e = CreateEventA(nullptr, TRUE, TRUE, nullptr);
  HANDLE moved = nullptr;
  CHECK(DuplicateHandle(nullptr, e, nullptr, &moved, 0, FALSE, DUPLICATE_CLOSE_SOURCE) == TRUE);
  CHECK(moved == e);
  CHECK(CloseHandle(moved) == TRUE); // only one close needed
}

static void test_invalid_inputs() {
  CHECK(SetEvent(nullptr) == FALSE);
  CHECK(ResetEvent(INVALID_HANDLE_VALUE) == FALSE);
  CHECK(WaitForSingleObject(nullptr, 0) == WAIT_FAILED);
  CHECK(ReleaseSemaphore(nullptr, 1, nullptr) == FALSE);
  CHECK(CloseHandle(nullptr) == FALSE);
  CHECK(CloseHandle(GetCurrentProcess()) == FALSE); // pseudo-handle is not ours

  HANDLE target = reinterpret_cast<HANDLE>(0x1);
  CHECK(DuplicateHandle(nullptr, nullptr, nullptr, &target, 0, FALSE, 0) == FALSE);
  CHECK(target == nullptr); // cleared on failure
}

int main() {
  test_semaphore_counting();
  test_semaphore_blocking();
  test_auto_reset_event();
  test_manual_reset_event();
  test_duplicate_handle();
  test_invalid_inputs();

  if (g_failures == 0) {
    std::printf("ALL WIN32-COMPAT TESTS PASSED\n");
    return 0;
  }

  std::printf("%d CHECK(S) FAILED\n", g_failures);
  return 1;
}
