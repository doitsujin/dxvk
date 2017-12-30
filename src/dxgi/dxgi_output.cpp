#include <cstdlib>
#include <cstring>

#include <sstream>
#include <string>

#include "dxgi_adapter.h"
#include "dxgi_output.h"

namespace dxvk {
  
  DxgiOutput::DxgiOutput(
              DxgiAdapter*  adapter,
              UINT          display)
  : m_adapter (adapter),
    m_display (display) {
    
  }
  
  
  DxgiOutput::~DxgiOutput() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIOutput);
    
    Logger::warn("DxgiOutput::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetParent(
          REFIID riid,
          void   **ppParent) {
    return m_adapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::FindClosestMatchingMode(
    const DXGI_MODE_DESC *pModeToMatch,
          DXGI_MODE_DESC *pClosestMatch,
          IUnknown       *pConcernedDevice) {
    Logger::err("DxgiOutput::FindClosestMatchingMode: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // Display name, Windows requires wide chars
    const char* displayName = SDL_GetDisplayName(m_display);
    
    if (displayName == nullptr) {
      Logger::err("DxgiOutput::GetDesc: Failed to get display name");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    std::memset(pDesc->DeviceName, 0, sizeof(pDesc->DeviceName));
    std::mbstowcs(pDesc->DeviceName, displayName, _countof(pDesc->DeviceName) - 1);
    
    // Current desktop rect of the display
    SDL_Rect rect;
    
    if (SDL_GetDisplayBounds(m_display, &rect)) {
      Logger::err("DxgiOutput::GetDesc: Failed to get display bounds");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    pDesc->DesktopCoordinates.left    = rect.x;
    pDesc->DesktopCoordinates.top     = rect.y;
    pDesc->DesktopCoordinates.right   = rect.x + rect.w;
    pDesc->DesktopCoordinates.bottom  = rect.y + rect.h;
    
    // We don't have any info for these
    pDesc->AttachedToDesktop  = 1;
    pDesc->Rotation           = DXGI_MODE_ROTATION_UNSPECIFIED;
    pDesc->Monitor            = nullptr;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplayModeList(
          DXGI_FORMAT    EnumFormat,
          UINT           Flags,
          UINT           *pNumModes,
          DXGI_MODE_DESC *pDesc) {
    if (pNumModes == nullptr)
      return DXGI_ERROR_INVALID_CALL;
      
    // In order to check whether a display mode is 'centered' or
    // 'streched' in DXGI terms, we compare its size to the desktop
    // 'mode. If they are the same, we consider the mode to be
    // 'centered', which most games will prefer over 'streched'. 
    SDL_DisplayMode desktopMode;
    
    if (SDL_GetDesktopDisplayMode(m_display, &desktopMode)) {
      Logger::err("DxgiOutput::GetDisplayModeList: Failed to list display modes");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    // Create a list of suitable display modes. Because of the way DXGI
    // swapchains are handled by DXVK, we can ignore the format constraints
    // here and just pick whatever modes SDL returns for the current display.
    std::vector<DXGI_MODE_DESC> modes;
    
    int numDisplayModes = SDL_GetNumDisplayModes(m_display);
    
    if (numDisplayModes < 0) {
      Logger::err("DxgiOutput::GetDisplayModeList: Failed to list display modes");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    for (int i = 0; i < numDisplayModes; i++) {
      SDL_DisplayMode currMode;
      
      if (SDL_GetDisplayMode(m_display, i, &currMode)) {
        Logger::err("DxgiOutput::GetDisplayModeList: Failed to list display modes");
        return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
      }
      
      // We don't want duplicates, so we'll filter out modes
      // with matching resolution and refresh rate.
      bool hasMode = false;
      
      for (int j = 0; j < i && !hasMode; j++) {
        SDL_DisplayMode testMode;
        
        if (SDL_GetDisplayMode(m_display, j, &testMode)) {
          Logger::err("DxgiOutput::GetDisplayModeList: Failed to list display modes");
          return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
        }
        
        hasMode = testMode.w            == currMode.w
               && testMode.h            == currMode.h
               && testMode.refresh_rate == currMode.refresh_rate;
      }
      
      // Convert the SDL display mode to a DXGI display mode info
      // structure and filter out any unwanted modes based on the
      // supplied flags.
      if (!hasMode) {
        DXGI_MODE_DESC mode;
        mode.Width                      = currMode.w;
        mode.Height                     = currMode.h;
        mode.RefreshRate.Numerator      = currMode.refresh_rate;
        mode.RefreshRate.Denominator    = 1;
        mode.Format                     = EnumFormat;
        mode.ScanlineOrdering           = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        mode.Scaling                    = DXGI_MODE_SCALING_CENTERED;
        modes.push_back(mode);
      }
    }
    
    // Copy list of display modes to the application-provided
    // destination buffer. The buffer may not be appropriately
    // sized by the time this is called.
    if (pDesc != nullptr) {
      for (uint32_t i = 0; i < modes.size() && i < *pNumModes; i++)
        pDesc[i] = modes.at(i);
    }
    
    // If the buffer is too small, we shall ask the application
    // to query the display mode list again by returning the
    // appropriate DXGI error code.
    if ((pDesc == nullptr) || (modes.size() <= *pNumModes)) {
      *pNumModes = modes.size();
      return S_OK;
    } else {
      return DXGI_ERROR_MORE_DATA;
    }
      
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplaySurfaceData(IDXGISurface *pDestination) {
    Logger::err("DxgiOutput::GetDisplaySurfaceData: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) {
    Logger::err("DxgiOutput::GetFrameStatistics: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControl(DXGI_GAMMA_CONTROL *pArray) {
    Logger::err("DxgiOutput::GetGammaControl: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) {
    Logger::err("DxgiOutput::GetGammaControlCapabilities: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void STDMETHODCALLTYPE DxgiOutput::ReleaseOwnership() {
    Logger::warn("DxgiOutput::ReleaseOwnership: Stub");
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetDisplaySurface(IDXGISurface *pScanoutSurface) {
    Logger::err("DxgiOutput::SetDisplaySurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetGammaControl(const DXGI_GAMMA_CONTROL *pArray) {
    Logger::err("DxgiOutput::SetGammaControl: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::TakeOwnership(
          IUnknown *pDevice,
          BOOL     Exclusive) {
    Logger::warn("DxgiOutput::TakeOwnership: Stub");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::WaitForVBlank() {
    Logger::warn("DxgiOutput::WaitForVBlank: Stub");
    return S_OK;
  }
  
}
