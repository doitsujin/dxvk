#include "d3d11_device.h"
#include "d3d11_context_imm.h"
#include "d3d11_present.h"

namespace dxvk {
  
  HRESULT STDMETHODCALLTYPE D3D11VkBackBuffer::QueryInterface(REFIID riid, void** ppvObject) {
    return m_texture->QueryInterface(riid, ppvObject);
  }
  
  
  Rc<DxvkImage> D3D11VkBackBuffer::GetDXVKImage() {
    return m_texture->GetCommonTexture()->GetImage();
  }
  
  
  D3D11Presenter:: D3D11Presenter(
            IDXGIObject*  pContainer,
            ID3D11Device* pDevice)
  : m_container(pContainer), m_device(pDevice) {
    
  }
  
  
  D3D11Presenter::~D3D11Presenter() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11Presenter::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11Presenter::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Presenter::QueryInterface(REFIID riid, void** ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Presenter::CreateSwapChainBackBuffer(
    const DXGI_SWAP_CHAIN_DESC1*      pSwapChainDesc,
          IDXGIVkBackBuffer**         ppInterface) {
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width              = std::max(pSwapChainDesc->Width,  1u);
    desc.Height             = std::max(pSwapChainDesc->Height, 1u);
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = pSwapChainDesc->Format;
    desc.SampleDesc         = pSwapChainDesc->SampleDesc;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;

    if (pSwapChainDesc->BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    
    try {
      *ppInterface = ref(new D3D11VkBackBuffer(
        new D3D11Texture2D(static_cast<D3D11Device*>(m_device), &desc)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Presenter::FlushRenderingCommands() {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_device->GetImmediateContext(&deviceContext);
    
    // The presentation code is run from the main rendering thread
    // rather than the command stream thread, so we synchronize.
    auto immediateContext = static_cast<D3D11ImmediateContext*>(deviceContext.ptr());
    immediateContext->Flush();
    immediateContext->SynchronizeCsThread();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Presenter::GetDevice(REFGUID riid, void** ppvDevice) {
    return m_device->QueryInterface(riid, ppvDevice);
  }
  
}
