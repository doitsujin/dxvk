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

    SpirvCompressedBuffer(const SpirvCodeBuffer& code);
    
    ~SpirvCompressedBuffer();
    
    SpirvCodeBuffer decompress() const;

  private:

    uint32_t             m_dwords;
    std::vector<uint8_t> m_code;

  };

}
