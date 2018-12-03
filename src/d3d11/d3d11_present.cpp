#include "d3d11_device.h"
#include "d3d11_present.h"

namespace dxvk {
  
  D3D11PresentDevice::D3D11PresentDevice(
            D3D11DXGIDevice*  pContainer,
            D3D11Device*      pDevice)
  : m_container (pContainer),
    m_device    (pDevice) {
    
  }
  
  
  D3D11PresentDevice::~D3D11PresentDevice() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11PresentDevice::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11PresentDevice::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::QueryInterface(REFIID riid, void** ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::CreateSwapChainForHwnd(
          HWND                    hWnd,
    const DXGI_SWAP_CHAIN_DESC1*  pDesc,
          IDXGIVkSwapChain**      ppSwapChain) {
    InitReturnPtr(ppSwapChain);

    try {
      *ppSwapChain = ref(new D3D11SwapChain(
        m_container, m_device, hWnd, pDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
}
