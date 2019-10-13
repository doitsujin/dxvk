#pragma once

#include "dxgi_include.h"

#include "../util/com/com_private_data.h"

namespace dxvk {
  
  template<typename Base>
  class DxgiObject : public ComObject<Base> {
    
  public:
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID       Name,
            UINT*         pDataSize,
            void*         pData) final {
      return m_privateData.getData(
        Name, pDataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID       Name,
            UINT          DataSize,
      const void*         pData) final {
      return m_privateData.setData(
        Name, DataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID       Name,
      const IUnknown*     pUnknown) final {
      return m_privateData.setInterface(
        Name, pUnknown);
    }
    
  private:
    
    ComPrivateData m_privateData;
    
  };
  
}
