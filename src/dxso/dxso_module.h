#pragma once

#include "dxso_reader.h"
#include "dxso_code.h"
#include "dxso_header.h"
#include "dxso_ctab.h"

#include "dxso_isgn.h"
#include "dxso_analysis.h"

#include "../d3d9/d3d9_constant_layout.h"
#include "../d3d9/d3d9_shader_permutations.h"

#include <vector>

namespace dxvk {

  class DxsoCompiler;
  class DxsoCode;
  struct DxsoModuleInfo;

  /**
   * \brief DXSO shader module, a d3d9 shader object.
   */
  class DxsoModule {

  public:

    DxsoModule(DxsoReader& reader);

    const DxsoProgramInfo& info() {
      return m_header.info();
    }

    DxsoAnalysisInfo analyze();

    /**
     * \brief Compiles DXSO shader to SPIR-V module
     * 
     * \param [in] moduleInfo DXSO module info
     * \param [in] fileName File name, will be added to
     *        the compiled SPIR-V for debugging purposes.
     * \returns The compiled shader object
     */
    DxsoPermutations compile(
      const DxsoModuleInfo&     moduleInfo,
      const std::string&        fileName,
      const DxsoAnalysisInfo&   analysis,
      const D3D9ConstantLayout& layout);

    const DxsoIsgn& isgn() {
      return m_isgn;
    }

    const DxsoShaderMetaInfo& meta() { return m_meta; }

    const DxsoDefinedConstants& constants() { return m_constants; }

    uint32_t usedSamplers() { return m_usedSamplers; }

    uint32_t usedRTs() { return m_usedRTs; }

  private:

    void runCompiler(
            DxsoCompiler&       compiler,
            DxsoCodeIter        iter) const;

    void runAnalyzer(
            DxsoAnalyzer&       analyzer,
            DxsoCodeIter        iter) const;

    DxsoHeader      m_header;
    DxsoCode        m_code;

    DxsoIsgn        m_isgn;
    uint32_t        m_usedSamplers;
    uint32_t        m_usedRTs;

    DxsoShaderMetaInfo   m_meta;
    DxsoDefinedConstants m_constants;

  };

}