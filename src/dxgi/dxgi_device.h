#pragma once

#include <dxvk_device.h>

#include "dxgi_adapter.h"
#include "dxgi_interfaces.h"

namespace dxvk {
  
  class DxgiFactory;
  
  class DxgiDevice : public DxgiObject<IDXGIDevicePrivate> {
    
  public:
    
    DxgiDevice(IDXGIAdapterPrivate* adapter);
    ~DxgiDevice();
    
    HRESULT QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    HRESULT CreateSurface(
      const DXGI_SURFACE_DESC*    pDesc,
            UINT                  NumSurfaces,
            DXGI_USAGE            Usage,
      const DXGI_SHARED_RESOURCE* pSharedResource,
            IDXGISurface**        ppSurface) final;
    
    HRESULT GetAdapter(
            IDXGIAdapter**        pAdapter) final;
    
    HRESULT GetGPUThreadPriority(
            INT*                  pPriority) final;
    
    HRESULT QueryResourceResidency(
            IUnknown* const*      ppResources,
            DXGI_RESIDENCY*       pResidencyStatus,
            UINT                  NumResources) final;
    
    HRESULT SetGPUThreadPriority(
            INT                   Priority) final;
    
    void SetDeviceLayer(
            IUnknown*             layer) final;
    
    Rc<DxvkDevice> GetDXVKDevice() final;
    
  private:
    
    Com<IDXGIAdapterPrivate> m_adapter;
    Rc<DxvkDevice>           m_device;
    
    IUnknown* m_layer = nullptr;
    
  };

}


extern "C" {
  
  DLLEXPORT HRESULT __stdcall DXGICreateDXVKDevice(
          IDXGIAdapterPrivate*   pAdapter,
          IDXGIDevicePrivate**   ppDevice);
  
}