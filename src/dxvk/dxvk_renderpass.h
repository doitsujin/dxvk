#pragma once

#include <mutex>
#include <unordered_map>

#include "dxvk_hash.h"
#include "dxvk_include.h"
#include "dxvk_limits.h"

namespace dxvk {
  
  /**
   * \brief Format and layout info for a sigle render target
   * 
   * Stores the format, initial layout and
   * final layout of a render target.
   */
  struct DxvkRenderTargetFormat {
    VkFormat      format        = VK_FORMAT_UNDEFINED;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout renderLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  
  
  /**
   * \brief Render pass format
   * 
   * Stores the formats of all render targets
   * that are used by a framebuffer object.
   */
  class DxvkRenderPassFormat {
    
  public:
    
    /**
     * \brief Retrieves color target format
     * 
     * If the color target has not been defined,
     * this will return \c VK_FORMAT_UNDEFINED.
     * \param [in] id Color target index
     * \returns Color target format
     */
    DxvkRenderTargetFormat getColorFormat(uint32_t id) const {
      return m_color.at(id);
    }
    
    /**
     * \brief Retrieves depth-stencil format
     * 
     * If the color target has not been defined,
     * this will return \c VK_FORMAT_UNDEFINED.
     */
    DxvkRenderTargetFormat getDepthFormat() const {
      return m_depth;
    }
    
    /**
     * \brief Retrieves sample count
     * 
     * If no sample count has been explicitly specitied,
     * this will return \c VK_SAMPLE_COUNT_1_BIT.
     * \returns Sample count
     */
    VkSampleCountFlagBits getSampleCount() const {
      return m_samples;
    }
    
    /**
     * \brief Sets color target format
     * 
     * \param [in] id Color target index
     * \param [in] fmt Color target format
     */
    void setColorFormat(uint32_t id, DxvkRenderTargetFormat fmt) {
      m_color.at(id) = fmt;
    }
    
    /**
     * \brief Sets depth-stencil format
     * \param [in] fmt Depth-stencil format
     */
    void setDepthFormat(DxvkRenderTargetFormat fmt) {
      m_depth = fmt;
    }
    
    /**
     * \brief Sets sample count
     * \param [in] samples Sample count
     */
    void setSampleCount(VkSampleCountFlagBits samples) {
      m_samples = samples;
    }
    
    /**
     * \brief Checks whether two render pass formats are compatible
     * 
     * \param [in] other The render pass format to compare to
     * \returns \c true if the render pass formats are compatible
     */
    bool matchesFormat(const DxvkRenderPassFormat& other) const;
    
  private:
    
    std::array<DxvkRenderTargetFormat, MaxNumRenderTargets> m_color;
    DxvkRenderTargetFormat                                  m_depth;
    VkSampleCountFlagBits                                   m_samples = VK_SAMPLE_COUNT_1_BIT;
    
  };
  
  
  /**
   * \brief DXVK render pass
   * 
   * Render pass objects are used internally to identify render
   * target formats and 
   */
  class DxvkRenderPass : public RcObject {
    
  public:
    
    DxvkRenderPass(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkRenderPassFormat& fmt);
    ~DxvkRenderPass();
    
    /**
     * \brief Render pass handle
     * 
     * Internal use only.
     * \returns Render pass handle
     */
    VkRenderPass handle() const {
      return m_renderPass;
    }
    
    /**
     * \brief Render pass sample count
     * \returns Render pass sample count
     */
    VkSampleCountFlagBits sampleCount() const {
      return m_format.getSampleCount();
    }
    
    /**
     * \brief Checks render pass format compatibility
     * 
     * This render pass object can be used with compatible render
     * pass formats. Two render pass formats are compatible if the
     * used attachments match in image format and layout.
     * \param [in] format The render pass format to test
     * \returns \c true if the formats match
     */
    bool matchesFormat(const DxvkRenderPassFormat& format) const {
      return m_format.matchesFormat(format);
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    DxvkRenderPassFormat  m_format;
    VkRenderPass          m_renderPass;
    
  };
  
  
  /**
   * \brief Render pass pool
   * 
   * Thread-safe class that manages the render pass
   * objects that are used within an application.
   */
  class DxvkRenderPassPool : public RcObject {
    
  public:
    
    DxvkRenderPassPool(const Rc<vk::DeviceFn>& vkd);
    ~DxvkRenderPassPool();
    
    /**
     * \brief Retrieves a render pass object
     * 
     * \param [in] fmt Render target formats
     * \returns Compatible render pass object
     */
    Rc<DxvkRenderPass> getRenderPass(
      const DxvkRenderPassFormat& fmt);
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    
    std::mutex                      m_mutex;
    std::vector<Rc<DxvkRenderPass>> m_renderPasses;
    
    Rc<DxvkRenderPass> createRenderPass(
      const DxvkRenderPassFormat& fmt);
    
  };
  
}