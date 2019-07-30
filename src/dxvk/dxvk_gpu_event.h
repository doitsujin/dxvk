#pragma once

#include <vector>

#include "dxvk_resource.h"

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
  struct DxvkGpuEventHandle {
    DxvkGpuEventPool* pool  = nullptr;
    VkEvent           event = VK_NULL_HANDLE;
  };


  /**
   * \brief GPU event
   *
   * An event managed by the GPU which allows
   * the application to check whether a specific
   * command has completed execution.
   */
  class DxvkGpuEvent : public DxvkResource {

  public:

    DxvkGpuEvent(const Rc<vk::DeviceFn>& vkd);
    ~DxvkGpuEvent();

    /**
     * \brief Retrieves event status
     * 
     * Only valid after the event has been
     * recorded intro a command buffer.
     * \returns Event status
     */
    DxvkGpuEventStatus test() const;

    /**
     * \brief Resets event
     * 
     * Assigns a new Vulkan event to this event
     * object and replaces the old one. The old
     * event should be freed as soon as the GPU
     * stops using it.
     * \param [in] handle New GPU event handle
     * \returns Old GPU event handle
     */
    DxvkGpuEventHandle reset(DxvkGpuEventHandle handle);

  private:

    Rc<vk::DeviceFn>   m_vkd;
    DxvkGpuEventHandle m_handle;

  };


  /**
   * \brief Event pool
   * 
   * Thread-safe event allocator that provides
   * a way to create and recycle Vulkan events.
   */
  class DxvkGpuEventPool {
  
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
    DxvkGpuEventHandle allocEvent();

    /**
     * \brief Recycles an event
     * 
     * \param [in] handle Event to free
     */
    void freeEvent(VkEvent event);

  private:

    Rc<vk::DeviceFn>     m_vkd;
    sync::Spinlock       m_mutex;
    std::vector<VkEvent> m_events;

  };


  /**
   * \brief GPU event tracker
   * 
   * Stores events currently accessed by the
   * GPU, and returns them to the event pool
   * once they are no longer in use.
   */
  class DxvkGpuEventTracker {

  public:

    DxvkGpuEventTracker();
    ~DxvkGpuEventTracker();

    /**
     * \brief Tracks an event
     * \param [in] handle Event to track
     */
    void trackEvent(DxvkGpuEventHandle handle);

    /**
     * \brief Resets event tracker
     * 
     * Releases all tracked events back
     * to the respective event pool
     */
    void reset();

  private:

    std::vector<DxvkGpuEventHandle> m_handles;

  };

}