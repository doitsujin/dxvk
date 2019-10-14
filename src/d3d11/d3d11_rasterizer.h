#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_rasterizer.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11RasterizerState : public D3D11DeviceChild<ID3D11RasterizerState2, NoWrapper> {
    
  public:
    
    using DescType = D3D11_RASTERIZER_DESC2;
    
    D3D11RasterizerState(
            D3D11Device*                    device,
      const D3D11_RASTERIZER_DESC2&         desc);
    ~D3D11RasterizerState();

    ULONG STDMETHODCALLTYPE AddRef() final;

    ULONG STDMETHODCALLTYPE Release() final;

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_RASTERIZER_DESC* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc1(
            D3D11_RASTERIZER_DESC1* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc2(
            D3D11_RASTERIZER_DESC2* pDesc) final;
    
    const D3D11_RASTERIZER_DESC2* Desc() const {
      return &m_desc;
    }
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx);
    
    D3D10RasterizerState* GetD3D10Iface() {
      return &m_d3d10;
    }
    
    static D3D11_RASTERIZER_DESC2 PromoteDesc(
      const D3D11_RASTERIZER_DESC*  pDesc);
    
    static D3D11_RASTERIZER_DESC2 PromoteDesc(
      const D3D11_RASTERIZER_DESC1* pDesc);
    
    static HRESULT NormalizeDesc(
            D3D11_RASTERIZER_DESC2* pDesc);
    
  private:
    
    D3D11Device* const     m_device;
    D3D11_RASTERIZER_DESC2 m_desc;
    DxvkRasterizerState    m_state;
    DxvkDepthBias          m_depthBias;
    D3D10RasterizerState   m_d3d10;

    std::atomic<uint32_t> m_refCount = { 0u };
    
  };
  
}
