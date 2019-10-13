#pragma once

#include "d3d11_include.h"

#include "../util/com/com_private_data.h"

namespace dxvk {
  
  template<typename Base, template<class> class Wrapper = ComObject>
  class D3D11DeviceChild : public Wrapper<Base> {
    
  public:
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID guid,
            UINT    *pDataSize,
            void    *pData) final {
      return m_privateData.getData(
        guid, pDataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID guid,
            UINT    DataSize,
      const void    *pData) final {
      return m_privateData.setData(
        guid, DataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID  guid,
      const IUnknown *pUnknown) final {
      return m_privateData.setInterface(
        guid, pUnknown);
    }
    
  private:
    
    ComPrivateData m_privateData;
    
  };
  
}
