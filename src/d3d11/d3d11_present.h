#pragma once

#include "../dxgi/dxgi_device.h"
#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11VkBackBuffer : public ComObject<IDXGIVkBackBuffer> {
    
  public:
    
    D3D11VkBackBuffer(D3D11Texture2D* pTexture);
    ~D3D11VkBackBuffer();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    Rc<DxvkImage> GetDXVKImage() final;
    
  public:
    
    D3D11Texture2D* m_texture;
    
  };
  
  
  /**
   * \brief Present device
   * 
   * Wires up some swap chain related
   * functions to the D3D11 device.
   */
  class D3D11Presenter final : public IDXGIVkPresenter {
    
  public:
    
    D3D11Presenter(
            IDXGIObject*  pContainer,
            ID3D11Device* pDevice);
    ~D3D11Presenter();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainBackBuffer(
      const DXGI_SWAP_CHAIN_DESC1*  pSwapChainDesc,
            IDXGIVkBackBuffer**     ppInterface);
    
    HRESULT STDMETHODCALLTYPE FlushRenderingCommands();
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFGUID                 riid,
            void**                  ppvDevice);
    
  private:
    
    IDXGIObject*  m_container;
    ID3D11Device* m_device;
    
  };
  
}
