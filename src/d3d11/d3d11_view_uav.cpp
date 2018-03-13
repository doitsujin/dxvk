#include "d3d11_device.h"
#include "d3d11_view_uav.h"

namespace dxvk {
  
  D3D11UnorderedAccessView::D3D11UnorderedAccessView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc,
    const Rc<DxvkBufferView>&               bufferView,
    const DxvkBufferSlice&                  counterSlice)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_bufferView(bufferView),
    m_counterSlice(counterSlice) { }
  
  
  D3D11UnorderedAccessView::D3D11UnorderedAccessView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc,
    const Rc<DxvkImageView>&                imageView,
    const DxvkBufferSlice&                  counterSlice)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_imageView(imageView),
    m_counterSlice(counterSlice) { }
  
  
  D3D11UnorderedAccessView::~D3D11UnorderedAccessView() {
    if (m_counterSlice.defined())
      m_device->FreeCounterSlice(m_counterSlice);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11UnorderedAccessView::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11View);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11UnorderedAccessView);
    
    Logger::warn("D3D11UnorderedAccessView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = m_resource.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}
