#pragma once

#include "dxvk_image.h"
#include "dxvk_renderpass.h"

namespace dxvk {
  
  /**
   * \brief Framebuffer size
   * 
   * Stores the width, height and number of layers
   * of a framebuffer. This can be used in case a
   * framebuffer does not have any attachments.
   */
  struct DxvkFramebufferSize {
    uint32_t width;
    uint32_t height;
    uint32_t layers;
  };
  
  
  /**
   * \brief Render target description
   * 
   * Stores render targets for a framebuffer object
   * and provides methods to query the render pass
   * format. Note that all render target views must
   * have the same size and number of array layers.
   */
  class DxvkRenderTargets {
    
  public:
    
    DxvkRenderTargets();
    ~DxvkRenderTargets();
    
    /**
     * \brief Retrieves color target
     * 
     * \param [in] id Color attachment ID
     * \returns Render target view
     */
    Rc<DxvkImageView> getColorTarget(uint32_t id) const {
      return m_colorTargets.at(id);
    }
    
    /**
     * \brief Retrieves depth-stencil target
     * \returns Depth-stencil target view
     */
    Rc<DxvkImageView> getDepthTarget() const {
      return m_depthTarget;
    }
    
    /**
     * \brief Sets color target
     * 
     * \param [in] id Color attachment ID
     * \param [in] view Render target view
     */
    void setColorTarget(uint32_t id, const Rc<DxvkImageView>& view) {
      m_colorTargets.at(id) = view;
    }
    
    /**
     * \brief Sets depth-stencil target
     * \param [in] view Depth-stencil target view
     */
    void setDepthTarget(const Rc<DxvkImageView>& view) {
      m_depthTarget = view;
    }
    
    /**
     * \brief Render pass format
     * 
     * Computes the render pass format based on
     * the color and depth-stencil attachments.
     * \returns Render pass format
     */
    DxvkRenderPassFormat renderPassFormat() const;
    
    /**
     * \brief Creates attachment list
     * \returns Framebuffer attachment list
     */
    std::vector<VkImageView> getAttachments() const;
    
    /**
     * \brief Framebuffer size
     * 
     * The width, height and layers
     * of the attached render targets.
     * \returns Framebuffer size
     */
    DxvkFramebufferSize getImageSize() const;
    
    /**
     * \brief Checks whether any attachments are defined
     * \returns \c false if no attachments are defined
     */
    bool hasAttachments() const;
    
  private:
    
    std::array<Rc<DxvkImageView>, MaxNumRenderTargets> m_colorTargets;
    Rc<DxvkImageView>                                  m_depthTarget;
    
    DxvkFramebufferSize renderTargetSize(
      const Rc<DxvkImageView>& renderTarget) const;
    
  };
  
  
  /**
   * \brief DXVK framebuffer
   * 
   * A framebuffer either stores a set of image views
   * that will be used as render targets, or in case
   * no render targets are being used, fixed viewport
   * dimensions.
   */
  class DxvkFramebuffer : public DxvkResource {
    
  public:
    
    DxvkFramebuffer(
      const Rc<vk::DeviceFn>&       vkd,
      const Rc<DxvkRenderPass>&     renderPass,
      const DxvkRenderTargets&      renderTargets);
    ~DxvkFramebuffer();
    
    /**
     * \brief Framebuffer handle
     * \returns Framebuffer handle
     */
    VkFramebuffer handle() const {
      return m_framebuffer;
    }
    
    /**
     * \brief Render pass handle
     * \returns Render pass handle
     */
    VkRenderPass renderPass() const {
      return m_renderPass->handle();
    }
    
    /**
     * \brief Framebuffer size
     * \returns Framebuffer size
     */
    DxvkFramebufferSize size() const {
      return m_framebufferSize;
    }
    
    /**
     * \brief Render target info
     * \returns Render target info
     */
    const DxvkRenderTargets& renderTargets() const {
      return m_renderTargets;
    }
    
    /**
     * \brief Sample count
     * \returns Sample count
     */
    VkSampleCountFlagBits sampleCount() const {
      return m_renderPass->sampleCount();
    }
    
  private:
    
    Rc<vk::DeviceFn>    m_vkd;
    Rc<DxvkRenderPass>  m_renderPass;
    
    DxvkRenderTargets   m_renderTargets;
    DxvkFramebufferSize m_framebufferSize = { 0, 0, 0 };
    
    VkFramebuffer       m_framebuffer     = VK_NULL_HANDLE;
    
  };
  
}