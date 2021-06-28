#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "util_error.h"

#include "./com/com_include.h"

#include "./rc/util_rc.h"
#include "./rc/util_rc_ptr.h"

namespace dxvk {

  /**
   * \brief Thread priority
   */
  enum class ThreadPriority : int32_t {
    Lowest      = THREAD_PRIORITY_LOWEST,
    Low         = THREAD_PRIORITY_BELOW_NORMAL,
    Normal      = THREAD_PRIORITY_NORMAL,
    High        = THREAD_PRIORITY_ABOVE_NORMAL,
    Highest     = THREAD_PRIORITY_HIGHEST,
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

    ThreadFn(Proc&& proc)
    : m_proc(std::move(proc)) {
      // Reference for the thread function
      this->incRef();

      m_handle = ::CreateThread(nullptr, 0x100000,
        ThreadFn::threadProc, this, STACK_SIZE_PARAM_IS_A_RESERVATION,
        nullptr);
      
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

    void set_priority(ThreadPriority priority) {
      ::SetThreadPriority(m_handle, int32_t(priority));
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

    void set_priority(ThreadPriority priority) {
      m_thread->set_priority(priority);
    }
    
    static uint32_t hardware_concurrency() {
      SYSTEM_INFO info = { };
      ::GetSystemInfo(&info);
      return info.dwNumberOfProcessors;
    }

  private:

    Rc<ThreadFn> m_thread;

  };


  namespace this_thread {
    inline void yield() {
      SwitchToThread();
    }
  }


  /**
   * \brief SRW-based mutex implementation
   *
   * Drop-in replacement for \c std::mutex that uses Win32
   * SRW locks, which are implemented with \c futex in wine.
   */
  class mutex {

  public:

    using native_handle_type = PSRWLOCK;

    mutex() { }

    mutex(const mutex&) = delete;
    mutex& operator = (const mutex&) = delete;

    void lock() {
      AcquireSRWLockExclusive(&m_lock);
    }

    void unlock() {
      ReleaseSRWLockExclusive(&m_lock);
    }

    bool try_lock() {
      return TryAcquireSRWLockExclusive(&m_lock);
    }

    native_handle_type native_handle() {
      return &m_lock;
    }

  private:

    SRWLOCK m_lock = SRWLOCK_INIT;

  };


  /**
   * \brief Recursive mutex implementation
   *
   * Drop-in replacement for \c std::recursive_mutex that
   * uses Win32 critical sections.
   */
  class recursive_mutex {

  public:

    using native_handle_type = PCRITICAL_SECTION;

    recursive_mutex() {
      InitializeCriticalSection(&m_lock);
    }

    ~recursive_mutex() {
      DeleteCriticalSection(&m_lock);
    }

    recursive_mutex(const recursive_mutex&) = delete;
    recursive_mutex& operator = (const recursive_mutex&) = delete;

    void lock() {
      EnterCriticalSection(&m_lock);
    }

    void unlock() {
      LeaveCriticalSection(&m_lock);
    }

    bool try_lock() {
      return TryEnterCriticalSection(&m_lock);
    }

    native_handle_type native_handle() {
      return &m_lock;
    }

  private:

    CRITICAL_SECTION m_lock;

  };


  /**
   * \brief SRW-based condition variable implementation
   *
   * Drop-in replacement for \c std::condition_variable that
   * uses Win32 condition variables on SRW locks.
   */
  class condition_variable {

  public:

    using native_handle_type = PCONDITION_VARIABLE;

    condition_variable() {
      InitializeConditionVariable(&m_cond);
    }

    condition_variable(condition_variable&) = delete;

    condition_variable& operator = (condition_variable&) = delete;

    void notify_one() {
      WakeConditionVariable(&m_cond);
    }

    void notify_all() {
      WakeAllConditionVariable(&m_cond);
    }

    void wait(std::unique_lock<dxvk::mutex>& lock) {
      auto srw = lock.mutex()->native_handle();
      SleepConditionVariableSRW(&m_cond, srw, INFINITE, 0);
    }

    template<typename Predicate>
    void wait(std::unique_lock<dxvk::mutex>& lock, Predicate pred) {
      while (!pred())
        wait(lock);
    }

    template<typename Clock, typename Duration>
    std::cv_status wait_until(std::unique_lock<dxvk::mutex>& lock, const std::chrono::time_point<Clock, Duration>& time) {
      auto now = Clock::now();

      return (now < time)
        ? wait_for(lock, now - time)
        : std::cv_status::timeout;
    }

    template<typename Clock, typename Duration, typename Predicate>
    bool wait_until(std::unique_lock<dxvk::mutex>& lock, const std::chrono::time_point<Clock, Duration>& time, Predicate pred) {
      if (pred())
        return true;

      auto now = Clock::now();
      return now < time && wait_for(lock, now - time, pred);
    }

    template<typename Rep, typename Period>
    std::cv_status wait_for(std::unique_lock<dxvk::mutex>& lock, const std::chrono::duration<Rep, Period>& timeout) {
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
      auto srw = lock.mutex()->native_handle();

      return SleepConditionVariableSRW(&m_cond, srw, ms.count(), 0)
        ? std::cv_status::no_timeout
        : std::cv_status::timeout;
    }

    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(std::unique_lock<dxvk::mutex>& lock, const std::chrono::duration<Rep, Period>& timeout, Predicate pred) {
      bool result = pred();

      if (!result && wait_for(lock, timeout) == std::cv_status::no_timeout)
        result = pred();

      return result;
    }

    native_handle_type native_handle() {
      return &m_cond;
    }

  private:

    CONDITION_VARIABLE m_cond;

  };

}
