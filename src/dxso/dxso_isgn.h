#pragma once

#include "dxso_decoder.h"

namespace dxvk {

  struct DxsoIsgnEntry {
    uint32_t     regNumber = 0;
    uint32_t     slot      = 0;
    DxsoSemantic semantic  = DxsoSemantic{ DxsoUsage::Position, 0 };
    DxsoRegMask  mask      = IdentityWriteMask;
    bool         centroid  = false;
  };

  struct DxsoIsgn {
    std::array<
      DxsoIsgnEntry,
      2 * DxsoMaxInterfaceRegs> elems;
    uint32_t elemCount = 0;
  };

}