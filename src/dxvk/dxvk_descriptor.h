#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_pipelayout.h"
#include "dxvk_recycler.h"
#include "dxvk_stats.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkDescriptorManager;
  
  /**
   * \brief DXVK context type
   *
   * Used as a hint to optimize certain usage patterns.
   */
  enum class DxvkContextType : uint32_t {
    Primary       = 0,
    Supplementary = 1,
  };

  /**
   * \brief Descriptor info
   * 
   * Stores information that is required to
   * update a single resource descriptor.
   */
  union DxvkDescriptorInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
    VkBufferView           texelBuffer;
  };
  
  
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

  public:

    DxvkDescriptorPool(
            DxvkDevice*               device,
            DxvkDescriptorManager*    manager,
            DxvkContextType           contextType);

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
     * \param [in] layout Binding layout
     * \param [in] setMask Descriptor set mask
     * \param [out] sets Descriptor sets
     */
    void alloc(
      const DxvkBindingLayoutObjects* layout,
            uint32_t                  setMask,
            VkDescriptorSet*          sets);

    /**
     * \brief Allocates a single descriptor set
     *
     * \param [in] layout Descriptor set layout
     * \returns The descriptor set
     */
    VkDescriptorSet alloc(
            VkDescriptorSetLayout     layout);

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
    DxvkDescriptorManager*    m_manager;
    DxvkContextType           m_contextType;

    std::vector<VkDescriptorPool> m_descriptorPools;

    std::unordered_map<
      VkDescriptorSetLayout,
      DxvkDescriptorSetList>  m_setLists;

    std::unordered_map<
      const DxvkBindingLayoutObjects*,
      DxvkDescriptorSetMap>   m_setMaps;

    std::pair<
      const DxvkBindingLayoutObjects*,
      DxvkDescriptorSetMap*>  m_cachedEntry;

    uint32_t m_setsAllocated  = 0;
    uint32_t m_setsUsed       = 0;

    uint32_t m_prevSetsAllocated = 0;

    uint32_t m_lowUsageFrames = 0;

    DxvkDescriptorSetMap* getSetMapCached(
      const DxvkBindingLayoutObjects*           layout);

    DxvkDescriptorSetMap* getSetMap(
      const DxvkBindingLayoutObjects*           layout);

    DxvkDescriptorSetList* getSetList(
            VkDescriptorSetLayout               layout);

    VkDescriptorSet allocSet(
            DxvkDescriptorSetList*    list,
            VkDescriptorSetLayout               layout);

    VkDescriptorSet allocSetFromPool(
            VkDescriptorPool                    pool,
            VkDescriptorSetLayout               layout);

    VkDescriptorPool addPool();

  };
  
  /*
   * \brief Descriptor pool manager
   */
  class DxvkDescriptorManager : public RcObject {

  public:

    DxvkDescriptorManager(
            DxvkDevice*                 device,
            DxvkContextType             contextType);

    ~DxvkDescriptorManager();

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
    DxvkContextType                     m_contextType;
    uint32_t                            m_maxSets = 0;
    DxvkRecycler<DxvkDescriptorPool, 8> m_pools;

    dxvk::mutex                         m_mutex;
    std::array<VkDescriptorPool, 8>     m_vkPools;
    size_t                              m_vkPoolCount = 0;

  };

}