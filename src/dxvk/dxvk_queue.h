#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"

#include "../vulkan/vulkan_presenter.h"

#include "dxvk_cmdlist.h"

namespace dxvk {
  
  class DxvkDevice;

  /**
   * \brief Queue submission info
   * 
   * Stores parameters used to submit
   * a command buffer to the device.
   */
  struct DxvkSubmitInfo {
    Rc<DxvkCommandList> cmdList;
    VkQueue             queue;
    VkSemaphore         waitSync;
    VkSemaphore         wakeSync;
  };
  
  
  /**
   * \brief Present info
   *
   * Stores parameters used to present
   * a swap chain image on the device.
   */
  struct DxvkPresentInfo {
    Rc<vk::Presenter>   presenter;
    VkSemaphore         waitSync;
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
      return m_pending.load();
    }
    
    /**
     * \brief Submits a command list asynchronously
     * 
     * Queues a command list for submission on the
     * dedicated submission thread. Use this to take
     * the submission overhead off the calling thread.
     * \param [in] submitInfo Submission parameters 
     */
    void submit(
            DxvkSubmitInfo  submitInfo);
    
    /**
     * \brief Presents an image synchronously
     *
     * Waits for queued command lists to be submitted
     * and then presents the current swap chain image
     * of the presenter. May stall the calling thread.
     * \param [in] present Present parameters
     * \returns Status of the operation
     */
    VkResult present(
            DxvkPresentInfo present);
    
    /**
     * \brief Synchronizes with queue submissions
     * 
     * Waits for all pending command lists to be
     * submitted to the GPU before returning.
     */
    void synchronize();

    /**
     * \brief Locks device queue
     *
     * Locks the mutex that protects the Vulkan queue
     * that DXVK uses for command buffer submission.
     * This is needed when the app submits its own
     * command buffers to the queue.
     */
    void lockDeviceQueue();

    /**
     * \brief Unlocks device queue
     *
     * Unlocks the mutex that protects the Vulkan
     * queue used for command buffer submission.
     */
    void unlockDeviceQueue();
    
  private:

    DxvkDevice*             m_device;
    
    std::atomic<bool>       m_stopped = { false };
    std::atomic<uint32_t>   m_pending = { 0u };

    std::mutex              m_mutex;
    std::mutex              m_mutexQueue;
    
    std::condition_variable m_appendCond;
    std::condition_variable m_submitCond;
    std::condition_variable m_finishCond;

    std::queue<DxvkSubmitInfo> m_submitQueue;
    std::queue<DxvkSubmitInfo> m_finishQueue;

    dxvk::thread            m_submitThread;
    dxvk::thread            m_finishThread;

    VkResult submitToQueue(
      const DxvkSubmitInfo& submission);

    void submitCmdLists();

    void finishCmdLists();
    
  };
  
}
