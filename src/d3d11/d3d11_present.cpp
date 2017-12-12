#include "d3d11_device.h"
#include "d3d11_present.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11PresentDevice:: D3D11PresentDevice() { }
  D3D11PresentDevice::~D3D11PresentDevice() { }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIPresentDevicePrivate);
    return m_device->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::WrapSwapChainBackBuffer(
          IDXGIImageResourcePrivate*  pResource,
    const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
          IUnknown**                  ppInterface) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width              = pSwapChainDesc->BufferDesc.Width;
    desc.Height             = pSwapChainDesc->BufferDesc.Height;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = pSwapChainDesc->BufferDesc.Format;
    desc.SampleDesc         = pSwapChainDesc->SampleDesc;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;
    
    *ppInterface = ref(new D3D11Texture2D(
      m_device, pResource, desc));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::FlushRenderingCommands() {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_device->GetImmediateContext(&deviceContext);
    
    deviceContext->Flush();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::GetDevice(REFGUID riid, void** ppvDevice) {
    return m_device->QueryInterface(riid, ppvDevice);
  }
  
}
