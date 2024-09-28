#pragma once

#include <vector>

#include "../util/util_small_vector.h"

#include "dxvk_meta_blit.h"

namespace dxvk {
  
  /**
   * \brief Mip map generation render pass
   * 
   * Stores image views, framebuffer objects and
   * a render pass object for mip map generation.
   * This must be created per image view.
   */
  class DxvkMetaMipGenViews {
    
  public:
    
    DxvkMetaMipGenViews(
      const Rc<DxvkImageView>&  view);
    
    ~DxvkMetaMipGenViews();
    
    /**
     * \brief Source image view type
     * 
     * Use this to figure out which type the
     * resource descriptor needs to have.
     * \returns Source image view type
     */
    VkImageViewType getSrcViewType() const {
      return m_srcViewType;
    }
    
    /**
     * \brief Render pass count
     * 
     * Number of mip levels to generate.
     * \returns Render pass count
     */
    uint32_t getPassCount() const {
      return m_passes.size();
    }
    
    /**
     * \brief Source image view
     * 
     * \param [in] pass Render pass index
     * \returns Source image view handle for the given pass
     */
    VkImageView getSrcViewHandle(uint32_t passId) const {
      return m_passes[passId].src->handle();
    }

    /**
     * \brief Destination image view
     * 
     * \param [in] pass Render pass index
     * \returns Destination image view handle for the given pass
     */
    VkImageView getDstViewHandle(uint32_t passId) const {
      return m_passes[passId].dst->handle();
    }

    /**
     * \brief Returns subresource that will only be read
     * \returns Top level of the image view
     */
    VkImageSubresourceRange getTopSubresource() const {
      VkImageSubresourceRange sr = m_view->imageSubresources();
      sr.levelCount = 1;
      return sr;
    }

    /**
     * \brief Returns subresource that will only be written
     * \returns Top level of the image view
     */
    VkImageSubresourceRange getBottomSubresource() const {
      VkImageSubresourceRange sr = m_view->imageSubresources();
      sr.baseMipLevel += sr.levelCount - 1;
      sr.levelCount = 1;
      return sr;
    }

    /**
     * \brief Returns all subresources that will be written
     * \returns All mip levels except the top level
     */
    VkImageSubresourceRange getAllTargetSubresources() const {
      VkImageSubresourceRange sr = m_view->imageSubresources();
      sr.baseMipLevel += 1;
      sr.levelCount -= 1;
      return sr;
    }

    /**
     * \brief Returns all subresources that will be read
     * \returns All mip levels except the bottom level
     */
    VkImageSubresourceRange getAllSourceSubresources() const {
      VkImageSubresourceRange sr = m_view->imageSubresources();
      sr.levelCount -= 1;
      return sr;
    }

    /**
     * \brief Returns subresource read in a given pass
     *
     * \param [in] pass Pass index
     * \returns The source subresource
     */
    VkImageSubresourceRange getSourceSubresource(uint32_t pass) const {
      VkImageSubresourceRange sr = m_view->imageSubresources();
      sr.baseMipLevel += pass;
      sr.levelCount = 1;
      return sr;
    }

    /**
     * \brief Framebuffer size for a given pass
     * 
     * Stores the width, height, and layer count
     * of the framebuffer for the given pass ID.
     */
    VkExtent3D computePassExtent(uint32_t passId) const;
    
  private:

    struct PassViews {
      Rc<DxvkImageView> src;
      Rc<DxvkImageView> dst;
    };

    Rc<DxvkImageView> m_view;
    
    VkImageViewType m_srcViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkImageViewType m_dstViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    
    small_vector<PassViews, 16> m_passes;
    
    PassViews createViews(uint32_t pass) const;
    
  };
  
}
