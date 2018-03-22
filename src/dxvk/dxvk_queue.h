#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "dxvk_cmdlist.h"
#include "dxvk_sync.h"

namespace dxvk {
  
  class DxvkDevice;
  
  /**
   * \brief Submission queue
   * 
   * 
   */
  class DxvkSubmissionQueue {
    
  public:
    
    DxvkSubmissionQueue(DxvkDevice* device);
    ~DxvkSubmissionQueue();
    
    void submit(const Rc<DxvkCommandList>& cmdList);
    
  private:
    
    DxvkDevice*             m_device;
    
    std::atomic<bool>       m_stopped = { false };
    
    std::mutex              m_mutex;
    std::condition_variable m_condOnAdd;
    std::condition_variable m_condOnTake;
    std::queue<Rc<DxvkCommandList>> m_entries;
    std::thread             m_thread;
    
    void threadFunc();
    
  };
  
}