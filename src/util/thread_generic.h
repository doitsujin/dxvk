#pragma once

#include <functional>

#include <dxvk.h>

#include <sys/sysinfo.h>
#include <pthread.h>

#include "util_error.h"
#include "util_env.h"

#include "./rc/util_rc.h"
#include "./rc/util_rc_ptr.h"

namespace dxvk {

  /**
   * \brief Thread priority
   */
  enum class ThreadPriority : int32_t {
    Lowest,
    Low,
    Normal,
    High,
    Highest,
  };

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

    ThreadFn(Proc&& proc) {
      // Reference for the thread function
      this->incRef();

      m_handle = env::g_native_info.pfn_create_thread(ThreadFn::threadProc, this);

      if(m_handle == nullptr)
        throw DxvkError("Failed to create thread");
    }

    ~ThreadFn() {
      if (this->joinable())
        std::terminate();
    }
    
    void detach() {
      env::g_native_info.pfn_detach_thread(m_handle);
    }

    void join() {
      env::g_native_info.pfn_join_thread(m_handle);
      this->detach();
    }

    bool joinable() const {
      return m_handle != nullptr;
    }

    void set_priority(ThreadPriority priority) {
      // TODO
    }

  private:

    Proc  m_proc;
    void  *m_handle;

    static void threadProc(void *arg) {
      auto thread = reinterpret_cast<ThreadFn*>(arg);
      thread->m_proc();
      thread->decRef();
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

    void set_priority(ThreadPriority priority) {
      m_thread->set_priority(priority);
    }
    
    static uint32_t hardware_concurrency() {
      return get_nprocs();
    }

  private:

    Rc<ThreadFn> m_thread;

  };


  namespace this_thread {
    inline void yield() {
      pthread_yield();
    }
  }
}