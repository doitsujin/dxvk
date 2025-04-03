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
    m_attributeCount  (numAttributes),
    m_bindingCount    (numBindings),
    m_d3d10           (this) {
    for (uint32_t i = 0; i < numAttributes; i++)
      m_inputs[i] = DxvkVertexInput(pAttributes[i]);

    for (uint32_t i = 0; i < numBindings; i++)
      m_inputs[i + numAttributes] = DxvkVertexInput(pBindings[i]);
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
    
    if (logQueryInterfaceError(__uuidof(ID3D11InputLayout), riid)) {
      Logger::warn("D3D11InputLayout::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  bool D3D11InputLayout::Compare(const D3D11InputLayout* pOther) const {
    if (m_attributeCount != pOther->m_attributeCount || m_bindingCount != pOther->m_bindingCount)
      return false;

    // Try to vectorize at least a little bit here. We can't use bcmpeq here
    // since there is no way at all to guaratee alignment for the array.
    for (uint32_t i = 0; i < m_attributeCount + m_bindingCount; i += 4u) {
      if (std::memcmp(&m_inputs[i], &pOther->m_inputs[i], 4u * sizeof(DxvkVertexInput)))
        return false;
    }

    return true;
  }
  
}
