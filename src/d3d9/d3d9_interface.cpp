#include "d3d9_interface.h"

#include "d3d9_monitor.h"
#include "d3d9_caps.h"
#include "d3d9_device.h"

#include <algorithm>

namespace dxvk {

  D3D9InterfaceEx::D3D9InterfaceEx(bool bExtended)
    : m_instance    ( new DxvkInstance() )
    , m_extended    ( bExtended ) 
    , m_d3d9Options ( nullptr, m_instance->config() ){
    for (uint32_t i = 0; m_instance->enumAdapters(i) != nullptr; i++)
      m_instance->enumAdapters(i)->logAdapterInfo();

    if (m_d3d9Options.dpiAware) {
      Logger::info("Process set as DPI aware");
      SetProcessDPIAware();
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3D9)
     || (m_extended && riid == __uuidof(IDirect3D9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9InterfaceEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::RegisterSoftwareDevice(void* pInitializeFunction) {
    Logger::warn("D3D9InterfaceEx::RegisterSoftwareDevice: Stub");
    return D3D_OK;
  }


  UINT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterCount() {
    return UINT(m_instance->adapterCount());
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterIdentifier(
          UINT                    Adapter,
          DWORD                   Flags,
          D3DADAPTER_IDENTIFIER9* pIdentifier) {
    auto dxvkAdapter = m_instance->enumAdapters(Adapter);

    if (dxvkAdapter == nullptr || pIdentifier == nullptr)
      return D3DERR_INVALIDCALL;
    
    const auto& props = dxvkAdapter->deviceProperties();

    uint32_t vendorId = m_d3d9Options.customVendorId == -1 ? props.vendorID : uint32_t(m_d3d9Options.customVendorId);

    std::memcpy(pIdentifier->Description, props.deviceName, 256); // The description is actually the device name.
    pIdentifier->DeviceId = m_d3d9Options.customDeviceId == -1 ? props.deviceID : uint32_t(m_d3d9Options.customDeviceId);
    std::memcpy(&pIdentifier->DeviceIdentifier, dxvkAdapter->devicePropertiesExt().coreDeviceId.deviceUUID, sizeof(GUID));
    std::strcpy(pIdentifier->DeviceName, R"(\\.\DISPLAY1)"); // The GDI device name. Not the actual device name.
    std::strcpy(pIdentifier->Driver, this->GetDriverDllName(DxvkGpuVendor(vendorId))); // This is the driver's dll.
    pIdentifier->DriverVersion.QuadPart = props.driverVersion;
    pIdentifier->Revision = 0;
    pIdentifier->SubSysId = 0;
    pIdentifier->VendorId = vendorId;
    pIdentifier->WHQLLevel = m_extended ? 1 : 0; // This doesn't check with the driver on Direct3D9Ex and is always 1.

    return D3D_OK;
  }


  UINT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) {
    D3DDISPLAYMODEFILTER filter;
    filter.Size             = sizeof(D3DDISPLAYMODEFILTER);
    filter.Format           = Format;
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    
    return this->GetAdapterModeCountEx(Adapter, &filter);
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) {
    constexpr D3DFORMAT format = D3DFMT_X8R8G8B8;
    const UINT mode = GetAdapterModeCount(Adapter, format) - 1;

    return this->EnumAdapterModes(Adapter, format, mode, pMode);
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceType(
          UINT       Adapter,
          D3DDEVTYPE DevType,
          D3DFORMAT  AdapterFormat,
          D3DFORMAT  BackBufferFormat,
          BOOL       bWindowed) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::CheckDeviceType(
      EnumerateFormat(AdapterFormat),
      EnumerateFormat(BackBufferFormat),
      bWindowed);
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceFormat(
          UINT            Adapter,
          D3DDEVTYPE      DeviceType,
          D3DFORMAT       AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE RType,
          D3DFORMAT       CheckFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::CheckDeviceFormat(
      EnumerateFormat(AdapterFormat),
      Usage, RType,
      EnumerateFormat(CheckFormat));
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceMultiSampleType(
          UINT                Adapter,
          D3DDEVTYPE          DeviceType,
          D3DFORMAT           SurfaceFormat,
          BOOL                Windowed,
          D3DMULTISAMPLE_TYPE MultiSampleType,
          DWORD*              pQualityLevels) { 
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::CheckDeviceMultiSampleType(
      EnumerateFormat(SurfaceFormat),
      Windowed,
      MultiSampleType, pQualityLevels);
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDepthStencilMatch(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DFORMAT  AdapterFormat,
          D3DFORMAT  RenderTargetFormat,
          D3DFORMAT  DepthStencilFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::CheckDepthStencilMatch(
      EnumerateFormat(AdapterFormat),
      EnumerateFormat(RenderTargetFormat),
      EnumerateFormat(DepthStencilFormat));
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceFormatConversion(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DFORMAT  SourceFormat,
          D3DFORMAT  TargetFormat) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::CheckDeviceFormatConversion(
      EnumerateFormat(SourceFormat),
      EnumerateFormat(TargetFormat));
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetDeviceCaps(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DCAPS9*  pCaps) {
    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    return caps::GetDeviceCaps(m_d3d9Options, Adapter, DeviceType, pCaps);
  }


  HMONITOR STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterMonitor(UINT Adapter) {
    if (Adapter >= this->GetAdapterCount())
      return nullptr;

    return GetDefaultMonitor();
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CreateDevice(
          UINT                   Adapter,
          D3DDEVTYPE             DeviceType,
          HWND                   hFocusWindow,
          DWORD                  BehaviorFlags,
          D3DPRESENT_PARAMETERS* pPresentationParameters,
          IDirect3DDevice9**     ppReturnedDeviceInterface) {
    return this->CreateDeviceEx(
      Adapter,
      DeviceType,
      hFocusWindow,
      BehaviorFlags,
      pPresentationParameters,
      nullptr, // <-- pFullscreenDisplayMode
      reinterpret_cast<IDirect3DDevice9Ex**>(ppReturnedDeviceInterface));
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::EnumAdapterModes(
          UINT            Adapter,
          D3DFORMAT       Format,
          UINT            Mode,
          D3DDISPLAYMODE* pMode) {
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    D3DDISPLAYMODEFILTER filter;
    filter.Format           = Format;
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    filter.Size             = sizeof(D3DDISPLAYMODEFILTER);

    D3DDISPLAYMODEEX modeEx;
    HRESULT hr = this->EnumAdapterModesEx(Adapter, &filter, Mode, &modeEx);

    if (FAILED(hr))
      return hr;

    pMode->Width       = modeEx.Width;
    pMode->Height      = modeEx.Height;
    pMode->RefreshRate = modeEx.RefreshRate;
    pMode->Format      = modeEx.Format;

    return D3D_OK;
  }


  // Ex Methods


  UINT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterModeCountEx(UINT Adapter, CONST D3DDISPLAYMODEFILTER* pFilter) {
    if (pFilter == nullptr)
      return 0;

    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    // We don't offer any interlaced formats here so early out and avoid destroying mode cache.
    if (pFilter->ScanLineOrdering == D3DSCANLINEORDERING_INTERLACED)
      return 0;

    CacheModes(EnumerateFormat(pFilter->Format));
    return m_modes.size();
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::EnumAdapterModesEx(
          UINT                  Adapter,
    const D3DDISPLAYMODEFILTER* pFilter,
          UINT                  Mode,
          D3DDISPLAYMODEEX*     pMode) {
    if (pMode == nullptr || pFilter == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9Format format =
      EnumerateFormat(pFilter->Format);

    if (Adapter >= this->GetAdapterCount())
      return D3DERR_INVALIDCALL;

    if (!IsSupportedMonitorFormat(format, FALSE))
      return D3DERR_INVALIDCALL;

    CacheModes(format);

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


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterDisplayModeEx(
          UINT                Adapter,
          D3DDISPLAYMODEEX*   pMode,
          D3DDISPLAYROTATION* pRotation) {
    D3DDISPLAYMODEFILTER filter;
    filter.Size             = sizeof(D3DDISPLAYMODEFILTER);
    filter.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    filter.Format           = D3DFMT_X8R8G8B8;

    return this->EnumAdapterModesEx(Adapter, &filter, 0, pMode);
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CreateDeviceEx(
          UINT                   Adapter,
          D3DDEVTYPE             DeviceType,
          HWND                   hFocusWindow,
          DWORD                  BehaviorFlags,
          D3DPRESENT_PARAMETERS* pPresentationParameters,
          D3DDISPLAYMODEEX*      pFullscreenDisplayMode,
          IDirect3DDevice9Ex**   ppReturnedDeviceInterface) {
    InitReturnPtr(ppReturnedDeviceInterface);

    if (ppReturnedDeviceInterface == nullptr
    || pPresentationParameters    == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = m_instance->enumAdapters(Adapter);

    if (dxvkAdapter == nullptr)
      return D3DERR_INVALIDCALL;

    std::string clientApi = str::format("D3D9", m_extended ? "Ex" : "");
    auto dxvkDevice = dxvkAdapter->createDevice(clientApi, D3D9DeviceEx::GetDeviceFeatures(dxvkAdapter));

    *ppReturnedDeviceInterface = ref(new D3D9DeviceEx(
      this,
      Adapter,
      DeviceType,
      hFocusWindow,
      BehaviorFlags,
      pFullscreenDisplayMode,
      m_extended,
      dxvkAdapter,
      dxvkDevice));

    HRESULT hr = (*ppReturnedDeviceInterface)->Reset(pPresentationParameters);

    if (FAILED(hr)) {
      Logger::warn("D3D9InterfaceEx::CreateDeviceEx: device initial reset failed.");
      *ppReturnedDeviceInterface = nullptr;
      return hr;
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterLUID(UINT Adapter, LUID* pLUID) {
    if (pLUID == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = m_instance->enumAdapters(Adapter);

    if (dxvkAdapter == nullptr)
      return D3DERR_INVALIDCALL;

    std::memcpy(pLUID, &dxvkAdapter->devicePropertiesExt().coreDeviceId.deviceLUID, sizeof(LUID));

    return D3D_OK;
  }


  void D3D9InterfaceEx::CacheModes(D3D9Format Format) {
    if (!m_modes.empty() && m_modeCacheFormat == Format)
      return; // We already cached the modes for this format. No need to do it again.

    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(GetDefaultMonitor(), reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("D3D9InterfaceEx::CacheModes: failed to query monitor info");
      return;
    }

    m_modes.clear();
    m_modeCacheFormat = Format;

    // Skip unsupported formats
    if (!IsSupportedMonitorFormat(Format, FALSE))
      return;

    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODEW devMode;

    uint32_t modeIndex = 0;

    while (::EnumDisplaySettingsW(monInfo.szDevice, modeIndex++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;

      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetMonitorFormatBpp(Format))
        continue;

      D3DDISPLAYMODEEX mode;
      mode.Size             = sizeof(D3DDISPLAYMODEEX);
      mode.Width            = devMode.dmPelsWidth;
      mode.Height           = devMode.dmPelsHeight;
      mode.RefreshRate      = devMode.dmDisplayFrequency;
      mode.Format           = static_cast<D3DFORMAT>(Format);
      mode.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

      m_modes.push_back(mode);
    }

    // Sort display modes by width, height and refresh rate,
    // in that order. Some games rely on correct ordering.
    std::sort(m_modes.begin(), m_modes.end(),
      [](const D3DDISPLAYMODEEX & a, const D3DDISPLAYMODEEX & b) {
        if (a.Width < b.Width)   return true;
        if (a.Width > b.Width)   return false;
        
        if (a.Height < b.Height) return true;
        if (a.Height > b.Height) return false;
        
        return a.RefreshRate < b.RefreshRate;
    });
  }


  const char* D3D9InterfaceEx::GetDriverDllName(DxvkGpuVendor vendor) {
    switch (vendor) {
      default:
      case DxvkGpuVendor::Nvidia: return "nvd3dum.dll";

#if defined(__x86_64__) || defined(_M_X64)
      case DxvkGpuVendor::Amd:    return "aticfx64.dll";
      case DxvkGpuVendor::Intel:  return "igdumd64.dll";
#else
      case DxvkGpuVendor::Amd:    return "aticfx32.dll";
      case DxvkGpuVendor::Intel:  return "igdumd32.dll";
#endif
    }
  }

}