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

  struct DxsoDefinedConstant {
    uint32_t uboIdx;

    // Only float constants may be indexed.
    // So that's the only ones we care about putting in the UBO.
    float    float32[4];
  };

  using DxsoDefinedConstants = std::vector<DxsoDefinedConstant>;

  struct DxsoShaderMetaInfo {
    bool needsConstantCopies = false;
    uint32_t maxConstIndexF = 0;
    uint32_t maxConstIndexI = 0;
    uint32_t maxConstIndexB = 0;

    uint32_t boolConstantMask = 0;
  };

}