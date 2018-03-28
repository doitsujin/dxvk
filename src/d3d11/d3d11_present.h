#pragma once

#include "../dxgi/dxgi_device.h"
#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11VkBackBuffer : public ComObject<IDXGIVkBackBuffer> {
    
  public:
    
    D3D11VkBackBuffer(D3D11Texture2D* pTexture)
    : m_texture(pTexture) { }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    Rc<DxvkImage> GetDXVKImage() final;
    
  public:
    
    Com<D3D11Texture2D> m_texture;
    
  };
  
  
  class D3D11PresentDevice : public ComObject<IDXGIVkPresenter> {
    
  public:
    
    D3D11PresentDevice();
    ~D3D11PresentDevice();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainBackBuffer(
      const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
            IDXGIVkBackBuffer**    ppInterface) final;
    
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
