#pragma once

#include "dxso_enums.h"

namespace dxvk {

  constexpr uint32_t InvalidOpcodeLength = UINT32_MAX;

  uint32_t DxsoGetDefaultOpcodeLength(DxsoOpcode opcode);

}