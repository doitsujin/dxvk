#pragma once

#include <functional>
#include <queue>

#include "dxvk_resource.h"

#include "../util/thread.h"

namespace dxvk {

  class DxvkDevice;

  using DxvkFenceEvent = std::function<void ()>;

  /**
   * \brief Fence create info
   */
  struct DxvkFenceCreateInfo {
    uint64_t        initialValue;
  };

  /**
   * \brief Fence
   *
   * Wrapper around Vulkan timeline semaphores that
   * can signal an event when the value changes.
   */
  class DxvkFence : public RcObject {

  public:

    DxvkFence(
            DxvkDevice*           device,
      const DxvkFenceCreateInfo&  info);

    ~DxvkFence();

    /**
     * \brief Semaphore handle
     */
    VkSemaphore handle() const {
      return m_semaphore;
    }

    /**
     * \brief Retrieves current semaphore value
     * \returns Current semaphore value
     */
    uint64_t getValue() {
      return m_lastValue.load();
    }

    /**
     * \brief Enqueues semaphore wait
     *
     * Signals the given event when the
     * semaphore reaches the given value.
     * \param [in] value Enqueue value
     * \param [in] event Callback
     */
    void enqueueWait(uint64_t value, DxvkFenceEvent&& event);

  private:

    struct QueueItem {
      QueueItem() { }
      QueueItem(uint32_t v, DxvkFenceEvent&& e)
      : value(v), event(std::move(e)) { }

      uint64_t        value;
      DxvkFenceEvent  event;

      bool operator == (const QueueItem& item) const { return value == item.value; }
      bool operator != (const QueueItem& item) const { return value != item.value; }
      bool operator <  (const QueueItem& item) const { return value <  item.value; }
      bool operator <= (const QueueItem& item) const { return value <= item.value; }
      bool operator >  (const QueueItem& item) const { return value >  item.value; }
      bool operator >= (const QueueItem& item) const { return value >= item.value; }
    };

    Rc<vk::DeviceFn>                m_vkd;
    DxvkFenceCreateInfo             m_info;
    VkSemaphore                     m_semaphore;

    std::priority_queue<QueueItem>  m_queue;
    std::atomic<uint64_t>           m_lastValue = { 0ull  };
    std::atomic<bool>               m_stop      = { false };

    dxvk::mutex                     m_mutex;
    dxvk::thread                    m_thread;

    void run();

  };

}