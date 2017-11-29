#pragma once

#include <dxgi_device.h>

#include "d3d11_include.h"

#include "../dxgi/dxgi_interfaces.h"
#include "../dxgi/dxgi_resource.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11PresentDevice : public ComObject<IDXGIPresentDevicePrivate> {
    
  public:
    
    D3D11PresentDevice();
    ~D3D11PresentDevice();
    
    HRESULT QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    HRESULT WrapSwapChainBackBuffer(
            IDXGIImageResourcePrivate*  pResource,
      const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
            IUnknown**                  ppInterface) final;
    
    HRESULT FlushRenderingCommands() final;
    
    HRESULT GetDevice(
            REFGUID                 riid,
            void**                  ppvDevice) final;
    
    void SetDeviceLayer(D3D11Device* pDevice) {
      m_device = pDevice;
    }
    
  private:
    
    D3D11Device* m_device = nullptr;
    
  };
  
}
