#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_pipelayout.h"
#include "dxvk_recycler.h"

namespace dxvk {

  class DxvkDevice;
  
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
  class DxvkPersistentDescriptorSetList {

  public:

    DxvkPersistentDescriptorSetList();
    ~DxvkPersistentDescriptorSetList();

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
  struct DxvkPersistentDescriptorSetMap {
    std::array<DxvkPersistentDescriptorSetList*, DxvkDescriptorSets::SetCount> sets;
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
  class DxvkPersistentDescriptorPool : public RcObject {

  public:

    DxvkPersistentDescriptorPool(
            DxvkDevice*               device,
            DxvkContextType           contextType);

    ~DxvkPersistentDescriptorPool();

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

  private:

    DxvkDevice*               m_device;
    DxvkContextType           m_contextType;

    std::vector<VkDescriptorPool>                                               m_descriptorPools;
    std::unordered_map<VkDescriptorSetLayout, DxvkPersistentDescriptorSetList>  m_setLists;
    std::unordered_map<VkPipelineLayout,      DxvkPersistentDescriptorSetMap>   m_setMaps;
    std::pair<const DxvkBindingLayoutObjects*, DxvkPersistentDescriptorSetMap*> m_cachedEntry;

    uint32_t m_setsAllocated  = 0;
    uint32_t m_setsUsed       = 0;
    uint32_t m_lowUsageFrames = 0;

    DxvkPersistentDescriptorSetMap* getSetMapCached(
      const DxvkBindingLayoutObjects*           layout);

    DxvkPersistentDescriptorSetMap* getSetMap(
      const DxvkBindingLayoutObjects*           layout);

    DxvkPersistentDescriptorSetList* getSetList(
            VkDescriptorSetLayout               layout);

    VkDescriptorSet allocSet(
            DxvkPersistentDescriptorSetList*    list,
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
     * \brief Retrieves or creates a descriptor type
     * \returns The descriptor pool
     */
    Rc<DxvkPersistentDescriptorPool> getDescriptorPool();

    /**
     * \brief Recycles descriptor pool
     *
     * Resets and recycles the given
     * descriptor pool for future use.
     */
    void recycleDescriptorPool(
      const Rc<DxvkPersistentDescriptorPool>&     pool);

  private:

    DxvkDevice*                         m_device;
    DxvkContextType                     m_contextType;
    DxvkRecycler<DxvkPersistentDescriptorPool, 8> m_pools;

  };

}