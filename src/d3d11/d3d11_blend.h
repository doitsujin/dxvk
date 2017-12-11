#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_util.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11BlendState : public D3D11DeviceChild<ID3D11BlendState> {
    
  public:
    
    using DescType = D3D11_BLEND_DESC;
    
    D3D11BlendState(
            D3D11Device*      device,
      const D3D11_BLEND_DESC& desc);
    ~D3D11BlendState();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    void GetDesc(
            D3D11_BLEND_DESC* pDesc) final;
    
    void BindToContext(
      const Rc<DxvkContext>&  ctx,
            UINT              sampleMask) const;
    
  private:
    
    D3D11Device* const            m_device;
    D3D11_BLEND_DESC              m_desc;
    
    std::array<DxvkBlendMode, 8>  m_blendModes;
    DxvkMultisampleState          m_msState;
    DxvkLogicOpState              m_loState;
    
    static DxvkBlendMode DecodeBlendMode(
      const D3D11_RENDER_TARGET_BLEND_DESC& blendDesc);
    
    static VkBlendFactor DecodeBlendFactor(
            D3D11_BLEND blendFactor,
            bool        isAlpha);
    
    static VkBlendOp DecodeBlendOp(
            D3D11_BLEND_OP blendOp);
    
  };
  
}
