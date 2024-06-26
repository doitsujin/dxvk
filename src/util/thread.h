#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

#include "util_error.h"

#include "./com/com_include.h"

#include "./rc/util_rc.h"
#include "./rc/util_rc_ptr.h"

namespace dxvk {

  /**
   * \brief Thread priority
   */
  enum class ThreadPriority : int32_t {
    Normal,
    Lowest,
  };

#ifdef _WIN32

  using ThreadProc = std::function<void()>;


  /**
   * \brief Thread object
   */
  struct ThreadData {
    ThreadData(ThreadProc&& proc_)
    : proc(std::move(proc_)) { }

    ~ThreadData() {
      if (handle)
        CloseHandle(handle);
    }

    HANDLE                handle = nullptr;
    DWORD                 id     = 0;
    std::atomic<uint32_t> refs   = { 2u };
    ThreadProc            proc;

    void decRef() {
      if (refs.fetch_sub(1, std::memory_order_release) == 1)
        delete this;
    }
  };


  /**
   * \brief Thread wrapper
   *
   * Drop-in replacement for std::thread
   * using plain win32 threads.
   */
  class thread {

  public:

    using id = uint32_t;
    using native_handle_type = HANDLE;

    thread() { }

    explicit thread(ThreadProc&& proc);

    ~thread();

    thread(thread&& other)
    : m_data(std::exchange(other.m_data, nullptr)) { }

    thread& operator = (thread&& other) {
      if (m_data)
        m_data->decRef();

      m_data = std::exchange(other.m_data, nullptr);
      return *this;
    }

    void detach() {
      m_data->decRef();
      m_data = nullptr;
    }

    bool joinable() const {
      return m_data != nullptr;
    }

    id get_id() const {
      return joinable() ? m_data->id : id();
    }

    native_handle_type native_handle() const {
      return joinable() ? m_data->handle : native_handle_type();
    }

    void swap(thread& other) {
      std::swap(m_data, other.m_data);
    }

    void join();

    void set_priority(ThreadPriority priority);

    static uint32_t hardware_concurrency();

  private:

    ThreadData* m_data = nullptr;

    static DWORD WINAPI threadProc(void* arg);

  };


  namespace this_thread {
    inline void yield() {
      SwitchToThread();
    }

    inline thread::id get_id() {
      return thread::id(GetCurrentThreadId());
    }

    bool isInModuleDetachment();
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

#else
  class thread : public std::thread {
  public:
    using std::thread::thread;

    void set_priority(ThreadPriority priority) {
      ::sched_param param = {};
      int32_t policy;
      switch (priority) {
        default:
        case ThreadPriority::Normal: policy = SCHED_OTHER; break;
#ifndef __linux__
        case ThreadPriority::Lowest: policy = SCHED_OTHER; break;
#else
        case ThreadPriority::Lowest: policy = SCHED_IDLE;  break;
#endif
      }
      ::pthread_setschedparam(this->native_handle(), policy, &param);
    }
  };

  using mutex              = std::mutex;
  using recursive_mutex    = std::recursive_mutex;
  using condition_variable = std::condition_variable;

  namespace this_thread {
    inline void yield() {
      std::this_thread::yield();
    }

    uint32_t get_id();

    inline bool isInModuleDetachment() {
      return false;
    }
  }
#endif

}
