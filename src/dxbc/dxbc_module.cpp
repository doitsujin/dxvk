#include "dxbc_analysis.h"
#include "dxbc_compiler.h"
#include "dxbc_module.h"

namespace dxvk {
  
  DxbcModule::DxbcModule(DxbcReader& reader)
  : m_header(reader) {
    for (uint32_t i = 0; i < m_header.numChunks(); i++) {
      
      // The chunk tag is stored at the beginning of each chunk
      auto chunkReader = reader.clone(m_header.chunkOffset(i));
      auto tag         = chunkReader.readTag();
      
      // The chunk size follows right after the four-character
      // code. This does not include the eight bytes that are
      // consumed by the FourCC and chunk length entry.
      auto chunkLength = chunkReader.readu32();
      
      chunkReader = chunkReader.clone(8);
      chunkReader = chunkReader.resize(chunkLength);
      
      if ((tag == "SHDR") || (tag == "SHEX"))
        m_shexChunk = new DxbcShex(chunkReader);
      
      if ((tag == "ISGN"))
        m_isgnChunk = new DxbcIsgn(chunkReader);
      
      if ((tag == "OSGN"))
        m_osgnChunk = new DxbcIsgn(chunkReader);
      
//       if ((tag == "OSG5"))
//         m_osgnChunk = new DxbcIsgn(chunkReader);
      
    }
  }
  
  
  DxbcModule::~DxbcModule() {
    
  }
  
  
  Rc<DxvkShader> DxbcModule::compile(const DxbcOptions& options) const {
    if (m_shexChunk == nullptr)
      throw DxvkError("DxbcModule::compile: No SHDR/SHEX chunk");
    
    DxbcAnalysisInfo analysisInfo;
    
    DxbcAnalyzer analyzer(options,
      m_shexChunk->version(),
      m_isgnChunk, m_osgnChunk,
      analysisInfo);
    
    DxbcCompiler compiler(options,
      m_shexChunk->version(),
      m_isgnChunk, m_osgnChunk,
      analysisInfo);
    
    this->runAnalyzer(analyzer, m_shexChunk->slice());
    this->runCompiler(compiler, m_shexChunk->slice());
    
    return compiler.finalize();
  }
  
  
  void DxbcModule::runAnalyzer(
          DxbcAnalyzer&       analyzer,
          DxbcCodeSlice       slice) const {
    DxbcDecodeContext decoder;
    
    while (!slice.atEnd()) {
      decoder.decodeInstruction(slice);
      
      analyzer.processInstruction(
        decoder.getInstruction());
    }
  }
  
  
  void DxbcModule::runCompiler(
          DxbcCompiler&       compiler,
          DxbcCodeSlice       slice) const {
    DxbcDecodeContext decoder;
    
    while (!slice.atEnd()) {
      decoder.decodeInstruction(slice);
      
      compiler.processInstruction(
        decoder.getInstruction());
    }
  }
  
}