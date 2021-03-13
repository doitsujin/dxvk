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
  
  
  VkConservativeRasterizationModeEXT DecodeConservativeRasterizationMode(
          D3D11_CONSERVATIVE_RASTERIZATION_MODE Mode) {
    switch (Mode) {
      case D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF:
        return VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
      case D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON:
        return VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
    }

    Logger::err(str::format("D3D11: Unsupported conservative raster mode: ", Mode));
    return VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
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


  VkFormat GetPackedDepthStencilFormat(DXGI_FORMAT Format) {
    switch (Format) {
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
      
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
      
      default:
        return VK_FORMAT_UNDEFINED;
    }
  }

}