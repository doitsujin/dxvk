#include "dxvk_barrier.h"

namespace dxvk {
  
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
    m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;

    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & vk::AccessDeviceMask;

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }
    }
  }


  void DxvkBarrierSet::accessBuffer(
    const DxvkBufferSliceHandle&    bufSlice,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);
    
    m_allBarrierSrcStages |= srcStages;
    m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;
    
    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & vk::AccessDeviceMask;

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }
    }

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

    m_allBarrierSrcStages |= srcStages & vk::StageDeviceMask;

    if (srcLayout == dstLayout) {
      m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
      m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
      m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;

      if (access.test(DxvkAccess::Write)) {
        m_memBarrier.dstAccessMask |= dstAccess;

        if (dstAccess & vk::AccessHostMask) {
          m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
          m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
        }
      }
    } else {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask                = srcStages & vk::StageDeviceMask;
      barrier.srcAccessMask               = srcAccess & vk::AccessWriteMask;
      barrier.dstStageMask                = dstStages & vk::StageDeviceMask;
      barrier.dstAccessMask               = dstAccess & vk::AccessDeviceMask;
      barrier.oldLayout                   = srcLayout;
      barrier.newLayout                   = dstLayout;
      barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                       = image->handle();
      barrier.subresourceRange            = subresources;
      barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
      m_imgBarriers.push_back(barrier);

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }

      access.set(DxvkAccess::Write);
    }

    m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
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
    barrier.srcStageMask                = srcStages & vk::StageDeviceMask;
    barrier.srcAccessMask               = srcAccess & vk::AccessWriteMask;
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

    if (dstAccess & vk::AccessHostMask) {
      acquire.m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
      acquire.m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
    }

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


  void DxvkBarrierSet::finalize(const Rc<DxvkCommandList>& commandList) {
    // Emit host barrier if necessary
    if (m_hostBarrierSrcStages) {
      m_memBarrier.srcStageMask |= m_hostBarrierSrcStages;
      m_memBarrier.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
      m_memBarrier.dstStageMask |= VK_PIPELINE_STAGE_HOST_BIT;
      m_memBarrier.dstAccessMask |= m_hostBarrierDstAccess;

      m_hostBarrierSrcStages = 0;
      m_hostBarrierDstAccess = 0;
    }

    this->recordCommands(commandList);
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

    uint32_t totalBarrierCount = depInfo.memoryBarrierCount
      + depInfo.bufferMemoryBarrierCount
      + depInfo.imageMemoryBarrierCount;

    if (!totalBarrierCount)
      return;

    // AMDVLK (and -PRO) will just crash if they encounter a very large structure
    // in one vkCmdPipelineBarrier2 call, so we need to split the barrier into parts.
    constexpr uint32_t MaxBarriersPerCall = 512;

    if (unlikely(totalBarrierCount > MaxBarriersPerCall)) {
      VkDependencyInfo splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.memoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.memoryBarrierCount = std::min(depInfo.memoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pMemoryBarriers = depInfo.pMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }

      splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.bufferMemoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.bufferMemoryBarrierCount = std::min(depInfo.bufferMemoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pBufferMemoryBarriers = depInfo.pBufferMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }

      splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.imageMemoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.imageMemoryBarrierCount = std::min(depInfo.imageMemoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pImageMemoryBarriers = depInfo.pImageMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }
    } else {
      // Otherwise, issue the barrier as-is
      commandList->cmdPipelineBarrier(m_cmdBuffer, &depInfo);
    }

    commandList->addStatCtr(DxvkStatCounter::CmdBarrierCount, 1);

    this->reset();
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
    if (flags & vk::AccessReadMask)  result.set(DxvkAccess::Read);
    if (flags & vk::AccessWriteMask) result.set(DxvkAccess::Write);
    return result;
  }
  
}