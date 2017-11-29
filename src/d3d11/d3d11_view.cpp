#include "d3d11_device.h"
#include "d3d11_view.h"

namespace dxvk {
  
  D3D11RenderTargetView::D3D11RenderTargetView(
          D3D11Device*                    device,
          ID3D11Resource*                 resource,
    const D3D11_RENDER_TARGET_VIEW_DESC&  desc,
          Rc<DxvkImageView>               view)
  : m_device  (device),
    m_resource(resource),
    m_desc    (desc),
    m_view    (view) {
    
  }
  
  
  D3D11RenderTargetView::~D3D11RenderTargetView() {
    
  }
  
  
  HRESULT D3D11RenderTargetView::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11View);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11RenderTargetView);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11RenderTargetViewPrivate);
    
    Logger::warn("D3D11RenderTargetView::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11RenderTargetView::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void D3D11RenderTargetView::GetResource(ID3D11Resource **ppResource) {
    *ppResource = m_resource.ref();
  }
  
  
  void D3D11RenderTargetView::GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  Rc<DxvkImageView> D3D11RenderTargetView::GetDXVKImageView() {
    return m_view;
  }
  
}
