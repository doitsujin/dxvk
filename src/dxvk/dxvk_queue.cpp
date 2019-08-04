#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device)
  : m_device(device),
    m_submitThread([this] () { submitCmdLists(); }),
    m_finishThread([this] () { finishCmdLists(); }) {
    // Asynchronous presentation seems to increase the
    // likelyhood of hangs on Nvidia for some reason.
    m_asyncPresent = !m_device->adapter()->matchesDriver(
      DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0);

    applyTristate(m_asyncPresent, m_device->config().asyncPresent);
  }
  
  
  DxvkSubmissionQueue::~DxvkSubmissionQueue() {
    { std::unique_lock<std::mutex> lock(m_mutex);
      m_stopped.store(true);
    }
    
    m_appendCond.notify_all();
    m_submitCond.notify_all();

    m_submitThread.join();
    m_finishThread.join();
  }
  
  
  void DxvkSubmissionQueue::submit(DxvkSubmitInfo submitInfo) {
    std::unique_lock<std::mutex> lock(m_mutex);

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
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_asyncPresent) {
      DxvkSubmitEntry entry = { };
      entry.status  = status;
      entry.present = std::move(presentInfo);

      m_submitQueue.push(std::move(entry));
      m_appendCond.notify_all();
    } else {
      m_submitCond.wait(lock, [this] {
        return m_submitQueue.empty();
      });

      VkResult result = presentInfo.presenter->presentImage(presentInfo.waitSync);
      status->result.store(result);
    }
  }


  void DxvkSubmissionQueue::synchronizeSubmission(
          DxvkSubmitStatus*   status) {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [status] {
      return status->result.load() != VK_NOT_READY;
    });
  }


  void DxvkSubmissionQueue::synchronize() {
    std::unique_lock<std::mutex> lock(m_mutex);

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


  void DxvkSubmissionQueue::submitCmdLists() {
    env::setThreadName("dxvk-submit");

    std::unique_lock<std::mutex> lock(m_mutex);

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

      { std::lock_guard<std::mutex> lock(m_mutexQueue);

        if (entry.submit.cmdList != nullptr) {
          status = entry.submit.cmdList->submit(
            entry.submit.waitSync,
            entry.submit.wakeSync);
        } else if (entry.present.presenter != nullptr) {
          status = entry.present.presenter->presentImage(
            entry.present.waitSync);
        }
      }

      if (entry.status)
        entry.status->result = status;
      
      // On success, pass it on to the queue thread
      lock = std::unique_lock<std::mutex>(m_mutex);

      if (status == VK_SUCCESS) {
        if (entry.submit.cmdList != nullptr)
          m_finishQueue.push(std::move(entry));
      } else if (entry.submit.cmdList != nullptr) {
        Logger::err(str::format("DxvkSubmissionQueue: Command submission failed: ", status));
      }

      m_submitQueue.pop();
      m_submitCond.notify_all();
    }
  }
  
  
  void DxvkSubmissionQueue::finishCmdLists() {
    env::setThreadName("dxvk-queue");

    std::unique_lock<std::mutex> lock(m_mutex);

    while (!m_stopped.load()) {
      if (m_finishQueue.empty()) {
        auto t0 = std::chrono::high_resolution_clock::now();

        m_submitCond.wait(lock, [this] {
          return m_stopped.load() || !m_finishQueue.empty();
        });

        auto t1 = std::chrono::high_resolution_clock::now();
        m_gpuIdle += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      }

      if (m_stopped.load())
        return;
      
      DxvkSubmitEntry entry = std::move(m_finishQueue.front());
      lock.unlock();
      
      VkResult status = entry.submit.cmdList->synchronize();
      
      if (status == VK_SUCCESS) {
        entry.submit.cmdList->notifySignals();
        entry.submit.cmdList->reset();
        
        m_device->recycleCommandList(entry.submit.cmdList);
      } else {
        Logger::err(str::format(
          "DxvkSubmissionQueue: Failed to sync fence: ",
          status));
      }

      lock = std::unique_lock<std::mutex>(m_mutex);
      m_pending -= 1;

      m_finishQueue.pop();
      m_finishCond.notify_all();
    }
  }
  
}