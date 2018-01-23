#include <cstring>

#include "dxvk_data.h"

namespace dxvk {
  
  DxvkDataBuffer:: DxvkDataBuffer() { }
  DxvkDataBuffer::~DxvkDataBuffer() { }
  
  DxvkDataBuffer::DxvkDataBuffer(size_t size) {
    m_data.resize(size);
  }
  
  
  DxvkDataSlice DxvkDataBuffer::alloc(size_t n) {
    const size_t offset = m_offset;
    
    if (offset + n <= m_data.size()) {
      m_offset += align(n, CACHE_LINE_SIZE);
      return DxvkDataSlice(this, offset, n);
    } return DxvkDataSlice();
  }
  
}