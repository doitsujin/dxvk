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
  
  
  DxvkFence::DxvkFence(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    VkFenceCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    
    if (m_vkd->vkCreateFence(m_vkd->device(), &info, nullptr, &m_fence) != VK_SUCCESS)
      throw DxvkError("DxvkFence::DxvkFence: Failed to create fence");
  }
  
  
  DxvkFence::~DxvkFence() {
    m_vkd->vkDestroyFence(
      m_vkd->device(), m_fence, nullptr);
  }
  
  
  bool DxvkFence::wait(uint64_t timeout) const {
    VkResult status = m_vkd->vkWaitForFences(
      m_vkd->device(), 1, &m_fence, VK_FALSE, timeout);
    
    if (status == VK_SUCCESS) return true;
    if (status == VK_TIMEOUT) return false;
    throw DxvkError("DxvkFence::wait: Failed to wait for fence");
  }
  
  
  void DxvkFence::reset() {
    if (m_vkd->vkResetFences(m_vkd->device(), 1, &m_fence) != VK_SUCCESS)
      throw DxvkError("DxvkFence::reset: Failed to reset fence");
  }
  
}