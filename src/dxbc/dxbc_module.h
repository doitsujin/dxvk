#pragma once

#include "../dxvk/dxvk_shader.h"

#include "dxbc_chunk_isgn.h"
#include "dxbc_chunk_shex.h"
#include "dxbc_header.h"
#include "dxbc_reader.h"

// References used for figuring out DXBC:
// - https://github.com/tgjones/slimshader-cpp
// - Wine

namespace dxvk {
  
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
     * \brief Compiles DXBC shader to SPIR-V module
     * \returns The compiled DXVK shader object
     */
    SpirvCodeBuffer compile() const;
    
  private:
    
    DxbcHeader m_header;
    
    Rc<DxbcIsgn> m_isgnChunk;
    Rc<DxbcIsgn> m_osgnChunk;
    Rc<DxbcShex> m_shexChunk;
    
  };
  
}