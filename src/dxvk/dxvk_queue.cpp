#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device)
  : m_device      (device),
    m_queueThread ([this] () { threadQueue();  }),
    m_submitThread([this] () { threadSubmit(); }) {
    
  }
  
  
  DxvkSubmissionQueue::~DxvkSubmissionQueue() {
    { std::unique_lock<std::mutex> lockQueue (m_queueLock);
      std::unique_lock<std::mutex> lockSubmit(m_submitLock);

      m_stopped.store(true);
    }
    
    m_submitCondOnAdd.notify_one();
    m_submitThread.join();

    m_queueCond.notify_one();
    m_queueThread.join();
  }
  
  
  void DxvkSubmissionQueue::submit(DxvkSubmission submission) {
    std::unique_lock<std::mutex> lock(m_submitLock);
    
    m_submitCondOnTake.wait(lock, [this] {
      return m_submitQueue.size() < MaxNumQueuedCommandBuffers;
    });

    m_submits += 1;
    m_submitQueue.push(std::move(submission));
    m_submitCondOnAdd.notify_one();
  }
  
  
  void DxvkSubmissionQueue::synchronize() {
    std::unique_lock<std::mutex> lock(m_submitLock);

    m_submitCondOnTake.wait(lock, [this] {
      return m_submitQueue.size() == 0;
    });
  }


  void DxvkSubmissionQueue::threadSubmit() {
    env::setThreadName(L"dxvk-submit");

    while (!m_stopped.load()) {
      DxvkSubmission submission;

      { std::unique_lock<std::mutex> lock(m_submitLock);

        m_submitCondOnAdd.wait(lock, [this] {
          return m_stopped.load() || (m_submitQueue.size() != 0);
        });

        if (m_submitQueue.size() != 0)
          submission = std::move(m_submitQueue.front());
      }

      if (submission.cmdList != nullptr) {
        // Make sure that the semaphores do not get deleted or
        // reused before the command submission has completed
        VkSemaphore waitSemaphore = VK_NULL_HANDLE;
        VkSemaphore wakeSemaphore = VK_NULL_HANDLE;

        if (submission.semWait != nullptr) {
          waitSemaphore = submission.semWait->handle();
          submission.cmdList->trackResource(submission.semWait);
        }
        
        if (submission.semWake != nullptr) {
          wakeSemaphore = submission.semWake->handle();
          submission.cmdList->trackResource(submission.semWake);
        }

        // Protect the Vulkan queue itself from concurrent access
        { std::unique_lock<std::mutex> lock(m_externalLock);

          VkResult status = submission.cmdList->submit(
            m_device->m_graphicsQueue.queueHandle,
            waitSemaphore, wakeSemaphore);
          
          if (status != VK_SUCCESS)
            Logger::err(str::format("Dxvk: Submission failed: ", status));
        }
        
        // Process this submission on the 'queue' thread
        { std::unique_lock<std::mutex> lock(m_queueLock);
          
          m_queueEntries.push(std::move(submission.cmdList));
          m_queueCond.notify_one();
        }

        // Remove submission now. We cannot do this earlier as
        // the synchronize method depends on this behaviour.
        { std::unique_lock<std::mutex> lock(m_submitLock);

          if (m_submitQueue.size() != 0)
            m_submitQueue.pop();
        }
      }

      m_submitCondOnTake.notify_one();
    }
  }


  void DxvkSubmissionQueue::threadQueue() {
    env::setThreadName(L"dxvk-queue");

    while (!m_stopped.load()) {
      Rc<DxvkCommandList> cmdList;
      
      { std::unique_lock<std::mutex> lock(m_queueLock);
        
        m_queueCond.wait(lock, [this] {
          return m_stopped.load() || (m_queueEntries.size() != 0);
        });
        
        if (m_queueEntries.size() != 0) {
          cmdList = std::move(m_queueEntries.front());
          m_queueEntries.pop();
        }
      }
      
      if (cmdList != nullptr) {
        VkResult status = cmdList->synchronize();
        
        if (status == VK_SUCCESS) {
          cmdList->writeQueryData();
          cmdList->signalEvents();
          cmdList->reset();
          
          m_device->recycleCommandList(cmdList);
        } else {
          Logger::err(str::format(
            "DxvkSubmissionQueue: Failed to sync fence: ",
            status));
        }
        
        m_submits -= 1;
      }
    }
  }
  
}