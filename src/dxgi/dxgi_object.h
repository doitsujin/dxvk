#pragma once

#include "dxgi_include.h"

#include "../util/com/com_private_data.h"

namespace dxvk {
  
  template<typename... Base>
  class DxgiObject : public ComObject<Base...> {
    
  public:
    
    HRESULT GetPrivateData(
            REFGUID Name,
            UINT    *pDataSize,
            void    *pData) final {
      return m_privateData.getData(
        Name, pDataSize, pData);
    }
    
    HRESULT SetPrivateData(
            REFGUID Name,
            UINT    DataSize,
      const void    *pData) final {
      return m_privateData.setData(
        Name, DataSize, pData);
    }
    
    HRESULT SetPrivateDataInterface(
            REFGUID  Name,
      const IUnknown *pUnknown) final {
      return m_privateData.setInterface(
        Name, pUnknown);
    }
    
  private:
    
    ComPrivateData m_privateData;
    
  };
  
}
