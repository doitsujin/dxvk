#include "d3d11_buffer.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*                device,
          IDXGIBufferResourcePrivate* resource,
    const D3D11_BUFFER_DESC&          desc)
  : m_device  (device),
    m_resource(resource),
    m_desc    (desc) {
    
  }
  
  
  D3D11Buffer::~D3D11Buffer() {
    
  }
  
  
  HRESULT D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Buffer);
    
    if (riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIBufferResourcePrivate))
      return m_resource->QueryInterface(riid, ppvObject);
      
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void D3D11Buffer::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
  }
  
  
  void D3D11Buffer::GetDesc(D3D11_BUFFER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}
