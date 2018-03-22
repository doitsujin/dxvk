#include "dxvk_sync.h"

namespace dxvk {
  
  DxvkSemaphore::DxvkSemaphore(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    VkSemaphoreCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    
    if (m_vkd->vkCreateSemaphore(m_vkd->device(), &info, nullptr, &m_semaphore) != VK_SUCCESS)
      throw DxvkError("DxvkSemaphore::DxvkSemaphore: Failed to create semaphore");
  }
  
  
  DxvkSemaphore::~DxvkSemaphore() {
    m_vkd->vkDestroySemaphore(
      m_vkd->device(), m_semaphore, nullptr);
  }
  
}