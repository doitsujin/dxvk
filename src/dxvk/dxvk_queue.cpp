#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device)
  : m_device(device),
    m_thread([this] () { threadFunc(); }) {
    
  }
  
  
  DxvkSubmissionQueue::~DxvkSubmissionQueue() {
    { std::unique_lock<std::mutex> lock(m_mutex);
      m_stopped.store(true);
    }
    
    m_condOnAdd.notify_one();
    m_thread.join();
  }
  
  
  void DxvkSubmissionQueue::submit(const Rc<DxvkCommandList>& cmdList) {
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      m_condOnTake.wait(lock, [this] {
        return m_entries.size() < MaxNumQueuedCommandBuffers;
      });
      
      m_submits += 1;
      m_entries.push(cmdList);
      m_condOnAdd.notify_one();
    }
  }
  
  
  void DxvkSubmissionQueue::threadFunc() {
    env::setThreadName("dxvk-queue");

    while (!m_stopped.load()) {
      Rc<DxvkCommandList> cmdList;
      
      { std::unique_lock<std::mutex> lock(m_mutex);
        
        m_condOnAdd.wait(lock, [this] {
          return m_stopped.load() || (m_entries.size() != 0);
        });
        
        if (m_entries.size() != 0) {
          cmdList = std::move(m_entries.front());
          m_entries.pop();
        }
        
        m_condOnTake.notify_one();
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