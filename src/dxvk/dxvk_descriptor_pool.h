#pragma once

#include <vector>

#include "dxvk_descriptor.h"
#include "dxvk_pipelayout.h"
#include "dxvk_recycler.h"
#include "dxvk_stats.h"

namespace dxvk {

  class DxvkDevice;
  
  /**
   * \brief Descriptor pool
   *
   * Legacy descriptor pool allocator with submission-based lifetime
   * tracking.
   */
  class DxvkDescriptorPool : public RcObject {
    constexpr static uint32_t MaxDesiredPoolCount = 2;
  public:

    DxvkDescriptorPool(DxvkDevice* device);

    ~DxvkDescriptorPool();

    /**
     * \brief Allocates one or multiple descriptor sets
     *
     * \param [in] trackingId Submission tracking ID
     * \param [in] layout Pipeline layout
     * \param [in] setMask Descriptor set mask
     * \param [out] sets Descriptor sets
     */
    void alloc(
            uint64_t                  trackingId,
      const DxvkPipelineLayout*       layout,
            uint32_t                  setMask,
            VkDescriptorSet*          sets);

    /**
     * \brief Allocates a single descriptor set
     *
     * \param [in] trackingId Submission tracking ID
     * \param [in] layout Descriptor set layout
     * \returns The descriptor set
     */
    VkDescriptorSet alloc(
            uint64_t                  trackingId,
      const DxvkDescriptorSetLayout*  layout);

    /**
     * \brief Declares given submission ID as complete
     *
     * Used for tracking descriptor pool lifetimes.
     * \param [in] trackingId last completed tracking ID
     */
    void notifyCompletion(
            uint64_t                    trackingId);

    /**
     * \brief Updates stat counters with set count
     * \param [out] counters Stat counters
     */
    void updateStats(DxvkStatCounters& counters);

  private:

    DxvkDevice* m_device = nullptr;

    uint64_t m_setsAllocated = 0u;

    enum class Status : uint32_t {
      Reset     = 0,
      InUse     = 1,
      InFlight  = 2,
    };

    struct DescriptorPool {
      VkDescriptorPool pool = VK_NULL_HANDLE;
      uint64_t trackingId = 0u;
      Status status = Status::Reset;
    };

    dxvk::mutex m_mutex;

    small_vector<DescriptorPool, 64u> m_pools;
    std::pair<size_t, DescriptorPool> m_pool = { };

    std::pair<size_t, DescriptorPool> getNextPool();

    VkDescriptorPool createDescriptorPool() const;

  };
  
}
