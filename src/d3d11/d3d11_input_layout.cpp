#include "d3d11_device.h"
#include "d3d11_input_layout.h"

namespace dxvk {
  
  D3D11InputLayout::D3D11InputLayout(
          D3D11Device*          pDevice,
          uint32_t              numAttributes,
    const DxvkVertexAttribute*  pAttributes,
          uint32_t              numBindings,
    const DxvkVertexBinding*    pBindings)
  : D3D11DeviceChild<ID3D11InputLayout>(pDevice),
    m_d3d10(this) {
    m_attributes.resize(numAttributes);
    m_bindings.resize(numBindings);
    
    for (uint32_t i = 0; i < numAttributes; i++)
      m_attributes.at(i) = pAttributes[i];
    
    for (uint32_t i = 0; i < numBindings; i++)
      m_bindings.at(i) = pBindings[i];
  }
  
  
  D3D11InputLayout::~D3D11InputLayout() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11InputLayout::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11InputLayout)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10InputLayout)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    Logger::warn("D3D11InputLayout::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void D3D11InputLayout::BindToContext(const Rc<DxvkContext>& ctx) {
    ctx->setInputLayout(
      m_attributes.size(),
      m_attributes.data(),
      m_bindings.size(),
      m_bindings.data());
  }
  
  
  bool D3D11InputLayout::Compare(const D3D11InputLayout* pOther) const {
    bool eq = m_attributes.size() == pOther->m_attributes.size()
           && m_bindings.size()   == pOther->m_bindings.size();
    
    for (uint32_t i = 0; eq && i < m_attributes.size(); i++) {
      eq &= m_attributes[i].location == pOther->m_attributes[i].location
         && m_attributes[i].binding  == pOther->m_attributes[i].binding
         && m_attributes[i].format   == pOther->m_attributes[i].format
         && m_attributes[i].offset   == pOther->m_attributes[i].offset;
    }
    
    for (uint32_t i = 0; eq && i < m_bindings.size(); i++) {
      eq &= m_bindings[i].binding    == pOther->m_bindings[i].binding
         && m_bindings[i].fetchRate  == pOther->m_bindings[i].fetchRate
         && m_bindings[i].inputRate  == pOther->m_bindings[i].inputRate;
    }
    
    return eq;
  }
  
}
