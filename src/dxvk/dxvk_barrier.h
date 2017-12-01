#pragma once

#include "dxvk_buffer.h"
#include "dxvk_cmdlist.h"
#include "dxvk_image.h"

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
        
    void accessBuffer(
      const Rc<DxvkBuffer>&           buffer,
            VkDeviceSize              offset,
            VkDeviceSize              size,
            VkPipelineStageFlags      stages,
            VkAccessFlags             access);
    
    void recordCommands(
            DxvkCommandList&          commandList);
    
    void reset();
    
  private:
    
    VkPipelineStageFlags m_srcStages = 0;
    VkPipelineStageFlags m_dstStages = 0;
    
    std::vector<VkMemoryBarrier>        m_memBarriers;
    std::vector<VkBufferMemoryBarrier>  m_bufBarriers;
    std::vector<VkImageMemoryBarrier>   m_imgBarriers;
    
    DxvkResourceAccessTypes getAccessTypes(VkAccessFlags flags) const;
    
  };
  
}