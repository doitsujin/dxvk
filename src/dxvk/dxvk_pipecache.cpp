#include "dxvk_pipecache.h"

namespace dxvk {
  
  DxvkPipelineCache::DxvkPipelineCache(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    VkPipelineCacheCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.pNext            = nullptr;
    info.flags            = 0;
    info.initialDataSize  = 0;
    info.pInitialData     = nullptr;
    
    if (m_vkd->vkCreatePipelineCache(m_vkd->device(),
        &info, nullptr, &m_handle) != VK_SUCCESS)
      throw DxvkError("DxvkPipelineCache: Failed to create cache");
  }
  
  
  DxvkPipelineCache::~DxvkPipelineCache() {
    m_vkd->vkDestroyPipelineCache(
      m_vkd->device(), m_handle, nullptr);
  }
  
}
