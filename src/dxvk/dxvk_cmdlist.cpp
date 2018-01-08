#include "dxvk_cmdlist.h"

namespace dxvk {
    
  DxvkCommandList::DxvkCommandList(
    const Rc<vk::DeviceFn>& vkd,
          DxvkDevice*       device,
          uint32_t          queueFamily)
  : m_vkd(vkd), m_descAlloc(vkd), m_stagingAlloc(device) {
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
    this->reset();
    
    m_vkd->vkDestroyCommandPool(
      m_vkd->device(), m_pool, nullptr);
  }
  
  
  void DxvkCommandList::submit(
          VkQueue         queue,
          VkSemaphore     waitSemaphore,
          VkSemaphore     wakeSemaphore,
          VkFence         fence) {
    const VkPipelineStageFlags waitStageMask
      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = nullptr;
    info.waitSemaphoreCount   = waitSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pWaitSemaphores      = &waitSemaphore;
    info.pWaitDstStageMask    = &waitStageMask;
    info.commandBufferCount   = 1;
    info.pCommandBuffers      = &m_buffer;
    info.signalSemaphoreCount = wakeSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pSignalSemaphores    = &wakeSemaphore;
    
    if (m_vkd->vkQueueSubmit(queue, 1, &info, fence) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::submitCommandList: Command submission failed");
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
  
  
  void DxvkCommandList::reset() {
    m_stagingAlloc.reset();
    m_descAlloc.reset();
    m_resources.reset();
  }
  
  
  void DxvkCommandList::bindResourceDescriptors(
          VkPipelineBindPoint     pipeline,
          VkPipelineLayout        pipelineLayout,
          VkDescriptorSetLayout   descriptorLayout,
          uint32_t                descriptorCount,
    const DxvkDescriptorSlot*     descriptorSlots,
    const DxvkDescriptorInfo*     descriptorInfos) {
    
    // Allocate a new descriptor set
    VkDescriptorSet dset = m_descAlloc.alloc(descriptorLayout);
    
    // Write data to the descriptor set
    std::array<VkWriteDescriptorSet, MaxNumResourceSlots> descriptorWrites;
    
    for (uint32_t i = 0; i < descriptorCount; i++) {
      auto& curr    = descriptorWrites[i];
      auto& binding = descriptorSlots[i];
      
      curr.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      curr.pNext            = nullptr;
      curr.dstSet           = dset;
      curr.dstBinding       = i;
      curr.dstArrayElement  = 0;
      curr.descriptorCount  = 1;
      curr.descriptorType   = binding.type;
      curr.pImageInfo       = &descriptorInfos[i].image;
      curr.pBufferInfo      = &descriptorInfos[i].buffer;
      curr.pTexelBufferView = &descriptorInfos[i].texelBuffer;
    }
    
    m_vkd->vkUpdateDescriptorSets(
      m_vkd->device(),
      descriptorCount,
      descriptorWrites.data(),
      0, nullptr);
    
    // Bind descriptor set to the pipeline
    m_vkd->vkCmdBindDescriptorSets(m_buffer,
      pipeline, pipelineLayout, 0, 1,
      &dset, 0, nullptr);
  }
  
  
  DxvkStagingBufferSlice DxvkCommandList::stagedAlloc(VkDeviceSize size) {
    return m_stagingAlloc.alloc(size);
  }
  
  
  void DxvkCommandList::stagedBufferCopy(
          VkBuffer                dstBuffer,
          VkDeviceSize            dstOffset,
          VkDeviceSize            dataSize,
    const DxvkStagingBufferSlice& dataSlice) {
    VkBufferCopy region;
    region.srcOffset = dataSlice.offset;
    region.dstOffset = dstOffset;
    region.size      = dataSize;
    
    m_vkd->vkCmdCopyBuffer(m_buffer,
      dataSlice.buffer, dstBuffer, 1, &region);
  }
  
  
  void DxvkCommandList::stagedBufferImageCopy(
          VkImage                 dstImage,
          VkImageLayout           dstImageLayout,
    const VkBufferImageCopy&      dstImageRegion,
    const DxvkStagingBufferSlice& dataSlice) {
    m_vkd->vkCmdCopyBufferToImage(m_buffer,
      dataSlice.buffer, dstImage, dstImageLayout,
      1, &dstImageRegion);
  }
  
}