#pragma once

#include "d3d11_device_child.h"

#include "../d3d10/d3d10_input_layout.h"

namespace dxvk {
  
  class D3D11Device;

  struct alignas(16) D3D11VertexInput {
    std::array<DxvkVertexInput, MaxNumVertexAttributes + MaxNumVertexBindings> inputs;
  };


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

    uint32_t GetAttributeCount() const {
      return m_attributeCount;
    }

    uint32_t GetBindingCount() const {
      return m_bindingCount;
    }

    DxvkVertexInput GetInput(uint32_t Index) const {
      return m_input.inputs[Index];
    }

    bool Compare(
      const D3D11InputLayout*     pOther) const;
    
    D3D10InputLayout* GetD3D10Iface() {
      return &m_d3d10;
    }
    
  private:

    D3D11VertexInput m_input = { };

    uint32_t m_attributeCount = 0;
    uint32_t m_bindingCount = 0;

    D3D10InputLayout m_d3d10;
    
  };
  
}
