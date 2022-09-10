#pragma once

#include "dxvk_image.h"
#include "dxvk_graphics_state.h"
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
   * \brief Framebuffer key
   */
  struct DxvkFramebufferKey {
    uint64_t            colorViews[MaxNumRenderTargets];
    uint64_t            depthView;
    VkRenderPass        renderPass;

    size_t hash() const {
      DxvkHashState state;
      state.add(depthView);
      for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
        state.add(colorViews[i]);
      state.add(uint64_t(renderPass));
      return state;
    }

    bool eq(const DxvkFramebufferKey& other) const {
      bool eq = depthView   == other.depthView
             && renderPass  == other.renderPass;
      for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
        eq &= colorViews[i] == other.colorViews[i];
      return eq;
    }
  };


  /**
   * \brief Framebuffer info
   *
   * Stores metadata about the current framebuffer,
   * without actually creating a framebuffer object.
   */
  class DxvkFramebufferInfo {

  public:

    DxvkFramebufferInfo();

    DxvkFramebufferInfo(
      const DxvkRenderTargets&      renderTargets,
      const DxvkFramebufferSize&    defaultSize);

    ~DxvkFramebufferInfo();

    /**
     * \brief Retrieves all attachments
     * \returns Render targets
     */
    const DxvkRenderTargets& attachments() const {
      return m_renderTargets;
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
      return m_sampleCount;
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
    int32_t getColorAttachmentIndex(uint32_t id) const {
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
     * \brief Generates render target state
     * \returns Render target state info
     */
    DxvkRtInfo getRtInfo() const;

  private:

    DxvkRenderTargets   m_renderTargets;
    DxvkFramebufferSize m_renderSize  = { 0u, 0u, 0u };
    VkSampleCountFlags  m_sampleCount = 0;

    uint32_t                                     m_attachmentCount = 0;
    std::array<int32_t, MaxNumRenderTargets + 1> m_attachments;

    DxvkFramebufferSize computeRenderSize(
      const DxvkFramebufferSize& defaultSize) const;

    DxvkFramebufferSize computeRenderTargetSize(
      const Rc<DxvkImageView>& renderTarget) const;

  };
  
}