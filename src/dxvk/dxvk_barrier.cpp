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
    | VK_ACCESS_MEMORY_READ_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT;
    
  constexpr static VkAccessFlags AccessWriteMask
    = VK_ACCESS_SHADER_WRITE_BIT
    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_TRANSFER_WRITE_BIT
    | VK_ACCESS_MEMORY_WRITE_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;

  constexpr static VkAccessFlags AccessDeviceMask
    = AccessWriteMask | AccessReadMask;

  constexpr static VkAccessFlags AccessHostMask
    = VK_ACCESS_HOST_READ_BIT
    | VK_ACCESS_HOST_WRITE_BIT;

  constexpr static VkPipelineStageFlags StageDeviceMask
    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
    | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
    | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
    | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
    | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
    | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
    | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    | VK_PIPELINE_STAGE_TRANSFER_BIT
    | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    | VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

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
    m_memBarrier.srcStageMask  |= srcStages & StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & StageDeviceMask;

    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & AccessDeviceMask;

      if (dstAccess & AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
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
    m_memBarrier.srcStageMask  |= srcStages & StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & StageDeviceMask;
    
    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & AccessDeviceMask;

      if (dstAccess & AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
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

    m_allBarrierSrcStages |= srcStages & StageDeviceMask;

    if (srcLayout == dstLayout) {
      m_memBarrier.srcStageMask  |= srcStages & StageDeviceMask;
      m_memBarrier.srcAccessMask |= srcAccess & AccessWriteMask;
      m_memBarrier.dstStageMask  |= dstStages & StageDeviceMask;

      if (access.test(DxvkAccess::Write)) {
        m_memBarrier.dstAccessMask |= dstAccess;

        if (dstAccess & AccessHostMask) {
          m_hostBarrierSrcStages |= srcStages & StageDeviceMask;
          m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
        }
      }
    } else {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask                = srcStages & StageDeviceMask;
      barrier.srcAccessMask               = srcAccess & AccessWriteMask;
      barrier.dstStageMask                = dstStages & StageDeviceMask;
      barrier.dstAccessMask               = dstAccess & AccessDeviceMask;
      barrier.oldLayout                   = srcLayout;
      barrier.newLayout                   = dstLayout;
      barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                       = image->handle();
      barrier.subresourceRange            = subresources;
      barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
      m_imgBarriers.push_back(barrier);

      if (dstAccess & AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages;
        m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
      }

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
    barrier.srcStageMask                = srcStages & StageDeviceMask;
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

    if (dstAccess & AccessHostMask) {
      acquire.m_hostBarrierSrcStages |= srcStages & StageDeviceMask;
      acquire.m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
    }

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
    barrier.srcStageMask                = srcStages & StageDeviceMask;
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

    if (dstAccess & AccessHostMask) {
      acquire.m_hostBarrierSrcStages |= srcStages & StageDeviceMask;
      acquire.m_hostBarrierDstAccess |= dstAccess & AccessHostMask;
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
    if (flags & AccessReadMask)  result.set(DxvkAccess::Read);
    if (flags & AccessWriteMask) result.set(DxvkAccess::Write);
    return result;
  }
  
}