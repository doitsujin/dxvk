#pragma once

#include <vector>

#include "dxvk_descriptor.h"
#include "dxvk_pipelayout.h"
#include "dxvk_recycler.h"
#include "dxvk_stats.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkDescriptorPoolSet;
  
  /**
   * \brief Descriptor set list
   */
  class DxvkDescriptorSetList {

  public:

    DxvkDescriptorSetList();
    ~DxvkDescriptorSetList();

    VkDescriptorSet alloc();

    void addSet(VkDescriptorSet set);

    void reset();

  private:

    size_t                        m_next = 0;
    std::vector<VkDescriptorSet>  m_sets;

  };


  /**
   * \brief Persistent descriptor set map
   *
   * Points to a list of set maps for each
   * defined set in a pipeline layout.
   */
  struct DxvkDescriptorSetMap {
    std::array<DxvkDescriptorSetList*, DxvkDescriptorSets::SetCount> sets;
  };

  
  /**
   * \brief Descriptor pool
   *
   * Manages descriptors that have the same lifetime. Sets are
   * intended to be reused as much as possible in order to reduce
   * overhead in the driver from descriptor set initialization,
   * but allocated sets will have unspecified contents and need
   * to be updated.
   */
  class DxvkDescriptorPool : public RcObject {
    constexpr static uint32_t MaxDesiredPoolCount = 2;
  public:

    DxvkDescriptorPool(
            DxvkDevice*               device,
            DxvkDescriptorPoolSet*    manager);

    ~DxvkDescriptorPool();

    /**
     * \brief Tests whether the descriptor pool should be replaced
     *
     * \param [in] endFrame Whether this is the end of the frame
     * \returns \c true if the pool should be submitted
     */
    bool shouldSubmit(bool endFrame);

    /**
     * \brief Allocates one or multiple descriptor sets
     *
     * \param [in] layout Pipeline layout
     * \param [in] setMask Descriptor set mask
     * \param [out] sets Descriptor sets
     */
    void alloc(
      const DxvkPipelineLayout*       layout,
            uint32_t                  setMask,
            VkDescriptorSet*          sets);

    /**
     * \brief Allocates a single descriptor set
     *
     * \param [in] layout Descriptor set layout
     * \returns The descriptor set
     */
    VkDescriptorSet alloc(
      const DxvkDescriptorSetLayout*  layout);

    /**
     * \brief Resets pool
     */
    void reset();

    /**
     * \brief Updates stat counters with set count
     * \param [out] counters Stat counters
     */
    void updateStats(DxvkStatCounters& counters);

  private:

    DxvkDevice*               m_device;
    DxvkDescriptorPoolSet*    m_manager;

    std::vector<VkDescriptorPool> m_descriptorPools;

    std::unordered_map<
      const DxvkDescriptorSetLayout*,
      DxvkDescriptorSetList>  m_setLists;

    std::unordered_map<
      const DxvkPipelineLayout*,
      DxvkDescriptorSetMap>   m_setMaps;

    std::pair<
      const DxvkPipelineLayout*,
      DxvkDescriptorSetMap*>  m_cachedEntry;

    uint32_t m_setsAllocated  = 0;
    uint32_t m_setsUsed       = 0;

    uint32_t m_prevSetsAllocated = 0;

    DxvkDescriptorSetMap* getSetMapCached(
      const DxvkPipelineLayout*                 layout);

    DxvkDescriptorSetMap* getSetMap(
      const DxvkPipelineLayout*                 layout);

    DxvkDescriptorSetList* getSetList(
      const DxvkDescriptorSetLayout*            layout);

    VkDescriptorSet allocSetWithLayout(
            DxvkDescriptorSetList*              list,
      const DxvkDescriptorSetLayout*            layout);

    VkDescriptorSet allocSetFromPool(
            VkDescriptorPool                    pool,
      const DxvkDescriptorSetLayout*            layout);

    VkDescriptorPool addPool();

  };
  
  /*
   * \brief Descriptor pool manager
   */
  class DxvkDescriptorPoolSet : public RcObject {

  public:

    DxvkDescriptorPoolSet(
            DxvkDevice*                 device);

    ~DxvkDescriptorPoolSet();

    /**
     * \brief Queries maximum number of descriptor sets per pool
     * \returns Maximum set count
     */
    uint32_t getMaxSetCount() const {
      return m_maxSets;
    }

    /**
     * \brief Retrieves or creates a descriptor type
     * \returns The descriptor pool
     */
    Rc<DxvkDescriptorPool> getDescriptorPool();

    /**
     * \brief Recycles descriptor pool
     *
     * Resets and recycles the given
     * descriptor pool for future use.
     */
    void recycleDescriptorPool(
      const Rc<DxvkDescriptorPool>&     pool);

    /**
     * \brief Creates a Vulkan descriptor pool
     *
     * Returns an existing unused pool or
     * creates a new one if necessary.
     * \returns The descriptor pool
     */
    VkDescriptorPool createVulkanDescriptorPool();

    /**
     * \brief Returns unused descriptor pool
     *
     * Caches the pool for future use, or destroys
     * it if there are too many objects in the cache
     * already.
     * \param [in] pool Vulkan descriptor pool
     */
    void recycleVulkanDescriptorPool(VkDescriptorPool pool);

  private:

    DxvkDevice*                         m_device;
    uint32_t                            m_maxSets = 0;
    DxvkRecycler<DxvkDescriptorPool, 8> m_pools;

    dxvk::mutex                         m_mutex;
    std::array<VkDescriptorPool, 8>     m_vkPools;
    size_t                              m_vkPoolCount = 0;

  };

}
