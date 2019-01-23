#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_rasterizer.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11RasterizerState : public D3D11DeviceChild<ID3D11RasterizerState1> {
    
  public:
    
    using DescType = D3D11_RASTERIZER_DESC1;
    
    D3D11RasterizerState(
            D3D11Device*                    device,
      const D3D11_RASTERIZER_DESC1&         desc);
    ~D3D11RasterizerState();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_RASTERIZER_DESC* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc1(
            D3D11_RASTERIZER_DESC1* pDesc) final;
    
    const D3D11_RASTERIZER_DESC1* Desc() const {
      return &m_desc;
    }
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx);
    
    D3D10RasterizerState* GetD3D10Iface() {
      return &m_d3d10;
    }
    
    static D3D11_RASTERIZER_DESC1 DefaultDesc();
    
    static D3D11_RASTERIZER_DESC1 PromoteDesc(
      const D3D11_RASTERIZER_DESC*  pDesc);
    
    static HRESULT NormalizeDesc(
            D3D11_RASTERIZER_DESC1* pDesc);
    
  private:
    
    D3D11Device* const     m_device;
    D3D11_RASTERIZER_DESC1 m_desc;
    DxvkRasterizerState    m_state;
    DxvkDepthBias          m_depthBias;
    D3D10RasterizerState   m_d3d10;
    
  };
  
}
