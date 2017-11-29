#pragma once

#include <dxvk_device.h>

#include "d3d11_resource.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11Texture2D : public D3D11Resource<ID3D11Texture2D> {
    
  public:
    
    D3D11Texture2D(
            D3D11Device*          device,
      const D3D11_TEXTURE2D_DESC& desc,
      const Rc<DxvkImage>&        image);
    ~D3D11Texture2D();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    void GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    void GetDesc(
            D3D11_TEXTURE2D_DESC *pDesc) final;
    
  private:
    
    Com<D3D11Device>      m_device;
    
    D3D11_TEXTURE2D_DESC  m_desc;
    Rc<DxvkImage>         m_image;
    
  };
  
}
