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
  
  
  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC mode) {
    switch (mode) {
      case D3D11_COMPARISON_NEVER:
        return VK_COMPARE_OP_NEVER;
        
      case D3D11_COMPARISON_LESS:
        return VK_COMPARE_OP_LESS;
        
      case D3D11_COMPARISON_EQUAL:
        return VK_COMPARE_OP_EQUAL;
        
      case D3D11_COMPARISON_LESS_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
        
      case D3D11_COMPARISON_GREATER:
        return VK_COMPARE_OP_GREATER;
        
      case D3D11_COMPARISON_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
        
      case D3D11_COMPARISON_GREATER_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
        
      case D3D11_COMPARISON_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
        
      default:
        Logger::err(str::format("D3D11: Unsupported compare op: ", mode));
        return VK_COMPARE_OP_ALWAYS;
    }
  }
  
  
  VkMemoryPropertyFlags GetMemoryFlagsForUsage(D3D11_USAGE usage) {
    VkMemoryPropertyFlags memoryFlags = 0;
    
    switch (usage) {
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