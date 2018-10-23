#pragma once

#include "../dxgi/dxgi_device.h"
#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"
#include "d3d11_swapchain.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Present device
   * 
   * Wires up some swap chain related
   * functions to the D3D11 device.
   */
  class D3D11PresentDevice final : public IDXGIVkPresentDevice {
    
  public:
    
    D3D11PresentDevice(
            IDXGIObject*  pContainer,
            ID3D11Device* pDevice);
    ~D3D11PresentDevice();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
            HWND                    hWnd,
      const DXGI_SWAP_CHAIN_DESC1*  pDesc,
            IDXGIVkSwapChain**      ppSwapChain);
    
  private:
    
    IDXGIObject*  m_container;
    ID3D11Device* m_device;
    
  };
  
}
