#pragma once

#include "util_error.h"

#include <windef.h>
#include <winbase.h>

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

    template<typename T>
    class thread_proc {
    private:
      T proc;
    public:
      thread_proc(T &proc) : proc(proc) {}

      static DWORD WINAPI nativeProc(void *arg) {
        thread_proc *proc = reinterpret_cast<thread_proc*>(arg);
        proc->proc();
        delete proc;
        return 0;
      }
    };

  public:
    template<class T>
    explicit thread(T &&func) {
        thread_proc<T> *proc = new thread_proc<T>(func);
        m_handle = ::CreateThread(nullptr, 0, thread_proc<T>::nativeProc, proc, 0, nullptr);
        if (!m_handle) {
            delete proc;
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
