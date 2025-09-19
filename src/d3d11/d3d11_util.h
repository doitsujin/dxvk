#pragma once

#include "../dxvk/dxvk_device.h"

#include "d3d11_include.h"

namespace dxvk {

  /**
   * \brief Shader binding mask
   *
   * Stores a bit masks of resource bindings
   * that are accessed by any given shader.
   */
  struct D3D11BindingMask {
    uint32_t cbvMask      = 0u;
    uint32_t samplerMask  = 0u;
    uint64_t uavMask      = 0u;
    std::array<uint64_t, 2> srvMask = { };

    void reset() {
      cbvMask = 0u;
      samplerMask = 0u;
      uavMask = 0u;
      srvMask = { };
    }

    bool empty() const {
      uint64_t mask = (uint64_t(cbvMask) | uint64_t(samplerMask) << 32u)
                    | (uavMask | srvMask[0] | srvMask[1]);
      return !mask;
    }

    void setCbv(uint32_t index) {
      cbvMask |= 1u << index;
    }

    void setSampler(uint32_t index) {
      samplerMask |= 1u << index;
    }

    void setUav(uint32_t index) {
      uavMask |= uint64_t(1u) << index;
    }

    void setSrv(uint32_t index) {
      uint32_t mask = index / 64u;
      uint32_t shift = index % 64u;

      srvMask[mask] |= uint64_t(1u) << shift;
    }

    D3D11BindingMask operator & (const D3D11BindingMask& other) const {
      D3D11BindingMask result = *this;
      result.cbvMask &= other.cbvMask;
      result.samplerMask &= other.samplerMask;
      result.uavMask &= other.uavMask;
      result.srvMask[0] &= other.srvMask[0];
      result.srvMask[1] &= other.srvMask[1];
      return result;
    }
  };


  template<typename T>
  UINT CompactSparseList(T* pData, UINT Mask) {
    uint32_t count = 0;
    
    for (uint32_t id : bit::BitMask(Mask))
      pData[count++] = pData[id];

    return count;
  }

  HRESULT DecodeSampleCount(
          UINT                      Count,
          VkSampleCountFlagBits*    pCount);
  
  VkSamplerAddressMode DecodeAddressMode(
          D3D11_TEXTURE_ADDRESS_MODE  mode);

  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC     Mode);

  VkSamplerReductionMode DecodeReductionMode(
          UINT                      Filter);

  VkConservativeRasterizationModeEXT DecodeConservativeRasterizationMode(
          D3D11_CONSERVATIVE_RASTERIZATION_MODE Mode);

  VkFormatFeatureFlags2 GetBufferFormatFeatures(
          UINT                      BindFlags);

  VkFormatFeatureFlags2 GetImageFormatFeatures(
          UINT                      BindFlags);
  
  VkFormat GetPackedDepthStencilFormat(
          DXGI_FORMAT               Format);

  BOOL IsMinMaxFilter(D3D11_FILTER Filter);

}
