#include "dxvk_barrier.h"

namespace dxvk {
  
  DxvkBarrierSet:: DxvkBarrierSet() { }
  DxvkBarrierSet::~DxvkBarrierSet() { }
  
  void DxvkBarrierSet::accessBuffer(
    const DxvkBufferSliceHandle&    bufSlice,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);
    
    m_srcStages |= srcStages;
    m_dstStages |= dstStages;
    
    if (srcStages == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
     || dstStages == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
      access.set(DxvkAccess::Write);
    
    if (access.test(DxvkAccess::Write)) {
      VkBufferMemoryBarrier barrier;
      barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.pNext               = nullptr;
      barrier.srcAccessMask       = srcAccess;
      barrier.dstAccessMask       = dstAccess;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer              = bufSlice.handle;
      barrier.offset              = bufSlice.offset;
      barrier.size                = bufSlice.length;
      m_bufBarriers.push_back(barrier);
    }

    m_bufSlices.push_back({ bufSlice, access });
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

    m_srcStages |= srcStages;
    m_dstStages |= dstStages;
    
    if (srcStages == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
     || dstStages == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
     || srcLayout != dstLayout)
      access.set(DxvkAccess::Write);
    
    if (access.test(DxvkAccess::Write)) {
      VkImageMemoryBarrier barrier;
      barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.pNext                       = nullptr;
      barrier.srcAccessMask               = srcAccess;
      barrier.dstAccessMask               = dstAccess;
      barrier.oldLayout                   = srcLayout;
      barrier.newLayout                   = dstLayout;
      barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                       = image->handle();
      barrier.subresourceRange            = subresources;
      barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
      m_imgBarriers.push_back(barrier);
    }

    m_imgSlices.push_back({ image.ptr(), subresources, access });
  }


  void DxvkBarrierSet::accessMemory(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    m_srcStages |= srcStages;
    m_dstStages |= dstStages;

    m_srcAccess |= srcAccess;
    m_dstAccess |= dstAccess;
  }
  
  
  bool DxvkBarrierSet::isBufferDirty(
    const DxvkBufferSliceHandle&    bufSlice,
          DxvkAccessFlags           bufAccess) {
    bool result = m_srcAccess || m_dstAccess;

    for (uint32_t i = 0; i < m_bufSlices.size() && !result; i++) {
      const DxvkBufferSliceHandle& dstSlice = m_bufSlices[i].slice;

      result = (bufSlice.handle == dstSlice.handle) && (bufAccess | m_bufSlices[i].access).test(DxvkAccess::Write)
            && (bufSlice.offset + bufSlice.length > dstSlice.offset)
            && (bufSlice.offset < dstSlice.offset + dstSlice.length);
    }

    return result;
  }


  bool DxvkBarrierSet::isImageDirty(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  imgSubres,
          DxvkAccessFlags           imgAccess) {
    bool result = (m_srcStages & image->info().stages)
               && (m_srcAccess & image->info().access);

    for (uint32_t i = 0; i < m_imgSlices.size() && !result; i++) {
      const VkImageSubresourceRange& dstSubres = m_imgSlices[i].subres;

      result = (image == m_imgSlices[i].image) && (imgAccess | m_imgSlices[i].access).test(DxvkAccess::Write)
            && (imgSubres.baseArrayLayer < dstSubres.baseArrayLayer + dstSubres.layerCount)
            && (imgSubres.baseArrayLayer + imgSubres.layerCount     > dstSubres.baseArrayLayer)
            && (imgSubres.baseMipLevel   < dstSubres.baseMipLevel   + dstSubres.levelCount)
            && (imgSubres.baseMipLevel   + imgSubres.levelCount     > dstSubres.baseMipLevel);
    }

    return result;
  }
  

  void DxvkBarrierSet::recordCommands(const Rc<DxvkCommandList>& commandList) {
    if (m_srcStages | m_dstStages) {
      VkPipelineStageFlags srcFlags = m_srcStages;
      VkPipelineStageFlags dstFlags = m_dstStages;
      
      if (!srcFlags) srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      if (!dstFlags) dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

      VkMemoryBarrier memBarrier;
      memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      memBarrier.pNext = nullptr;
      memBarrier.srcAccessMask = m_srcAccess;
      memBarrier.dstAccessMask = m_dstAccess;

      VkMemoryBarrier* pMemBarrier = nullptr;
      if (m_srcAccess | m_dstAccess)
        pMemBarrier = &memBarrier;
      
      commandList->cmdPipelineBarrier(
        srcFlags, dstFlags, 0,
        pMemBarrier ? 1 : 0, pMemBarrier,
        m_bufBarriers.size(), m_bufBarriers.data(),
        m_imgBarriers.size(), m_imgBarriers.data());
      
      this->reset();
    }
  }
  
  
  void DxvkBarrierSet::reset() {
    m_srcStages = 0;
    m_dstStages = 0;
    
    m_bufBarriers.resize(0);
    m_imgBarriers.resize(0);

    m_bufSlices.resize(0);
    m_imgSlices.resize(0);
  }
  
  
  DxvkAccessFlags DxvkBarrierSet::getAccessTypes(VkAccessFlags flags) const {
    const VkAccessFlags rflags
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
      
    const VkAccessFlags wflags
      = VK_ACCESS_SHADER_WRITE_BIT
      | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
      | VK_ACCESS_TRANSFER_WRITE_BIT
      | VK_ACCESS_HOST_WRITE_BIT
      | VK_ACCESS_MEMORY_WRITE_BIT
      | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
      | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
    
    DxvkAccessFlags result;
    if (flags & rflags) result.set(DxvkAccess::Read);
    if (flags & wflags) result.set(DxvkAccess::Write);
    return result;
  }
  
}