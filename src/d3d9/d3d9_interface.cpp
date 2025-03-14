#include "d3d9_interface.h"

#include "d3d9_monitor.h"
#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_bridge.h"

#include "../util/util_singleton.h"

#include <algorithm>

namespace dxvk {

  Singleton<DxvkInstance> g_dxvkInstance;

  D3D9InterfaceEx::D3D9InterfaceEx(bool bExtended)
    : m_instance    ( g_dxvkInstance.acquire(DxvkInstanceFlag::ClientApiIsD3D9) )
    , m_d3d8Bridge  ( this )
    , m_extended    ( bExtended ) 
    , m_d3d9Options ( nullptr, m_instance->config() )
    , m_d3d9Interop ( this ) {
    // D3D9 doesn't enumerate adapters like physical adapters...
    // only as connected displays.

    // Let's create some "adapters" for the amount of displays we have.
    // We'll go through and match up displays -> our adapters in order.
    // If we run out of adapters, then we'll just make repeats of the first one.
    // We can't match up by names on Linux/Wine as they don't match at all
    // like on Windows, so this is our best option.
#ifdef _WIN32
    if (m_d3d9Options.enumerateByDisplays) {
      DISPLAY_DEVICEA device = { };
      device.cb = sizeof(device);

      uint32_t adapterOrdinal = 0;
      uint32_t i = 0;
      while (::EnumDisplayDevicesA(nullptr, i++, &device, 0)) {
        // If we aren't attached, skip over.
        if (!(device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
          continue;

        // If we are a mirror, skip over this device.
        if (device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
          continue;

        Rc<DxvkAdapter> adapter = adapterOrdinal >= m_instance->adapterCount()
          ? m_instance->enumAdapters(0)
          : m_instance->enumAdapters(adapterOrdinal);

        if (adapter != nullptr)
          m_adapters.emplace_back(this, adapter, adapterOrdinal++, i - 1);
      }
    }
    else
#endif
    {
      const uint32_t adapterCount = m_instance->adapterCount();
      m_adapters.reserve(adapterCount);

      for (uint32_t i = 0; i < adapterCount; i++)
        m_adapters.emplace_back(this, m_instance->enumAdapters(i), i, 0);
    }

#ifdef _WIN32
    if (m_d3d9Options.dpiAware) {
      Logger::info("Process set as DPI aware");
      SetProcessDPIAware();
    }
#endif

    if (unlikely(m_d3d9Options.shaderModel == 0))
      Logger::warn("D3D9InterfaceEx: WARNING! Fixed-function exclusive mode is enabled.");
  }


  D3D9InterfaceEx::~D3D9InterfaceEx() {
    g_dxvkInstance.release();
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

    if (riid == __uuidof(IDxvkD3D8InterfaceBridge)) {
      *ppvObject = ref(&m_d3d8Bridge);
      return S_OK;
    }

    if (riid == __uuidof(ID3D9VkInteropInterface)
     || riid == __uuidof(ID3D9VkInteropInterface1)) {
      *ppvObject = ref(&m_d3d9Interop);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDirect3D9), riid)) {
      Logger::warn("D3D9InterfaceEx::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
    if (auto* adapter = GetAdapter(Adapter)) {
      D3DDISPLAYMODEEX modeEx = { };
      modeEx.Size = sizeof(D3DDISPLAYMODEEX);
      HRESULT hr = adapter->GetAdapterDisplayModeEx(&modeEx, nullptr);

      if (FAILED(hr))
        return hr;

      pMode->Width       = modeEx.Width;
      pMode->Height      = modeEx.Height;
      pMode->RefreshRate = modeEx.RefreshRate;
      pMode->Format      = modeEx.Format;

      return D3D_OK;
    }

    return D3DERR_INVALIDCALL;
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

    D3DDISPLAYMODEEX modeEx = { };
    modeEx.Size = sizeof(D3DDISPLAYMODEEX);
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

    if (unlikely(ppReturnedDeviceInterface  == nullptr
              || pPresentationParameters    == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(DeviceType == D3DDEVTYPE_SW))
      return D3DERR_INVALIDCALL;

    // D3DDEVTYPE_REF devices can be created with D3D8, but not
    // with D3D9, unless the Windows SDK 8.0 or later is installed.
    // Report it unavailable, as it would be on most end-user systems.
    if (unlikely(DeviceType == D3DDEVTYPE_REF && !m_isD3D8Compatible))
      return D3DERR_NOTAVAILABLE;

    // Creating a device with D3DCREATE_PUREDEVICE only works in conjunction
    // with D3DCREATE_HARDWARE_VERTEXPROCESSING on native drivers.
    if (unlikely(BehaviorFlags & D3DCREATE_PUREDEVICE &&
               !(BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING)))
      return D3DERR_INVALIDCALL;

    HRESULT hr;
    // Black Desert creates a D3DDEVTYPE_NULLREF device and
    // expects it be created despite passing invalid parameters.
    if (likely(DeviceType != D3DDEVTYPE_NULLREF)) {
      hr = ValidatePresentationParameters(pPresentationParameters);

      if (unlikely(FAILED(hr)))
        return hr;
    }

    auto* adapter = GetAdapter(Adapter);

    if (adapter == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = adapter->GetDXVKAdapter();

    try {
      auto dxvkDevice = dxvkAdapter->createDevice(m_instance, D3D9DeviceEx::GetDeviceFeatures(dxvkAdapter));

      auto* device = new D3D9DeviceEx(
        this,
        adapter,
        DeviceType,
        hFocusWindow,
        BehaviorFlags,
        dxvkDevice);

      hr = device->InitialReset(pPresentationParameters, pFullscreenDisplayMode);

      if (unlikely(FAILED(hr)))
        return hr;

      *ppReturnedDeviceInterface = ref(device);
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9InterfaceEx::GetAdapterLUID(UINT Adapter, LUID* pLUID) {
    if (auto* adapter = GetAdapter(Adapter))
      return adapter->GetAdapterLUID(pLUID);

    return D3DERR_INVALIDCALL;
  }


  HRESULT D3D9InterfaceEx::ValidatePresentationParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (m_extended) {
      // The swap effect value on a D3D9Ex device
      // can not be higher than D3DSWAPEFFECT_FLIPEX.
      if (unlikely(pPresentationParameters->SwapEffect > D3DSWAPEFFECT_FLIPEX))
        return D3DERR_INVALIDCALL;

      // 30 is the highest supported back buffer count for Ex devices.
      if (unlikely(pPresentationParameters->BackBufferCount > D3DPRESENT_BACK_BUFFERS_MAX_EX))
        return D3DERR_INVALIDCALL;
    } else {
      // The swap effect value on a non-Ex D3D9 device
      // can not be higher than D3DSWAPEFFECT_COPY.
      if (unlikely(pPresentationParameters->SwapEffect > D3DSWAPEFFECT_COPY))
        return D3DERR_INVALIDCALL;

      // 3 is the highest supported back buffer count for non-Ex devices.
      if (unlikely(pPresentationParameters->BackBufferCount > D3DPRESENT_BACK_BUFFERS_MAX))
        return D3DERR_INVALIDCALL;
    }

    // The swap effect value can not be 0.
    if (unlikely(!pPresentationParameters->SwapEffect))
      return D3DERR_INVALIDCALL;

    // D3DSWAPEFFECT_COPY can not be used with more than one back buffer.
    // Allow D3DSWAPEFFECT_COPY to bypass this restriction in D3D8 compatibility
    // mode, since it may be a remapping of D3DSWAPEFFECT_COPY_VSYNC and RC Cars
    // depends on it not being validated.
    if (unlikely(!IsD3D8Compatible()
              && pPresentationParameters->SwapEffect == D3DSWAPEFFECT_COPY
              && pPresentationParameters->BackBufferCount > 1))
      return D3DERR_INVALIDCALL;

    // Valid fullscreen presentation intervals must be known values.
    if (unlikely(!pPresentationParameters->Windowed
            && !(pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_ONE
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_TWO
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_THREE
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_FOUR
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE)))
      return D3DERR_INVALIDCALL;

    // In windowed mode, only a subset of the presentation interval flags can be used.
    if (unlikely(pPresentationParameters->Windowed
            && !(pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_ONE
              || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }

}