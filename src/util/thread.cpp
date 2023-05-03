#include <atomic>

#include "thread.h"
#include "util_likely.h"

#ifdef _WIN32

namespace dxvk {

  thread::thread(ThreadProc&& proc)
  : m_data(new ThreadData(std::move(proc))) {
    m_data->handle = ::CreateThread(nullptr, 0x100000,
      thread::threadProc, m_data, STACK_SIZE_PARAM_IS_A_RESERVATION,
      &m_data->id);

    if (!m_data->handle) {
      delete m_data;
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "Failed to create thread");
    }
  }


  thread::~thread() {
    if (joinable())
      std::terminate();
  }


  void thread::join() {
    if (!joinable())
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Thread not joinable");

    if (get_id() == this_thread::get_id())
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "Cannot join current thread");

    if(::WaitForSingleObjectEx(m_data->handle, INFINITE, FALSE) == WAIT_FAILED)
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Joining thread failed");

    detach();
  }


  void thread::set_priority(ThreadPriority priority) {
    int32_t value;
    switch (priority) {
      default:
      case ThreadPriority::Normal: value = THREAD_PRIORITY_NORMAL; break;
      case ThreadPriority::Lowest: value = THREAD_PRIORITY_LOWEST; break;
    }

    if (m_data)
      ::SetThreadPriority(m_data->handle, int32_t(value));
  }


  uint32_t thread::hardware_concurrency() {
    SYSTEM_INFO info = { };
    ::GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
  }


  DWORD WINAPI thread::threadProc(void* arg) {
    auto data = reinterpret_cast<ThreadData*>(arg);
    DWORD exitCode = 0;

    try {
      data->proc();
    } catch (...) {
      exitCode = 1;
    }

    data->decRef();
    return exitCode;
  }

}


namespace dxvk::this_thread {

  bool isInModuleDetachment() {
    using PFN_RtlDllShutdownInProgress = BOOLEAN (WINAPI *)();

    static auto RtlDllShutdownInProgress = reinterpret_cast<PFN_RtlDllShutdownInProgress>(
      ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlDllShutdownInProgress"));

    return RtlDllShutdownInProgress();
  }

}

#else

namespace dxvk::this_thread {
  
  static std::atomic<uint32_t> g_threadCtr = { 0u };
  static thread_local uint32_t g_threadId  = 0u;
  
  // This implementation returns thread ids unique to the current instance.
  // ie. if you use this across multiple .so's then you might get conflicting ids.
  //
  // This isn't an issue for us, as it is only used by the spinlock implementation,
  // but may be for you if you use this elsewhere.
  uint32_t get_id() {
    if (unlikely(!g_threadId))
      g_threadId = ++g_threadCtr;

    return g_threadId;
  }

}

#endif
