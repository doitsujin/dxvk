#pragma once

#include "dxso_reader.h"
#include "dxso_code.h"
#include "dxso_header.h"
#include "dxso_ctab.h"

#include "dxso_decoder.h"
#include "dxso_analysis.h"

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
    Rc<DxvkShader> compile(
      const DxsoModuleInfo& moduleInfo,
      const std::string&    fileName);

    const std::array<DxsoDeclaration, 16>& getDecls() {
      return m_decls;
    }

  private:

    void runCompiler(
            DxsoCompiler&       compiler,
            DxsoCodeIter        iter) const;

    void runAnalyzer(
            DxsoAnalyzer&       analyzer,
            DxsoCodeIter        iter) const;

    DxsoHeader      m_header;
    Rc<DxsoCode>    m_code;

    std::array<DxsoDeclaration, 16> m_decls;

  };

}