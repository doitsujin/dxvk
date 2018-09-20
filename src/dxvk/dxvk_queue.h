#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"
#include "dxvk_cmdlist.h"
#include "dxvk_sync.h"

namespace dxvk {
  
  class DxvkDevice;

  struct DxvkSubmission {
    Rc<DxvkCommandList> cmdList;
    Rc<DxvkSemaphore>   semWait;
    Rc<DxvkSemaphore>   semWake;
  };
  
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
     * \param [in] submission Command submission
     */
    void submit(DxvkSubmission submission);

    /**
     * \brief Synchronizes with submission thread
     * 
     * Waits until all submissions queued prior
     * to this call are submitted to the GPU.
     */
    void synchronize();

    /**
     * \brief Locks external queue lock
     * Protects the Vulkan queue.
     */
    void lock() {
      m_externalLock.lock();
    }

    /**
     * \brief Locks external queue lock
     * Releases the Vulkan queue.
     */
    void unlock() {
      m_externalLock.unlock();
    }
    
  private:
    
    DxvkDevice*             m_device;
    
    std::atomic<bool>       m_stopped = { false };
    std::atomic<uint32_t>   m_submits = { 0u };

    std::mutex                      m_externalLock;
    
    std::mutex                      m_queueLock;
    std::condition_variable         m_queueCond;
    std::queue<Rc<DxvkCommandList>> m_queueEntries;
    dxvk::thread                    m_queueThread;
    
    std::mutex                      m_submitLock;
    std::condition_variable         m_submitCondOnAdd;
    std::condition_variable         m_submitCondOnTake;
    std::queue<DxvkSubmission>      m_submitQueue;
    dxvk::thread                    m_submitThread;
    
    void threadSubmit();
    void threadQueue();
    
  };
  
}
