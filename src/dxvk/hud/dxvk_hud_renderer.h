#pragma once

#include "../dxvk_device.h"

#include "dxvk_hud_font.h"

namespace dxvk::hud {

  /**
   * \brief HUD options
   */
  struct HudOptions {
    float scale = 1.0f;
    float opacity = 1.0f;
  };


  /**
   * \brief HUD coordinates
   * 
   * Coordinates relative to the top-left
   * corner of the swap image, in pixels.
   */
  struct HudPos {
    int32_t x = 0;
    int32_t y = 0;
  };


  /**
   * \brief Draw parameters for text
   */
  struct HudTextDrawInfo {
    uint32_t textOffset = 0u;
    uint16_t textLength = 0u;
    uint16_t fontSize = 0u;
    int16_t  posX = 0;
    int16_t  posY = 0;
    uint32_t color = 0u;
  };


  struct HudPushConstants {
    VkExtent2D surfaceSize;
    float opacity;
    float scale;
  };


  /**
   * \brief Pipeline key
   */
  struct HudPipelineKey {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(format));
      hash.add(uint32_t(colorSpace));
      return hash;
    }

    bool eq(const HudPipelineKey& other) const {
      return format == other.format && colorSpace == other.colorSpace;
    }
  };


  /**
   * \brief Specialization constants
   */
  struct HudSpecConstants {
    VkColorSpaceKHR dstSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkBool32 dstIsSrgb = VK_FALSE;
  };


  /**
   * \brief Shader module info
   */
  struct HudShaderModule {
    VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    VkPipelineShaderStageCreateInfo stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  };


  /**
   * \brief Text renderer for the HUD
   * 
   * Can be used by the presentation backend to
   * display performance and driver information.
   */
  class HudRenderer {
    constexpr static VkDeviceSize DataBufferSize = 16384;
  public:
    
    HudRenderer(
      const Rc<DxvkDevice>&   device);
    
    ~HudRenderer();

    void beginFrame(
      const DxvkContextObjects& ctx,
      const Rc<DxvkImageView>&  dstView,
      const HudOptions&         options);

    void endFrame(
      const DxvkContextObjects& ctx);

    void drawText(
            uint32_t            size,
            HudPos              pos,
            uint32_t            color,
      const std::string&        text);

    void drawTextIndirect(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const VkDescriptorBufferInfo& drawArgs,
      const VkDescriptorBufferInfo& drawInfos,
            VkBufferView        text,
            uint32_t            drawCount);

    void flushDraws(
      const DxvkContextObjects& ctx,
      const Rc<DxvkImageView>&  dstView,
      const HudOptions&         options);

    HudPipelineKey getPipelineKey(
      const Rc<DxvkImageView>&  dstView) const;

    HudSpecConstants getSpecConstants(
      const HudPipelineKey&     key) const;

    HudPushConstants getPushConstants() const;

    VkSpecializationInfo getSpecInfo(
      const HudSpecConstants*   constants) const;

    void createShaderModule(
            HudShaderModule&    shader,
            VkShaderStageFlagBits stage,
            size_t              size,
      const uint32_t*           code) const;

  private:

    Rc<DxvkDevice>          m_device;

    Rc<DxvkBuffer>          m_fontBuffer;
    Rc<DxvkImage>           m_fontTexture;
    Rc<DxvkImageView>       m_fontTextureView;
    Rc<DxvkSampler>         m_fontSampler;

    Rc<DxvkBuffer>          m_textBuffer;
    Rc<DxvkBufferView>      m_textBufferView;

    std::vector<HudTextDrawInfo>  m_textDraws;
    std::vector<char>             m_textData;

    HudShaderModule         m_textVs;
    HudShaderModule         m_textFs;

    VkDescriptorSetLayout   m_textSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout        m_textPipelineLayout = VK_NULL_HANDLE;

    HudPushConstants        m_pushConstants = { };

    std::unordered_map<HudPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_textPipelines;

    void createFontResources();

    void uploadFontResources(
      const DxvkContextObjects& ctx);

    VkDescriptorSetLayout createSetLayout();

    VkPipelineLayout createPipelineLayout();

    VkPipeline createPipeline(
      const HudPipelineKey&     key);

    VkPipeline getPipeline(
      const HudPipelineKey&     key);

  };
  
}