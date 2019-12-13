#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Named stat counters
   * 
   * Enumerates available stat counters. Used
   * thogether with \ref DxvkStatCounters.
   */
  enum class DxvkStatCounter : uint32_t {
    CmdDrawCalls,             ///< Number of draw calls
    CmdDispatchCalls,         ///< Number of compute calls
    CmdRenderPassCount,       ///< Number of render passes
    PipeCountGraphics,        ///< Number of graphics pipelines
    PipeCountCompute,         ///< Number of compute pipelines
    PipeCompilerBusy,         ///< Boolean indicating compiler activity
    QueueSubmitCount,         ///< Number of command buffer submissions
    QueuePresentCount,        ///< Number of present calls / frames
    GpuIdleTicks,             ///< GPU idle time in microseconds
    NumCounters,              ///< Number of counters available
  };
  
  
  /**
   * \brief Stat counters
   * 
   * Collects various statistics that may be
   * useful to identify performance bottlenecks.
   */
  class DxvkStatCounters {
    
  public:
    
    DxvkStatCounters();
    ~DxvkStatCounters();
    
    /**
     * \brief Retrieves a counter value
     * 
     * \param [in] ctr The counter
     * \returns Counter value
     */
    uint64_t getCtr(DxvkStatCounter ctr) const {
      return m_counters[uint32_t(ctr)];
    }
    
    /**
     * \brief Sets a counter value
     * 
     * \param [in] ctr The counter
     * \param [in] val Counter value
     */
    void setCtr(DxvkStatCounter ctr, uint64_t val) {
      m_counters[uint32_t(ctr)] = val;
    }
    
    /**
     * \brief Increments a counter value
     * 
     * \param [in] ctr Counter to increment
     * \param [in] val Number to add to counter value
     */
    void addCtr(DxvkStatCounter ctr, uint64_t val) {
      m_counters[uint32_t(ctr)] += val;
    }
    
    /**
     * \brief Resets a counter
     * \param [in] ctr The counter
     */
    void clrCtr(DxvkStatCounter ctr) {
      m_counters[uint32_t(ctr)] = 0;
    }
    
    /**
     * \brief Computes difference
     * 
     * Computes difference between counter values.
     * \param [in] other Counters to subtract
     * \returns Difference between counter sets
     */
    DxvkStatCounters diff(const DxvkStatCounters& other) const;
    
    /**
     * \brief Merges counters
     * 
     * Adds counter values from another set
     * of counters to this set of counters.
     * \param [in] other Counters to add
     */
    void merge(const DxvkStatCounters& other);
    
    /**
     * \brief Resets counters
     * 
     * Sets all counters to zero.
     */
    void reset();
    
  private:
    
    std::array<uint64_t, uint32_t(DxvkStatCounter::NumCounters)> m_counters;
    
  };
  
}
