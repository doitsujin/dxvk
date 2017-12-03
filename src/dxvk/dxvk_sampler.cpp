#include "dxvk_sampler.h"

namespace dxvk {
    
  DxvkSampler::DxvkSampler(
    const Rc<vk::DeviceFn>&       vkd,
    const DxvkSamplerCreateInfo&  info)
  : m_vkd(vkd), m_info(info) {
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
    samplerInfo.borderColor             = info.borderColor;
    samplerInfo.unnormalizedCoordinates = info.usePixelCoord;
    
    if (m_vkd->vkCreateSampler(m_vkd->device(),
        &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
      throw DxvkError("DxvkSampler::DxvkSampler: Failed to create sampler");
  }
  
  
  DxvkSampler::~DxvkSampler() {
    m_vkd->vkDestroySampler(
      m_vkd->device(), m_sampler, nullptr);
  }
  
}
