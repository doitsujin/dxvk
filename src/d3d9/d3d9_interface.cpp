#include "d3d9_interface.h"

#include "d3d9_monitor.h"
#include "d3d9_caps.h"
#include "d3d9_device.h"

#include <algorithm>

namespace dxvk {

  D3D9InterfaceEx::D3D9InterfaceEx(bool bExtended)
    : m_instance    ( new DxvkInstance() )
    , m_extended    ( bExtended ) 
    , m_d3d9Options ( nullptr, m_instance->config() ) {
    m_adapters.reserve(m_instance->adapterCount());
    for (uint32_t i = 0; i < m_instance->adapterCount(); i++)
      m_adapters.emplace_back(this, m_instance->enumAdapters(i), i);

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
    return UINT(m_adapters.size());
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterIdentifier(
          UINT                    Adapter,
          DWORD                   Flags,
          D3DADAPTER_IDENTIFIER9* pIdentifier) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetAdapterIdentifier(Flags, pIdentifier);

    return D3DERR_INVALIDCALL;
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
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->CheckDeviceType(
        DevType, EnumerateFormat(AdapterFormat),
        EnumerateFormat(BackBufferFormat), bWindowed);

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceFormat(
          UINT            Adapter,
          D3DDEVTYPE      DeviceType,
          D3DFORMAT       AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE RType,
          D3DFORMAT       CheckFormat) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->CheckDeviceFormat(
        DeviceType, EnumerateFormat(AdapterFormat),
        Usage, RType,
        EnumerateFormat(CheckFormat));

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceMultiSampleType(
          UINT                Adapter,
          D3DDEVTYPE          DeviceType,
          D3DFORMAT           SurfaceFormat,
          BOOL                Windowed,
          D3DMULTISAMPLE_TYPE MultiSampleType,
          DWORD*              pQualityLevels) { 
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->CheckDeviceMultiSampleType(
        DeviceType, EnumerateFormat(SurfaceFormat),
        Windowed, MultiSampleType,
        pQualityLevels);

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDepthStencilMatch(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DFORMAT  AdapterFormat,
          D3DFORMAT  RenderTargetFormat,
          D3DFORMAT  DepthStencilFormat) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->CheckDepthStencilMatch(
        DeviceType, EnumerateFormat(AdapterFormat),
        EnumerateFormat(RenderTargetFormat),
        EnumerateFormat(DepthStencilFormat));

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::CheckDeviceFormatConversion(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DFORMAT  SourceFormat,
          D3DFORMAT  TargetFormat) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->CheckDeviceFormatConversion(
        DeviceType, EnumerateFormat(SourceFormat),
        EnumerateFormat(TargetFormat));

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetDeviceCaps(
          UINT       Adapter,
          D3DDEVTYPE DeviceType,
          D3DCAPS9*  pCaps) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetDeviceCaps(
        DeviceType, pCaps);

    return D3DERR_INVALIDCALL;
  }


  HMONITOR STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterMonitor(UINT Adapter) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetMonitor();

    return nullptr;
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
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetAdapterModeCountEx(pFilter);

    return 0;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::EnumAdapterModesEx(
          UINT                  Adapter,
    const D3DDISPLAYMODEFILTER* pFilter,
          UINT                  Mode,
          D3DDISPLAYMODEEX*     pMode) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->EnumAdapterModesEx(pFilter, Mode, pMode);

    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterDisplayModeEx(
          UINT                Adapter,
          D3DDISPLAYMODEEX*   pMode,
          D3DDISPLAYROTATION* pRotation) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetAdapterDisplayModeEx(pMode, pRotation);

    return D3DERR_INVALIDCALL;
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

    auto* adapter = GetAdapter(Adapter);

    if (adapter == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = adapter->GetDXVKAdapter();

    std::string clientApi = str::format("D3D9", m_extended ? "Ex" : "");
    auto dxvkDevice = dxvkAdapter->createDevice(clientApi, D3D9DeviceEx::GetDeviceFeatures(dxvkAdapter));

    *ppReturnedDeviceInterface = ref(new D3D9DeviceEx(
      this,
      adapter,
      DeviceType,
      hFocusWindow,
      BehaviorFlags,
      pFullscreenDisplayMode,
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
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetAdapterLUID(pLUID);

    return D3DERR_INVALIDCALL;
  }

}