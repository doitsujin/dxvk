#include "d3d9_interface.h"

#include "d3d9_monitor.h"
#include "d3d9_caps.h"
#include "d3d9_device.h"

#include <algorithm>

namespace dxvk {

  Direct3D9Ex::Direct3D9Ex(bool extended)
    : m_instance{ new DxvkInstance() }
    , m_extended{ extended } {
    for (uint32_t i = 0; m_instance->enumAdapters(i) != nullptr; i++)
      m_instance->enumAdapters(i)->logAdapterInfo();
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3D9)
     || (m_extended && riid == __uuidof(IDirect3D9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3D9Ex::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::RegisterSoftwareDevice(void* pInitializeFunction) {
    Logger::warn("Direct3D9Ex::RegisterSoftwareDevice: Stub");
    return D3D_OK;
  }

  UINT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterCount() {
    return UINT(m_instance->adapterCount());
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterIdentifier(
    UINT Adapter,
    DWORD Flags,
    D3DADAPTER_IDENTIFIER9* pIdentifier) {
    auto dxvkAdapter = m_instance->enumAdapters(Adapter);

    if (dxvkAdapter == nullptr || pIdentifier == nullptr)
      return D3DERR_INVALIDCALL;
    
    std::memcpy(pIdentifier->Description, dxvkAdapter->deviceProperties().deviceName, 256); // The description is actually the device name.
    pIdentifier->DeviceId = dxvkAdapter->deviceProperties().deviceID;
    std::memcpy(&pIdentifier->DeviceIdentifier, dxvkAdapter->devicePropertiesExt().coreDeviceId.deviceUUID, sizeof(GUID));
    std::strcpy(pIdentifier->DeviceName, R"(\\.\DISPLAY1)"); // The GDI device name. Not the actual device name.
    std::strcpy(pIdentifier->Driver, "d3d9.dll"); // This is the driver's dll. It is important that it ends in dll.
    pIdentifier->DriverVersion.QuadPart = dxvkAdapter->deviceProperties().driverVersion;
    pIdentifier->Revision = 0;
    pIdentifier->SubSysId = 0;
    pIdentifier->VendorId = dxvkAdapter->deviceProperties().vendorID;
    pIdentifier->WHQLLevel = m_extended ? 1 : 0; // This doesn't check with the driver on Direct3D9Ex and is always 1.

    return D3D_OK;
  }

  UINT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) {
    D3DDISPLAYMODEFILTER filter;
    filter.Size = sizeof(D3DDISPLAYMODEFILTER);
    filter.Format = Format;
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    
    return this->GetAdapterModeCountEx(Adapter, &filter);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) {
    return this->EnumAdapterModes(Adapter, D3DFMT_X8R8G8B8, 0, pMode);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CheckDeviceType(
    UINT Adapter,
    D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat,
    D3DFORMAT BackBufferFormat,
    BOOL bWindowed) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::checkDeviceType(fixupFormat(AdapterFormat), fixupFormat(BackBufferFormat), bWindowed);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CheckDeviceFormat(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    D3DFORMAT AdapterFormat,
    DWORD Usage,
    D3DRESOURCETYPE RType,
    D3DFORMAT CheckFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::checkDeviceFormat(fixupFormat(AdapterFormat), Usage, RType, fixupFormat(CheckFormat));
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CheckDeviceMultiSampleType(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    D3DFORMAT SurfaceFormat,
    BOOL Windowed,
    D3DMULTISAMPLE_TYPE MultiSampleType,
    DWORD* pQualityLevels) { 
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::checkDeviceMultiSampleType(fixupFormat(SurfaceFormat), Windowed, MultiSampleType, pQualityLevels);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CheckDepthStencilMatch(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    D3DFORMAT AdapterFormat,
    D3DFORMAT RenderTargetFormat,
    D3DFORMAT DepthStencilFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::checkDepthStencilMatch(fixupFormat(AdapterFormat), fixupFormat(RenderTargetFormat), fixupFormat(DepthStencilFormat));
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CheckDeviceFormatConversion(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    D3DFORMAT SourceFormat,
    D3DFORMAT TargetFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::checkDeviceFormatConversion(fixupFormat(SourceFormat), fixupFormat(TargetFormat));
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::GetDeviceCaps(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    D3DCAPS9* pCaps) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::getDeviceCaps(Adapter, DeviceType, pCaps);
  }

  HMONITOR STDMETHODCALLTYPE Direct3D9Ex::GetAdapterMonitor(UINT Adapter) {
    if (Adapter >= this->GetAdapterCount())
      return nullptr;

    return GetDefaultMonitor();
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CreateDevice(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    HWND hFocusWindow,
    DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface) {
    return createDeviceInternal(
      false,
      Adapter,
      DeviceType,
      hFocusWindow,
      BehaviorFlags,
      pPresentationParameters,
      nullptr,
      reinterpret_cast<IDirect3DDevice9Ex**>(ppReturnedDeviceInterface));
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::EnumAdapterModes(
    UINT Adapter,
    D3DFORMAT Format,
    UINT Mode,
    D3DDISPLAYMODE* pMode) {
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    D3DDISPLAYMODEFILTER filter;
    filter.Format = Format;
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    filter.Size = sizeof(D3DDISPLAYMODEFILTER);
    D3DDISPLAYMODEEX extendedMode;
    HRESULT hr = this->EnumAdapterModesEx(Adapter, &filter, Mode, &extendedMode);

    if (FAILED(hr))
      return hr;

    pMode->Width = extendedMode.Width;
    pMode->Height = extendedMode.Height;
    pMode->RefreshRate = extendedMode.RefreshRate;
    pMode->Format = extendedMode.Format;

    return D3D_OK;
  }

  // Ex Methods

  UINT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterModeCountEx(UINT Adapter, CONST D3DDISPLAYMODEFILTER* pFilter) {
    if (pFilter == nullptr)
      return 0;

    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    // We don't offer any interlaced formats here so early out and avoid destroying mode cache.
    if (pFilter->ScanLineOrdering == D3DSCANLINEORDERING_INTERLACED)
      return 0;

    cacheModes(fixupFormat(pFilter->Format));
    return m_modes.size();
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::EnumAdapterModesEx(
    UINT Adapter,
    CONST D3DDISPLAYMODEFILTER* pFilter,
    UINT Mode,
    D3DDISPLAYMODEEX* pMode) {
    if (pMode == nullptr || pFilter == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9Format format = fixupFormat(pFilter->Format);

    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    if (!IsSupportedMonitorFormat(format))
      return D3DERR_INVALIDCALL;

    cacheModes(format);

    // We don't return any scanline orderings that aren't progressive,
    // The format filtering is already handled for us by cache modes
    // So we can early out here and then just index.
    if (pFilter->ScanLineOrdering == D3DSCANLINEORDERING_INTERLACED)
      return D3DERR_INVALIDCALL;

    if (Mode >= m_modes.size())
      return D3DERR_INVALIDCALL;

    *pMode = m_modes[Mode];

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterDisplayModeEx(
    UINT Adapter,
    D3DDISPLAYMODEEX* pMode,
    D3DDISPLAYROTATION* pRotation) {
    D3DDISPLAYMODEFILTER filter;
    filter.Size = sizeof(D3DDISPLAYMODEFILTER);
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    filter.Format = D3DFMT_X8R8G8B8;

    return this->EnumAdapterModesEx(Adapter, &filter, 0, pMode);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::CreateDeviceEx(
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    HWND hFocusWindow,
    DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    D3DDISPLAYMODEEX* pFullscreenDisplayMode,
    IDirect3DDevice9Ex** ppReturnedDeviceInterface) {
    return createDeviceInternal(
      true,
      Adapter,
      DeviceType,
      hFocusWindow,
      BehaviorFlags,
      pPresentationParameters,
      pFullscreenDisplayMode,
      ppReturnedDeviceInterface);
  }

  HRESULT STDMETHODCALLTYPE Direct3D9Ex::GetAdapterLUID(UINT Adapter, LUID* pLUID) {
    if (pLUID == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = m_instance->enumAdapters(Adapter);

    if (dxvkAdapter == nullptr)
      return D3DERR_INVALIDCALL;

    std::memcpy(pLUID, &dxvkAdapter->devicePropertiesExt().coreDeviceId.deviceLUID, sizeof(LUID));

    return D3D_OK;
  }

  void Direct3D9Ex::cacheModes(D3D9Format enumFormat) {
    if (!m_modes.empty() && m_modeCacheFormat == enumFormat)
      return; // We already cached the modes for this format. No need to do it again.

    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(GetDefaultMonitor(), reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Direct3D9Ex::cacheModes: failed to query monitor info");
      return;
    }

    m_modes.clear();
    m_modeCacheFormat = enumFormat;

    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODEW devMode;

    uint32_t modeIndex = 0;

    while (::EnumDisplaySettingsW(monInfo.szDevice, modeIndex++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;

      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetMonitorFormatBpp(enumFormat))
        continue;

      D3DDISPLAYMODEEX mode;
      mode.Size = sizeof(D3DDISPLAYMODEEX);
      mode.Width = devMode.dmPelsWidth;
      mode.Height = devMode.dmPelsHeight;
      mode.RefreshRate = devMode.dmDisplayFrequency;
      mode.Format = static_cast<D3DFORMAT>(enumFormat);
      mode.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

      m_modes.push_back(mode);
    }

    // Sort display modes by width, height and refresh rate,
    // in reverse order. Some games rely on correct ordering.
    // This is the opposite ordering to DXGI.
    std::sort(m_modes.begin(), m_modes.end(),
      [](const D3DDISPLAYMODEEX & a, const D3DDISPLAYMODEEX & b) {
        if (a.Width < b.Width) return false;
        if (a.Width > b.Width) return true;

        if (a.Height < b.Height) return false;
        if (a.Height > b.Height) return true;

        return b.RefreshRate < a.RefreshRate;
    });
  }

  HRESULT Direct3D9Ex::createDeviceInternal(
    bool extended,
    UINT adapter,
    D3DDEVTYPE deviceType,
    HWND window,
    DWORD flags,
    D3DPRESENT_PARAMETERS* presentParams,
    D3DDISPLAYMODEEX* displayMode,
    IDirect3DDevice9Ex** outDevice) {
    InitReturnPtr(outDevice);

    if (outDevice == nullptr || presentParams == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = m_instance->enumAdapters(adapter);

    if (dxvkAdapter == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkDevice = dxvkAdapter->createDevice(Direct3DDevice9Ex::GetDeviceFeatures(dxvkAdapter));

    *outDevice = ref(new Direct3DDevice9Ex{ 
      extended,
      this,
      adapter,
      dxvkAdapter,
      dxvkDevice,
      deviceType,
      window,
      flags,
      presentParams,
      displayMode });

    return D3D_OK;
  }

}