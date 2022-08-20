#pragma once

#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "dxvk_resource.h"

#include "../util/thread.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkFence;

  using DxvkFenceEvent = std::function<void ()>;

  /**
   * \brief Fence create info
   */
  struct DxvkFenceCreateInfo {
    uint64_t        initialValue;
    VkExternalSemaphoreHandleTypeFlagBits sharedType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FLAG_BITS_MAX_ENUM;
    union {
      // When we want to implement this on non-Windows platforms,
      // we could add a `int fd` here, etc.
      HANDLE          sharedHandle = INVALID_HANDLE_VALUE;
    };
  };

  /**
   * \brief Fence-value pair
   */
  struct DxvkFenceValuePair {
    DxvkFenceValuePair() { }
    DxvkFenceValuePair(Rc<DxvkFence>&& fence_, uint64_t value_)
    : fence(std::move(fence_)), value(value_) { }
    DxvkFenceValuePair(const Rc<DxvkFence>& fence_, uint64_t value_)
    : fence(fence_), value(value_) { }

    Rc<DxvkFence> fence;
    uint64_t value;
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

    /**
     * \brief Create a new shared handle to timeline semaphore backing the fence
     * \returns The shared handle with the type given by DxvkFenceCreateInfo::sharedType
     */
    HANDLE sharedHandle() const;

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
