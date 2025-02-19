#pragma once

#include "dxso_enums.h"

namespace dxvk {

  constexpr uint32_t InvalidOpcodeLength = std::numeric_limits<uint32_t>::max();

  uint32_t DxsoGetDefaultOpcodeLength(DxsoOpcode opcode);

}