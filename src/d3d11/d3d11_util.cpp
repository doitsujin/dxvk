#include "d3d11_util.h"

namespace dxvk {
  
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
  
}