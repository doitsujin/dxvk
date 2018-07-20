#pragma once

#include "util_error.h"

#include <functional>
#include "./com/com_include.h"

/*
 * This is needed mostly  for winelib builds. Wine needs to setup each thread that
 * calls Windows APIs. It means that in winelib builds, we can't let standard C++
 * library create threads and need to use Wine for that instead. We use a thin wrapper
 * around Windows thread functions so that the rest of code just has to use
 * dxvk::thread class instead of std::thread.
 */
namespace dxvk {
  class thread {
  private:
    HANDLE m_handle;
    std::function<void()> proc;

    static DWORD WINAPI nativeProc(void *arg) {
      auto* proc = reinterpret_cast<thread*>(arg);
      proc->proc();
      return 0;
    }

  public:
    explicit thread(std::function<void()> func) : proc(func) {
        m_handle = ::CreateThread(nullptr, 0, thread::nativeProc, this, 0, nullptr);
        if (!m_handle) {
            throw DxvkError("Failed to create thread");
        }
    }

    thread() : m_handle(nullptr) {}

    ~thread() {
      if (m_handle)
        ::CloseHandle(m_handle);
    }

    void join() {
      ::WaitForSingleObject(m_handle, INFINITE);
    }
  };

  namespace this_thread {
    inline void yield() {
      Sleep(0);
    }
  }
}
