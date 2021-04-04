#include "dxvk_sampler.h"
#include "dxvk_device.h"

namespace dxvk {
    
  DxvkSampler::DxvkSampler(
          DxvkDevice*             device,
    const DxvkSamplerCreateInfo&  info)
  : m_vkd(device->vkd()) {
    VkSamplerCustomBorderColorCreateInfoEXT borderColorInfo;
    borderColorInfo.sType               = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
    borderColorInfo.pNext               = nullptr;
    borderColorInfo.customBorderColor   = info.borderColor;
    borderColorInfo.format              = VK_FORMAT_UNDEFINED;

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

    if (!device->features().core.features.samplerAnisotropy)
      samplerInfo.anisotropyEnable = VK_FALSE;

    if (samplerInfo.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || samplerInfo.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || samplerInfo.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
      samplerInfo.borderColor = getBorderColor(device, info);

    if (samplerInfo.borderColor == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT)
      samplerInfo.pNext = &borderColorInfo;

    if (m_vkd->vkCreateSampler(m_vkd->device(),
        &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
      throw DxvkError("DxvkSampler::DxvkSampler: Failed to create sampler");
  }
  
  
  DxvkSampler::~DxvkSampler() {
    m_vkd->vkDestroySampler(
      m_vkd->device(), m_sampler, nullptr);
  }


  VkBorderColor DxvkSampler::getBorderColor(const Rc<DxvkDevice>& device, const DxvkSamplerCreateInfo& info) {
    static const std::array<std::pair<VkClearColorValue, VkBorderColor>, 3> s_borderColors = {{
      { { { 0.0f, 0.0f, 0.0f, 0.0f } }, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
      { { { 0.0f, 0.0f, 0.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
      { { { 1.0f, 1.0f, 1.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
    }};

    // Ignore G/B/A components for shadow samplers
    size_t size = !info.compareToDepth
      ? sizeof(VkClearColorValue)
      : sizeof(float);

    for (const auto& e : s_borderColors) {
      if (!std::memcmp(&e.first, &info.borderColor, size))
        return e.second;
    }

    if (!device->features().extCustomBorderColor.customBorderColorWithoutFormat) {
      Logger::warn("DXVK: Custom border colors not supported");
      return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }

    return VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
  }

}
