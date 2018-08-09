#include "d3d11_util.h"

namespace dxvk {
  
  HRESULT DecodeSampleCount(UINT Count, VkSampleCountFlagBits* pCount) {
    VkSampleCountFlagBits flag;
    
    switch (Count) {
      case  1: flag = VK_SAMPLE_COUNT_1_BIT;  break;
      case  2: flag = VK_SAMPLE_COUNT_2_BIT;  break;
      case  4: flag = VK_SAMPLE_COUNT_4_BIT;  break;
      case  8: flag = VK_SAMPLE_COUNT_8_BIT;  break;
      case 16: flag = VK_SAMPLE_COUNT_16_BIT; break;
      default: return E_INVALIDARG;
    }
    
    if (pCount != nullptr) {
      *pCount = flag;
      return S_OK;
    } return S_FALSE;
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
      { 1.0f, 1.0f, 1.0f, 1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
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


  VkShaderStageFlagBits GetShaderStage(DxbcProgramType ProgramType) {
    switch (ProgramType) {
      case DxbcProgramType::VertexShader:   return VK_SHADER_STAGE_VERTEX_BIT;
      case DxbcProgramType::HullShader:     return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
      case DxbcProgramType::DomainShader:   return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
      case DxbcProgramType::GeometryShader: return VK_SHADER_STAGE_GEOMETRY_BIT;
      case DxbcProgramType::PixelShader:    return VK_SHADER_STAGE_FRAGMENT_BIT;
      case DxbcProgramType::ComputeShader:  return VK_SHADER_STAGE_COMPUTE_BIT;
      default:                              return VkShaderStageFlagBits(0);
    }
  }
  

  VkBufferUsageFlags GetBufferUsageFlags(UINT BindFlags) {
    VkBufferUsageFlags usage = 0;

    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    
    return 0;
  }
  

  VkImageUsageFlags GetImageUsageFlags(UINT BindFlags) {
    VkImageUsageFlags usage = 0;

    if (BindFlags & D3D11_BIND_DEPTH_STENCIL)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (BindFlags & D3D11_BIND_RENDER_TARGET)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    
    return usage;
  }


  VkFormatFeatureFlags GetBufferFormatFeatures(UINT BindFlags) {
    VkFormatFeatureFlags features = 0;

    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      features |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      features |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
    
    return features;
  }


  VkFormatFeatureFlags GetImageFormatFeatures(UINT BindFlags) {
    VkFormatFeatureFlags features = 0;

    if (BindFlags & D3D11_BIND_DEPTH_STENCIL)
      features |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (BindFlags & D3D11_BIND_RENDER_TARGET)
      features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      features |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    
    return features;
  }

}