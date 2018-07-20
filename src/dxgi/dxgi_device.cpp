#include "dxgi_device.h"
#include "dxgi_factory.h"

namespace dxvk {

  constexpr uint32_t DxgiDevice::DefaultFrameLatency;
  
  DxgiDevice::DxgiDevice(
          IDXGIObject*              pContainer,
          IDXGIVkAdapter*           pAdapter,
    const VkPhysicalDeviceFeatures* pFeatures)
  : m_container (pContainer),
    m_adapter   (pAdapter) {
    m_device = m_adapter->GetDXVKAdapter()->createDevice(*pFeatures);

    for (uint32_t i = 0; i < m_frameEvents.size(); i++)
      m_frameEvents[i] = new DxvkEvent();
  }
  
  
  DxgiDevice::~DxgiDevice() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE DxgiDevice::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE DxgiDevice::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::QueryInterface(REFIID riid, void** ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetParent(REFIID riid, void** ppParent) {
    return m_adapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetPrivateData(
    REFGUID Name, UINT* pDataSize, void* pData) {
    return m_container->GetPrivateData(Name, pDataSize, pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::SetPrivateData(
    REFGUID Name, UINT DataSize, const void* pData) {
    return m_container->SetPrivateData(Name, DataSize,pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::SetPrivateDataInterface(
    REFGUID Name, const IUnknown* pUnknown) {
    return m_container->SetPrivateDataInterface(Name, pUnknown);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::CreateSurface(
    const DXGI_SURFACE_DESC*    pDesc,
          UINT                  NumSurfaces,
          DXGI_USAGE            Usage,
    const DXGI_SHARED_RESOURCE* pSharedResource,
          IDXGISurface**        ppSurface) {
    InitReturnPtr(ppSurface);
    
    Logger::err("DxgiDevice::CreateSurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetAdapter(
          IDXGIAdapter**        pAdapter) {
    if (pAdapter == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
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
    
    Logger::err("DXGI: SetGPUThreadPriority: Ignoring");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::GetMaximumFrameLatency(
          UINT*                 pMaxLatency) {
    *pMaxLatency = m_frameLatency;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::SetMaximumFrameLatency(
          UINT                  MaxLatency) {
    if (MaxLatency == 0)
      MaxLatency = DefaultFrameLatency;
    
    if (MaxLatency > m_frameEvents.size())
      MaxLatency = m_frameEvents.size();
    
    m_frameLatency = MaxLatency;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiDevice::OfferResources( 
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          DXGI_OFFER_RESOURCE_PRIORITY  Priority) {

    Logger::err("DxgiDevice::OfferResources: Not implemented");
    return DXGI_ERROR_UNSUPPORTED;
  }


  HRESULT STDMETHODCALLTYPE DxgiDevice::ReclaimResources( 
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          BOOL*                         pDiscarded) {
    Logger::err("DxgiDevice::ReclaimResources: Not implemented");
    return DXGI_ERROR_UNSUPPORTED;    
  }


  HRESULT STDMETHODCALLTYPE DxgiDevice::EnqueueSetEvent(HANDLE hEvent) {
    Logger::err("DxgiDevice::EnqueueSetEvent: Not implemented");
    return DXGI_ERROR_UNSUPPORTED;           
  }
  
  
  Rc<DxvkDevice> STDMETHODCALLTYPE DxgiDevice::GetDXVKDevice() {
    return m_device;
  }
  
  
  Rc<DxvkEvent> STDMETHODCALLTYPE DxgiDevice::GetFrameSyncEvent() {
    uint32_t frameId = m_frameId++ % m_frameLatency;
    return m_frameEvents[frameId];
  }
  
}
