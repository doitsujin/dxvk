#include "d3d11_util.h"

namespace dxvk {
  
  HRESULT GetSampleCount(UINT Count, VkSampleCountFlagBits* pCount) {
    switch (Count) {
      case  1: *pCount = VK_SAMPLE_COUNT_1_BIT;  return S_OK;
      case  2: *pCount = VK_SAMPLE_COUNT_2_BIT;  return S_OK;
      case  4: *pCount = VK_SAMPLE_COUNT_4_BIT;  return S_OK;
      case  8: *pCount = VK_SAMPLE_COUNT_8_BIT;  return S_OK;
      case 16: *pCount = VK_SAMPLE_COUNT_16_BIT; return S_OK;
    }
    
    return E_INVALIDARG;
  }
  
  
  VkSamplerAddressMode DecodeAddressMode(
          D3D11_TEXTURE_ADDRESS_MODE  mode) {
    switch (mode) {
      case D3D11_TEXTURE_ADDRESS_WRAP:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        
      case D3D11_TEXTURE_ADDRESS_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      
      case D3D11_TEXTURE_ADDRESS_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        
      case D3D11_TEXTURE_ADDRESS_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        
      case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
      
      default:
        Logger::err(str::format("D3D11: Unsupported address mode: ", mode));
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
  }
  
  
  VkBorderColor DecodeBorderColor(const FLOAT BorderColor[4]) {
    struct BorderColorEntry {
      float r, g, b, a;
      VkBorderColor bc;
    };
    
    // Vulkan only supports a very limited set of border colors
    const std::array<BorderColorEntry, 3> borderColorMap = {{
      { 0.0f, 0.0f, 0.0f, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
      { 0.0f, 0.0f, 0.0f, 1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
      { 1.0f, 1.0f, 1.0f, 1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
    }};
    
    for (const auto& e : borderColorMap) {
      if (e.r == BorderColor[0] && e.g == BorderColor[1]
       && e.b == BorderColor[2] && e.a == BorderColor[3])
        return e.bc;
    }
      
    Logger::warn(str::format(
      "D3D11Device: No matching border color found for (",
      BorderColor[0], ",", BorderColor[1], ",",
      BorderColor[2], ",", BorderColor[3], ")"));
    
    return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  }
  
  
  VkCompareOp DecodeCompareOp(D3D11_COMPARISON_FUNC Mode) {
    switch (Mode) {
      case D3D11_COMPARISON_NEVER:          return VK_COMPARE_OP_NEVER;
      case D3D11_COMPARISON_LESS:           return VK_COMPARE_OP_LESS;
      case D3D11_COMPARISON_EQUAL:          return VK_COMPARE_OP_EQUAL;
      case D3D11_COMPARISON_LESS_EQUAL:     return VK_COMPARE_OP_LESS_OR_EQUAL;
      case D3D11_COMPARISON_GREATER:        return VK_COMPARE_OP_GREATER;
      case D3D11_COMPARISON_NOT_EQUAL:      return VK_COMPARE_OP_NOT_EQUAL;
      case D3D11_COMPARISON_GREATER_EQUAL:  return VK_COMPARE_OP_GREATER_OR_EQUAL;
      case D3D11_COMPARISON_ALWAYS:         return VK_COMPARE_OP_ALWAYS;
    }
    
    if (Mode != 0)  // prevent log spamming when apps use ZeroMemory
      Logger::err(str::format("D3D11: Unsupported compare op: ", Mode));
    return VK_COMPARE_OP_NEVER;
  }
  
  
  VkMemoryPropertyFlags GetMemoryFlagsForUsage(D3D11_USAGE Usage) {
    VkMemoryPropertyFlags memoryFlags = 0;
    
    switch (Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
      
      case D3D11_USAGE_DYNAMIC:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
      
      case D3D11_USAGE_STAGING:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    }
    
    return memoryFlags;
  }
  
}