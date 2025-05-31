#include "dxvk_descriptor_pool.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorSetList::DxvkDescriptorSetList() {

  }


  DxvkDescriptorSetList::~DxvkDescriptorSetList() {

  }


  VkDescriptorSet DxvkDescriptorSetList::alloc() {
    if (unlikely(m_next == m_sets.size()))
      return VK_NULL_HANDLE;

    return m_sets[m_next++];
  }


  void DxvkDescriptorSetList::addSet(VkDescriptorSet set) {
    m_sets.push_back(set);
    m_next = m_sets.size();
  }


  void DxvkDescriptorSetList::reset() {
    m_next = 0;
  }



  DxvkDescriptorPool::DxvkDescriptorPool(
          DxvkDevice*               device,
          DxvkDescriptorPoolSet*    manager)
  : m_device(device), m_manager(manager),
    m_cachedEntry(nullptr, nullptr) {

  }


  DxvkDescriptorPool::~DxvkDescriptorPool() {
    auto vk = m_device->vkd();

    for (auto pool : m_descriptorPools)
      vk->vkDestroyDescriptorPool(vk->device(), pool, nullptr);

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount,
      uint64_t(-int64_t(m_descriptorPools.size())));
    m_device->addStatCtr(DxvkStatCounter::DescriptorSetCount,
      uint64_t(-int64_t(m_setsAllocated)));
  }


  bool DxvkDescriptorPool::shouldSubmit(bool endFrame) {
    // Never submit empty descriptor pools
    if (!m_setsAllocated)
      return false;

    // Submit at the end of each frame to make it more likely
    // to get similar descriptor set layouts the next time the
    // pool gets used.
    if (endFrame)
      return true;

    // Submit very large descriptor pools to prevent extreme
    // memory bloat. This may be necessary for off-screen
    // rendering applications, or in situations where games
    // pre-render a lot of images without presenting in between.
    return m_device->features().nvDescriptorPoolOverallocation.descriptorPoolOverallocation ?
      m_setsAllocated > MaxDesiredPoolCount * m_manager->getMaxSetCount() :
      m_descriptorPools.size() > MaxDesiredPoolCount;
  }


  void DxvkDescriptorPool::alloc(
    const DxvkPipelineLayout*       layout,
          uint32_t                  setMask,
          VkDescriptorSet*          sets) {
    auto setMap = getSetMapCached(layout);

    for (auto setIndex : bit::BitMask(setMask)) {
      auto list = setMap->sets[setIndex];

      if (unlikely(!(sets[setIndex] = list->alloc()))) {
        sets[setIndex] = allocSetWithLayout(list,
          layout->getDescriptorSetLayout(setIndex));
      }

      m_setsUsed += 1;
    }
  }


  VkDescriptorSet DxvkDescriptorPool::alloc(
    const DxvkDescriptorSetLayout*  layout) {
    auto list = getSetList(layout);

    VkDescriptorSet set = list->alloc();

    if (unlikely(!set))
      set = allocSetWithLayout(list, layout);

    return set;
  }


  void DxvkDescriptorPool::reset() {
    // As a heuristic to save memory, check how many descriptor
    // sets were actually being used in past submissions.
    size_t poolCount = m_descriptorPools.size();
    bool needsReset = poolCount > MaxDesiredPoolCount;

    if (poolCount > 1 || m_setsAllocated > m_manager->getMaxSetCount() / 2) {
      double factor = std::max(11.0 / 3.0 - double(poolCount) / 3.0, 1.0);
      needsReset = double(m_setsUsed) * factor < double(m_setsAllocated);
    }

    m_setsUsed = 0;

    if (!needsReset) {
      for (auto& entry : m_setLists)
        entry.second.reset();
    } else {
      // If most sets are no longer needed, reset and destroy
      // descriptor pools and reset all lookup tables in order
      // to accomodate more descriptors of different layouts.
      for (auto pool : m_descriptorPools)
        m_manager->recycleVulkanDescriptorPool(pool);

      m_descriptorPools.clear();
      m_setLists.clear();
      m_setMaps.clear();

      m_setsAllocated = 0;
    }

    m_cachedEntry = { nullptr, nullptr };
  }


  void DxvkDescriptorPool::updateStats(DxvkStatCounters& counters) {
    counters.addCtr(DxvkStatCounter::DescriptorSetCount,
      uint64_t(int64_t(m_setsAllocated) - int64_t(m_prevSetsAllocated)));

    m_prevSetsAllocated = m_setsAllocated;
  }


  DxvkDescriptorSetMap* DxvkDescriptorPool::getSetMapCached(
    const DxvkPipelineLayout*                 layout) {
    if (likely(m_cachedEntry.first == layout))
      return m_cachedEntry.second;

    auto map = getSetMap(layout);
    m_cachedEntry = std::make_pair(layout, map);
    return map;
  }


  DxvkDescriptorSetMap* DxvkDescriptorPool::getSetMap(
    const DxvkPipelineLayout*                 layout) {
    auto pair = m_setMaps.find(layout);

    if (likely(pair != m_setMaps.end()))
      return &pair->second;

    auto iter = m_setMaps.emplace(
      std::piecewise_construct,
      std::tuple(layout),
      std::tuple());

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      const auto* setLayout = layout->getDescriptorSetLayout(i);

      iter.first->second.sets[i] = (setLayout && !setLayout->isEmpty())
        ? getSetList(setLayout)
        : nullptr;
    }

    return &iter.first->second;
  }


  DxvkDescriptorSetList* DxvkDescriptorPool::getSetList(
    const DxvkDescriptorSetLayout*            layout) {
    auto pair = m_setLists.find(layout);

    if (pair != m_setLists.end())
      return &pair->second;

    auto iter = m_setLists.emplace(
      std::piecewise_construct,
      std::tuple(layout),
      std::tuple());
    return &iter.first->second;
  }


  VkDescriptorSet DxvkDescriptorPool::allocSetWithLayout(
          DxvkDescriptorSetList*              list,
    const DxvkDescriptorSetLayout*            layout) {
    VkDescriptorSet set = VK_NULL_HANDLE;

    if (!m_descriptorPools.empty())
      set = allocSetFromPool(m_descriptorPools.back(), layout);

    if (!set)
      set = allocSetFromPool(addPool(), layout);

    list->addSet(set);
    m_setsAllocated += 1;

    return set;
  }


  VkDescriptorSet DxvkDescriptorPool::allocSetFromPool(
          VkDescriptorPool                    pool,
    const DxvkDescriptorSetLayout*            layout) {
    auto vk = m_device->vkd();

    VkDescriptorSetLayout setLayout = layout->getSetLayout();

    VkDescriptorSetAllocateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &setLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    
    if (vk->vkAllocateDescriptorSets(vk->device(), &info, &set) != VK_SUCCESS)
      return VK_NULL_HANDLE;

    return set;
  }


  VkDescriptorPool DxvkDescriptorPool::addPool() {
    VkDescriptorPool pool = m_manager->createVulkanDescriptorPool();
    m_descriptorPools.push_back(pool);
    return pool;
  }

  
  DxvkDescriptorPoolSet::DxvkDescriptorPoolSet(
          DxvkDevice*                 device)
  : m_device(device) {
    // Deliberately pick a very high number of descriptor sets so that
    // we will typically end up using all available pool memory before
    // the descriptor set limit becomes the limiting factor.
    m_maxSets = env::is32BitHostPlatform() ? 24576u : 49152u;
  }


  DxvkDescriptorPoolSet::~DxvkDescriptorPoolSet() {
    auto vk = m_device->vkd();

    for (size_t i = 0; i < m_vkPoolCount; i++)
      vk->vkDestroyDescriptorPool(vk->device(), m_vkPools[i], nullptr);

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount,
      uint64_t(-int64_t(m_vkPoolCount)));
  }


  Rc<DxvkDescriptorPool> DxvkDescriptorPoolSet::getDescriptorPool() {
    Rc<DxvkDescriptorPool> pool = m_pools.retrieveObject();

    if (pool == nullptr)
      pool = new DxvkDescriptorPool(m_device, this);

    return pool;
  }


  void DxvkDescriptorPoolSet::recycleDescriptorPool(
    const Rc<DxvkDescriptorPool>&     pool) {
    pool->reset();

    m_pools.returnObject(pool);
  }


  VkDescriptorPool DxvkDescriptorPoolSet::createVulkanDescriptorPool() {
    auto vk = m_device->vkd();

    { std::lock_guard lock(m_mutex);

      if (m_vkPoolCount)
        return m_vkPools[--m_vkPoolCount];
    }

    // Samplers and uniform buffers may be special on some implementations
    // so we should allocate space for a reasonable number of both, but
    // assume that all other descriptor types share pool memory.
    std::array<VkDescriptorPoolSize, 6> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          m_maxSets / 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          m_maxSets / 64 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   m_maxSets / 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   m_maxSets / 64 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         m_maxSets * 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         m_maxSets / 2  },
    }};
    
    VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    info.maxSets       = m_maxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();

    if (m_device->features().nvDescriptorPoolOverallocation.descriptorPoolOverallocation) {
      info.flags |= VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_POOLS_BIT_NV
                 |  VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_SETS_BIT_NV;
    }

    VkDescriptorPool pool = VK_NULL_HANDLE;

    if (vk->vkCreateDescriptorPool(vk->device(), &info, nullptr, &pool) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorPool: Failed to create descriptor pool");

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount, 1);
    return pool;
  }

  
  void DxvkDescriptorPoolSet::recycleVulkanDescriptorPool(VkDescriptorPool pool) {
    auto vk = m_device->vkd();
    vk->vkResetDescriptorPool(vk->device(), pool, 0);

    { std::lock_guard lock(m_mutex);

      if (m_vkPoolCount < m_vkPools.size()) {
        m_vkPools[m_vkPoolCount++] = pool;
        return;
      }
    }

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount, uint64_t(-1ll));
    vk->vkDestroyDescriptorPool(vk->device(), pool, nullptr);
  }

}
