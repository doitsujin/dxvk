#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device)
  : m_device(device),
    m_submitThread([this] () { submitCmdLists(); }),
    m_finishThread([this] () { finishCmdLists(); }) {
    auto vk = m_device->vkd();

    VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;

    VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeInfo };

    if (vk->vkCreateSemaphore(vk->device(), &info, nullptr, &m_semaphore))
      throw DxvkError("Failed to create global timeline semaphore");
  }
  
  
  DxvkSubmissionQueue::~DxvkSubmissionQueue() {
    auto vk = m_device->vkd();

    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_stopped.store(true);
    }

    m_appendCond.notify_all();
    m_submitCond.notify_all();

    m_submitThread.join();
    m_finishThread.join();

    synchronizeSemaphore(m_semaphoreValue);

    vk->vkDestroySemaphore(vk->device(), m_semaphore, nullptr);
  }
  
  
  void DxvkSubmissionQueue::submit(DxvkSubmitInfo submitInfo) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_finishCond.wait(lock, [this] {
      return m_submitQueue.size() + m_finishQueue.size() <= MaxNumQueuedCommandBuffers;
    });

    DxvkSubmitEntry entry = { };
    entry.submit = std::move(submitInfo);

    m_pending += 1;
    m_submitQueue.push(std::move(entry));
    m_appendCond.notify_all();
  }


  void DxvkSubmissionQueue::present(DxvkPresentInfo presentInfo, DxvkSubmitStatus* status) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    DxvkSubmitEntry entry = { };
    entry.status  = status;
    entry.present = std::move(presentInfo);

    m_submitQueue.push(std::move(entry));
    m_appendCond.notify_all();
  }


  void DxvkSubmissionQueue::synchronizeSubmission(
          DxvkSubmitStatus*   status) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [status] {
      return status->result.load() != VK_NOT_READY;
    });
  }


  void DxvkSubmissionQueue::synchronize() {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [this] {
      return m_submitQueue.empty();
    });
  }


  void DxvkSubmissionQueue::lockDeviceQueue() {
    m_mutexQueue.lock();
  }


  void DxvkSubmissionQueue::unlockDeviceQueue() {
    m_mutexQueue.unlock();
  }


  VkResult DxvkSubmissionQueue::synchronizeSemaphore(
          uint64_t        semaphoreValue) {
    auto vk = m_device->vkd();

    VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &semaphoreValue;

    // 32-bit winevulkan on Proton seems to be broken here
    // and returns with VK_TIMEOUT, even though the timeout
    // is infinite. Work around this by spinning.
    VkResult vr = VK_TIMEOUT;

    while (vr == VK_TIMEOUT)
      vr = vk->vkWaitSemaphores(vk->device(), &waitInfo, ~0ull);

    if (vr)
      Logger::err(str::format("Failed to synchronize with global timeline semaphore: ", vr));

    return vr;
  }


  void DxvkSubmissionQueue::submitCmdLists() {
    env::setThreadName("dxvk-submit");

    std::unique_lock<dxvk::mutex> lock(m_mutex);

    while (!m_stopped.load()) {
      m_appendCond.wait(lock, [this] {
        return m_stopped.load() || !m_submitQueue.empty();
      });
      
      if (m_stopped.load())
        return;
      
      DxvkSubmitEntry entry = std::move(m_submitQueue.front());
      lock.unlock();

      // Submit command buffer to device
      VkResult status = VK_NOT_READY;

      if (m_lastError != VK_ERROR_DEVICE_LOST) {
        std::lock_guard<dxvk::mutex> lock(m_mutexQueue);

        if (entry.submit.cmdList != nullptr) {
          status = entry.submit.cmdList->submit(m_semaphore, m_semaphoreValue);
          entry.submit.semaphoreValue = m_semaphoreValue;
        } else if (entry.present.presenter != nullptr) {
          status = entry.present.presenter->presentImage();
        }
      } else {
        // Don't submit anything after device loss
        // so that drivers get a chance to recover
        status = VK_ERROR_DEVICE_LOST;
      }

      if (entry.status)
        entry.status->result = status;
      
      // On success, pass it on to the queue thread
      lock = std::unique_lock<dxvk::mutex>(m_mutex);

      if (status == VK_SUCCESS) {
        if (entry.submit.cmdList != nullptr)
          m_finishQueue.push(std::move(entry));
      } else if (status == VK_ERROR_DEVICE_LOST || entry.submit.cmdList != nullptr) {
        Logger::err(str::format("DxvkSubmissionQueue: Command submission failed: ", status));
        m_lastError = status;
        m_device->waitForIdle();
      }

      m_submitQueue.pop();
      m_submitCond.notify_all();
    }
  }
  
  
  void DxvkSubmissionQueue::finishCmdLists() {
    env::setThreadName("dxvk-queue");

    while (!m_stopped.load()) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);

      if (m_finishQueue.empty()) {
        auto t0 = dxvk::high_resolution_clock::now();

        m_submitCond.wait(lock, [this] {
          return m_stopped.load() || !m_finishQueue.empty();
        });

        auto t1 = dxvk::high_resolution_clock::now();
        m_gpuIdle += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      }

      if (m_stopped.load())
        return;
      
      DxvkSubmitEntry entry = std::move(m_finishQueue.front());
      lock.unlock();
      
      VkResult status = m_lastError.load();
      
      if (status != VK_ERROR_DEVICE_LOST)
        status = synchronizeSemaphore(entry.submit.semaphoreValue);
      
      if (status != VK_SUCCESS) {
        m_lastError = status;
        m_device->waitForIdle();
      }

      // Release resources and signal events, then immediately wake
      // up any thread that's currently waiting on a resource in
      // order to reduce delays as much as possible.
      entry.submit.cmdList->notifyObjects();

      lock.lock();
      m_pending -= 1;

      m_finishQueue.pop();
      m_finishCond.notify_all();
      lock.unlock();

      // Free the command list and associated objects now
      entry.submit.cmdList->reset();
      m_device->recycleCommandList(entry.submit.cmdList);
    }
  }
  
}