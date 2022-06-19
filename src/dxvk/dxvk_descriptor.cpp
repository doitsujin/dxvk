#include "dxvk_descriptor.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkDescriptorPool::DxvkDescriptorPool(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    constexpr uint32_t MaxSets = 2048;

    std::array<VkDescriptorPoolSize, 8> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,                MaxSets * 2 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxSets * 2 } }};
    
    VkDescriptorPoolCreateInfo info;
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext         = nullptr;
    info.flags         = 0;
    info.maxSets       = MaxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();
    
    if (m_vkd->vkCreateDescriptorPool(m_vkd->device(), &info, nullptr, &m_pool) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorPool: Failed to create descriptor pool");
  }
  
  
  DxvkDescriptorPool::~DxvkDescriptorPool() {
    m_vkd->vkDestroyDescriptorPool(
      m_vkd->device(), m_pool, nullptr);
  }
  
  
  VkDescriptorSet DxvkDescriptorPool::alloc(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info;
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext              = nullptr;
    info.descriptorPool     = m_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layout;
    
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (m_vkd->vkAllocateDescriptorSets(m_vkd->device(), &info, &set) != VK_SUCCESS)
      return VK_NULL_HANDLE;
    return set;
  }
  
  
  void DxvkDescriptorPool::reset() {
    m_vkd->vkResetDescriptorPool(
      m_vkd->device(), m_pool, 0);
  }


  DxvkPersistentDescriptorSetList::DxvkPersistentDescriptorSetList() {

  }


  DxvkPersistentDescriptorSetList::~DxvkPersistentDescriptorSetList() {

  }


  VkDescriptorSet DxvkPersistentDescriptorSetList::alloc() {
    if (unlikely(m_next == m_sets.size()))
      return VK_NULL_HANDLE;

    return m_sets[m_next++];
  }


  void DxvkPersistentDescriptorSetList::addSet(VkDescriptorSet set) {
    m_sets.push_back(set);
    m_next = m_sets.size();
  }


  void DxvkPersistentDescriptorSetList::reset() {
    m_next = 0;
  }



  DxvkPersistentDescriptorPool::DxvkPersistentDescriptorPool(
          DxvkDevice*               device,
          DxvkContextType           contextType)
  : m_device(device), m_contextType(contextType),
    m_cachedEntry(nullptr, nullptr) {

  }


  DxvkPersistentDescriptorPool::~DxvkPersistentDescriptorPool() {
    auto vk = m_device->vkd();

    for (auto pool : m_descriptorPools)
      vk->vkDestroyDescriptorPool(vk->device(), pool, nullptr);
  }


  void DxvkPersistentDescriptorPool::alloc(
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


  VkDescriptorSet DxvkPersistentDescriptorPool::alloc(
          VkDescriptorSetLayout     layout) {
    auto setList = getSetList(layout);
    return allocSet(setList, layout);
  }


  void DxvkPersistentDescriptorPool::reset() {
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
      auto vk = m_device->vkd();
      vk->vkResetDescriptorPool(vk->device(), m_descriptorPools[0], 0);

      for (uint32_t i = 1; i < m_descriptorPools.size(); i++)
        vk->vkDestroyDescriptorPool(vk->device(), m_descriptorPools[i], nullptr);

      m_descriptorPools.resize(1);
      m_setLists.clear();
      m_setMaps.clear();

      m_setsAllocated = 0;
      m_lowUsageFrames = 0;
    }

    m_cachedEntry = { nullptr, nullptr };
  }


  DxvkPersistentDescriptorSetMap* DxvkPersistentDescriptorPool::getSetMapCached(
    const DxvkBindingLayoutObjects*           layout) {
    if (likely(m_cachedEntry.first == layout))
      return m_cachedEntry.second;

    auto map = getSetMap(layout);
    m_cachedEntry = std::make_pair(layout, map);
    return map;
  }


  DxvkPersistentDescriptorSetMap* DxvkPersistentDescriptorPool::getSetMap(
    const DxvkBindingLayoutObjects*           layout) {
    auto pair = m_setMaps.find(layout->getPipelineLayout());
    if (likely(pair != m_setMaps.end())) {
      return &pair->second;
    }

    auto iter = m_setMaps.emplace(
      std::piecewise_construct,
      std::tuple(layout->getPipelineLayout()),
      std::tuple());

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      iter.first->second.sets[i] = (layout->getSetMask() & (1u << i))
        ? getSetList(layout->getSetLayout(i))
        : nullptr;
    }

    return &iter.first->second;
  }


  DxvkPersistentDescriptorSetList* DxvkPersistentDescriptorPool::getSetList(
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


  VkDescriptorSet DxvkPersistentDescriptorPool::allocSet(
          DxvkPersistentDescriptorSetList*    list,
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


  VkDescriptorSet DxvkPersistentDescriptorPool::allocSetFromPool(
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


  VkDescriptorPool DxvkPersistentDescriptorPool::addPool() {
    auto vk = m_device->vkd();

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
    
    VkDescriptorPoolCreateInfo info;
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext         = nullptr;
    info.flags         = 0;
    info.maxSets       = maxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();
    
    VkDescriptorPool pool = VK_NULL_HANDLE;

    if (vk->vkCreateDescriptorPool(vk->device(), &info, nullptr, &pool) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorPool: Failed to create descriptor pool");

    m_descriptorPools.push_back(pool);
    return pool;
  }


  DxvkDescriptorPoolTracker::DxvkDescriptorPoolTracker(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkDescriptorPoolTracker::~DxvkDescriptorPoolTracker() {

  }


  void DxvkDescriptorPoolTracker::trackDescriptorPool(Rc<DxvkDescriptorPool> pool) {
    m_pools.push_back(std::move(pool));
  }

  
  void DxvkDescriptorPoolTracker::reset() {
    for (const auto& pool : m_pools) {
      pool->reset();
      m_device->recycleDescriptorPool(pool);
    }

    m_pools.clear();
  }
  
  DxvkDescriptorManager::DxvkDescriptorManager(
          DxvkDevice*                 device,
          DxvkContextType             contextType)
  : m_device(device), m_contextType(contextType) {

  }


  DxvkDescriptorManager::~DxvkDescriptorManager() {

  }


  Rc<DxvkPersistentDescriptorPool> DxvkDescriptorManager::getDescriptorPool() {
    Rc<DxvkPersistentDescriptorPool> pool = m_pools.retrieveObject();

    if (pool == nullptr)
      pool = new DxvkPersistentDescriptorPool(m_device, m_contextType);

    return pool;
  }


  void DxvkDescriptorManager::recycleDescriptorPool(
    const Rc<DxvkPersistentDescriptorPool>&     pool) {
    pool->reset();

    m_pools.returnObject(pool);
  }

}