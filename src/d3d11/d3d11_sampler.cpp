#include "d3d11_device.h"
#include "d3d11_sampler.h"

namespace dxvk {
  
  D3D11SamplerState::D3D11SamplerState(
          D3D11Device*        device,
    const D3D11_SAMPLER_DESC& desc,
    const Rc<DxvkSampler>&    sampler)
  : m_device(device),
    m_desc(desc),
    m_sampler(sampler) {
    
  }
  
  
  D3D11SamplerState::~D3D11SamplerState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11SamplerState::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11SamplerState);
    
    Logger::warn("D3D11SamplerState::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11SamplerState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11SamplerState::GetDesc(D3D11_SAMPLER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}
