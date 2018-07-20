#pragma once

#include <dxvk_device.h>

#include "dxgi_adapter.h"
#include "dxgi_interfaces.h"

namespace dxvk {
  
  class DxgiFactory;
  
  class DxgiDevice : public IDXGIVkDevice {
    constexpr static uint32_t DefaultFrameLatency = 3;
  public:
    
    DxgiDevice(
            IDXGIObject*              pContainer,
            IDXGIVkAdapter*           pAdapter,
      const VkPhysicalDeviceFeatures* pFeatures);
    ~DxgiDevice();
    
    ULONG STDMETHODCALLTYPE AddRef() final;
    
    ULONG STDMETHODCALLTYPE Release() final;
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                riid,
            void**                ppParent) final;
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID               Name,
            UINT*                 pDataSize,
            void*                 pData) final;
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID               Name,
            UINT                  DataSize,
      const void*                 pData) final;
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID               Name,
      const IUnknown*             pUnknown) final;
    
    HRESULT STDMETHODCALLTYPE CreateSurface(
      const DXGI_SURFACE_DESC*    pDesc,
            UINT                  NumSurfaces,
            DXGI_USAGE            Usage,
      const DXGI_SHARED_RESOURCE* pSharedResource,
            IDXGISurface**        ppSurface) final;
    
    HRESULT STDMETHODCALLTYPE GetAdapter(
            IDXGIAdapter**        pAdapter) final;
    
    HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(
            INT*                  pPriority) final;
    
    HRESULT STDMETHODCALLTYPE QueryResourceResidency(
            IUnknown* const*      ppResources,
            DXGI_RESIDENCY*       pResidencyStatus,
            UINT                  NumResources) final;
    
    HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(
            INT                   Priority) final;
    
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                 pMaxLatency) final;
    
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                  MaxLatency) final;

    HRESULT STDMETHODCALLTYPE OfferResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            DXGI_OFFER_RESOURCE_PRIORITY  Priority) final;
        
    HRESULT STDMETHODCALLTYPE ReclaimResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            BOOL*                         pDiscarded) final;
        
    HRESULT STDMETHODCALLTYPE EnqueueSetEvent( 
            HANDLE                hEvent) final;
    
    Rc<DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice() final;

    Rc<DxvkEvent> STDMETHODCALLTYPE GetFrameSyncEvent();
    
  private:
    
    IDXGIObject*        m_container;
    
    Com<IDXGIVkAdapter> m_adapter;
    Rc<DxvkDevice>      m_device;

    uint32_t            m_frameLatency = DefaultFrameLatency;
    uint32_t            m_frameId      = 0;

    std::array<Rc<DxvkEvent>, 16> m_frameEvents;
    
  };

}
