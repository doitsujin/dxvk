#pragma once

#include "d3d11_device_child.h"

#include "../d3d10/d3d10_input_layout.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11InputLayout : public D3D11DeviceChild<ID3D11InputLayout> {
    
  public:
    
    D3D11InputLayout(
            D3D11Device*          pDevice,
            uint32_t              numAttributes,
      const DxvkVertexAttribute*  pAttributes,
            uint32_t              numBindings,
      const DxvkVertexBinding*    pBindings);
    
    ~D3D11InputLayout();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject) final;
    
    void BindToContext(
      const Rc<DxvkContext>&      ctx);
    
    bool Compare(
      const D3D11InputLayout*     pOther) const;
    
    D3D10InputLayout* GetD3D10Iface() {
      return &m_d3d10;
    }
    
  private:
    
    std::vector<DxvkVertexAttribute> m_attributes;
    std::vector<DxvkVertexBinding>   m_bindings;

    D3D10InputLayout m_d3d10;
    
  };
  
}
