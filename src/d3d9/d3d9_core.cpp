#include "d3d9_core.h"

#include "../util/log/log.h"
#include "../util/com/com_pointer.h"
#include "../util/util_string.h"
#include "../util/util_error.h"

#define CHECK_ADAPTER(adapter) { if (!ValidAdapter(adapter)) { return D3DERR_INVALIDCALL; } }
#define CHECK_DEV_TYPE(ty) { if (ty != D3DDEVTYPE_HAL) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
  Direct3D9::Direct3D9() {
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&m_factory))))
      throw DxvkError("Failed to create DXGI factory");

    UINT i = 0;
    IDXGIAdapter1* adapter;
    while (m_factory->EnumAdapters1(i++, &adapter) != DXGI_ERROR_NOT_FOUND) {
      m_adapters.emplace_back(Com(adapter));
    }
  }

  Direct3D9::~Direct3D9() = default;

  bool Direct3D9::ValidAdapter(UINT adapter) {
    return adapter < m_adapters.size();
  }

  D3D9Adapter& Direct3D9::GetAdapter(UINT adapter) {
    return m_adapters[adapter];
  }

  HRESULT Direct3D9::RegisterSoftwareDevice(void*) {
    // Applications would call this if there aren't any GPUs available
    // and want to fall back to software rasterization.
    Logger::info("Ignoring RegisterSoftwareDevice: software rasterizers are not supported");

    // Since we know we always have at least one Vulkan GPU,
    // we simply fake success.
    return D3D_OK;
  }

  UINT Direct3D9::GetAdapterCount() {
    return m_adapters.size();
  }

  HRESULT Direct3D9::GetAdapterIdentifier(UINT Adapter,
    DWORD, D3DADAPTER_IDENTIFIER9* pIdentifier) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pIdentifier);

    // Note: we ignore the second parameter, Flags, since
    // checking if the driver is WHQL'd is irrelevant to Wine.

    auto& ident = *pIdentifier;

    return GetAdapter(Adapter).GetIdentifier(ident);
  }

  static bool SupportedModeFormat(D3DFORMAT Format) {
    // This is the list of back buffer formats which D3D9 accepts.
    // These formats are supported on pretty much all modern GPUs,
    // so we don't do any checks for them.
    switch (Format) {
      case D3DFMT_A1R5G5B5:
      case D3DFMT_A2R10G10B10:
      case D3DFMT_A8R8G8B8:
      case D3DFMT_R5G6B5:
      case D3DFMT_X1R5G5B5:
      case D3DFMT_X8R8G8B8:
        return true;
      default:
        Logger::err(str::format("Unsupported display mode format: ", Format));
        return false;
    }
  }

  UINT Direct3D9::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) {
    if (!ValidAdapter(Adapter))
      return 0;

    if (!SupportedModeFormat(Format))
      return 0;

    return GetAdapter(Adapter).GetModeCount();
  }

  HRESULT Direct3D9::EnumAdapterModes(UINT Adapter,
    D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pMode);

    if (!SupportedModeFormat(Format))
      return D3DERR_INVALIDCALL;

    auto& mode = *pMode;

    mode.Format = Format;

    GetAdapter(Adapter).GetMode(Mode, mode);

    return S_OK;
  }

  HRESULT Direct3D9::GetAdapterDisplayMode(UINT Adapter,
    D3DDISPLAYMODE* pMode) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pMode);

    Logger::trace("GetAdapterDisplayMode");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
    BOOL bWindowed) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    Logger::trace("CheckDeviceType");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, DWORD Usage,
    D3DRESOURCETYPE, D3DFORMAT CheckFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // In principle, on Vulkan / D3D11 hardware (modern GPUs),
    // all of the formats and features should be supported.
    // That is why we return OK here and fail later, if that's not the case.

    const auto usage = str::format("usage (", Usage, ")");
    const auto format = str::format("format (", CheckFormat, ")");

    Logger::warn(str::format("CheckDeviceFormat called with ", usage, ", ", format));
    Logger::warn("CheckDeviceFormat always reports all formats are supported");

    return D3D_OK;
  }

  HRESULT Direct3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT SurfaceFormat, BOOL,
    D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // Note: we ignore the `windowed` parameter, since Vulkan doesn't care.

    // TODO: Right now, we just say we don't support multisampling.
    // We should create a D3D11 Device and actually check for support.

    Logger::err("CheckDeviceMultiSampleType: multisampling not yet supported");

    return D3DERR_NOTAVAILABLE;
  }

  HRESULT Direct3D9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat,
    D3DFORMAT DepthStencilFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    Logger::trace("CheckDepthStencilMatch");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    Logger::trace("CheckDeviceFormatConversion");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DevType,
    D3DCAPS9* pCaps) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    Logger::trace("GetDeviceCaps");
    throw DxvkError("not supported");
  }

  HMONITOR Direct3D9::GetAdapterMonitor(UINT Adapter) {
    if (!ValidAdapter(Adapter))
      return nullptr;

    Logger::trace("GetAdapterMonitor");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DevType,
    HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);
    CHECK_NOT_NULL(ppReturnedDeviceInterface);

    Logger::trace("CreateDevice");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3D9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
}
