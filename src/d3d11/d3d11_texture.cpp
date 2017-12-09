#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                device,
          IDXGIImageResourcePrivate*  resource,
    const D3D11_TEXTURE2D_DESC&       desc)
  : m_device  (device),
    m_resource(resource),
    m_desc    (desc) {
    
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  HRESULT D3D11Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Texture2D);
    
    if (riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIImageResourcePrivate))
      return m_resource->QueryInterface(riid, ppvObject);
    
    Logger::warn("D3D11Texture2D::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
    
  void D3D11Texture2D::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void D3D11Texture2D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }
  
  
  UINT D3D11Texture2D::GetEvictionPriority() {
    UINT EvictionPriority = DXGI_RESOURCE_PRIORITY_NORMAL;
    m_resource->GetEvictionPriority(&EvictionPriority);
    return EvictionPriority;
  }
  
  
  void D3D11Texture2D::SetEvictionPriority(UINT EvictionPriority) {
    m_resource->SetEvictionPriority(EvictionPriority);
  }
  
  
  void D3D11Texture2D::GetDesc(D3D11_TEXTURE2D_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  Rc<DxvkImage> D3D11Texture2D::GetDXVKImage() {
    return m_resource->GetDXVKImage();
  }
  
}
