#include "d3d11_device.h"
#include "d3d11_input_layout.h"

namespace dxvk {
  
  D3D11InputLayout::D3D11InputLayout(
          D3D11Device*                pDevice,
          uint32_t                    numAttributes,
    const DxvkVertexAttribute*        pAttributes,
          uint32_t                    numBindings,
    const DxvkVertexBinding*          pBindings)
  : m_device(pDevice) {
    m_attributes.resize(numAttributes);
    m_bindings.resize(numBindings);
    
    for (uint32_t i = 0; i < numAttributes; i++)
      m_attributes.at(i) = pAttributes[i];
    
    for (uint32_t i = 0; i < numBindings; i++)
      m_bindings.at(i) = pBindings[i];
  }
  
  
  D3D11InputLayout::~D3D11InputLayout() {
    
  }
  
  
  HRESULT D3D11InputLayout::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11InputLayout);
    
    Logger::warn("D3D11InputLayout::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11InputLayout::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void D3D11InputLayout::BindToContext(const Rc<DxvkContext>& ctx) {
    ctx->setInputLayout(
      m_attributes.size(),
      m_attributes.data(),
      m_bindings.size(),
      m_bindings.data());
  }
  
}
