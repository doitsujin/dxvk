#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_blend.h"

#include "d3d11_device_child.h"
#include "d3d11_util.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11BlendState : public D3D11StateObject<ID3D11BlendState1> {
    
  public:
    
    using DescType = D3D11_BLEND_DESC1;
    
    D3D11BlendState(
            D3D11Device*       device,
      const D3D11_BLEND_DESC1& desc);
    ~D3D11BlendState();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_BLEND_DESC* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc1(
            D3D11_BLEND_DESC1* pDesc) final;
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx,
            UINT              sampleMask) const;
    
    D3D10BlendState* GetD3D10Iface() {
      return &m_d3d10;
    }
    
    static D3D11_BLEND_DESC1 PromoteDesc(
      const D3D11_BLEND_DESC*   pSrcDesc);
    
    static HRESULT NormalizeDesc(
            D3D11_BLEND_DESC1*  pDesc);

  private:
    
    D3D11_BLEND_DESC1             m_desc;
    
    std::array<DxvkBlendMode, 8>  m_blendModes;
    DxvkMultisampleState          m_msState;
    DxvkLogicOpState              m_loState;

    D3D10BlendState               m_d3d10;
    
    static DxvkBlendMode DecodeBlendMode(
      const D3D11_RENDER_TARGET_BLEND_DESC1& BlendDesc);
    
    static VkBlendFactor DecodeBlendFactor(
            D3D11_BLEND BlendFactor,
            bool        IsAlpha);
    
    static VkBlendOp DecodeBlendOp(
            D3D11_BLEND_OP BlendOp);
    
    static VkLogicOp DecodeLogicOp(
            D3D11_LOGIC_OP LogicOp);

    static bool ValidateBlendFactor(
            D3D11_BLEND    Blend);

    static bool ValidateBlendFactorAlpha(
            D3D11_BLEND    BlendAlpha);

    static bool ValidateBlendOp(
            D3D11_BLEND_OP BlendOp);

    static bool ValidateLogicOp(
            D3D11_LOGIC_OP LogicOp);

    static bool ValidateBlendOperations(
            D3D11_BLEND    SrcBlend,
            D3D11_BLEND    SrcBlendAlpha,
            D3D11_BLEND    SestBlend,
            D3D11_BLEND    DestBlendAlpha,
            D3D11_BLEND_OP BlendOp,
            D3D11_BLEND_OP BlendOpAlpha);
  };
  
}
