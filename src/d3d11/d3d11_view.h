#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11RenderTargetView : public D3D11DeviceChild<ID3D11RenderTargetView> {
    
  public:
    
    D3D11RenderTargetView(
            D3D11Device*                    device,
            ID3D11Resource*                 resource,
      const D3D11_RENDER_TARGET_VIEW_DESC&  desc,
            Rc<DxvkImageView>               view);
    ~D3D11RenderTargetView();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    void GetResource(
            ID3D11Resource **ppResource) final;
    
    void GetDesc(
            D3D11_RENDER_TARGET_VIEW_DESC* pDesc) final;
    
    Rc<DxvkImageView> GetDXVKImageView();
    
  private:
    
    Com<D3D11Device>              m_device;
    Com<ID3D11Resource>           m_resource;
    
    D3D11_RENDER_TARGET_VIEW_DESC m_desc;
    Rc<DxvkImageView>             m_view;
    
  };
  
}
