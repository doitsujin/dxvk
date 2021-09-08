#pragma once

#include <cstdint>

#include "dxso_common.h"
#include "dxso_decoder.h"

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
    const uint32_t stageOffset = (VSCount + 4) * uint32_t(shaderStage);

    if (bindingType == DxsoBindingType::ConstantBuffer)
      return bindingIndex + stageOffset;
    else // if (bindingType == DxsoBindingType::Image)
      return bindingIndex + stageOffset + (shaderStage == DxsoProgramType::PixelShader ? PSCount : VSCount);
  }

  // TODO: Intergrate into compute resource slot ID/refactor all of this?
  constexpr uint32_t getSWVPBufferSlot() {
    return 27; // From last pixel shader slot, above.
  }

  uint32_t RegisterLinkerSlot(DxsoSemantic semantic);

}