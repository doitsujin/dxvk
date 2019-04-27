#pragma once

#include <cstdint>

#include "dxso_common.h"

namespace dxvk {

  enum class DxsoBindingType : uint32_t {
    ConstantBuffer,
    Image
  };

  enum DxsoConstantBuffers : uint32_t {
    VSConstantBuffer = 0,
    VSClipPlanes     = 1,
    VSFixedFunction  = 2,
    VSCount,

    PSConstantBuffer = 0,
    PSRenderStates   = 1,
    PSFixedFunction  = 2,
    PSCount
  };

  constexpr size_t DxsoMaxTempRegs      = 32;
  constexpr size_t DxsoMaxTextureRegs   = 10;
  constexpr size_t DxsoMaxInterfaceRegs = 16;
  constexpr size_t DxsoMaxOperandCount  = 8;

  uint32_t computeResourceSlotId(
          DxsoProgramType shaderStage,
          DxsoBindingType bindingType,
          uint32_t        bindingIndex);

}