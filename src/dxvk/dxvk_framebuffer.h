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
   * \brief Framebuffer attachment
   * 
   * Stores an attachment, as well as the image layout
   * that will be used for rendering to the attachment.
   */
  struct DxvkAttachment {
    Rc<DxvkImageView> view    = nullptr;
    VkImageLayout     layout  = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  
  
  /**
   * \brief Render targets
   * 
   * Stores all depth-stencil and color
   * attachments attached to a framebuffer.
   */
  struct DxvkRenderTargets {
    DxvkAttachment depth;
    DxvkAttachment color[MaxNumRenderTargets];
  };


  /**
   * \brief Render target layouts
   */
  struct DxvkRenderTargetLayouts {
    VkImageLayout color[MaxNumRenderTargets];
    VkImageLayout depth;
  };
  
  
  /**
   * \brief Framebuffer
   * 
   * A framebuffer either stores a set of image views
   * that will be used as render targets, or in case
   * no render targets are attached, fixed dimensions.
   */
  class DxvkFramebuffer : public DxvkResource {
    
  public:
    
    DxvkFramebuffer(
      const Rc<vk::DeviceFn>&       vkd,
            DxvkRenderPass*         renderPass,
      const DxvkRenderTargets&      renderTargets,
      const DxvkFramebufferSize&    defaultSize);
    
    ~DxvkFramebuffer();
    
    /**
     * \brief Framebuffer handle
     * \returns Framebuffer handle
     */
    VkFramebuffer handle() const {
      return m_handle;
    }
    
    /**
     * \brief Framebuffer size
     * \returns Framebuffer size
     */
    DxvkFramebufferSize size() const {
      return m_renderSize;
    }
    
    /**
     * \brief Framebuffer sample count
     * 
     * Returns the sample count of the color
     * and depth-stencil attachments, or 0 if
     * there are no attachments.
     * \returns Sample count
     */
    VkSampleCountFlags getSampleCount() const {
      return m_attachmentCount != 0
        ? m_renderPass->getSampleCount()
        : 0;
    }
    
    /**
     * \brief Retrieves default render pass handle
     * 
     * Retrieves the render pass handle that was used
     * to create the Vulkan framebuffer object with,
     * and that should be used to create pipelines.
     * \returns The default render pass handle
     */
    VkRenderPass getDefaultRenderPassHandle() const {
      return m_renderPass->getDefaultHandle();
    }
    
    /**
     * \brief Retrieves render pass handle
     * 
     * Retrieves a render pass handle that can
     * be used to begin a render pass instance.
     * \param [in] ops Render pass ops
     * \returns The render pass handle
     */
    VkRenderPass getRenderPassHandle(const DxvkRenderPassOps& ops) const {
      return m_renderPass->getHandle(ops);
    }
    
    /**
     * \brief Retrieves render pass
     * \returns Render pass reference
     */
    DxvkRenderPass* getRenderPass() const {
      return m_renderPass;
    }
    
    /**
     * \brief Depth-stencil target
     * \returns Depth-stencil target
     */
    const DxvkAttachment& getDepthTarget() const {
      return m_renderTargets.depth;
    }
    
    /**
     * \brief Color target
     * 
     * \param [in] id Target Index
     * \returns The color target
     */
    const DxvkAttachment& getColorTarget(uint32_t id) const {
      return m_renderTargets.color[id];
    }
    
    /**
     * \brief Number of framebuffer attachment
     * \returns Total attachment count
     */
    uint32_t numAttachments() const {
      return m_attachmentCount;
    }
    
    /**
     * \brief Queries color attachment index of a given attachment
     * \returns The index, or -1 if the given attachment is the depth attachment
     */
    const int32_t getColorAttachmentIndex(uint32_t id) const {
      return m_attachments[id];
    }
    
    /**
     * \brief Retrieves attachment by index
     * 
     * \param [in] id Framebuffer attachment ID
     * \returns The framebuffer attachment
     */
    const DxvkAttachment& getAttachment(uint32_t id) const {
      int32_t idx = getColorAttachmentIndex(id);
      return idx < 0 ? m_renderTargets.depth : m_renderTargets.color[idx];
    }

    /**
     * \brief Finds attachment index by view
     * 
     * Color attachments start at 0
     * \param [in] view Image view
     * \returns Attachment index
     */
    int32_t findAttachment(const Rc<DxvkImageView>& view) const;
    
    /**
     * \brief Checks whether the framebuffer's targets match
     * 
     * \param [in] renderTargets Render targets to check
     * \returns \c true if the render targets are the same
     *          as the ones used for this framebuffer object.
     */
    bool hasTargets(const DxvkRenderTargets& renderTargets);
    
    /**
     * \brief Checks whether view and framebuffer sizes match
     *
     * Tests whether the size of the framebuffer is the same
     * as the size of one of its views. This may be \c false
     * when mixing attachments with mismatched dimensions.
     * \param [in] view Image view to test
     * \returns \c true if \c view has the same size as
     *          the framebuffer.
     */
    bool isFullSize(const Rc<DxvkImageView>& view) const;

    /**
     * \brief Checks whether an attachment is writable
     *
     * Needed for certain clear optimizations.
     * \param [in] attachmentIndex Attachment to check
     * \param [in] aspects Aspect mask to check
     * \returns \c true if all aspects can be written for the given attachment
     */
    bool isWritable(uint32_t attachmentIndex, VkImageAspectFlags aspects) const;
    
    /**
     * \brief Generatess render pass format
     * 
     * This render pass format can be used to
     * look up a compatible render pass.
     * \param [in] renderTargets Render targets
     * \returns The render pass format
     */
    static DxvkRenderPassFormat getRenderPassFormat(
      const DxvkRenderTargets&  renderTargets);
    
  private:
    
    const Rc<vk::DeviceFn>    m_vkd;
          DxvkRenderPass*     m_renderPass;
    const DxvkRenderTargets   m_renderTargets;
    const DxvkFramebufferSize m_renderSize;
    
    uint32_t                                     m_attachmentCount = 0;
    std::array<int32_t, MaxNumRenderTargets + 1> m_attachments;
    
    VkFramebuffer m_handle = VK_NULL_HANDLE;
    
    DxvkFramebufferSize computeRenderSize(
      const DxvkFramebufferSize& defaultSize) const;
    
    DxvkFramebufferSize computeRenderTargetSize(
      const Rc<DxvkImageView>& renderTarget) const;
    
  };
  
}