#pragma once

#include "dxvk_recorder.h"

namespace dxvk {
  
  /**
   * \brief Barrier set
   * 
   * Accumulates memory barriers and provides a
   * method to record all those barriers into a
   * command buffer at once.
   */
  class DxvkBarrierSet {
    
  public:
    
    DxvkBarrierSet();
    ~DxvkBarrierSet();
    
    bool hasBarriers() const;
    
    void addMemoryBarrier(
            VkPipelineStageFlags    srcFlags,
            VkPipelineStageFlags    dstFlags,
      const VkMemoryBarrier&        barrier);
    
    void addBufferBarrier(
            VkPipelineStageFlags    srcFlags,
            VkPipelineStageFlags    dstFlags,
      const VkBufferMemoryBarrier&  barrier);
    
    void addImageBarrier(
            VkPipelineStageFlags    srcFlags,
            VkPipelineStageFlags    dstFlags,
      const VkImageMemoryBarrier&   barrier);
    
    void recordCommands(
            DxvkRecorder&           recorder);
    
    void reset();
    
  private:
    
    VkPipelineStageFlags m_srcFlags = 0;
    VkPipelineStageFlags m_dstFlags = 0;
    
    std::vector<VkMemoryBarrier>        m_memory;
    std::vector<VkBufferMemoryBarrier>  m_buffer;
    std::vector<VkImageMemoryBarrier>   m_image;
    
  };
  
}