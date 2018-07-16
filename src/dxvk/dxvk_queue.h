#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"
#include "dxvk_cmdlist.h"
#include "dxvk_sync.h"

namespace dxvk {
  
  class DxvkDevice;
  
  /**
   * \brief Submission queue
   */
  class DxvkSubmissionQueue {
    
  public:
    
    DxvkSubmissionQueue(DxvkDevice* device);
    ~DxvkSubmissionQueue();

    /**
     * \brief Number of pending submissions
     * 
     * A return value of 0 indicates
     * that the GPU is currently idle.
     * \returns Pending submission count
     */
    uint32_t pendingSubmissions() const {
      return m_submits.load();
    }
    
    /**
     * \brief Submits a command list
     * 
     * Submits a command list to the queue thread.
     * This thread will wait for the command list
     * to finish executing on the GPU and signal
     * any queries and events that are used by
     * the command list in question.
     * \param [in] cmdList The command list
     */
    void submit(const Rc<DxvkCommandList>& cmdList);
    
  private:
    
    DxvkDevice*             m_device;
    
    std::atomic<bool>       m_stopped = { false };
    std::atomic<uint32_t>   m_submits = { 0u };
    
    std::mutex              m_mutex;
    std::condition_variable m_condOnAdd;
    std::condition_variable m_condOnTake;
    std::queue<Rc<DxvkCommandList>> m_entries;
    dxvk::thread             m_thread;
    
    void threadFunc();
    
  };
  
}
