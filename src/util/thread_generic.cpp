#include "thread_generic.h"

#include "./log/log.h"

#include <dxvk.h>

namespace dxvk {

    ThreadFn::ThreadFn(Proc&& proc)
    : m_proc(std::move(proc)) {
      // Reference for the thread function
      this->incRef();

      m_handle = ::g_native_info.pfn_create_thread(ThreadFn::threadProc, this);

      if(m_handle == nullptr)
        throw DxvkError("Failed to create thread");
    }

    ThreadFn::~ThreadFn() {
      if (this->joinable())
        std::terminate();
    }

    void ThreadFn::join() {
      if(!::g_native_info.pfn_join_thread(m_handle))
        throw DxvkError("Failed to join thread");
      this->detach();
    }

    bool ThreadFn::joinable() const {
        return m_handle != nullptr;
    }

    void ThreadFn::detach() {
      ::g_native_info.pfn_detach_thread(m_handle);
      m_handle = nullptr;
    }

    void ThreadFn::set_priority(ThreadPriority priority)
    {
      // Based on wine staging server-Realtime_Priority patch

      struct sched_param param;
      int policy = SCHED_OTHER;

      switch (priority) {
        case ThreadPriority::Highest:
          policy = SCHED_FIFO;
          param.sched_priority = 2;
          break;

        case ThreadPriority::High:
          policy = SCHED_FIFO;
          param.sched_priority = 0;
          break;

        case ThreadPriority::Normal:
          policy = SCHED_OTHER;
          break;

        case ThreadPriority::Low:
          policy = SCHED_IDLE;
          break;

        case ThreadPriority::Lowest:
          policy = SCHED_BATCH;
          break;
      }

      if (pthread_setschedparam(pthread_self(), policy, &param) == -1)
        Logger::warn("Failed to set thread priority");
    }

    void ThreadFn::threadProc(void *arg) {
      auto thread = reinterpret_cast<ThreadFn*>(arg);
      thread->m_proc();
      thread->decRef();
    }

}