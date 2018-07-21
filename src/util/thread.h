#pragma once

#include <functional>

#include "util_error.h"

#include "./com/com_include.h"

namespace dxvk {
  /**
   * \brief Thread helper class
   * 
   * This is needed mostly  for winelib builds. Wine needs to setup each thread that
   * calls Windows APIs. It means that in winelib builds, we can't let standard C++
   * library create threads and need to use Wine for that instead. We use a thin wrapper
   * around Windows thread functions so that the rest of code just has to use
   * dxvk::thread class instead of std::thread.
   */
  class thread {

  public:

    thread()
    : m_handle(nullptr) { }

    explicit thread(std::function<void()> func) : m_proc(func) {
      m_handle = ::CreateThread(nullptr, 0, thread::nativeProc, this, 0, nullptr);

      if (!m_handle)
        throw DxvkError("Failed to create thread");
    }

    ~thread() {
      if (m_handle)
        ::CloseHandle(m_handle);
    }

    void join() {
      ::WaitForSingleObject(m_handle, INFINITE);
    }

  private:

    std::function<void()> m_proc;
    HANDLE                m_handle;

    static DWORD WINAPI nativeProc(void *arg) {
      auto* proc = reinterpret_cast<thread*>(arg);
      proc->m_proc();
      return 0;
    }

  };

  namespace this_thread {
    inline void yield() {
      Sleep(0);
    }
  }
}
