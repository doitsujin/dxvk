#pragma once

#include <vector>

#include "spirv_code_buffer.h"

namespace dxvk {

  /**
   * \brief Compressed SPIR-V code buffer
   *
   * Implements a fast in-memory compression
   * to keep memory footprint low.
   */
  class SpirvCompressedBuffer {

  public:

    SpirvCompressedBuffer();

    SpirvCompressedBuffer(SpirvCodeBuffer& code);
    
    ~SpirvCompressedBuffer();
    
    SpirvCodeBuffer decompress() const;

  private:

    size_t                m_size;
    std::vector<uint32_t> m_code;

    void encodeDword(uint32_t dw);

    uint32_t decodeDword(size_t& offset) const;

  };

}