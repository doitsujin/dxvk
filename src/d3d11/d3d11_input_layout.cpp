#include "d3d11_device.h"
#include "d3d11_input_layout.h"

namespace dxvk {
  
  D3D11InputLayout::D3D11InputLayout(
          D3D11Device*                pDevice,
    const Rc<DxvkInputLayout>&        inputLayout)
  : m_device(pDevice), m_inputLayout(inputLayout) {
    
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
  
}
