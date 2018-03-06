#pragma once

#include <dxvk_device.h>

#include "dxgi_adapter.h"
#include "dxgi_interfaces.h"

namespace dxvk {
  
  class DxgiFactory;
  
  class DxgiDevice : public DxgiObject<IDXGIDevicePrivate> {
    
  public:
    
    DxgiDevice(
            IDXGIAdapterPrivate*      adapter,
      const VkPhysicalDeviceFeatures* features);
    ~DxgiDevice();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID riid,
            void   **ppParent) final;
    
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
    
    void STDMETHODCALLTYPE SetDeviceLayer(
            IUnknown*             layer) final;
    
    Rc<DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice() final;
    
  private:
    
    Com<IDXGIAdapterPrivate> m_adapter;
    Rc<DxvkDevice>           m_device;
    
    IUnknown* m_layer = nullptr;
    
  };

}


extern "C" {
  
  HRESULT __stdcall DXGICreateDevicePrivate(
          IDXGIAdapterPrivate*      pAdapter,
    const VkPhysicalDeviceFeatures* features,
          IDXGIDevicePrivate**      ppDevice);
  
}