#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  
  class D3D11Texture2D : public D3D11DeviceChild<ID3D11Texture2D> {
    
  public:
    
    D3D11Texture2D(
            D3D11Device*                device,
            IDXGIImageResourcePrivate*  resource,
            DxgiFormatMode              formatMode,
      const D3D11_TEXTURE2D_DESC&       desc);
    ~D3D11Texture2D();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_TEXTURE2D_DESC *pDesc) final;
    
    DxgiFormatMode GetFormatMode() const {
      return m_formatMode;
    }
    
    Rc<DxvkImage> GetDXVKImage() const {
      return m_resource->GetDXVKImage();
    }
    
  private:
    
    Com<D3D11Device>                m_device;
    Com<IDXGIImageResourcePrivate>  m_resource;
    DxgiFormatMode                  m_formatMode;
    D3D11_TEXTURE2D_DESC            m_desc;
    
  };
  
}
