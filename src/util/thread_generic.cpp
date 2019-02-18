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
        // TODO
    }

    void ThreadFn::threadProc(void *arg) {
      auto thread = reinterpret_cast<ThreadFn*>(arg);
      thread->m_proc();
      thread->decRef();
    }

}