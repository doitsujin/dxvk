#include "dxvk_device.h"
#include "dxvk_fence.h"

namespace dxvk {

  DxvkFence::DxvkFence(
          DxvkDevice*           device,
    const DxvkFenceCreateInfo&  info)
  : m_vkd(device->vkd()), m_info(info) {
    VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue = info.initialValue;
    
    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeInfo };

    VkResult vr = m_vkd->vkCreateSemaphore(m_vkd->device(),
      &semaphoreInfo, nullptr, &m_semaphore);

    if (vr != VK_SUCCESS)
      throw DxvkError("Failed to create timeline semaphore");

    m_thread = dxvk::thread([this] { run(); });
  }


  DxvkFence::~DxvkFence() {
    m_stop.store(true);
    m_thread.join();

    m_vkd->vkDestroySemaphore(m_vkd->device(), m_semaphore, nullptr);
  }


  void DxvkFence::enqueueWait(uint64_t value, DxvkFenceEvent&& event) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    if (value > m_lastValue.load())
      m_queue.emplace(value, std::move(event));
    else
      event();
  }
  

  void DxvkFence::run() {
    uint64_t value = 0ull;

    VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;

    while (!m_stop.load()) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);

      // Query actual semaphore value and start from there, so that
      // we can skip over large increments in the semaphore value
      VkResult vr = m_vkd->vkGetSemaphoreCounterValue(m_vkd->device(), m_semaphore, &value);

      if (vr != VK_SUCCESS) {
        Logger::err(str::format("Failed to query semaphore value: ", vr));
        return;
      }

      m_lastValue.store(value);

      // Signal all enqueued events whose value is not greater than
      // the current semaphore value
      while (!m_queue.empty() && m_queue.top().value <= value) {
        m_queue.top().event();
        m_queue.pop();
      }

      if (m_stop)
        return;

      lock.unlock();

      // Wait for the semaphore to be singaled again and update state.
      // The timeout is unfortunate, but we can't always know when a
      // signal operation has been recorded, and the alternative would
      // be to create a teardown semaphore and use WAIT_ANY, which may
      // be fall back to a busy-wait loop on some drivers.
      value += 1;

      vr = m_vkd->vkWaitSemaphores(
        m_vkd->device(), &waitInfo, 10'000'000ull);

      if (vr != VK_SUCCESS && vr != VK_TIMEOUT) {
        Logger::err(str::format("Failed to wait for semaphore: ", vr));
        return;
      }
    }
  }

}
