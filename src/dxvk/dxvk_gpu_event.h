#pragma once

#include <atomic>
#include <vector>

#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkGpuEventPool;

  /**
   * \brief Event status
   * 
   * Reports whether the event is in
   * a signaled or unsignaled state.
   */
  enum class DxvkGpuEventStatus : uint32_t {
    Invalid   = 0,
    Pending   = 1,
    Signaled  = 2,
  };


  /**
   * \brief Event handle
   *
   * Stores the event handle itself as well
   * as a pointer to the pool that the event
   * was allocated from.
   */
  class DxvkGpuEvent {

  public:

    explicit DxvkGpuEvent(
            DxvkGpuEventPool*           parent);

    ~DxvkGpuEvent();

    /**
     * \brief Increments ref count
     */
    void incRef() {
      m_refs.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements ref count
     *
     * Returns event to the pool if no further
     * references exist for this event.
     */
    void decRef() {
      if (m_refs.fetch_sub(1u, std::memory_order_release) == 1u)
        free();
    }

    /**
     * \brief Queries event handle
     * \returns Event handle
     */
    VkEvent handle() const {
      return m_event;
    }

  private:

    DxvkGpuEventPool*     m_pool  = nullptr;
    VkEvent               m_event = VK_NULL_HANDLE;

    std::atomic<uint32_t> m_refs  = { 0u };

    void free();

  };


  /**
   * \brief GPU event
   *
   * An event managed by the GPU which allows
   * the application to check whether a specific
   * command has completed execution.
   */
  class DxvkEvent {
    friend class DxvkContext;
  public:

    DxvkEvent(const Rc<DxvkDevice>& device);
    ~DxvkEvent();

    /**
     * \brief Increments reference count
     */
    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements reference count
     * Frees the event as necessary.
     */
    force_inline void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Retrieves event status
     * 
     * Only valid after the event has been
     * recorded intro a command buffer.
     * \returns Event status
     */
    DxvkGpuEventStatus test();

  private:

    std::atomic<uint32_t> m_refCount = { 0u };

    sync::Spinlock    m_mutex;
    VkResult          m_status = VK_NOT_READY;

    Rc<DxvkDevice>    m_device;
    Rc<DxvkGpuEvent>  m_gpuEvent;

    void assignGpuEvent(
            Rc<DxvkGpuEvent>            event);

  };


  /**
   * \brief Event pool
   * 
   * Thread-safe event allocator that provides
   * a way to create and recycle Vulkan events.
   */
  class DxvkGpuEventPool {
    friend class DxvkGpuEvent;
  public:

    DxvkGpuEventPool(const DxvkDevice* device);
    ~DxvkGpuEventPool();

    /**
     * \brief Allocates an event
     * 
     * Either returns a recycled event, or
     * creates a new one if necessary. The
     * state of the event is undefined.
     * \returns An event handle
     */
    Rc<DxvkGpuEvent> allocEvent();

    /**
     * \brief Recycles an event
     * 
     * \param [in] event Event object to free
     */
    void freeEvent(DxvkGpuEvent* event);

  private:

    Rc<vk::DeviceFn>            m_vkd;

    dxvk::mutex                 m_mutex;
    std::vector<DxvkGpuEvent*>  m_freeEvents;

  };

}
