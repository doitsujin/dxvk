#include "dxvk_descriptor.h"

namespace dxvk {
  
  DxvkDescriptorAlloc::DxvkDescriptorAlloc(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    // Allocate one pool right away so that there
    // is always at least one pool available when
    // allocating a descriptor set
    m_pools.push_back(createDescriptorPool());
  }
  
  
  DxvkDescriptorAlloc::~DxvkDescriptorAlloc() {
    for (auto p : m_pools) {
      m_vkd->vkDestroyDescriptorPool(
        m_vkd->device(), p, nullptr);
    }
  }
  
  
  VkDescriptorSet DxvkDescriptorAlloc::alloc(VkDescriptorSetLayout layout) {
    VkDescriptorSet set = allocFrom(m_pools[m_poolId], layout);
    
    if (set == VK_NULL_HANDLE) {
      if (++m_poolId >= m_pools.size())
        m_pools.push_back(createDescriptorPool());
      
      set = allocFrom(m_pools[m_poolId], layout);
    }
    
    return set;
  }
  
  
  void DxvkDescriptorAlloc::reset() {
    for (auto p : m_pools) {
      m_vkd->vkResetDescriptorPool(
        m_vkd->device(), p, 0);
    }
    
    m_poolId = 0;
  }
  
  
  VkDescriptorPool DxvkDescriptorAlloc::createDescriptorPool() {
    constexpr uint32_t MaxSets = 256;
    constexpr uint32_t MaxDesc = 2048;
    
    std::array<VkDescriptorPoolSize, 9> pools = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,                MaxDesc },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxDesc },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxDesc },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxDesc },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxDesc },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxDesc },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxDesc },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MaxDesc },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, MaxDesc } }};
    
    VkDescriptorPoolCreateInfo info;
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext         = nullptr;
    info.flags         = 0;
    info.maxSets       = MaxSets;
    info.poolSizeCount = pools.size();
    info.pPoolSizes    = pools.data();
    
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorPool(m_vkd->device(),
          &info, nullptr, &pool) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorAlloc: Failed to create descriptor pool");
    return pool;
  }
  
  
  VkDescriptorSet DxvkDescriptorAlloc::allocFrom(
          VkDescriptorPool      pool,
          VkDescriptorSetLayout layout) const {
    VkDescriptorSetAllocateInfo info;
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext              = nullptr;
    info.descriptorPool     = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layout;
    
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (m_vkd->vkAllocateDescriptorSets(m_vkd->device(), &info, &set) != VK_SUCCESS)
      return VK_NULL_HANDLE;
    return set;
  }
  
}