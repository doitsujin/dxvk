#pragma once

#include <cstdint>

namespace dxvk {

  class Direct3DCommonBuffer9;

  struct D3D9ConstantSets {
    constexpr static uint32_t SetSize    = (256 + 16 + 16) * sizeof(uint32_t);
    constexpr static uint32_t SetCount   = 8; // Magic number for now. May change later
    constexpr static uint32_t BufferSize = SetSize * SetCount;

    Direct3DCommonBuffer9* buffer   = nullptr;
    uint32_t               setIndex = SetCount - 1; // This will cause an initial discard.
    bool                   dirty    = true;
  };

}