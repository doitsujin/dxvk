#include "dxvk_sampler.h"

namespace dxvk {
    
  DxvkSampler::DxvkSampler(
    const Rc<vk::DeviceFn>&       vkd,
    const DxvkSamplerCreateInfo&  info)
  : m_vkd(vkd) {
    VkSamplerCreateInfo samplerInfo;
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext                   = nullptr;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = info.magFilter;
    samplerInfo.minFilter               = info.minFilter;
    samplerInfo.mipmapMode              = info.mipmapMode;
    samplerInfo.addressModeU            = info.addressModeU;
    samplerInfo.addressModeV            = info.addressModeV;
    samplerInfo.addressModeW            = info.addressModeW;
    samplerInfo.mipLodBias              = info.mipmapLodBias;
    samplerInfo.anisotropyEnable        = info.useAnisotropy;
    samplerInfo.maxAnisotropy           = info.maxAnisotropy;
    samplerInfo.compareEnable           = info.compareToDepth;
    samplerInfo.compareOp               = info.compareOp;
    samplerInfo.minLod                  = info.mipmapLodMin;
    samplerInfo.maxLod                  = info.mipmapLodMax;
    samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = info.usePixelCoord;

    if (samplerInfo.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || samplerInfo.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || samplerInfo.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
      samplerInfo.borderColor = getBorderColor(info.compareToDepth, info.borderColor);
    
    if (m_vkd->vkCreateSampler(m_vkd->device(),
        &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
      throw DxvkError("DxvkSampler::DxvkSampler: Failed to create sampler");
  }
  
  
  DxvkSampler::~DxvkSampler() {
    m_vkd->vkDestroySampler(
      m_vkd->device(), m_sampler, nullptr);
  }


  VkBorderColor DxvkSampler::getBorderColor(bool depthCompare, VkClearColorValue borderColor) const {
    static const std::array<std::pair<VkClearColorValue, VkBorderColor>, 3> s_borderColors = {{
      { { { 0.0f, 0.0f, 0.0f, 0.0f } }, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
      { { { 0.0f, 0.0f, 0.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
      { { { 1.0f, 1.0f, 1.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
    }};

    if (depthCompare) {
      // If we are a depth compare sampler:
      // Replicate the first index, only R matters.
      for (uint32_t i = 1; i < 4; i++)
        borderColor.float32[i] = borderColor.float32[0];
    }

    for (const auto& e : s_borderColors) {
      if (!std::memcmp(&e.first, &borderColor, sizeof(VkClearColorValue)))
        return e.second;
    }

    Logger::warn(str::format(
      "DXVK: No matching border color found for (",
      borderColor.float32[0], ",", borderColor.float32[1], ",",
      borderColor.float32[2], ",", borderColor.float32[3], ")"));
    return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  }
  
}
