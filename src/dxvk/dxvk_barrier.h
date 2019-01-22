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
      const DxvkBufferSliceHandle&    bufSlice,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    void accessImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources,
            VkImageLayout             srcLayout,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkImageLayout             dstLayout,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    void accessMemory(
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    bool isBufferDirty(
      const DxvkBufferSliceHandle&    bufSlice,
            DxvkAccessFlags           bufAccess);

    bool isImageDirty(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  imgSubres,
            DxvkAccessFlags           imgAccess);
    
    void recordCommands(
      const Rc<DxvkCommandList>&      commandList);
    
    void reset();
    
  private:

    struct BufSlice {
      DxvkBufferSliceHandle   slice;
      DxvkAccessFlags         access;
    };

    struct ImgSlice {
      DxvkImage*              image;
      VkImageSubresourceRange subres;
      DxvkAccessFlags         access;
    };
    
    VkPipelineStageFlags m_srcStages = 0;
    VkPipelineStageFlags m_dstStages = 0;

    VkAccessFlags m_srcAccess = 0;
    VkAccessFlags m_dstAccess = 0;
    
    std::vector<VkBufferMemoryBarrier>  m_bufBarriers;
    std::vector<VkImageMemoryBarrier>   m_imgBarriers;

    std::vector<BufSlice> m_bufSlices;
    std::vector<ImgSlice> m_imgSlices;
    
    DxvkAccessFlags getAccessTypes(VkAccessFlags flags) const;
    
  };
  
}