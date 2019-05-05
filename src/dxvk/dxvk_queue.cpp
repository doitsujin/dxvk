#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device)
  : m_device(device),
    m_submitThread([this] () { submitCmdLists(); }),
    m_finishThread([this] () { finishCmdLists(); }) {
    
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

    m_pending += 1;
    m_submitQueue.push(std::move(submitInfo));
    m_appendCond.notify_all();
  }


  VkResult DxvkSubmissionQueue::present(DxvkPresentInfo presentInfo) {
    this->synchronize();
    
    std::unique_lock<std::mutex> lock(m_mutexQueue);
    return presentInfo.presenter->presentImage(presentInfo.waitSync);
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
      
      DxvkSubmitInfo submitInfo = std::move(m_submitQueue.front());
      lock.unlock();

      // Submit command buffer to device
      VkResult status;

      { std::lock_guard<std::mutex> lock(m_mutexQueue);

        status = submitInfo.cmdList->submit(
          submitInfo.queue,
          submitInfo.waitSync,
          submitInfo.wakeSync);
      }
      
      // On success, pass it on to the queue thread
      lock = std::unique_lock<std::mutex>(m_mutex);

      if (status == VK_SUCCESS) {
        m_finishQueue.push(std::move(submitInfo));
        m_submitQueue.pop();
        m_submitCond.notify_all();
      } else {
        Logger::err(str::format(
          "DxvkSubmissionQueue: Command submission failed with ",
          status));
        m_pending -= 1;
      }
    }
  }
  
  
  void DxvkSubmissionQueue::finishCmdLists() {
    env::setThreadName("dxvk-queue");

    std::unique_lock<std::mutex> lock(m_mutex);

    while (!m_stopped.load()) {
      m_submitCond.wait(lock, [this] {
        return m_stopped.load() || !m_finishQueue.empty();
      });

      if (m_stopped.load())
        return;
      
      DxvkSubmitInfo submitInfo = std::move(m_finishQueue.front());
      lock.unlock();
      
      VkResult status = submitInfo.cmdList->synchronize();
      
      if (status == VK_SUCCESS) {
        submitInfo.cmdList->signalEvents();
        submitInfo.cmdList->reset();
        
        m_device->recycleCommandList(submitInfo.cmdList);
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