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
  
  
  void DxvkSubmissionQueue::submit(
    const Rc<DxvkFence>&        fence,
    const Rc<DxvkCommandList>&  cmdList) {
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      m_condOnTake.wait(lock, [this] {
        return m_entries.size() < MaxNumQueuedCommandBuffers;
      });
      
      m_entries.push({ fence, cmdList });
    }
    
    m_condOnAdd.notify_one();
  }
  
  
  void DxvkSubmissionQueue::threadFunc() {
    while (!m_stopped.load()) {
      Entry entry;
      
      { std::unique_lock<std::mutex> lock(m_mutex);
        
        m_condOnAdd.wait(lock, [this] {
          return m_stopped.load() || (m_entries.size() != 0);
        });
        
        if (m_entries.size() != 0) {
          entry = std::move(m_entries.front());
          m_entries.pop();
        }
      }
      
      m_condOnTake.notify_one();
      
      if (entry.fence != nullptr) {
        entry.fence->wait(std::numeric_limits<uint64_t>::max());
        
        entry.cmdList->writeQueryData();
        entry.cmdList->reset();
        m_device->recycleCommandList(entry.cmdList);
      }
    }
  }
  
}