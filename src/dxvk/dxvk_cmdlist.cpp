#include "dxvk_cmdlist.h"

namespace dxvk {
    
  DxvkCommandList::DxvkCommandList(
    const Rc<vk::DeviceFn>& vkd,
          uint32_t          queueFamily)
  : m_vkd(vkd), m_descAlloc(vkd) {
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
    m_descAlloc.reset();
    m_resources.reset();
  }
  
  
  void DxvkCommandList::bindShaderResources(
          VkPipelineBindPoint     pipeline,
          VkPipelineLayout        pipelineLayout,
          VkDescriptorSetLayout   descriptorLayout,
          uint32_t                bindingCount,
    const DxvkResourceBinding*    bindings) {
    VkDescriptorSet dset = m_descAlloc.alloc(descriptorLayout);
    
    if (bindingCount > m_descriptorSetWrites.size())
      m_descriptorSetWrites.resize(bindingCount);
    
    for (uint32_t i = 0; i < bindingCount; i++) {
      VkWriteDescriptorSet& info = m_descriptorSetWrites.at(i);
      
      info.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      info.pNext            = nullptr;
      info.dstSet           = dset;
      info.dstBinding       = i;
      info.dstArrayElement  = 0;
      info.descriptorCount  = 1;
      info.descriptorType   = bindings[i].type;
      info.pImageInfo       = &bindings[i].image;
      info.pBufferInfo      = &bindings[i].buffer;
      info.pTexelBufferView = nullptr;
    }
    
    m_vkd->vkUpdateDescriptorSets(m_vkd->device(),
      bindingCount, m_descriptorSetWrites.data(), 0, nullptr);
    
    m_vkd->vkCmdBindDescriptorSets(m_buffer,
      pipeline, pipelineLayout, 0, 1, &dset, 0, nullptr);
  }
  
  
  void DxvkCommandList::cmdBeginRenderPass(
    const VkRenderPassBeginInfo*  pRenderPassBegin,
          VkSubpassContents       contents) {
    m_vkd->vkCmdBeginRenderPass(m_buffer,
      pRenderPassBegin, contents);
  }
  
  
  void DxvkCommandList::cmdBindIndexBuffer(
          VkBuffer                buffer,
          VkDeviceSize            offset,
          VkIndexType             indexType) {
    m_vkd->vkCmdBindIndexBuffer(m_buffer,
      buffer, offset, indexType);
  }
  
  
  void DxvkCommandList::cmdBindPipeline(
          VkPipelineBindPoint     pipelineBindPoint,
          VkPipeline              pipeline) {
    m_vkd->vkCmdBindPipeline(m_buffer,
      pipelineBindPoint, pipeline);
  }
  
  
  void DxvkCommandList::cmdBindVertexBuffers(
          uint32_t                firstBinding,
          uint32_t                bindingCount,
    const VkBuffer*               pBuffers,
    const VkDeviceSize*           pOffsets) {
    m_vkd->vkCmdBindVertexBuffers(m_buffer,
      firstBinding, bindingCount, pBuffers, pOffsets);
  }
  
  
  void DxvkCommandList::cmdClearAttachments(
          uint32_t                attachmentCount,
    const VkClearAttachment*      pAttachments,
          uint32_t                rectCount,
    const VkClearRect*            pRects) {
    m_vkd->vkCmdClearAttachments(m_buffer,
      attachmentCount, pAttachments,
      rectCount, pRects);
  }
  
  
  void DxvkCommandList::cmdClearColorImage(
          VkImage                 image,
          VkImageLayout           imageLayout,
    const VkClearColorValue*      pColor,
          uint32_t                rangeCount,
    const VkImageSubresourceRange* pRanges) {
    m_vkd->vkCmdClearColorImage(m_buffer,
      image, imageLayout, pColor,
      rangeCount, pRanges);
  }
  
  
  void DxvkCommandList::cmdCopyBuffer(
          VkBuffer                srcBuffer,
          VkBuffer                dstBuffer,
          uint32_t                regionCount,
    const VkBufferCopy*           pRegions) {
    m_vkd->vkCmdCopyBuffer(m_buffer,
      srcBuffer, dstBuffer,
      regionCount, pRegions);
  }
  
  
  void DxvkCommandList::cmdDispatch(
          uint32_t                x,
          uint32_t                y,
          uint32_t                z) {
    m_vkd->vkCmdDispatch(m_buffer, x, y, z);
  }
  
  
  void DxvkCommandList::cmdDraw(
          uint32_t                vertexCount,
          uint32_t                instanceCount,
          uint32_t                firstVertex,
          uint32_t                firstInstance) {
    m_vkd->vkCmdDraw(m_buffer,
      vertexCount, instanceCount,
      firstVertex, firstInstance);
  }
  
  
  void DxvkCommandList::cmdDrawIndexed(
          uint32_t                indexCount,
          uint32_t                instanceCount,
          uint32_t                firstIndex,
          uint32_t                vertexOffset,
          uint32_t                firstInstance) {
    m_vkd->vkCmdDrawIndexed(m_buffer,
      indexCount, instanceCount,
      firstIndex, vertexOffset,
      firstInstance);
  }
  
  
  void DxvkCommandList::cmdEndRenderPass() {
    m_vkd->vkCmdEndRenderPass(m_buffer);
  }
  
  
  void DxvkCommandList::cmdPipelineBarrier(
          VkPipelineStageFlags    srcStageMask,
          VkPipelineStageFlags    dstStageMask,
          VkDependencyFlags       dependencyFlags,
          uint32_t                memoryBarrierCount,
    const VkMemoryBarrier*        pMemoryBarriers,
          uint32_t                bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*  pBufferMemoryBarriers,
          uint32_t                imageMemoryBarrierCount,
    const VkImageMemoryBarrier*   pImageMemoryBarriers) {
    m_vkd->vkCmdPipelineBarrier(m_buffer,
      dstStageMask, srcStageMask, dependencyFlags,
      memoryBarrierCount, pMemoryBarriers,
      bufferMemoryBarrierCount, pBufferMemoryBarriers,
      imageMemoryBarrierCount, pImageMemoryBarriers);
  }
  
  
  void DxvkCommandList::cmdSetScissor(
          uint32_t                firstScissor,
          uint32_t                scissorCount,
    const VkRect2D*               scissors) {
    m_vkd->vkCmdSetScissor(m_buffer,
      firstScissor, scissorCount, scissors);
  }
  
  
  void DxvkCommandList::cmdSetViewport(
          uint32_t                firstViewport,
          uint32_t                viewportCount,
    const VkViewport*             viewports) {
    m_vkd->vkCmdSetViewport(m_buffer,
      firstViewport, viewportCount, viewports);
  }
  
}