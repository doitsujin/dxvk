#pragma once

#include <cstdint>

#include "dxso_common.h"
#include "dxso_decoder.h"

namespace dxvk {

  enum class DxsoBindingType : uint32_t {
    ConstantBuffer,
    ColorImage,
    DepthImage // <-- We use whatever one is bound to determine whether an image should be 'shadow' sampled or not.
  };

  enum DxsoConstantBuffers : uint32_t {
    VSConstantBuffer = 0,
    VSClipPlanes     = 1,
    VSFixedFunction  = 2,
    VSVertexBlendData = 3,
    VSCount,

    PSConstantBuffer = 0,
    PSFixedFunction  = 1,
    PSShared         = 2,
    PSCount
  };

  uint32_t computeResourceSlotId(
          DxsoProgramType shaderStage,
          DxsoBindingType bindingType,
          uint32_t        bindingIndex);

  uint32_t getSWVPBufferSlot();

  uint32_t RegisterLinkerSlot(DxsoSemantic semantic);

}