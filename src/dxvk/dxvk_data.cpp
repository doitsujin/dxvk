#include <cstring>

#include "dxvk_data.h"

namespace dxvk {
  
  DxvkDataBuffer:: DxvkDataBuffer() { }
  DxvkDataBuffer::~DxvkDataBuffer() { }
  
  DxvkDataBuffer::DxvkDataBuffer(
          size_t  size) {
    m_data.resize(size);
  }
  
  DxvkDataBuffer::DxvkDataBuffer(
    const void*   data,
          size_t  size) {
    m_data.resize(size);
    std::memcpy(m_data.data(), data, size);
  }
  
}