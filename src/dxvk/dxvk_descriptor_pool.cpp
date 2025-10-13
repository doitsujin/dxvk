#include "dxvk_descriptor_pool.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorPool::DxvkDescriptorPool(
          DxvkDevice*               device)
  : m_device(device) {

  }


  DxvkDescriptorPool::~DxvkDescriptorPool() {
    auto vk = m_device->vkd();

    for (auto pool : m_pools)
      vk->vkDestroyDescriptorPool(vk->device(), pool.pool, nullptr);

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount,
      uint64_t(-int64_t(m_pools.size())));
  }


  void DxvkDescriptorPool::alloc(
          uint64_t                  trackingId,
    const DxvkPipelineLayout*       layout,
          uint32_t                  setMask,
          VkDescriptorSet*          sets) {
    for (auto setIndex : bit::BitMask(setMask))
      sets[setIndex] = alloc(trackingId, layout->getDescriptorSetLayout(setIndex));
  }


  VkDescriptorSet DxvkDescriptorPool::alloc(
          uint64_t                  trackingId,
    const DxvkDescriptorSetLayout*  layout) {
    auto vk = m_device->vkd();

    VkDescriptorSetLayout setLayout = layout->getSetLayout();

    VkResult vr = VK_ERROR_OUT_OF_POOL_MEMORY;
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    info.descriptorPool = m_pool.second.pool;
    info.descriptorSetCount = 1u;
    info.pSetLayouts = &setLayout;

    if (likely(info.descriptorPool))
      vr = vk->vkAllocateDescriptorSets(vk->device(), &info, &set);

    if (unlikely(vr != VK_SUCCESS)) {
      m_pool = getNextPool();

      info.descriptorPool = m_pool.second.pool;
      vr = vk->vkAllocateDescriptorSets(vk->device(), &info, &set);

      if (vr != VK_SUCCESS)
        throw DxvkError(str::format("Failed to allocate descriptor set: ", vr));
    }

    m_pool.second.trackingId = trackingId;

    m_setsAllocated++;
    return set;
  }


  void DxvkDescriptorPool::notifyCompletion(
          uint64_t                    trackingId) {
    small_vector<std::pair<size_t, VkDescriptorPool>, 16u> pools;

    { std::lock_guard lock(m_mutex);

      for (size_t i = 0u; i < m_pools.size(); i++) {
        auto& pool = m_pools[i];

        if (trackingId >= pool.trackingId && pool.status == Status::InFlight)
          pools.push_back(std::make_pair(i, pool.pool));
      }
    }

    if (!pools.empty()) {
      auto vk = m_device->vkd();

      for (const auto& pool : pools)
        vk->vkResetDescriptorPool(vk->device(), pool.second, 0u);

      std::lock_guard lock(m_mutex);

      for (const auto& pool : pools)
        m_pools[pool.first].status = Status::Reset;
    }
  }


  void DxvkDescriptorPool::updateStats(DxvkStatCounters& counters) {
    counters.addCtr(DxvkStatCounter::DescriptorSetCount, m_setsAllocated);
    m_setsAllocated = 0u;
  }


  std::pair<size_t, DxvkDescriptorPool::DescriptorPool> DxvkDescriptorPool::getNextPool() {
    std::lock_guard lock(m_mutex);

    // Commit current pool and mark as in flight
    if (m_pool.second.pool) {
      m_pools[m_pool.first] = m_pool.second;
      m_pools[m_pool.first].status = Status::InFlight;
    }

    // Find available pool that's not in use
    for (size_t i = 0u; i < m_pools.size(); i++) {
      if (m_pools[i].status == Status::Reset) {
        m_pools[i].status = Status::InUse;
        return std::make_pair(i, m_pools[i]);
      }
    }

    // Create new pool as necessary
    auto poolIndex = m_pools.size();

    auto& pool = m_pools.emplace_back();
    pool.pool = createDescriptorPool();
    pool.status = Status::InUse;

    return std::make_pair(poolIndex, pool);
  }


  VkDescriptorPool DxvkDescriptorPool::createDescriptorPool() const {
    auto vk = m_device->vkd();

    // Samplers and uniform buffers may be special on some implementations
    // so we should allocate space for a reasonable number of both, but
    // assume that all other descriptor types share pool memory.
    constexpr static uint32_t MaxSets = 1024u;

    static const std::array<VkDescriptorPoolSize, 6> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxSets / 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxSets / 64 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxSets / 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxSets / 64 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxSets * 2  },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxSets / 2  },
    }};

    VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    info.maxSets       = MaxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;

    VkResult vr = vk->vkCreateDescriptorPool(vk->device(), &info, nullptr, &pool);

    if (vr)
      throw DxvkError(str::format("Failed create descriptor pool: ", vr));

    m_device->addStatCtr(DxvkStatCounter::DescriptorPoolCount, 1);
    return pool;
  }

}
