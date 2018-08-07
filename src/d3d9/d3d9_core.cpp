#include "d3d9_core.h"

#define CHECK_ADAPTER(adapter) { if (!ValidAdapter(adapter)) { return D3DERR_INVALIDCALL; } }
#define CHECK_DEV_TYPE(ty) { if (ty != D3DDEVTYPE_HAL) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
  static void FillCaps(UINT adapter, D3DCAPS9& caps);
  static bool SupportedModeFormat(D3DFORMAT Format);

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

    auto& mode = *pMode;

    // We don't really known nor care what the real screen format is,
    // since modern GPUs can handle render targets in another format.
    // WineD3D does something similar.
    mode.Format = D3DFMT_X8R8G8B8;

    // Fill in the current width / height.
    // TODO: this returns the maximum / native monitor resolution,
    // but not the current one. We should fix this.
    GetAdapter(Adapter).GetMode(0, mode);

    return D3D_OK;
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
    D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // In principle, on Vulkan / D3D11 hardware (modern GPUs),
    // all of the formats and features should be supported.
    return D3D_OK;
  }

  HRESULT Direct3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT SurfaceFormat, BOOL,
    D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // Note: we ignore the `windowed` parameter, since Vulkan doesn't care.

    // D3D11-level hardware guarantees at least 8x multisampling
    // for the formats we're interested in.

    // TODO: we should at least validate the SurfaceFormat parameter.

    // TODO: we should use ID3D11Device::CheckMultisampleQualityLevels
    // to support AA modes > 8 samples.

    if (pQualityLevels) {
      // We don't mess with quality levels:
      // we either support a certain AA sample count, or we don't.
      *pQualityLevels = 1;
    }

    if (MultiSampleType > 16)
      return D3DERR_INVALIDCALL;

    const UINT sampleCount = MultiSampleType;

    // TODO: we could try to round up the other non-power-of-two-values,
    // instead of not supporting them.
    switch (sampleCount) {
      case 1:
        return S_OK;
      case 2:
        return S_OK;
      case 4:
        return S_OK;
      case 8:
        return S_OK;
      default:
        return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
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
    CHECK_NOT_NULL(pCaps);

    auto& caps = *pCaps;

    FillCaps(Adapter, caps);

    return D3D_OK;
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

  // Fills a D3D9 capabilities structure.
  static void FillCaps(UINT adapter, D3DCAPS9& caps) {
    // All Vulkan-capable devices already support pretty much all the D3D9 features,
    // which is why we fill this out without checking for support.

    // First, zero this structure out.
    caps = D3DCAPS9{};

    caps.DeviceType = D3DDEVTYPE_HAL;
    caps.AdapterOrdinal = adapter;

    caps.Caps = 0;

    caps.Caps2 = D3DCAPS2_CANAUTOGENMIPMAP
      | D3DCAPS2_CANCALIBRATEGAMMA
      | D3DCAPS2_FULLSCREENGAMMA
      // TODO: D3D9Ex only: D3DCAPS2_CANSHARERESOURCE
      | D3DCAPS2_CANMANAGERESOURCE
      | D3DCAPS2_DYNAMICTEXTURES;

    caps.Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD
      | D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM
      // TODO: D3D9Ex D3DCAPS3_DXVAHD
      | D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION;

    // Enable all present intervals.
    caps.PresentationIntervals = D3DPRESENT_INTERVAL_IMMEDIATE | 0b1111;

    caps.CursorCaps = D3DCURSORCAPS_COLOR
      | D3DCURSORCAPS_LOWRES;

    caps.DevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL
      | D3DDEVCAPS_CANRENDERAFTERFLIP
      // These flags indicate hardware which is at least DirectX 5 / 7 compatible.
      | D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX
      | D3DDEVCAPS_DRAWPRIMTLVERTEX
      // Not sure what these flags indicate, but we probably support them anyhow.
      | D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY
      | D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_HWTRANSFORMANDLIGHT
      | D3DDEVCAPS_PUREDEVICE
      /*
      TODO: determine what these refer to, and enable them if possible.
      | D3DDEVCAPS_NPATCHES | D3DDEVCAPS_QUINTICRTPATCHES
      | D3DDEVCAPS_RTPATCHES | D3DDEVCAPS_RTPATCHHANDLEZERO
      */
      | D3DDEVCAPS_SEPARATETEXTUREMEMORIES | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
      | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY
      | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    caps.PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ
      // In theory we could support both cull modes,
      // but it's better to just use the default one.
      | D3DPMISCCAPS_CULLCW
      | D3DPMISCCAPS_COLORWRITEENABLE
      | D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
      | D3DPMISCCAPS_CLIPTLVERTS
      | D3DPMISCCAPS_BLENDOP
      // Modern hardware supports using textures in all stages.
      | D3DPMISCCAPS_TSSARGTEMP
      | D3DPMISCCAPS_INDEPENDENTWRITEMASKS
      | D3DPMISCCAPS_PERSTAGECONSTANT
      | D3DPMISCCAPS_POSTBLENDSRGBCONVERT
      | D3DPMISCCAPS_FOGANDSPECULARALPHA
      | D3DPMISCCAPS_SEPARATEALPHABLEND
      | D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS
      | D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING;

    caps.RasterCaps = D3DPRASTERCAPS_ANISOTROPY
      | D3DPRASTERCAPS_COLORPERSPECTIVE
      // We don't need dithering on modern displays,
      // but it doesn't hurt to advertise support for this.
      | D3DPRASTERCAPS_DITHER
      | D3DPRASTERCAPS_DEPTHBIAS
      | D3DPRASTERCAPS_FOGRANGE
      | D3DPRASTERCAPS_FOGVERTEX
      | D3DPRASTERCAPS_MIPMAPLODBIAS
      // TODO: We can't really support this efficiently on D3D11.
      // Don't know if any games would require this to be emulated.
      // D3DPRASTERCAPS_MULTISAMPLE_TOGGLE
      | D3DPRASTERCAPS_SCISSORTEST
      | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS
      // TODO: Not sure if we can support the ones below:
      //| D3DPRASTERCAPS_WBUFFER
      //| D3DPRASTERCAPS_WFOG
      | D3DPRASTERCAPS_ZBUFFERLESSHSR
      | D3DPRASTERCAPS_ZFOG
      // Depth queries.
      | D3DPRASTERCAPS_ZTEST;

    // We support everything, basically.
    caps.AlphaCmpCaps = caps.ZCmpCaps = (1 << 8) - 1;

    // Modern hardware supports mostly everything.
    caps.DestBlendCaps = caps.SrcBlendCaps = (1 << 16) - 1;

    caps.ShadeCaps = D3DPSHADECAPS_ALPHAGOURAUDBLEND
      | D3DPSHADECAPS_COLORGOURAUDRGB
      | D3DPSHADECAPS_FOGGOURAUD
      | D3DPSHADECAPS_SPECULARGOURAUDRGB;

    // Support for everything
    caps.TextureCaps = ((1 << 22) - 1)
      // This cap indicates lack of support, so we mask it.
      & (~D3DPTEXTURECAPS_NOPROJECTEDBUMPENV);

    // All the filters.
    caps.TextureFilterCaps = caps.CubeTextureFilterCaps =
      caps.VolumeTextureFilterCaps = (1 << 29) - 1;

    caps.TextureAddressCaps =
      caps.VolumeTextureAddressCaps = (1 << 6) - 1;

    // All the features.
    caps.LineCaps = (1 << 6) - 1;

    // The OpenGL 4.1 spec guarantees this minimum max texture size.
    caps.MaxTextureWidth = caps.MaxTextureHeight = caps.MaxVolumeExtent = 16384;

    // Not sure what this maximum should be, just leave the maximum possible.
    caps.MaxTextureRepeat = UINT_MAX;

    // Max ratio would be 16384 by 1.
    caps.MaxTextureAspectRatio = 16384;

    caps.MaxAnisotropy = 16;

    caps.MaxVertexW = 16777216.f;

    caps.GuardBandLeft = 0;
    caps.GuardBandTop = 0;
    caps.GuardBandRight = 0;
    caps.GuardBandBottom = 0;

    caps.ExtentsAdjust = 0;

    caps.StencilCaps = (1 << 9) - 1;

    // We support flexible vertex format capabilities.
    caps.FVFCaps = D3DFVFCAPS_PSIZE
      // This indicates we support up to 8 texture coordinates in a vertex shader.
      | (((1 << 16) - 1) & D3DFVFCAPS_TEXCOORDCOUNTMASK);

    // Enable all the bits.
    caps.TextureOpCaps = (~0);

    // These are pretty much unlimited on modern hardware,
    // so we set some very big numbers here to indicate that.
    caps.MaxTextureBlendStages = caps.MaxSimultaneousTextures =
      caps.MaxActiveLights = caps.MaxUserClipPlanes = caps.MaxVertexBlendMatrices =
      caps.MaxVertexBlendMatrixIndex = (1 << 31);

    caps.VertexProcessingCaps = (1 << 9) - 1;

    // We've no way to query how big this is on modern hardware,
    // but it's safe to assume it's pretty big.
    caps.MaxPointSize = 16384.f;

    caps.MaxPrimitiveCount = caps.MaxVertexIndex = UINT_MAX;

    // Valid range is 1 through 16, we expose the max.
    caps.MaxStreams = 16;

    // Some large power of two.
    caps.MaxStreamStride = 1 << 31;

    caps.VertexShaderVersion = D3DVS_VERSION(3, 0);
    // This the max you can get in D3D11.
    caps.MaxVertexShaderConst = 1 << 16;

    caps.PixelShaderVersion = D3DPS_VERSION(3, 0);
    caps.PixelShader1xMaxValue = 65536.f;

    // All of the caps!
    caps.DevCaps2 = (1 << 7) - 1;

    caps.MaxNpatchTessellationLevel = 256;

    // We don't support adapter groups / multi-GPU configs.
    // Just report each GPU as independent.
    caps.MasterAdapterOrdinal = adapter;
    caps.NumberOfAdaptersInGroup = 1;
    caps.AdapterOrdinalInGroup = 0;

    // Support all types.
    caps.DeclTypes = (1 << 10) - 1;

    // There is no real limit on modern GPUs, except for available VRAM.
    // Limit this to a reasonable number.
    caps.NumSimultaneousRTs = 64;

    // All the possible filters.
    caps.StretchRectFilterCaps = (~0);

    auto& vsc = caps.VS20Caps;
    vsc.Caps = ~0;
    vsc.DynamicFlowControlDepth = 24;
    vsc.NumTemps = 16384;
    // Practically infinite, just give a nice big number here.
    vsc.StaticFlowControlDepth = 1 << 24;

    auto& psc = caps.PS20Caps;
    psc.Caps = ~0;
    psc.DynamicFlowControlDepth = 24;
    psc.NumTemps = 16384;
    psc.StaticFlowControlDepth = 1 << 24;
    psc.NumInstructionSlots = 1 << 31;

    caps.VertexTextureFilterCaps = ~0;

    caps.MaxVShaderInstructionsExecuted = UINT_MAX;
    caps.MaxPShaderInstructionsExecuted = UINT_MAX;

    // Set this to the max possible value.
    caps.MaxVertexShader30InstructionSlots = 32768;
    caps.MaxPixelShader30InstructionSlots = 32768;
  }
}
