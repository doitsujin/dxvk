#pragma once

#include <functional>

#include "util_error.h"

#include "./com/com_include.h"

#include "./rc/util_rc.h"
#include "./rc/util_rc_ptr.h"

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
  class ThreadFn : public RcObject {
    using Proc = std::function<void()>;
  public:

    ThreadFn(Proc&& proc)
    : m_proc(std::move(proc)) {
      // Reference for the thread function
      this->incRef();

      m_handle = ::CreateThread(nullptr, 0,
        ThreadFn::threadProc, this, 0, nullptr);
      
      if (m_handle == nullptr)
        throw DxvkError("Failed to create thread");
    }

    ~ThreadFn() {
      if (this->joinable())
        std::terminate();
    }
    
    void detach() {
      ::CloseHandle(m_handle);
      m_handle = nullptr;
    }

    void join() {
      if(::WaitForSingleObjectEx(m_handle, INFINITE, FALSE) == WAIT_FAILED)
        throw DxvkError("Failed to join thread");
      this->detach();
    }

    bool joinable() const {
      return m_handle != nullptr;
    }

  private:

    Proc    m_proc;
    HANDLE  m_handle;

    static DWORD WINAPI threadProc(void *arg) {
      auto thread = reinterpret_cast<ThreadFn*>(arg);
      thread->m_proc();
      thread->decRef();
      return 0;
    }

  };


  /**
   * \brief RAII thread wrapper
   * 
   * Wrapper for \c ThreadFn that can be used
   * as a drop-in replacement for \c std::thread.
   */
  class thread {

  public:

    thread() { }

    explicit thread(std::function<void()>&& func)
    : m_thread(new ThreadFn(std::move(func))) { }

    thread(thread&& other)
    : m_thread(std::move(other.m_thread)) { }

    thread& operator = (thread&& other) {
      m_thread = std::move(other.m_thread);
      return *this;
    }

    void detach() {
      m_thread->detach();
    }

    void join() {
      m_thread->join();
    }

    bool joinable() const {
      return m_thread != nullptr
          && m_thread->joinable();
    }

  private:

    Rc<ThreadFn> m_thread;

  };


  namespace this_thread {
    inline void yield() {
      Sleep(0);
    }
  }
}
