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
      const Rc<DxvkImageView>&  view,
            VkPipelineBindPoint bindPoint);
    
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
    Rc<DxvkImageView> getSrcView(uint32_t passId) const {
      return m_passes[passId].src;
    }

    /**
     * \brief Destination image view
     * 
     * \param [in] pass Render pass index
     * \returns Destination image view handle for the given pass
     */
    Rc<DxvkImageView> getDstView(uint32_t passId) const {
      return m_passes[passId].dst;
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

    VkPipelineBindPoint m_bindPoint;
    
    VkImageViewType m_srcViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkImageViewType m_dstViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    
    small_vector<PassViews, 16> m_passes;
    
    PassViews createViews(uint32_t pass) const;
    
  };


  /**
   * \brief Push data layout for mip gen pass
   */
  struct DxvkMetaMipGenPushConstants {
    VkDeviceAddress atomicCounterVa = 0u;
    uint32_t samplerIndex = 0u;
    uint32_t mipCount = 0u;
  };


  /**
   * \brief Mip gen pipeline info
   */
  struct DxvkMetaMipGenPipeline {
    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t mipsPerStep = 0u;
  };


  /**
   * \brief Spec constants for mip gen pipeline
   */
  struct DxvkMetaMipGenSpecConstants {
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t formatDwords = 0u;
  };


  /**
   * \brief Mip gen pipeline objects
   */
  class DxvkMetaMipGenObjects {

  public:

    constexpr static uint32_t MipCount = 6u;

    DxvkMetaMipGenObjects(DxvkDevice* device);
    ~DxvkMetaMipGenObjects();

    /**
     * \brief Checks format-specific support
     *
     * \param [in] format Format to query
     * \returns \c true if compute mip-gen can be used
     *    for the given format on the given device.
     */
    bool checkFormatSupport(
            VkFormat              viewFormat);

    /**
     * \brief Queries pipeline and properties of that pipeline
     *
     * Must only be called for supported formats.
     * \param [in] format Format to create the pipeline for
     * \returns Pipeline properties
     */
    DxvkMetaMipGenPipeline getPipeline(
            VkFormat              viewFormat);

  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    const DxvkPipelineLayout* m_layout;

    std::unordered_map<VkFormat, bool> m_formatSupport;
    std::unordered_map<VkFormat, DxvkMetaMipGenPipeline> m_pipelines;

    const DxvkPipelineLayout* createPipelineLayout() const;

    DxvkMetaMipGenPipeline createPipeline(VkFormat format) const;

    bool queryFormatSupport(VkFormat viewFormat) const;

  };

}
