#include "dxso_module.h"

#include "dxso_code.h"
#include "dxso_compiler.h"

#include <memory>

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
    const DxsoModuleInfo&     moduleInfo,
    const std::string&        fileName,
    const DxsoAnalysisInfo&   analysis,
    const D3D9ConstantLayout& layout) {
    auto compiler = std::make_unique<DxsoCompiler>(
      fileName, moduleInfo,
      m_header.info(), analysis,
      layout);

    this->runCompiler(*compiler, m_code.iter());
    m_isgn = compiler->isgn();

    m_meta            = compiler->meta();
    m_constants       = compiler->constants();
    m_maxDefinedConst = compiler->maxDefinedConstant();
    m_usedSamplers    = compiler->usedSamplers();

    compiler->finalize();

    // SM 1 doesn't have explicit output registers and uses R0 instead.
    // The shader compiler emits the C0 write in finalize, so we have to get the rt mask
    // after that.
    m_usedRTs = compiler->usedRTs();

    return compiler->compile();
  }

  void DxsoModule::runAnalyzer(
          DxsoAnalyzer&       analyzer,
          DxsoCodeIter        iter) const {
    DxsoCodeIter start = iter;

    DxsoDecodeContext decoder(m_header.info());

    while (decoder.decodeInstruction(iter))
      analyzer.processInstruction(
        decoder.getInstructionContext());

    size_t tokenCount = size_t(iter.ptrAt(0) - start.ptrAt(0));

    // We need to account for the header token in the bytecode size...

    // At this point, start is offset by the header due to us this being
    // a *code* iterator, and not the general reader class.
    // [start token] ^(start caret)^ [frog rendering code] [end token] ^(end caret)^
    // where the tokenCount above is inbetween the start and end carets.

    // We need to account for this otherwise it will show up as us not
    // accounting for the *end* token in GetFunction due to the total size being
    // offset by -1.
    // [start token] [frog rendering code] (end of tokenCount) [end token]
    tokenCount += 1;

    analyzer.finalize(tokenCount);
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