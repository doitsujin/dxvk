#include "dxso_module.h"

#include "dxso_code.h"
#include "dxso_compiler.h"

namespace dxvk {

  DxsoModule::DxsoModule(DxsoReader& reader)
    : m_header{ reader } {
    m_code = new DxsoCode{ reader };
  }

  Rc<DxvkShader> DxsoModule::compile(
    const DxsoModuleInfo& moduleInfo,
    const std::string&    fileName) {
    if (m_code == nullptr)
      throw DxvkError("DxsoModule::compile: no code");

    DxsoCompiler compiler(
      fileName, moduleInfo,
      m_header.info() );

    this->runCompiler(compiler, m_code->slice());

    m_decls = compiler.getDeclarations();

    return compiler.finalize();
  }

  void DxsoModule::runCompiler(
          DxsoCompiler&       compiler,
          DxsoCodeSlice       slice) const {
    DxsoDecodeContext decoder(m_header.info());

    while (!slice.atEnd()) {
      decoder.decodeInstruction(slice);

      compiler.processInstruction(
        decoder.getInstructionContext());
    }
  }

}