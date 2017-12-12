#pragma once

#include "../dxgi/dxgi_device.h"
#include "../dxgi/dxgi_interfaces.h"
#include "../dxgi/dxgi_resource.h"

#include "d3d11_include.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11PresentDevice : public ComObject<IDXGIPresentDevicePrivate> {
    
  public:
    
    D3D11PresentDevice();
    ~D3D11PresentDevice();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE WrapSwapChainBackBuffer(
            IDXGIImageResourcePrivate*  pResource,
      const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
            IUnknown**                  ppInterface) final;
    
    HRESULT STDMETHODCALLTYPE FlushRenderingCommands() final;
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFGUID                 riid,
            void**                  ppvDevice) final;
    
    void SetDeviceLayer(D3D11Device* pDevice) {
      m_device = pDevice;
    }
    
  private:
    
    D3D11Device* m_device = nullptr;
    
  };
  
}
