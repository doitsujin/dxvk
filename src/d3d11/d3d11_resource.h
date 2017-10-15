#pragma once

#include "d3d11_device_child.h"

namespace dxvk {
  
  template<typename Base>
  class D3D11Resource : public D3D11DeviceChild<Base> {
    
  public:
    
    UINT GetEvictionPriority() final {
      return m_evictionPriority;
    }
    
    void SetEvictionPriority(UINT EvictionPriority) final {
      m_evictionPriority = EvictionPriority;
    }
    
  private:
    
    UINT m_evictionPriority = 0;
    
  };
  
}
