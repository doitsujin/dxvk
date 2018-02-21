#pragma once

#include <unordered_map>

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Meta resolve framebuffer
   * 
   * Stores a framebuffer and image view objects
   * for a meta resolve operation. Can be tracked.
   */
  class DxvkMetaResolveFramebuffer : public DxvkResource {
    
  public:
    
    DxvkMetaResolveFramebuffer(
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkImage>&            dstImage,
            VkImageSubresourceLayers  dstLayers,
      const Rc<DxvkImage>&            srcImage,
            VkImageSubresourceLayers  srcLayers,
            VkFormat                  format);
    
    ~DxvkMetaResolveFramebuffer();
    
    VkRenderPass renderPass() const {
      return m_renderPass;
    }
    
    VkFramebuffer framebuffer() const {
      return m_framebuffer;
    }
    
  private:
    
    const Rc<vk::DeviceFn> m_vkd;
    
    VkRenderPass  m_renderPass;
    VkImageView   m_dstImageView;
    VkImageView   m_srcImageView;
    VkFramebuffer m_framebuffer;
    
  };
  
}
