#pragma once

#include <atomic>
#include <array>

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Statistics counter
   */
  enum class DxvkStat : uint32_t {
    CtxDescriptorUpdates, ///< # of descriptor set writes
    CtxDrawCalls,         ///< # of vkCmdDraw/vkCmdDrawIndexed
    CtxDispatchCalls,     ///< # of vkCmdDispatch
    CtxFramebufferBinds,  ///< # of render pass begin/end
    CtxPipelineBinds,     ///< # of vkCmdBindPipeline
    DevQueueSubmissions,  ///< # of vkQueueSubmit
    DevQueuePresents,     ///< # of vkQueuePresentKHR (aka frames)
    DevSynchronizations,  ///< # of vkDeviceWaitIdle
    ResBufferCreations,   ///< # of buffer creations
    ResBufferUpdates,     ///< # of unmapped buffer updates
    ResImageCreations,    ///< # of image creations
    ResImageUpdates,      ///< # of unmapped image updates
    // Do not remove
    MaxCounterId
  };
  
  
  /**
   * \brief Device statistics
   * 
   * Stores a bunch of counters that may be useful
   * for performance evaluation and optimization.
   */
  class DxvkStatCounters {
    
  public:
    
    DxvkStatCounters();
    ~DxvkStatCounters();
    
    DxvkStatCounters(
      const DxvkStatCounters& other);
    
    DxvkStatCounters& operator = (const DxvkStatCounters& other);
    
    /**
     * \brief Increments a counter by a given value
     * 
     * \param [in] counter The counter to increment
     * \param [in] amount Number to add to the counter
     */
    void increment(DxvkStat counter, uint32_t amount) {
      m_counters.at(counterId(counter)) += amount;
    }
    
    /**
     * \brief Returns a counter
     * 
     * \param [in] counter The counter to retrieve
     * \returns Current value of the counter
     */
    uint32_t get(DxvkStat counter) const {
      return m_counters.at(counterId(counter));
    }
    
    /**
     * \brief Computes delta to a previous state
     * 
     * \param [in] oldState previous state
     * \returns Difference to previous state
     */
    DxvkStatCounters delta(
      const DxvkStatCounters& oldState) const;
    
    /**
     * \brief Adds counters from another source
     * 
     * Adds each counter from the source operand to the
     * corresponding counter in this set. Useful to merge
     * context counters and device counters.
     * \param [in] counters Counters to add
     */
    void addCounters(
      const DxvkStatCounters& counters);
    
    /**
     * \brief Clears counters
     * 
     * Should be used to clear per-context counters.
     * Do not clear the global device counters.
     */
    void clear();
    
  private:
    
    std::array<std::atomic<uint32_t>,
      static_cast<uint32_t>(DxvkStat::MaxCounterId)> m_counters;
    
    static size_t counterId(DxvkStat counter) {
      return static_cast<uint32_t>(counter);
    }
    
  };
  
}
