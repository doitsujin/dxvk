#include "dxvk_barrier.h"

namespace dxvk {
  
  constexpr static VkAccessFlags AccessReadMask
    = VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    | VK_ACCESS_INDEX_READ_BIT
    | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    | VK_ACCESS_UNIFORM_READ_BIT
    | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    | VK_ACCESS_SHADER_READ_BIT
    | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
    | VK_ACCESS_TRANSFER_READ_BIT
    | VK_ACCESS_HOST_READ_BIT
    | VK_ACCESS_MEMORY_READ_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT;
    
  constexpr static VkAccessFlags AccessWriteMask
    = VK_ACCESS_SHADER_WRITE_BIT
    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_TRANSFER_WRITE_BIT
    | VK_ACCESS_HOST_WRITE_BIT
    | VK_ACCESS_MEMORY_WRITE_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
  
  DxvkBarrierSet:: DxvkBarrierSet(DxvkCmdBuffer cmdBuffer)
  : m_cmdBuffer(cmdBuffer) {

  }


  DxvkBarrierSet::~DxvkBarrierSet() {

  }

  
  void DxvkBarrierSet::accessMemory(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);

    m_allBarrierSrcStages |= srcStages;
    m_memBarrier.srcStageMask  |= srcStages;
    m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages;

    if (access.test(DxvkAccess::Write))
      m_memBarrier.dstAccessMask |= dstAccess;
  }


  void DxvkBarrierSet::accessBuffer(
    const DxvkBufferSliceHandle&    bufSlice,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);
    
    m_allBarrierSrcStages |= srcStages;
    m_memBarrier.srcStageMask  |= srcStages;
    m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages;
    
    if (access.test(DxvkAccess::Write))
      m_memBarrier.dstAccessMask |= dstAccess;

    m_bufSlices.insert(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, access));
  }
  
  
  void DxvkBarrierSet::accessImage(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);

    m_allBarrierSrcStages |= srcStages;

    if (srcLayout == dstLayout) {
      m_memBarrier.srcStageMask  |= srcStages;
      m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
      m_memBarrier.dstStageMask  |= dstStages;

      if (access.test(DxvkAccess::Write))
        m_memBarrier.dstAccessMask |= dstAccess;
    } else {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask                = srcStages;
      barrier.srcAccessMask               = srcAccess & AccessWriteMask;
      barrier.dstStageMask                = dstStages;
      barrier.dstAccessMask               = dstAccess;
      barrier.oldLayout                   = srcLayout;
      barrier.newLayout                   = dstLayout;
      barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                       = image->handle();
      barrier.subresourceRange            = subresources;
      barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
      m_imgBarriers.push_back(barrier);

      access.set(DxvkAccess::Write);
    }

    m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
  }


  void DxvkBarrierSet::releaseBuffer(
          DxvkBarrierSet&           acquire,
    const DxvkBufferSliceHandle&    bufSlice,
          uint32_t                  srcQueue,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          uint32_t                  dstQueue,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    auto& release = *this;

    m_allBarrierSrcStages |= srcStages;

    VkBufferMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    barrier.srcStageMask                = srcStages;
    barrier.srcAccessMask               = srcAccess & AccessWriteMask;
    barrier.dstStageMask                = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrier.dstAccessMask               = 0;
    barrier.srcQueueFamilyIndex         = srcQueue;
    barrier.dstQueueFamilyIndex         = dstQueue;
    barrier.buffer                      = bufSlice.handle;
    barrier.offset                      = bufSlice.offset;
    barrier.size                        = bufSlice.length;
    release.m_bufBarriers.push_back(barrier);

    barrier.srcStageMask                = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask               = 0;
    barrier.dstStageMask                = dstStages;
    barrier.dstAccessMask               = dstAccess;
    acquire.m_bufBarriers.push_back(barrier);

    DxvkAccessFlags access(DxvkAccess::Read, DxvkAccess::Write);
    release.m_bufSlices.insert(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, access));
    acquire.m_bufSlices.insert(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, access));
  }


  void DxvkBarrierSet::releaseImage(
          DxvkBarrierSet&           acquire,
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources,
          uint32_t                  srcQueue,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          uint32_t                  dstQueue,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    auto& release = *this;

    m_allBarrierSrcStages |= srcStages;

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask                = srcStages;
    barrier.srcAccessMask               = srcAccess & AccessWriteMask;
    barrier.dstStageMask                = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrier.dstAccessMask               = 0;
    barrier.oldLayout                   = srcLayout;
    barrier.newLayout                   = dstLayout;
    barrier.srcQueueFamilyIndex         = srcQueue;
    barrier.dstQueueFamilyIndex         = dstQueue;
    barrier.image                       = image->handle();
    barrier.subresourceRange            = subresources;
    barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
    release.m_imgBarriers.push_back(barrier);

    if (srcQueue == dstQueue)
      barrier.oldLayout = dstLayout;

    barrier.srcStageMask                = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask               = 0;
    barrier.dstStageMask                = dstStages;
    barrier.dstAccessMask               = dstAccess;
    acquire.m_imgBarriers.push_back(barrier);

    DxvkAccessFlags access(DxvkAccess::Read, DxvkAccess::Write);
    release.m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
    acquire.m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
  }


  bool DxvkBarrierSet::isBufferDirty(
    const DxvkBufferSliceHandle&    bufSlice,
          DxvkAccessFlags           bufAccess) {
    return m_bufSlices.isDirty(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, bufAccess));
  }


  bool DxvkBarrierSet::isImageDirty(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  imgSubres,
          DxvkAccessFlags           imgAccess) {
    return m_imgSlices.isDirty(image->handle(),
      DxvkBarrierImageSlice(imgSubres, imgAccess));
  }


  DxvkAccessFlags DxvkBarrierSet::getBufferAccess(
    const DxvkBufferSliceHandle&    bufSlice) {
    return m_bufSlices.getAccess(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, 0));
  }

  
  DxvkAccessFlags DxvkBarrierSet::getImageAccess(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  imgSubres) {
    return m_imgSlices.getAccess(image->handle(),
      DxvkBarrierImageSlice(imgSubres, 0));
  }


  void DxvkBarrierSet::recordCommands(const Rc<DxvkCommandList>& commandList) {
    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    if (m_memBarrier.srcStageMask | m_memBarrier.dstStageMask) {
      depInfo.memoryBarrierCount = 1;
      depInfo.pMemoryBarriers = &m_memBarrier;
    }

    if (!m_bufBarriers.empty()) {
      depInfo.bufferMemoryBarrierCount = m_bufBarriers.size();
      depInfo.pBufferMemoryBarriers = m_bufBarriers.data();
    }

    if (!m_imgBarriers.empty()) {
      depInfo.imageMemoryBarrierCount = m_imgBarriers.size();
      depInfo.pImageMemoryBarriers = m_imgBarriers.data();
    }

    if (depInfo.memoryBarrierCount + depInfo.bufferMemoryBarrierCount + depInfo.imageMemoryBarrierCount) {
      commandList->cmdPipelineBarrier(m_cmdBuffer, &depInfo);
      commandList->addStatCtr(DxvkStatCounter::CmdBarrierCount, 1);

      this->reset();
    }
  }
  
  
  void DxvkBarrierSet::reset() {
    m_allBarrierSrcStages = 0;

    m_memBarrier.srcStageMask = 0;
    m_memBarrier.srcAccessMask = 0;
    m_memBarrier.dstStageMask = 0;
    m_memBarrier.dstAccessMask = 0;
    
    m_bufBarriers.resize(0);
    m_imgBarriers.resize(0);

    m_bufSlices.clear();
    m_imgSlices.clear();
  }
  
  
  DxvkAccessFlags DxvkBarrierSet::getAccessTypes(VkAccessFlags flags) {
    DxvkAccessFlags result;
    if (flags & AccessReadMask)  result.set(DxvkAccess::Read);
    if (flags & AccessWriteMask) result.set(DxvkAccess::Write);
    return result;
  }
  
}