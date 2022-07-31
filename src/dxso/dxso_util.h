#pragma once

#include <cstdint>

#include "dxso_common.h"
#include "dxso_decoder.h"

#include "../d3d9/d3d9_caps.h"

namespace dxvk {

  enum class DxsoBindingType : uint32_t {
    ConstantBuffer,
    Image,
  };

  enum class DxsoConstantBufferType : uint32_t {
    Float,
    Int,
    Bool
  };

  enum DxsoConstantBuffers : uint32_t {
    VSConstantBuffer = 0,
    VSFloatConstantBuffer = 0,
    VSIntConstantBuffer = 1,
    VSBoolConstantBuffer = 2,
    VSClipPlanes     = 3,
    VSFixedFunction  = 4,
    VSVertexBlendData = 5,
    VSCount,

    PSConstantBuffer = 0,
    PSFixedFunction  = 1,
    PSShared         = 2,
    PSCount
  };

  constexpr uint32_t computeResourceSlotId(
        DxsoProgramType shaderStage,
        DxsoBindingType bindingType,
        uint32_t        bindingIndex) {
    const uint32_t stageOffset = (DxsoConstantBuffers::VSCount + caps::MaxTexturesVS) * uint32_t(shaderStage);

    if (bindingType == DxsoBindingType::ConstantBuffer)
      return bindingIndex + stageOffset;
    else // if (bindingType == DxsoBindingType::Image)
      return bindingIndex + stageOffset + (shaderStage == DxsoProgramType::PixelShader ? DxsoConstantBuffers::PSCount : DxsoConstantBuffers::VSCount);
  }

  // TODO: Intergrate into compute resource slot ID/refactor all of this?
  constexpr uint32_t getSWVPBufferSlot() {
    return DxsoConstantBuffers::VSCount + caps::MaxTexturesVS + DxsoConstantBuffers::PSCount + caps::MaxTexturesPS + 1; // From last pixel shader slot, above.
  }

  constexpr uint32_t getSpecConstantBufferSlot() {
    return getSWVPBufferSlot() + 1;
  }

  uint32_t RegisterLinkerSlot(DxsoSemantic semantic);

}