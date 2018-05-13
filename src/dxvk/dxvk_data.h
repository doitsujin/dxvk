#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  class DxvkDataSlice;
  
  /**
   * \brief Data buffer
   * 
   * Provides a fixed-size buffer with a linear memory
   * allocator for arbitrary data. Can be used to copy
   * data to or from resources. Note that allocations
   * will be aligned to a cache line boundary.
   */
  class DxvkDataBuffer : public RcObject {
    friend class DxvkDataSlice;
  public:
    
    DxvkDataBuffer();
    DxvkDataBuffer(size_t size);
    ~DxvkDataBuffer();
    
    /**
     * \brief Allocates a slice
     * 
     * If the desired slice length is larger than the
     * number of bytes left in the buffer, this will
     * fail and the returned slice points to \c nullptr.
     * \param [in] n Number of bytes to allocate
     * \returns The slice, or an empty slice on failure
     */
    DxvkDataSlice alloc(size_t n);
    
  private:
    
    char*   m_data   = nullptr;
    size_t  m_size   = 0;
    size_t  m_offset = 0;
    
  };
  
  
  /**
   * \brief Data buffer slice
   * 
   * A slice of a \ref DxvkDataBuffer which stores
   * a strong reference to the backing buffer object.
   */
  class DxvkDataSlice {
    
  public:
    
    DxvkDataSlice() { }
    DxvkDataSlice(
      const Rc<DxvkDataBuffer>& buffer,
            size_t              offset,
            size_t              length)
    : m_buffer(buffer),
      m_offset(offset),
      m_length(length) { }
    
    void* ptr() const {
      return m_buffer != nullptr
        ? m_buffer->m_data + m_offset
        : nullptr;
    }
    
    size_t offset() const { return m_offset; }
    size_t length() const { return m_length; }
    
  private:
    
    Rc<DxvkDataBuffer> m_buffer;
    size_t             m_offset = 0;
    size_t             m_length = 0;
    
  };
  
  
}