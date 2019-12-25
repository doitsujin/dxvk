#pragma once

#include "dxso_modinfo.h"
#include "dxso_decoder.h"

namespace dxvk {

  struct DxsoAnalysisInfo {
    uint32_t bytecodeByteLength;

    bool usesDerivatives = false;
    bool usesKill        = false;

    std::vector<DxsoInstructionContext> coissues;
  };

  class DxsoAnalyzer {

  public:

    DxsoAnalyzer(
            DxsoAnalysisInfo& analysis);

    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(
      const DxsoInstructionContext& ctx);

    void finalize(size_t tokenCount);

  private:

    DxsoAnalysisInfo* m_analysis = nullptr;

    DxsoOpcode m_parentOpcode;

  };

}