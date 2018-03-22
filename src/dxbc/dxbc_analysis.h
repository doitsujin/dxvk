#pragma once

#include "dxbc_chunk_isgn.h"
#include "dxbc_decoder.h"
#include "dxbc_defs.h"
#include "dxbc_names.h"
#include "dxbc_options.h"
#include "dxbc_util.h"

namespace dxvk {
  
  struct DxbcUavInfo {
    bool accessTypedRead = false;
    bool accessAtomicOp  = false;
  };
  
  struct DxbcAnalysisInfo {
    std::array<DxbcUavInfo, 64> uavInfos;
  };
  
  /**
   * \brief DXBC shader analysis pass
   * 
   * Collects information about the shader itself
   * and the resources used by the shader, which
   * will later be used by the actual compiler.
   */
  class DxbcAnalyzer {
    
  public:
    
    DxbcAnalyzer(
      const DxbcOptions&        options,
      const DxbcProgramVersion& version);
    
    ~DxbcAnalyzer();
    
    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(
      const DxbcShaderInstruction&  ins);
    
  private:
    
    
    
  };
  
}