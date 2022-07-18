#include "dxvk_descriptor.h"
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
          DxvkDescriptorManager*    manager,
          DxvkContextType           contextType)
  : m_device(device), m_manager(manager), m_contextType(contextType),
    m_cachedEntry(nullptr, nullptr) {

  }


  DxvkDescriptorPool::~DxvkDescriptorPool() {
    auto vk = m_device->vkd();

    for (auto pool : m_descriptorPools)
      vk->vkDestroyDescriptorPool(vk->device(), pool, nullptr);

    if (m_contextType == DxvkContextType::Primary) {
      m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount,
        uint64_t(-int64_t(m_descriptorPools.size())));
      m_device->addStatCtr(DxvkStatCounter::DescriptorSetCount,
        uint64_t(-int64_t(m_setsAllocated)));
    }
  }


  bool DxvkDescriptorPool::shouldSubmit(bool endFrame) {
    // Never submit empty descriptor pools
    if (!m_setsAllocated)
      return false;

    // No frame tracking for supplementary contexts, so
    // always submit those at the end of a command list
    if (endFrame || m_contextType != DxvkContextType::Primary)
      return true;

    // Submit very large descriptor pools to prevent extreme
    // memory bloat. This may be necessary for off-screen
    // rendering applications, or in situations where games
    // pre-render a lot of images without presenting in between.
    return m_descriptorPools.size() >= 8;
  }


  void DxvkDescriptorPool::alloc(
    const DxvkBindingLayoutObjects* layout,
          uint32_t                  setMask,
          VkDescriptorSet*          sets) {
    auto setMap = getSetMapCached(layout);

    while (setMask) {
      uint32_t setIndex = bit::tzcnt(setMask);

      sets[setIndex] = allocSet(
        setMap->sets[setIndex],
        layout->getSetLayout(setIndex));

      m_setsUsed += 1;
      setMask &= setMask - 1;
    }
  }


  VkDescriptorSet DxvkDescriptorPool::alloc(
          VkDescriptorSetLayout     layout) {
    auto setList = getSetList(layout);
    return allocSet(setList, layout);
  }


  void DxvkDescriptorPool::reset() {
    // As a heuristic to save memory, check how many descriptors
    // have actively been used in the past couple of submissions.
    bool isLowUsageFrame = false;

    size_t poolCount = m_descriptorPools.size();

    if (poolCount > 1) {
      double factor = std::max(11.0 / 3.0 - double(poolCount) / 3.0, 1.0);
      isLowUsageFrame = double(m_setsUsed) * factor < double(m_setsAllocated);
    }

    m_lowUsageFrames = isLowUsageFrame
      ? m_lowUsageFrames + 1
      : 0;
    m_setsUsed = 0;

    if (m_lowUsageFrames < 16) {
      for (auto& entry : m_setLists)
        entry.second.reset();
    } else {
      // If most sets are no longer being used, reset and destroy
      // descriptor pools and reset all lookup tables in order to
      // accomodate more descriptors of different layouts.
      for (auto pool : m_descriptorPools)
        m_manager->recycleVulkanDescriptorPool(pool);

      m_descriptorPools.clear();
      m_setLists.clear();
      m_setMaps.clear();

      m_setsAllocated = 0;
      m_lowUsageFrames = 0;
    }

    m_cachedEntry = { nullptr, nullptr };
  }


  void DxvkDescriptorPool::updateStats(DxvkStatCounters& counters) {
    if (m_contextType == DxvkContextType::Primary) {
      counters.addCtr(DxvkStatCounter::DescriptorSetCount,
        uint64_t(int64_t(m_setsAllocated) - int64_t(m_prevSetsAllocated)));
    }

    m_prevSetsAllocated = m_setsAllocated;
  }


  DxvkDescriptorSetMap* DxvkDescriptorPool::getSetMapCached(
    const DxvkBindingLayoutObjects*           layout) {
    if (likely(m_cachedEntry.first == layout))
      return m_cachedEntry.second;

    auto map = getSetMap(layout);
    m_cachedEntry = std::make_pair(layout, map);
    return map;
  }


  DxvkDescriptorSetMap* DxvkDescriptorPool::getSetMap(
    const DxvkBindingLayoutObjects*           layout) {
    auto pair = m_setMaps.find(layout->getPipelineLayout(false));
    if (likely(pair != m_setMaps.end())) {
      return &pair->second;
    }

    auto iter = m_setMaps.emplace(
      std::piecewise_construct,
      std::tuple(layout->getPipelineLayout(false)),
      std::tuple());

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      iter.first->second.sets[i] = (layout->getSetMask() & (1u << i))
        ? getSetList(layout->getSetLayout(i))
        : nullptr;
    }

    return &iter.first->second;
  }


  DxvkDescriptorSetList* DxvkDescriptorPool::getSetList(
          VkDescriptorSetLayout               layout) {
    auto pair = m_setLists.find(layout);
    if (pair != m_setLists.end())
      return &pair->second;

    auto iter = m_setLists.emplace(
      std::piecewise_construct,
      std::tuple(layout),
      std::tuple());
    return &iter.first->second;
  }


  VkDescriptorSet DxvkDescriptorPool::allocSet(
          DxvkDescriptorSetList*              list,
          VkDescriptorSetLayout               layout) {
    VkDescriptorSet set = list->alloc();

    if (unlikely(!set)) {
      if (!m_descriptorPools.empty())
        set = allocSetFromPool(m_descriptorPools.back(), layout);

      if (!set)
        set = allocSetFromPool(addPool(), layout);

      list->addSet(set);
      m_setsAllocated += 1;
    }

    return set;
  }


  VkDescriptorSet DxvkDescriptorPool::allocSetFromPool(
          VkDescriptorPool                    pool,
          VkDescriptorSetLayout               layout) {
    auto vk = m_device->vkd();

    VkDescriptorSetAllocateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

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

  
  DxvkDescriptorManager::DxvkDescriptorManager(
          DxvkDevice*                 device,
          DxvkContextType             contextType)
  : m_device(device), m_contextType(contextType) {

  }


  DxvkDescriptorManager::~DxvkDescriptorManager() {
    auto vk = m_device->vkd();

    for (size_t i = 0; i < m_vkPoolCount; i++)
      vk->vkDestroyDescriptorPool(vk->device(), m_vkPools[i], nullptr);

    if (m_contextType == DxvkContextType::Primary) {
      m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount,
        uint64_t(-int64_t(m_vkPoolCount)));
    }
  }


  Rc<DxvkDescriptorPool> DxvkDescriptorManager::getDescriptorPool() {
    Rc<DxvkDescriptorPool> pool = m_pools.retrieveObject();

    if (pool == nullptr)
      pool = new DxvkDescriptorPool(m_device, this, m_contextType);

    return pool;
  }


  void DxvkDescriptorManager::recycleDescriptorPool(
    const Rc<DxvkDescriptorPool>&     pool) {
    pool->reset();

    m_pools.returnObject(pool);
  }


  VkDescriptorPool DxvkDescriptorManager::createVulkanDescriptorPool() {
    auto vk = m_device->vkd();

    { std::lock_guard lock(m_mutex);

      if (m_vkPoolCount)
        return m_vkPools[--m_vkPoolCount];
    }

    uint32_t maxSets = m_contextType == DxvkContextType::Primary
      ? 8192 : 256;

    std::array<VkDescriptorPoolSize, 8> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,                maxSets * 2  },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          maxSets * 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          maxSets / 64 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         maxSets * 4  },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         maxSets * 1  },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   maxSets * 1  },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   maxSets / 64 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 1  } }};
    
    VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    info.maxSets       = maxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();
    
    VkDescriptorPool pool = VK_NULL_HANDLE;

    if (vk->vkCreateDescriptorPool(vk->device(), &info, nullptr, &pool) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorPool: Failed to create descriptor pool");

    if (m_contextType == DxvkContextType::Primary)
      m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount, 1);
    return pool;
  }

  
  void DxvkDescriptorManager::recycleVulkanDescriptorPool(VkDescriptorPool pool) {
    auto vk = m_device->vkd();
    vk->vkResetDescriptorPool(vk->device(), pool, 0);

    { std::lock_guard lock(m_mutex);

      if (m_vkPoolCount < m_vkPools.size()) {
        m_vkPools[m_vkPoolCount++] = pool;
        return;
      }
    }

    if (m_contextType == DxvkContextType::Primary)
      m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount, uint64_t(-1ll));

    vk->vkDestroyDescriptorPool(vk->device(), pool, nullptr);
  }

}