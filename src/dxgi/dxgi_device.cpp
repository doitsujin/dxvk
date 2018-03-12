#include "dxgi_device.h"
#include "dxgi_factory.h"

namespace dxvk {
  
  DxgiDevice::DxgiDevice(
          IDXGIAdapterPrivate*      adapter,
    const VkPhysicalDeviceFeatures* features)
  : m_adapter(adapter) {
    m_device = m_adapter->GetDXVKAdapter()->createDevice(*features);
  }
  
  
  DxgiDevice::~DxgiDevice() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDevice);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDevice1);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDevicePrivate);
    
    if (m_layer != nullptr)
      return m_layer->QueryInterface(riid, ppvObject);
    
    Logger::warn("DxgiDevice::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetParent(REFIID riid, void** ppParent) {
    return m_adapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::CreateSurface(
    const DXGI_SURFACE_DESC*    pDesc,
          UINT                  NumSurfaces,
          DXGI_USAGE            Usage,
    const DXGI_SHARED_RESOURCE* pSharedResource,
          IDXGISurface**        ppSurface) {
    Logger::err("DxgiDevice::CreateSurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetAdapter(
          IDXGIAdapter**        pAdapter) {
    *pAdapter = static_cast<IDXGIAdapter*>(m_adapter.ref());
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetGPUThreadPriority(
          INT*                  pPriority) {
    *pPriority = 0;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::QueryResourceResidency(
          IUnknown* const*      ppResources,
          DXGI_RESIDENCY*       pResidencyStatus,
          UINT                  NumResources) {
    Logger::err("DxgiDevice::QueryResourceResidency: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::SetGPUThreadPriority(
          INT                   Priority) {
    if (Priority < -7 || Priority > 7)
      return E_INVALIDARG;
    
    Logger::err("DxgiDevice::SetGPUThreadPriority: Ignoring");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetMaximumFrameLatency(
          UINT*                 pMaxLatency) {
    Logger::warn("DxgiDevice::GetMaximumFrameLatency: Stub");
    *pMaxLatency = 1;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::SetMaximumFrameLatency(
          UINT                  MaxLatency) {
    Logger::warn("DxgiDevice::SetMaximumFrameLatency: Stub");
    return S_OK;
  }
  
  
  void STDMETHODCALLTYPE DxgiDevice::SetDeviceLayer(IUnknown* layer) {
    m_layer = layer;
  }
  
  
  Rc<DxvkDevice> STDMETHODCALLTYPE DxgiDevice::GetDXVKDevice() {
    return m_device;
  }
  
}


extern "C" {
  
  DLLEXPORT HRESULT __stdcall DXGICreateDevicePrivate(
          IDXGIAdapterPrivate*      pAdapter,
    const VkPhysicalDeviceFeatures* features,
          IDXGIDevicePrivate**      ppDevice) {
    try {
      *ppDevice = dxvk::ref(new dxvk::DxgiDevice(pAdapter, features));
      return S_OK;
    } catch (const dxvk::DxvkError& e) {
      dxvk::Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
}