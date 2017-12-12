#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  
  class D3D11Buffer : public D3D11DeviceChild<ID3D11Buffer> {
    
  public:
    
    D3D11Buffer(
            D3D11Device*                device,
            IDXGIBufferResourcePrivate* resource,
      const D3D11_BUFFER_DESC&          desc);
    ~D3D11Buffer();
    
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
            D3D11_BUFFER_DESC *pDesc) final;
    
    Rc<DxvkBuffer> GetDXVKBuffer();
    
  private:
    
    Com<D3D11Device>                m_device;
    Com<IDXGIBufferResourcePrivate> m_resource;
    D3D11_BUFFER_DESC               m_desc;
    
  };
  
}
