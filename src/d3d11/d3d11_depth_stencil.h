#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_util.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11DepthStencilState : public D3D11DeviceChild<ID3D11DepthStencilState> {
    
  public:
    
    using DescType = D3D11_DEPTH_STENCIL_DESC;
    
    D3D11DepthStencilState(
            D3D11Device*              device,
      const D3D11_DEPTH_STENCIL_DESC& desc);
    ~D3D11DepthStencilState();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_DEPTH_STENCIL_DESC* pDesc) final;
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx);
    
    static D3D11_DEPTH_STENCIL_DESC DefaultDesc();
    
    static HRESULT NormalizeDesc(
            D3D11_DEPTH_STENCIL_DESC* pDesc);
    
  private:
    
    D3D11Device* const        m_device;
    D3D11_DEPTH_STENCIL_DESC  m_desc;
    DxvkDepthStencilState     m_state;
    
    VkStencilOpState DecodeStencilOpState(
      const D3D11_DEPTH_STENCILOP_DESC& StencilDesc,
      const D3D11_DEPTH_STENCIL_DESC&   Desc) const;
    
    VkStencilOp DecodeStencilOp(
            D3D11_STENCIL_OP            Op) const;
    
  };
  
}
