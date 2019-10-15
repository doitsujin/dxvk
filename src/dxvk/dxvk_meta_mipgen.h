#pragma once

#include <vector>

#include "dxvk_meta_blit.h"

namespace dxvk {
  
  /**
   * \brief Mip map generation render pass
   * 
   * Stores image views, framebuffer objects and
   * a render pass object for mip map generation.
   * This must be created per image view.
   */
  class DxvkMetaMipGenRenderPass : public DxvkResource {
    
  public:
    
    DxvkMetaMipGenRenderPass(
      const Rc<vk::DeviceFn>&   vkd,
      const Rc<DxvkImageView>&  view);
    
    ~DxvkMetaMipGenRenderPass();
    
    /**
     * \brief Render pass handle
     * \returns Render pass handle
     */
    VkRenderPass renderPass() const {
      return m_renderPass;
    }
    
    /**
     * \brief Source image view type
     * 
     * Use this to figure out which type the
     * resource descriptor needs to have.
     * \returns Source image view type
     */
    VkImageViewType viewType() const {
      return m_srcViewType;
    }
    
    /**
     * \brief Render pass count
     * 
     * Number of mip levels to generate.
     * \returns Render pass count
     */
    uint32_t passCount() const {
      return m_passes.size();
    }
    
    /**
     * \brief Framebuffer handles
     * 
     * Returns image view and framebuffer handles
     * required to generate a single mip level.
     * \param [in] pass Render pass index
     * \returns Object handles for the given pass
     */
    DxvkMetaBlitPass pass(uint32_t passId) const {
      return m_passes.at(passId);
    }
    
    /**
     * \brief Framebuffer size for a given pass
     * 
     * Stores the width, height, and layer count
     * of the framebuffer for the given pass ID.
     */
    VkExtent3D passExtent(uint32_t passId) const;
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    Rc<DxvkImageView> m_view;
    
    VkRenderPass m_renderPass;
    
    VkImageViewType m_srcViewType;
    VkImageViewType m_dstViewType;
    
    std::vector<DxvkMetaBlitPass> m_passes;
    
    VkRenderPass createRenderPass() const;
    
    DxvkMetaBlitPass createFramebuffer(uint32_t pass) const;
    
  };
  
}
