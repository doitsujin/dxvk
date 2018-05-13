#include <cstring>

#include "dxvk_data.h"

namespace dxvk {
  
  DxvkDataBuffer:: DxvkDataBuffer() { }
  DxvkDataBuffer::DxvkDataBuffer(size_t size)
  : m_data(new char[size]), m_size(size) { }
  
  
  DxvkDataBuffer::~DxvkDataBuffer() {
    delete[] m_data;
  }
  
  
  DxvkDataSlice DxvkDataBuffer::alloc(size_t n) {
    const size_t offset = m_offset;
    
    if (offset + n <= m_size) {
      m_offset += align(n, CACHE_LINE_SIZE);
      return DxvkDataSlice(this, offset, n);
    } return DxvkDataSlice();
  }
  
}