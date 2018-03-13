#include "d3d11_device.h"
#include "d3d11_context_imm.h"
#include "d3d11_present.h"

namespace dxvk {
  
  HRESULT STDMETHODCALLTYPE D3D11PresentBackBuffer::QueryInterface(REFIID riid, void** ppvObject) {
    return m_texture->QueryInterface(riid, ppvObject);
  }
  
  
  Rc<DxvkImage> D3D11PresentBackBuffer::GetDXVKImage() {
    return m_texture->GetCommonTexture()->GetImage();
  }
  
  
  D3D11PresentDevice:: D3D11PresentDevice() { }
  D3D11PresentDevice::~D3D11PresentDevice() { }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIPresentDevicePrivate);
    return m_device->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::CreateSwapChainBackBuffer(
    const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
          IDXGIPresentBackBuffer**    ppInterface) {
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width              = pSwapChainDesc->BufferDesc.Width;
    desc.Height             = pSwapChainDesc->BufferDesc.Height;
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = pSwapChainDesc->BufferDesc.Format;
    desc.SampleDesc         = pSwapChainDesc->SampleDesc;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;
    
    if (pSwapChainDesc->BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    
    try {
      *ppInterface = ref(new D3D11PresentBackBuffer(
        new D3D11Texture2D(m_device, &desc)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::FlushRenderingCommands() {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_device->GetImmediateContext(&deviceContext);
    
    // The presentation code is run from the main rendering thread
    // rather than the command stream thread, so we synchronize.
    auto immediateContext = static_cast<D3D11ImmediateContext*>(deviceContext.ptr());
    immediateContext->Flush();
    immediateContext->SynchronizeCsThread();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11PresentDevice::GetDevice(REFGUID riid, void** ppvDevice) {
    return m_device->QueryInterface(riid, ppvDevice);
  }
  
}
