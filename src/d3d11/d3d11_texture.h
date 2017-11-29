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
      const D3D11_TEXTURE2D_DESC&       desc);
    ~D3D11Texture2D();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    void GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT GetEvictionPriority() final;
    
    void SetEvictionPriority(UINT EvictionPriority) final;
    
    void GetDesc(
            D3D11_TEXTURE2D_DESC *pDesc) final;
    
  private:
    
    Com<D3D11Device>                m_device;
    Com<IDXGIImageResourcePrivate>  m_resource;
    D3D11_TEXTURE2D_DESC            m_desc;
    
  };
  
}
