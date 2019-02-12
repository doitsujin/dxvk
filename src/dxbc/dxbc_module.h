#pragma once

#include "../dxvk/dxvk_shader.h"

#include "dxbc_chunk_isgn.h"
#include "dxbc_chunk_shex.h"
#include "dxbc_header.h"
#include "dxbc_modinfo.h"
#include "dxbc_reader.h"

// References used for figuring out DXBC:
// - https://github.com/tgjones/slimshader-cpp
// - Wine

namespace dxvk {
  
  class DxbcAnalyzer;
  class DxbcCompiler;
  
  /**
   * \brief DXBC shader module
   * 
   * Reads the DXBC byte code and extracts information
   * about the resource bindings and the instruction
   * stream. A module can then be compiled to SPIR-V.
   */
  class DxbcModule {
    
  public:
    
    DxbcModule(DxbcReader& reader);
    ~DxbcModule();
    
    /**
     * \brief Shader type
     * \returns Shader type
     */
    DxbcProgramInfo programInfo() const {
      return m_shexChunk->programInfo();
    }
    
    /**
     * \brief Input and output signature chunks
     * 
     * Parts of the D3D11 API need access to the
     * input or output signature of the shader.
     */
    Rc<DxbcIsgn> isgn() const { return m_isgnChunk; }
    Rc<DxbcIsgn> osgn() const { return m_osgnChunk; }
    
    /**
     * \brief Compiles DXBC shader to SPIR-V module
     * 
     * \param [in] moduleInfo DXBC module info
     * \param [in] fileName File name, will be added to
     *        the compiled SPIR-V for debugging purposes.
     * \returns The compiled shader object
     */
    Rc<DxvkShader> compile(
      const DxbcModuleInfo& moduleInfo,
      const std::string&    fileName) const;
    
    /**
     * \brief Compiles a pass-through geometry shader
     *
     * Applications can pass a vertex shader to create
     * a geometry shader with stream output. In this
     * case, we have to create a passthrough geometry
     * shader, which operates in point to point mode.
     * \param [in] moduleInfo DXBC module info
     * \param [in] fileName SPIR-V shader name
     */
    Rc<DxvkShader> compilePassthroughShader(
      const DxbcModuleInfo& moduleInfo,
      const std::string&    fileName) const;
    
  private:
    
    DxbcHeader   m_header;
    
    Rc<DxbcIsgn> m_isgnChunk;
    Rc<DxbcIsgn> m_osgnChunk;
    Rc<DxbcIsgn> m_psgnChunk;
    Rc<DxbcShex> m_shexChunk;
    
    void runAnalyzer(
            DxbcAnalyzer&       analyzer,
            DxbcCodeSlice       slice) const;
    
    void runCompiler(
            DxbcCompiler&       compiler,
            DxbcCodeSlice       slice) const;
    
  };
  
}
