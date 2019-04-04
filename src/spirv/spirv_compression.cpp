#include "spirv_compression.h"

namespace dxvk {

  SpirvCompressedBuffer::SpirvCompressedBuffer()
  : m_size(0) {

  }


  SpirvCompressedBuffer::SpirvCompressedBuffer(
    const SpirvCodeBuffer& code)
  : m_size(code.dwords()) {
    const uint32_t* data = code.data();

    // The compression works by eliminating leading null bytes
    // from DWORDs, exploiting that SPIR-V IDs are consecutive
    // integers that usually fall into the 16-bit range. For
    // each DWORD, a two-bit integer is stored which indicates
    // the number of bytes it takes in the compressed buffer.
    // This way, it can achieve a compression ratio of ~50%.
    m_mask.reserve((m_size + NumMaskWords - 1) / NumMaskWords);
    m_code.reserve((m_size + 1) / 2);

    uint64_t dstWord  = 0;
    uint32_t dstShift = 0;

    for (uint32_t i = 0; i < m_size; i += NumMaskWords) {
      uint64_t byteCounts = 0;

      for (uint32_t w = 0; w < NumMaskWords && i + w < m_size; w++) {
        uint64_t word = data[i + w];
        uint64_t bytes = 0;

        if      (word < (1 <<  8)) bytes = 0;
        else if (word < (1 << 16)) bytes = 1;
        else if (word < (1 << 24)) bytes = 2;
        else                       bytes = 3;

        byteCounts |= bytes << (2 * w);

        uint32_t bits = 8 * bytes + 8;
        uint32_t rem  = bit::pack(dstWord, dstShift, word, bits);

        if (unlikely(rem != 0)) {
          m_code.push_back(dstWord);

          dstWord  = 0;
          dstShift = 0;

          bit::pack(dstWord, dstShift, word >> (bits - rem), rem);
        }
      }

      m_mask.push_back(byteCounts);
    }

    if (dstShift)
      m_code.push_back(dstWord);

    m_mask.shrink_to_fit();
    m_code.shrink_to_fit();
  }

    
  SpirvCompressedBuffer::~SpirvCompressedBuffer() {

  }


  SpirvCodeBuffer SpirvCompressedBuffer::decompress() const {
    SpirvCodeBuffer code(m_size);
    uint32_t* data = code.data();

    if (m_size == 0)
      return code;

    uint32_t maskIdx = 0;
    uint32_t codeIdx = 0;

    uint64_t srcWord  = m_code[codeIdx++];
    uint32_t srcShift = 0;

    for (uint32_t i = 0; i < m_size; i += NumMaskWords) {
      uint64_t srcMask = m_mask[maskIdx++];

      for (uint32_t w = 0; w < NumMaskWords && i + w < m_size; w++) {
        uint32_t bits = 8 * ((srcMask & 3) + 1);

        uint64_t word = 0;
        uint32_t rem = bit::unpack(word, srcWord, srcShift, bits);

        if (unlikely(rem != 0)) {
          srcWord  = m_code[codeIdx++];
          srcShift = 0;

          uint64_t tmp = 0;
          bit::unpack(tmp, srcWord, srcShift, rem);
          word |= tmp << (bits - rem);
        }

        data[i + w] = word;
        srcMask >>= 2;
      }
    }

    return code;
  }

}