#include "dxvk_barrier.h"

namespace dxvk {
  
  DxvkBarrierSet:: DxvkBarrierSet() { }
  DxvkBarrierSet::~DxvkBarrierSet() { }
  
  bool DxvkBarrierSet::hasBarriers() const {
    return (m_srcFlags | m_dstFlags) != 0;
  }
  
  
  void DxvkBarrierSet::addMemoryBarrier(
          VkPipelineStageFlags    srcFlags,
          VkPipelineStageFlags    dstFlags,
    const VkMemoryBarrier&        barrier) {
    m_srcFlags |= srcFlags;
    m_dstFlags |= dstFlags;
    m_memory.push_back(barrier);
  }
  
  
  void DxvkBarrierSet::addBufferBarrier(
          VkPipelineStageFlags    srcFlags,
          VkPipelineStageFlags    dstFlags,
    const VkBufferMemoryBarrier&  barrier) {
    m_srcFlags |= srcFlags;
    m_dstFlags |= dstFlags;
    m_buffer.push_back(barrier);
  }
  
  
  void DxvkBarrierSet::addImageBarrier(
          VkPipelineStageFlags    srcFlags,
          VkPipelineStageFlags    dstFlags,
    const VkImageMemoryBarrier&   barrier) {
    m_srcFlags |= srcFlags;
    m_dstFlags |= dstFlags;
    m_image.push_back(barrier);
  }
  
  
  void DxvkBarrierSet::recordCommands(
          DxvkRecorder&           recorder) {
    VkPipelineStageFlags srcFlags = m_srcFlags;
    VkPipelineStageFlags dstFlags = m_dstFlags;
    
    if (srcFlags == 0) srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dstFlags == 0) dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    
    recorder.cmdPipelineBarrier(
      srcFlags, dstFlags, 0,
      m_memory.size(), m_memory.data(),
      m_buffer.size(), m_buffer.data(),
      m_image.size(),  m_image.data());
    
    this->reset();
  }
  
  
  void DxvkBarrierSet::reset() {
    m_srcFlags = 0;
    m_dstFlags = 0;
    
    m_memory.resize(0);
    m_buffer.resize(0);
    m_image .resize(0);
  }
  
}