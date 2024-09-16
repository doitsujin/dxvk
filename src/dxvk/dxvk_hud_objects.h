#pragma once

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_image.h"
#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  /**
   * \brief HUD coordinates
   *
   * Coordinates relative to the top-left
   * corner of the swap image, in pixels.
   */
  struct HudPos {
    float x;
    float y;
  };

  /**
   * \brief Color
   *
   * SRGB color with alpha channel. The text
   * will use this color for the most part.
   */
  struct HudColor {
    float r;
    float g;
    float b;
    float a;
  };

  /**
   * \brief Normalized color
   *
   * SRGB color with alpha channel.
   */
  struct HudNormColor {
    uint8_t a;
    uint8_t b;
    uint8_t g;
    uint8_t r;
  };

  /**
   * \brief Graph point with color
   */
  struct HudGraphPoint {
    float         value;
    HudNormColor  color;
  };

  /**
   * \brief HUD push constant data
   */
  struct HudTextPushConstants {
    HudColor color;
    HudPos pos;
    uint32_t offset;
    float size;
    HudPos scale;
  };

  struct HudGraphPushConstants {
    uint32_t offset;
    uint32_t count;
    HudPos pos;
    HudPos size;
    HudPos scale;
    float  opacity;
  };

  struct DxvkHudPipelinesKey {
    VkSampleCountFlagBits samples;
    VkFormat              viewFormat;
    VkColorSpaceKHR       colorSpace;

    bool eq(const DxvkHudPipelinesKey& other) const {
      return this->samples  == other.samples
        && this->viewFormat == other.viewFormat
        && this->colorSpace == other.colorSpace;
    }

    size_t hash() const {
      DxvkHashState result;
      result.add(uint32_t(this->samples));
      result.add(uint32_t(this->viewFormat));
      result.add(uint32_t(this->colorSpace));
      return result;
    }
  };


  struct DxvkHudPipelines {
    VkDescriptorSetLayout textDSetLayout;
    VkPipelineLayout textPipeLayout;
    VkPipeline textPipeHandle;
    VkDescriptorSetLayout graphDSetLayout;
    VkPipelineLayout graphPipeLayout;
    VkPipeline graphPipeHandle;
  };

  class DxvkDevice;

  class DxvkHudObjects {

  public:

    DxvkHudObjects(const DxvkDevice* device);
    ~DxvkHudObjects();

    DxvkHudPipelines getPipelines(
        VkSampleCountFlagBits samples,
        VkFormat              viewFormat,
        VkColorSpaceKHR       colorSpace);

    VkSampler getFontSampler() const {
      return m_fontSampler;
    }

  private:

    Rc<vk::DeviceFn>  m_vkd;

    VkShaderModule     m_textVs;
    VkShaderModule     m_textFs;
    VkShaderModule     m_graphVs;
    VkShaderModule     m_graphFs;

    VkSampler m_fontSampler;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkHudPipelinesKey,
      DxvkHudPipelines,
      DxvkHash, DxvkEq> m_pipelines;

    void createShaders(const DxvkDevice* device);

    void createFontSampler();

    VkPipeline createPipeline(
          VkShaderModule        vs,
          VkShaderModule        fs,
          VkPrimitiveTopology   topology,
          VkSampleCountFlagBits samples,
          VkFormat              viewFormat,
          VkColorSpaceKHR       colorSpace,
          VkPipelineLayout      pipeLayout);

    DxvkHudPipelines createPipelines(
      const DxvkHudPipelinesKey& key);

    VkDescriptorSetLayout createTextDescriptorSetLayout();
    VkDescriptorSetLayout createGraphDescriptorSetLayout();

    VkPipelineLayout createTextPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
    VkPipelineLayout createGraphPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

  };

}
