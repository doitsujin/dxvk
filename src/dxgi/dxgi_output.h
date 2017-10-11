#pragma once

#include "dxgi_object.h"

namespace dxvk {
  
  class DxgiAdapter;
  
  class DxgiOutput : public DxgiObject<IDXGIOutput> {
    
  public:
    
    DxgiOutput(
            DxgiAdapter*  adapter,
            UINT          display);
    
    ~DxgiOutput();
    
    HRESULT QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    HRESULT FindClosestMatchingMode(
      const DXGI_MODE_DESC *pModeToMatch,
            DXGI_MODE_DESC *pClosestMatch,
            IUnknown       *pConcernedDevice) final;
    
    HRESULT GetDesc(
            DXGI_OUTPUT_DESC *pDesc) final;
    
    HRESULT GetDisplayModeList(
            DXGI_FORMAT    EnumFormat,
            UINT           Flags,
            UINT           *pNumModes,
            DXGI_MODE_DESC *pDesc) final;
    
    HRESULT GetDisplaySurfaceData(
            IDXGISurface *pDestination) final;
    
    HRESULT GetFrameStatistics(
            DXGI_FRAME_STATISTICS *pStats) final;
    
    HRESULT GetGammaControl(
            DXGI_GAMMA_CONTROL *pArray) final;    
    
    HRESULT GetGammaControlCapabilities(
            DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) final;
    
    void ReleaseOwnership() final;
    
    HRESULT SetDisplaySurface(
            IDXGISurface *pScanoutSurface) final;
    
    HRESULT SetGammaControl(
      const DXGI_GAMMA_CONTROL *pArray) final;
    
    HRESULT TakeOwnership(
            IUnknown *pDevice,
            BOOL     Exclusive) final;
    
    HRESULT WaitForVBlank() final;
    
  private:
    
    DxgiAdapter*  m_adapter;
    UINT          m_display;
    
  };

}
