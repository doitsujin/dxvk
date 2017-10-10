#include "dxvk_cmdlist.h"

namespace dxvk {
    
  DxvkCommandList::DxvkCommandList(
    const Rc<vk::DeviceFn>& vkd,
          uint32_t          queueFamily)
  : m_vkd(vkd) {
    VkCommandPoolCreateInfo poolInfo;
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext            = nullptr;
    poolInfo.flags            = 0;
    poolInfo.queueFamilyIndex = queueFamily;
    
    if (m_vkd->vkCreateCommandPool(m_vkd->device(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList::DxvkCommandList: Failed to create command pool");
    
    VkCommandBufferAllocateInfo cmdInfo;
    cmdInfo.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.pNext             = nullptr;
    cmdInfo.commandPool       = m_pool;
    cmdInfo.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    
    if (m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfo, &m_buffer) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList::DxvkCommandList: Failed to allocate command buffer");
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    m_resources.reset();
    
    m_vkd->vkDestroyCommandPool(
      m_vkd->device(), m_pool, nullptr);
  }
  
  
  void DxvkCommandList::beginRecording() {
    VkCommandBufferBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext            = nullptr;
    info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = nullptr;
    
    if (m_vkd->vkResetCommandPool(m_vkd->device(), m_pool, 0) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList::beginRecording: Failed to reset command pool");
    
    if (m_vkd->vkBeginCommandBuffer(m_buffer, &info) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList::beginRecording: Failed to begin command buffer recording");
  }
  
  
  void DxvkCommandList::endRecording() {
    if (m_vkd->vkEndCommandBuffer(m_buffer) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList::endRecording: Failed to record command buffer");
  }
  
  
  void DxvkCommandList::trackResource(const Rc<DxvkResource>& rc) {
    m_resources.trackResource(rc);
  }
  
  
  void DxvkCommandList::reset() {
    m_resources.reset();
  }
  
}