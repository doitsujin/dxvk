#include "dxvk_cmdlist.h"
#include "dxvk_device.h"

namespace dxvk {
    
  DxvkCommandList::DxvkCommandList(
          DxvkDevice*       device,
          uint32_t          queueFamily)
  : m_vkd           (device->vkd()),
    m_cmdBuffersUsed(0),
    m_descriptorPoolTracker(device),
    m_stagingAlloc  (device) {
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;
    
    if (m_vkd->vkCreateFence(m_vkd->device(), &fenceInfo, nullptr, &m_fence) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to create fence");
    
    VkCommandPoolCreateInfo poolInfo;
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext            = nullptr;
    poolInfo.flags            = 0;
    poolInfo.queueFamilyIndex = queueFamily;
    
    if (m_vkd->vkCreateCommandPool(m_vkd->device(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to create command pool");
    
    VkCommandBufferAllocateInfo cmdInfo;
    cmdInfo.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.pNext             = nullptr;
    cmdInfo.commandPool       = m_pool;
    cmdInfo.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    
    if (m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfo, &m_execBuffer) != VK_SUCCESS
     || m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfo, &m_initBuffer) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to allocate command buffer");
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();
    
    m_vkd->vkDestroyCommandPool(m_vkd->device(), m_pool,  nullptr);
    m_vkd->vkDestroyFence      (m_vkd->device(), m_fence, nullptr);
  }
  
  
  VkResult DxvkCommandList::submit(
          VkQueue         queue,
          VkSemaphore     waitSemaphore,
          VkSemaphore     wakeSemaphore) {
    std::array<VkCommandBuffer, 2> cmdBuffers;
    uint32_t cmdBufferCount = 0;
    
    if (m_cmdBuffersUsed.test(DxvkCmdBufferFlag::InitBuffer))
      cmdBuffers[cmdBufferCount++] = m_initBuffer;
    if (m_cmdBuffersUsed.test(DxvkCmdBufferFlag::ExecBuffer))
      cmdBuffers[cmdBufferCount++] = m_execBuffer;
    
    const VkPipelineStageFlags waitStageMask
      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = nullptr;
    info.waitSemaphoreCount   = waitSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pWaitSemaphores      = &waitSemaphore;
    info.pWaitDstStageMask    = &waitStageMask;
    info.commandBufferCount   = cmdBufferCount;
    info.pCommandBuffers      = cmdBuffers.data();
    info.signalSemaphoreCount = wakeSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pSignalSemaphores    = &wakeSemaphore;
    
    return m_vkd->vkQueueSubmit(queue, 1, &info, m_fence);
  }
  
  
  VkResult DxvkCommandList::synchronize() {
    VkResult status = VK_TIMEOUT;
    
    while (status == VK_TIMEOUT) {
      status = m_vkd->vkWaitForFences(
        m_vkd->device(), 1, &m_fence, VK_FALSE,
        1'000'000'000ull);
    }
    
    return status;
  }
  
  
  void DxvkCommandList::beginRecording() {
    VkCommandBufferBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext            = nullptr;
    info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = nullptr;
    
    if (m_vkd->vkResetCommandPool(m_vkd->device(), m_pool, 0) != VK_SUCCESS)
      Logger::err("DxvkCommandList: Failed to reset command buffer");
    
    if (m_vkd->vkBeginCommandBuffer(m_execBuffer, &info) != VK_SUCCESS
     || m_vkd->vkBeginCommandBuffer(m_initBuffer, &info) != VK_SUCCESS)
      Logger::err("DxvkCommandList: Failed to begin command buffer");
    
    if (m_vkd->vkResetFences(m_vkd->device(), 1, &m_fence) != VK_SUCCESS)
      Logger::err("DxvkCommandList: Failed to reset fence");
    
    // Unconditionally mark the exec buffer as used. There
    // is virtually no use case where this isn't correct.
    m_cmdBuffersUsed.set(DxvkCmdBufferFlag::ExecBuffer);
  }
  
  
  void DxvkCommandList::endRecording() {
    if (m_vkd->vkEndCommandBuffer(m_execBuffer) != VK_SUCCESS
     || m_vkd->vkEndCommandBuffer(m_initBuffer) != VK_SUCCESS)
      Logger::err("DxvkCommandList::endRecording: Failed to record command buffer");
  }
  
  
  void DxvkCommandList::reset() {
    m_statCounters.reset();
    m_bufferTracker.reset();
    m_gpuEventTracker.reset();
    m_eventTracker.reset();
    m_queryTracker.reset();
    m_stagingAlloc.reset();
    m_descriptorPoolTracker.reset();
    m_resources.reset();
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
    
    m_vkd->vkCmdCopyBuffer(m_execBuffer,
      dataSlice.buffer, dstBuffer, 1, &region);
  }
  
  
  void DxvkCommandList::stagedBufferImageCopy(
          VkImage                 dstImage,
          VkImageLayout           dstImageLayout,
    const VkBufferImageCopy&      dstImageRegion,
    const DxvkStagingBufferSlice& dataSlice) {
    m_vkd->vkCmdCopyBufferToImage(m_execBuffer,
      dataSlice.buffer, dstImage, dstImageLayout,
      1, &dstImageRegion);
  }
  
}