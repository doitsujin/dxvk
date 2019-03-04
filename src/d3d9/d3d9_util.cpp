#include "d3d9_util.h"

namespace dxvk {

  HRESULT DecodeMultiSampleType(
        D3DMULTISAMPLE_TYPE       MultiSample,
        VkSampleCountFlagBits*    pCount) {
    VkSampleCountFlagBits flag;

    switch (MultiSample) {
    case D3DMULTISAMPLE_NONE:
    case D3DMULTISAMPLE_NONMASKABLE: flag = VK_SAMPLE_COUNT_1_BIT;  break;
    case D3DMULTISAMPLE_2_SAMPLES: flag = VK_SAMPLE_COUNT_2_BIT;  break;
    case D3DMULTISAMPLE_4_SAMPLES: flag = VK_SAMPLE_COUNT_4_BIT;  break;
    case D3DMULTISAMPLE_8_SAMPLES: flag = VK_SAMPLE_COUNT_8_BIT;  break;
    case D3DMULTISAMPLE_16_SAMPLES: flag = VK_SAMPLE_COUNT_16_BIT; break;
    default: return D3DERR_INVALIDCALL;
    }

    if (pCount != nullptr)
      *pCount = flag;

    return D3D_OK;
  }

  bool    ResourceBindable(
      DWORD                     Usage,
      D3DPOOL                   Pool) {
    return true;
  }

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format) {
    switch (Format) {
    case D3D9Format::D15S1:
      return VK_FORMAT_D16_UNORM_S8_UINT; // This should never happen!

    case D3D9Format::D16:
    case D3D9Format::D16_LOCKABLE:
    case D3D9Format::DF16:
      return VK_FORMAT_D16_UNORM;

    case D3D9Format::D24X8:
    case D3D9Format::DF24:
      return VK_FORMAT_X8_D24_UNORM_PACK32;

    case D3D9Format::D24X4S4:
    case D3D9Format::D24FS8:
    case D3D9Format::D24S8:
    case D3D9Format::INTZ:
      return VK_FORMAT_D24_UNORM_S8_UINT;

    case D3D9Format::D32:
    case D3D9Format::D32_LOCKABLE:
    case D3D9Format::D32F_LOCKABLE:
      return VK_FORMAT_D32_SFLOAT;

    case D3D9Format::S8_LOCKABLE:
      return VK_FORMAT_S8_UINT;

    default:
      return VK_FORMAT_UNDEFINED;
    }
  }

  VkFormatFeatureFlags GetImageFormatFeatures(DWORD Usage) {
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    if (Usage & D3DUSAGE_DEPTHSTENCIL)
      features |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Usage & D3DUSAGE_RENDERTARGET)
      features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

    return features;
  }

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage) {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (Usage & D3DUSAGE_DEPTHSTENCIL)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Usage & D3DUSAGE_RENDERTARGET)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return usage;
  }

  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          DWORD                   Usage) {
    VkMemoryPropertyFlags memoryFlags = 0;

    if (Usage & D3DUSAGE_DYNAMIC) {
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else {
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    return memoryFlags;
  }

}