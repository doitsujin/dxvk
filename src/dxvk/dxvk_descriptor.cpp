#include "dxvk_descriptor.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkDescriptorPool::DxvkDescriptorPool(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    constexpr uint32_t MaxSets = 2048;

    std::array<VkDescriptorPoolSize, 9> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,                MaxSets * 2 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxSets * 3 },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxSets / 8 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MaxSets * 3 },
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
  
}