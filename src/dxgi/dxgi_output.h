#pragma once

#include "dxgi_monitor.h"
#include "dxgi_object.h"

namespace dxvk {
  
  class DxgiAdapter;
  class DxgiFactory;
  
  /**
   * \brief Number of gamma control points
   */
  constexpr uint32_t DXGI_VK_GAMMA_CP_COUNT = 1024;
  
  /**
   * \brief Computes gamma control point location
   * 
   * \param [in] CpIndex Control point ID
   * \returns Location of the control point
   */
  inline float GammaControlPointLocation(uint32_t CpIndex) {
    return float(CpIndex) / float(DXGI_VK_GAMMA_CP_COUNT - 1);
  }
  
  
  class DxgiOutput : public DxgiObject<IDXGIOutput6> {
    
  public:
    
    DxgiOutput(
      const Com<DxgiFactory>& factory,
      const Com<DxgiAdapter>& adapter,
            HMONITOR          monitor);
    
    ~DxgiOutput();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                riid,
            void**                ppParent) final;
    
    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
      const DXGI_MODE_DESC*       pModeToMatch,
            DXGI_MODE_DESC*       pClosestMatch,
            IUnknown*             pConcernedDevice) final;

    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(
      const DXGI_MODE_DESC1*      pModeToMatch,
            DXGI_MODE_DESC1*      pClosestMatch,
            IUnknown*             pConcernedDevice) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_OUTPUT_DESC*     pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc1(
            DXGI_OUTPUT_DESC1*    pDesc) final;

    HRESULT STDMETHODCALLTYPE GetDisplayModeList(
            DXGI_FORMAT           EnumFormat,
            UINT                  Flags,
            UINT*                 pNumModes,
            DXGI_MODE_DESC*       pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDisplayModeList1(
            DXGI_FORMAT           EnumFormat,
            UINT                  Flags,
            UINT*                 pNumModes,
            DXGI_MODE_DESC1*      pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
            IDXGISurface*         pDestination) final;

    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(
            IDXGIResource*        pDestination) final;
    
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_FRAME_STATISTICS* pStats) final;
    
    HRESULT STDMETHODCALLTYPE GetGammaControl(
            DXGI_GAMMA_CONTROL*   pArray) final;    
    
    HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
            DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) final;
    
    void STDMETHODCALLTYPE ReleaseOwnership() final;
    
    HRESULT STDMETHODCALLTYPE SetDisplaySurface(
            IDXGISurface*         pScanoutSurface) final;
    
    HRESULT STDMETHODCALLTYPE SetGammaControl(
      const DXGI_GAMMA_CONTROL*   pArray) final;
    
    HRESULT STDMETHODCALLTYPE TakeOwnership(
            IUnknown*             pDevice,
            BOOL                  Exclusive) final;
    
    HRESULT STDMETHODCALLTYPE WaitForVBlank() final;

    HRESULT STDMETHODCALLTYPE DuplicateOutput(
            IUnknown*                 pDevice,
            IDXGIOutputDuplication**  ppOutputDuplication) final;
    
    HRESULT STDMETHODCALLTYPE DuplicateOutput1(
            IUnknown*                 pDevice,
            UINT                      Flags,
            UINT                      SupportedFormatsCount,
      const DXGI_FORMAT*              pSupportedFormats,
            IDXGIOutputDuplication**  ppOutputDuplication) final;
    
    BOOL STDMETHODCALLTYPE SupportsOverlays() final;

    HRESULT STDMETHODCALLTYPE CheckOverlaySupport(
            DXGI_FORMAT           EnumFormat,
            IUnknown*             pConcernedDevice,
            UINT*                 pFlags) final;
    
    HRESULT STDMETHODCALLTYPE CheckOverlayColorSpaceSupport(
            DXGI_FORMAT           Format,
            DXGI_COLOR_SPACE_TYPE ColorSpace,
            IUnknown*             pConcernedDevice,
            UINT*                 pFlags) final;
    
    HRESULT STDMETHODCALLTYPE CheckHardwareCompositionSupport(
            UINT*                 pFlags) final;

  private:
    
    DxgiMonitorInfo* m_monitorInfo = nullptr;
    Com<DxgiAdapter> m_adapter = nullptr;
    HMONITOR         m_monitor = nullptr;

    static void FilterModesByDesc(
            std::vector<DXGI_MODE_DESC1>& Modes,
      const DXGI_MODE_DESC1&              TargetMode);
    
  };

}
