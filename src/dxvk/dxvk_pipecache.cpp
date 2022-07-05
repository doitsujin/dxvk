#include "dxvk_pipecache.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkPipelineCache::DxvkPipelineCache(DxvkDevice* device)
  : m_device(device) {
    auto vk = m_device->vkd();

    // It's not critical if this fails since this is only an in-memory cache
    VkPipelineCacheCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    
    if (vk->vkCreatePipelineCache(vk->device(), &info, nullptr, &m_handle))
      Logger::err("DxvkPipelineCache: Failed to create cache");
  }
  
  
  DxvkPipelineCache::~DxvkPipelineCache() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipelineCache(vk->device(), m_handle, nullptr);
  }
  
}
