#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11RasterizerState : public D3D11DeviceChild<ID3D11RasterizerState> {
    
  public:
    
    using DescType = D3D11_RASTERIZER_DESC;
    
    D3D11RasterizerState(
            D3D11Device*                    device,
      const D3D11_RASTERIZER_DESC&          desc);
    ~D3D11RasterizerState();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_RASTERIZER_DESC* pDesc) final;
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx);
    
  private:
    
    D3D11Device* const    m_device;
    D3D11_RASTERIZER_DESC m_desc;
    DxvkRasterizerState   m_state;
    
  };
  
}
