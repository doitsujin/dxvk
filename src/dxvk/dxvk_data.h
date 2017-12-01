#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Data buffer
   * 
   * Stores immutable data. Used for temporary
   * copies of data that can be transferred to
   * or from DXVK resources.
   */
  class DxvkDataBuffer : public RcObject {
    
  public:
    
    DxvkDataBuffer();
    DxvkDataBuffer(
      const void*   data,
            size_t  size);
    ~DxvkDataBuffer();
    
    size_t size() const {
      return m_data.size();
    }
    
    void* data() {
      return m_data.data();
    }
    
    const void* data() const {
      return m_data.data();
    }
    
  private:
    
    std::vector<char> m_data;
    
  };
  
}