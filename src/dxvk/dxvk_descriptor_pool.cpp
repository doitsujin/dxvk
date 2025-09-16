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

    DescriptorPool* pool = nullptr;

    if (!m_pools.empty())
      pool = &m_pools[m_poolIndex];

    VkDescriptorSetLayout setLayout = layout->getSetLayout();

    VkResult vr = VK_ERROR_OUT_OF_POOL_MEMORY;
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    info.descriptorSetCount = 1u;
    info.pSetLayouts = &setLayout;

    if (likely(pool)) {
      info.descriptorPool = pool->pool;

      vr = vk->vkAllocateDescriptorSets(vk->device(), &info, &set);
    }

    if (unlikely(vr != VK_SUCCESS)) {
      pool = &getNextPool();

      info.descriptorPool = pool->pool;
      vr = vk->vkAllocateDescriptorSets(vk->device(), &info, &set);

      if (vr != VK_SUCCESS)
        throw DxvkError(str::format("Failed to allocate descriptor set: ", vr));
    }

    pool->trackingId = trackingId;

    m_setsAllocated++;
    return set;
  }


  void DxvkDescriptorPool::notifyCompletion(
          uint64_t                    trackingId) {
    m_lastCompleteTrackingId.store(trackingId, std::memory_order_release);
  }


  void DxvkDescriptorPool::updateStats(DxvkStatCounters& counters) {
    counters.addCtr(DxvkStatCounter::DescriptorSetCount, m_setsAllocated);
    m_setsAllocated = 0u;
  }


  DxvkDescriptorPool::DescriptorPool& DxvkDescriptorPool::getNextPool() {
    uint64_t lastComplete = m_lastCompleteTrackingId.load(std::memory_order_acquire);

    bool foundFreePool = false;

    if (!m_pools.empty()) {
      for (size_t i = 1u; i < m_pools.size() && !foundFreePool; i++) {
        m_poolIndex += 1u;
        m_poolIndex %= m_pools.size();

        foundFreePool = lastComplete >= m_pools[m_poolIndex].trackingId;
      }
    }

    if (!foundFreePool) {
      auto& pool = m_pools.emplace_back();
      pool.pool = createDescriptorPool();

      m_poolIndex = m_pools.size() - 1u;
      return pool;
    } else {
      auto& result = m_pools[m_poolIndex];

      auto vk = m_device->vkd();
      VkResult vr = vk->vkResetDescriptorPool(vk->device(), result.pool, 0u);

      if (vr)
        throw DxvkError(str::format("Failed reset descriptor pool: ", vr));

      return result;
    }
  }


  VkDescriptorPool DxvkDescriptorPool::createDescriptorPool() const {
    auto vk = m_device->vkd();

    // Samplers and uniform buffers may be special on some implementations
    // so we should allocate space for a reasonable number of both, but
    // assume that all other descriptor types share pool memory.
    constexpr static uint32_t MaxSets = env::is32BitHostPlatform() ? 24576u : 49152u;

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
