#include "d3d11_device.h"
#include "d3d11_view_srv.h"

namespace dxvk {
  
  D3D11ShaderResourceView::D3D11ShaderResourceView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC&  desc,
    const Rc<DxvkBufferView>&               bufferView)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_bufferView(bufferView) { }
  
  
  D3D11ShaderResourceView::D3D11ShaderResourceView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC&  desc,
    const Rc<DxvkImageView>&                imageView)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_imageView(imageView) { }
  
  
  D3D11ShaderResourceView::~D3D11ShaderResourceView() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ShaderResourceView::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11View);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11ShaderResourceView);
    
    Logger::warn("D3D11ShaderResourceView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = m_resource.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}
