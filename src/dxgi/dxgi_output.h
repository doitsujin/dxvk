#pragma once

#include <mutex>

#include "dxgi_object.h"

namespace dxvk {
  
  class DxgiAdapter;
  class DxgiSwapChain;
  
  class DxgiOutput : public DxgiObject<IDXGIOutput> {
    
  public:
    
    DxgiOutput(
      const Com<DxgiAdapter>& adapter,
            HMONITOR          monitor);
    
    ~DxgiOutput();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
      const DXGI_MODE_DESC *pModeToMatch,
            DXGI_MODE_DESC *pClosestMatch,
            IUnknown       *pConcernedDevice) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_OUTPUT_DESC *pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDisplayModeList(
            DXGI_FORMAT    EnumFormat,
            UINT           Flags,
            UINT           *pNumModes,
            DXGI_MODE_DESC *pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
            IDXGISurface *pDestination) final;
    
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_FRAME_STATISTICS *pStats) final;
    
    HRESULT STDMETHODCALLTYPE GetGammaControl(
            DXGI_GAMMA_CONTROL *pArray) final;    
    
    HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
            DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) final;
    
    void STDMETHODCALLTYPE ReleaseOwnership() final;
    
    HRESULT STDMETHODCALLTYPE SetDisplaySurface(
            IDXGISurface *pScanoutSurface) final;
    
    HRESULT STDMETHODCALLTYPE SetGammaControl(
      const DXGI_GAMMA_CONTROL *pArray) final;
    
    HRESULT STDMETHODCALLTYPE TakeOwnership(
            IUnknown *pDevice,
            BOOL     Exclusive) final;
    
    HRESULT STDMETHODCALLTYPE WaitForVBlank() final;
    
  private:
    
    Com<DxgiAdapter> m_adapter = nullptr;
    HMONITOR         m_monitor = nullptr;
    
    uint32_t GetFormatBpp(DXGI_FORMAT Format) const;
    
  };

}
