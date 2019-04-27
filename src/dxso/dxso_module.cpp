#include "dxso_module.h"

#include "dxso_code.h"
#include "dxso_compiler.h"

namespace dxvk {

  DxsoModule::DxsoModule(DxsoReader& reader)
    : m_header( reader )
    , m_code  ( reader ) { }

  DxsoAnalysisInfo DxsoModule::analyze() {
    DxsoAnalysisInfo info;

    DxsoAnalyzer analyzer(info);

    this->runAnalyzer(analyzer, m_code.iter());

    return info;
  }

  Rc<DxvkShader> DxsoModule::compile(
    const DxsoModuleInfo&   moduleInfo,
    const std::string&      fileName,
    const DxsoAnalysisInfo& analysis) {
    DxsoCompiler compiler(
      fileName, moduleInfo,
      m_header.info(), analysis);

    this->runCompiler(compiler, m_code.iter());
    m_isgn = compiler.isgn();

    return compiler.finalize();
  }

  void DxsoModule::runAnalyzer(
          DxsoAnalyzer&       analyzer,
          DxsoCodeIter        iter) const {
    DxsoCodeIter start = iter;

    DxsoDecodeContext decoder(m_header.info());

    while (decoder.decodeInstruction(iter))
      analyzer.processInstruction(
        decoder.getInstructionContext());

    analyzer.finalize(
      size_t(iter.ptrAt(0) - start.ptrAt(0)));
  }

  void DxsoModule::runCompiler(
          DxsoCompiler&       compiler,
          DxsoCodeIter        iter) const {
    DxsoDecodeContext decoder(m_header.info());

    while (decoder.decodeInstruction(iter))
      compiler.processInstruction(
        decoder.getInstructionContext());
  }

}