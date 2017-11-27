#include "dxgi_device.h"
#include "dxgi_factory.h"

namespace dxvk {
  
  DxgiDevice::DxgiDevice(IDXGIAdapterPrivate* adapter)
  : m_adapter(adapter) {
    m_device = m_adapter->GetDXVKAdapter()->createDevice();
  }
  
  
  DxgiDevice::~DxgiDevice() {
    
  }
  
  
  HRESULT DxgiDevice::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDevice);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDevicePrivate);
    
    if (m_layer != nullptr)
      return m_layer->QueryInterface(riid, ppvObject);
    
    Logger::warn("DxgiDevice::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiDevice::GetParent(REFIID riid, void** ppParent) {
    return m_adapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT DxgiDevice::CreateSurface(
    const DXGI_SURFACE_DESC*    pDesc,
          UINT                  NumSurfaces,
          DXGI_USAGE            Usage,
    const DXGI_SHARED_RESOURCE* pSharedResource,
          IDXGISurface**        ppSurface) {
    Logger::err("DxgiDevice::CreateSurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiDevice::GetAdapter(
          IDXGIAdapter**        pAdapter) {
    *pAdapter = static_cast<IDXGIAdapter*>(m_adapter.ref());
    return S_OK;
  }
  
  
  HRESULT DxgiDevice::GetGPUThreadPriority(
          INT*                  pPriority) {
    *pPriority = 0;
    return S_OK;
  }
  
  
  HRESULT DxgiDevice::QueryResourceResidency(
          IUnknown* const*      ppResources,
          DXGI_RESIDENCY*       pResidencyStatus,
          UINT                  NumResources) {
    Logger::err("DxgiDevice::QueryResourceResidency: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiDevice::SetGPUThreadPriority(
          INT                   Priority) {
    if (Priority < -7 || Priority > 7)
      return E_INVALIDARG;
    
    Logger::err("DxgiDevice::SetGPUThreadPriority: Ignoring");
    return S_OK;
  }
  
  
  void DxgiDevice::SetDeviceLayer(IUnknown* layer) {
    m_layer = layer;
  }
  
  
  Rc<DxvkDevice> DxgiDevice::GetDXVKDevice() {
    return m_device;
  }
  
}


extern "C" {
  
  DLLEXPORT HRESULT __stdcall DXGICreateDXVKDevice(
          IDXGIAdapterPrivate*   pAdapter,
          IDXGIDevicePrivate**   ppDevice) {
    try {
      *ppDevice = dxvk::ref(new dxvk::DxgiDevice(pAdapter));
      return S_OK;
    } catch (const dxvk::DxvkError& e) {
      dxvk::Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
}