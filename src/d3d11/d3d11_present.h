#pragma once

#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"
#include "d3d11_swapchain.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DXGIDevice;
  
  /**
   * \brief Present device
   * 
   * Wires up some swap chain related
   * functions to the D3D11 device.
   */
  class D3D11PresentDevice final : public IDXGIVkPresentDevice {
    
  public:
    
    D3D11PresentDevice(
            D3D11DXGIDevice*  pContainer,
            D3D11Device*      pDevice);
    
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
    
    D3D11DXGIDevice*  m_container;
    D3D11Device*      m_device;
    
  };
  
}
