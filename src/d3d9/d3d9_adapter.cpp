#include "d3d9_adapter.h"

#include "d3d9_interface.h"
#include "d3d9_monitor.h"
#include "d3d9_caps.h"
#include "d3d9_util.h"

#include "../util/util_bit.h"
#include "../util/util_luid.h"
#include "../util/util_ratio.h"
#include "../util/util_string.h"

#include "../wsi/wsi_monitor.h"

#include <cfloat>

namespace dxvk {

  const char* GetDriverDLL(DxvkGpuVendor vendor) {
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


  D3D9Adapter::D3D9Adapter(
          D3D9InterfaceEx* pParent,
          Rc<DxvkAdapter>  Adapter,
          UINT             Ordinal,
          UINT             DisplayIndex)
  : m_parent          (pParent),
    m_adapter         (Adapter),
    m_ordinal         (Ordinal),
    m_displayIndex    (DisplayIndex),
    m_modeCacheFormat (D3D9Format::Unknown),
    m_d3d9Formats     (Adapter, m_parent->GetOptions()) {
    m_adapter->logAdapterInfo();
  }

  template <size_t N>
  static void copyToStringArray(char (&dst)[N], const char* src) {
    dxvk::str::strlcpy(dst, src, N);
  }


  HRESULT D3D9Adapter::GetAdapterIdentifier(
          DWORD                   Flags,
          D3DADAPTER_IDENTIFIER9* pIdentifier) {
    if (unlikely(pIdentifier == nullptr))
      return D3DERR_INVALIDCALL;

    auto& options = m_parent->GetOptions();
    
    const auto& props = m_adapter->deviceProperties();

    WCHAR wideDisplayName[32] = { };
    if (!wsi::getDisplayName(wsi::getDefaultMonitor(), wideDisplayName)) {
      Logger::err("D3D9Adapter::GetAdapterIdentifier: Failed to query monitor info");
      return D3DERR_INVALIDCALL;
    }

    std::string displayName = str::fromws(wideDisplayName);

    GUID guid          = bit::cast<GUID>(m_adapter->devicePropertiesExt().vk11.deviceUUID);

    uint32_t vendorId  = options.customVendorId == -1     ? props.vendorID   : uint32_t(options.customVendorId);
    uint32_t deviceId  = options.customDeviceId == -1     ? props.deviceID   : uint32_t(options.customDeviceId);
    const char*  desc  = options.customDeviceDesc.empty() ? props.deviceName : options.customDeviceDesc.c_str();
    const char* driver = GetDriverDLL(DxvkGpuVendor(vendorId));

    copyToStringArray(pIdentifier->Description, desc);
    copyToStringArray(pIdentifier->DeviceName,  displayName.c_str()); // The GDI device name. Not the actual device name.
    copyToStringArray(pIdentifier->Driver,      driver);            // This is the driver's dll.

    pIdentifier->DeviceIdentifier       = guid;
    pIdentifier->DeviceId               = deviceId;
    pIdentifier->VendorId               = vendorId;
    pIdentifier->Revision               = 0;
    pIdentifier->SubSysId               = 0;
    pIdentifier->WHQLLevel              = m_parent->IsExtended() ? 1 : 0; // This doesn't check with the driver on Direct3D9Ex and is always 1.
    pIdentifier->DriverVersion.QuadPart = INT64_MAX;

    return D3D_OK;
  }


  HRESULT D3D9Adapter::CheckDeviceType(
          D3DDEVTYPE DevType,
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       bWindowed) {
    if (!IsSupportedAdapterFormat(AdapterFormat))
      return D3DERR_NOTAVAILABLE;

    if (!IsSupportedBackBufferFormat(AdapterFormat, BackBufferFormat, bWindowed))
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }


  HRESULT D3D9Adapter::CheckDeviceFormat(
          D3DDEVTYPE      DeviceType,
          D3D9Format      AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE RType,
          D3D9Format      CheckFormat) {
    if (!IsSupportedAdapterFormat(AdapterFormat))
      return D3DERR_NOTAVAILABLE;

    const bool dmap = Usage & D3DUSAGE_DMAP;
    const bool rt   = Usage & D3DUSAGE_RENDERTARGET;
    const bool ds   = Usage & D3DUSAGE_DEPTHSTENCIL;

    const bool surface = RType == D3DRTYPE_SURFACE;
    const bool texture = RType == D3DRTYPE_TEXTURE;

    const bool twoDimensional = surface || texture;

    const bool srgb = (Usage & (D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE)) != 0;

    if (CheckFormat == D3D9Format::INST)
      return D3D_OK;

    if (rt && CheckFormat == D3D9Format::A8 && m_parent->GetOptions().disableA8RT)
      return D3DERR_NOTAVAILABLE;

    if (ds && !IsDepthFormat(CheckFormat))
      return D3DERR_NOTAVAILABLE;

    if (rt && CheckFormat == D3D9Format::NULL_FORMAT && twoDimensional)
      return D3D_OK;

    if (rt && CheckFormat == D3D9Format::RESZ && surface)
      return D3D_OK;

    if (CheckFormat == D3D9Format::ATOC && surface)
      return D3D_OK;

    if (CheckFormat == D3D9Format::NVDB && surface)
      return m_adapter->features().core.features.depthBounds
        ? D3D_OK
        : D3DERR_NOTAVAILABLE;

    // I really don't want to support this...
    if (dmap)
      return D3DERR_NOTAVAILABLE;

    auto mapping = m_d3d9Formats.GetFormatMapping(CheckFormat);
    if (mapping.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (mapping.FormatSrgb  == VK_FORMAT_UNDEFINED && srgb)
      return D3DERR_NOTAVAILABLE;

    if (RType == D3DRTYPE_VERTEXBUFFER || RType == D3DRTYPE_INDEXBUFFER)
      return D3D_OK;

    // Let's actually ask Vulkan now that we got some quirks out the way!
    return CheckDeviceVkFormat(mapping.FormatColor, Usage, RType);
  }


  HRESULT D3D9Adapter::CheckDeviceMultiSampleType(
        D3DDEVTYPE          DeviceType,
        D3D9Format          SurfaceFormat,
        BOOL                Windowed,
        D3DMULTISAMPLE_TYPE MultiSampleType,
        DWORD*              pQualityLevels) {
    if (pQualityLevels != nullptr)
      *pQualityLevels = 1;

    auto dst = ConvertFormatUnfixed(SurfaceFormat);
    if (dst.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (MultiSampleType != D3DMULTISAMPLE_NONE
     && (SurfaceFormat == D3D9Format::D32_LOCKABLE
      || SurfaceFormat == D3D9Format::D32F_LOCKABLE
      || SurfaceFormat == D3D9Format::D16_LOCKABLE
      || SurfaceFormat == D3D9Format::INTZ))
      return D3DERR_NOTAVAILABLE;

    uint32_t sampleCount = std::max<uint32_t>(MultiSampleType, 1u);

    // Check if this is a power of two...
    if (sampleCount & (sampleCount - 1))
      return D3DERR_NOTAVAILABLE;
    
    // Therefore...
    VkSampleCountFlags sampleFlags = VkSampleCountFlags(sampleCount);

    auto availableFlags = m_adapter->deviceProperties().limits.framebufferColorSampleCounts
                        & m_adapter->deviceProperties().limits.framebufferDepthSampleCounts;

    if (!(availableFlags & sampleFlags))
      return D3DERR_NOTAVAILABLE;

    if (pQualityLevels != nullptr) {
      if (MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
        *pQualityLevels = 32 - bit::lzcnt(availableFlags);
      else
        *pQualityLevels = 1;
    }

    return D3D_OK;
  }


  HRESULT D3D9Adapter::CheckDepthStencilMatch(
          D3DDEVTYPE DeviceType,
          D3D9Format AdapterFormat,
          D3D9Format RenderTargetFormat,
          D3D9Format DepthStencilFormat) {
    if (!IsDepthFormat(DepthStencilFormat))
      return D3DERR_NOTAVAILABLE;

    if (RenderTargetFormat == dxvk::D3D9Format::NULL_FORMAT)
      return D3D_OK;

    auto mapping = ConvertFormatUnfixed(RenderTargetFormat);
    if (mapping.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }


  HRESULT D3D9Adapter::CheckDeviceFormatConversion(
          D3DDEVTYPE DeviceType,
          D3D9Format SourceFormat,
          D3D9Format TargetFormat) {
    bool sourceSupported = SourceFormat != D3D9Format::Unknown
                        && IsSupportedBackBufferFormat(SourceFormat);
    bool targetSupported = TargetFormat == D3D9Format::X1R5G5B5
                        || TargetFormat == D3D9Format::A1R5G5B5
                        || TargetFormat == D3D9Format::R5G6B5
                     // || TargetFormat == D3D9Format::R8G8B8 <-- We don't support R8G8B8
                        || TargetFormat == D3D9Format::X8R8G8B8
                        || TargetFormat == D3D9Format::A8R8G8B8
                        || TargetFormat == D3D9Format::A2R10G10B10
                        || TargetFormat == D3D9Format::A16B16G16R16
                        || TargetFormat == D3D9Format::A2B10G10R10
                        || TargetFormat == D3D9Format::A8B8G8R8
                        || TargetFormat == D3D9Format::X8B8G8R8
                        || TargetFormat == D3D9Format::A16B16G16R16F
                        || TargetFormat == D3D9Format::A32B32G32R32F;

    return (sourceSupported && targetSupported)
      ? D3D_OK
      : D3DERR_NOTAVAILABLE;
  }


  HRESULT D3D9Adapter::GetDeviceCaps(
          D3DDEVTYPE DeviceType,
          D3DCAPS9*  pCaps) {
    using namespace dxvk::caps;

    if (pCaps == nullptr)
      return D3DERR_INVALIDCALL;

    auto& options = m_parent->GetOptions();

    // TODO: Actually care about what the adapter supports here.
    // ^ For Intel and older cards most likely here.

    // Device Type
    pCaps->DeviceType               = DeviceType;
    // Adapter Id
    pCaps->AdapterOrdinal           = m_ordinal;
    // Caps 1
    pCaps->Caps                     = D3DCAPS_READ_SCANLINE;
    // Caps 2
    pCaps->Caps2                    = D3DCAPS2_FULLSCREENGAMMA
                                 /* | D3DCAPS2_CANCALIBRATEGAMMA */
                                 /* | D3DCAPS2_RESERVED */
                                 /* | D3DCAPS2_CANMANAGERESOURCE */
                                    | D3DCAPS2_DYNAMICTEXTURES
                                    | D3DCAPS2_CANAUTOGENMIPMAP
                                 /* | D3DCAPS2_CANSHARERESOURCE */;
    // Caps 3
    pCaps->Caps3                    = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD
                                    | D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION
                                    | D3DCAPS3_COPY_TO_VIDMEM
                                    | D3DCAPS3_COPY_TO_SYSTEMMEM
                                 /* | D3DCAPS3_DXVAHD */
                                 /* | D3DCAPS3_DXVAHD_LIMITED */;
    // Presentation Intervals
    pCaps->PresentationIntervals    = D3DPRESENT_INTERVAL_DEFAULT
                                    | D3DPRESENT_INTERVAL_ONE
                                    | D3DPRESENT_INTERVAL_TWO
                                    | D3DPRESENT_INTERVAL_THREE
                                    | D3DPRESENT_INTERVAL_FOUR
                                    | D3DPRESENT_INTERVAL_IMMEDIATE;
    // Cursor
    pCaps->CursorCaps               = D3DCURSORCAPS_COLOR; // I do not support Cursor yet, but I don't want to say I don't support it for compatibility reasons.
    // Dev Caps
    pCaps->DevCaps                  = D3DDEVCAPS_EXECUTESYSTEMMEMORY
                                    | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                                    | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                                    | D3DDEVCAPS_TLVERTEXVIDEOMEMORY
                                 /* | D3DDEVCAPS_TEXTURESYSTEMMEMORY */
                                    | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                                    | D3DDEVCAPS_DRAWPRIMTLVERTEX
                                    | D3DDEVCAPS_CANRENDERAFTERFLIP
                                    | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
                                    | D3DDEVCAPS_DRAWPRIMITIVES2
                                 /* | D3DDEVCAPS_SEPARATETEXTUREMEMORIES */
                                    | D3DDEVCAPS_DRAWPRIMITIVES2EX
                                    | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                                    | D3DDEVCAPS_CANBLTSYSTONONLOCAL
                                    | D3DDEVCAPS_HWRASTERIZATION
                                    | D3DDEVCAPS_PUREDEVICE
                                 /* | D3DDEVCAPS_QUINTICRTPATCHES */
                                 /* | D3DDEVCAPS_RTPATCHES */
                                 /* | D3DDEVCAPS_RTPATCHHANDLEZERO */
                                 /* | D3DDEVCAPS_NPATCHES */;
    // Primitive Misc. Caps
    pCaps->PrimitiveMiscCaps        = D3DPMISCCAPS_MASKZ
                                    | D3DPMISCCAPS_CULLNONE
                                    | D3DPMISCCAPS_CULLCW
                                    | D3DPMISCCAPS_CULLCCW
                                    | D3DPMISCCAPS_COLORWRITEENABLE
                                    | D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
                                    | D3DPMISCCAPS_CLIPTLVERTS
                                    | D3DPMISCCAPS_TSSARGTEMP
                                    | D3DPMISCCAPS_BLENDOP
                                 /* | D3DPMISCCAPS_NULLREFERENCE */
                                    | D3DPMISCCAPS_INDEPENDENTWRITEMASKS
                                    | D3DPMISCCAPS_PERSTAGECONSTANT
                                    | D3DPMISCCAPS_FOGANDSPECULARALPHA
                                    | D3DPMISCCAPS_SEPARATEALPHABLEND
                                    | D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS
                                    | D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING
                                    | D3DPMISCCAPS_FOGVERTEXCLAMPED
                                    | D3DPMISCCAPS_POSTBLENDSRGBCONVERT;
    // Raster Caps
    pCaps->RasterCaps               = D3DPRASTERCAPS_DITHER
                                    | D3DPRASTERCAPS_ZTEST
                                    | D3DPRASTERCAPS_FOGVERTEX
                                    | D3DPRASTERCAPS_FOGTABLE
                                    | D3DPRASTERCAPS_MIPMAPLODBIAS
                                 /* | D3DPRASTERCAPS_ZBUFFERLESSHSR */
                                    | D3DPRASTERCAPS_FOGRANGE
                                    | D3DPRASTERCAPS_ANISOTROPY
                                 /* | D3DPRASTERCAPS_WBUFFER */
                                    | D3DPRASTERCAPS_WFOG
                                    | D3DPRASTERCAPS_ZFOG
                                    | D3DPRASTERCAPS_COLORPERSPECTIVE
                                    | D3DPRASTERCAPS_SCISSORTEST
                                    | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS
                                    | D3DPRASTERCAPS_DEPTHBIAS
                                    | D3DPRASTERCAPS_MULTISAMPLE_TOGGLE; // <-- TODO! (but difficult in Vk)
    // Z Comparison Caps
    pCaps->ZCmpCaps                 = D3DPCMPCAPS_NEVER
                                    | D3DPCMPCAPS_LESS
                                    | D3DPCMPCAPS_EQUAL
                                    | D3DPCMPCAPS_LESSEQUAL
                                    | D3DPCMPCAPS_GREATER
                                    | D3DPCMPCAPS_NOTEQUAL
                                    | D3DPCMPCAPS_GREATEREQUAL
                                    | D3DPCMPCAPS_ALWAYS;
    // Source Blend Caps
    pCaps->SrcBlendCaps             = D3DPBLENDCAPS_ZERO
                                    | D3DPBLENDCAPS_ONE
                                    | D3DPBLENDCAPS_SRCCOLOR
                                    | D3DPBLENDCAPS_INVSRCCOLOR
                                    | D3DPBLENDCAPS_SRCALPHA
                                    | D3DPBLENDCAPS_INVSRCALPHA
                                    | D3DPBLENDCAPS_DESTALPHA
                                    | D3DPBLENDCAPS_INVDESTALPHA
                                    | D3DPBLENDCAPS_DESTCOLOR
                                    | D3DPBLENDCAPS_INVDESTCOLOR
                                    | D3DPBLENDCAPS_SRCALPHASAT
                                    | D3DPBLENDCAPS_BOTHSRCALPHA
                                    | D3DPBLENDCAPS_BOTHINVSRCALPHA
                                    | D3DPBLENDCAPS_BLENDFACTOR
                                    | D3DPBLENDCAPS_INVSRCCOLOR2
                                    | D3DPBLENDCAPS_SRCCOLOR2;
    // Destination Blend Caps
    pCaps->DestBlendCaps            = pCaps->SrcBlendCaps;
    // Alpha Comparison Caps
    pCaps->AlphaCmpCaps             = pCaps->ZCmpCaps;
    // Shade Caps
    pCaps->ShadeCaps                = D3DPSHADECAPS_COLORGOURAUDRGB
                                    | D3DPSHADECAPS_SPECULARGOURAUDRGB
                                    | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                                    | D3DPSHADECAPS_FOGGOURAUD;
    // Texture Caps
    pCaps->TextureCaps              = D3DPTEXTURECAPS_PERSPECTIVE
                                 /* | D3DPTEXTURECAPS_POW2 */
                                    | D3DPTEXTURECAPS_ALPHA
                                 /* | D3DPTEXTURECAPS_SQUAREONLY */
                                    | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE
                                    | D3DPTEXTURECAPS_ALPHAPALETTE
                                 /* | D3DPTEXTURECAPS_NONPOW2CONDITIONAL */
                                    | D3DPTEXTURECAPS_PROJECTED
                                    | D3DPTEXTURECAPS_CUBEMAP
                                    | D3DPTEXTURECAPS_VOLUMEMAP
                                    | D3DPTEXTURECAPS_MIPMAP
                                    | D3DPTEXTURECAPS_MIPVOLUMEMAP
                                    | D3DPTEXTURECAPS_MIPCUBEMAP
                                 /* | D3DPTEXTURECAPS_CUBEMAP_POW2 */
                                 /* | D3DPTEXTURECAPS_VOLUMEMAP_POW2 */
                                 /* | D3DPTEXTURECAPS_NOPROJECTEDBUMPENV */;
    // Texture Filter Caps
    pCaps->TextureFilterCaps        = D3DPTFILTERCAPS_MINFPOINT
                                    | D3DPTFILTERCAPS_MINFLINEAR
                                    | D3DPTFILTERCAPS_MINFANISOTROPIC
                                 /* | D3DPTFILTERCAPS_MINFPYRAMIDALQUAD */
                                 /* | D3DPTFILTERCAPS_MINFGAUSSIANQUAD */
                                    | D3DPTFILTERCAPS_MIPFPOINT
                                    | D3DPTFILTERCAPS_MIPFLINEAR
                                 /* | D3DPTFILTERCAPS_CONVOLUTIONMONO */
                                    | D3DPTFILTERCAPS_MAGFPOINT
                                    | D3DPTFILTERCAPS_MAGFLINEAR
                                    | D3DPTFILTERCAPS_MAGFANISOTROPIC
                                 /* | D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD */
                                 /* | D3DPTFILTERCAPS_MAGFGAUSSIANQUAD */;
    // Cube Texture Filter Caps
    pCaps->CubeTextureFilterCaps    = pCaps->TextureFilterCaps;
    // Volume Texture Filter Caps
    pCaps->VolumeTextureFilterCaps  = pCaps->TextureFilterCaps;
    // Texture Address Caps
    pCaps->TextureAddressCaps       = D3DPTADDRESSCAPS_WRAP
                                    | D3DPTADDRESSCAPS_MIRROR
                                    | D3DPTADDRESSCAPS_CLAMP
                                    | D3DPTADDRESSCAPS_BORDER
                                    | D3DPTADDRESSCAPS_INDEPENDENTUV
                                    | D3DPTADDRESSCAPS_MIRRORONCE;
    // Volume Texture Address Caps
    pCaps->VolumeTextureAddressCaps = pCaps->TextureAddressCaps;
    // Line Caps
    pCaps->LineCaps                 = D3DLINECAPS_TEXTURE
                                    | D3DLINECAPS_ZTEST
                                    | D3DLINECAPS_BLEND
                                    | D3DLINECAPS_ALPHACMP
                                    | D3DLINECAPS_FOG
                                    | D3DLINECAPS_ANTIALIAS; //<-- Lying about doing AA lines here, we don't *fully* support that.
    // Max Texture Width
    pCaps->MaxTextureWidth          = MaxTextureDimension;
    // Max Texture Height
    pCaps->MaxTextureHeight         = MaxTextureDimension;
    // Max Volume Extent
    pCaps->MaxVolumeExtent          = 8192;
    // Max Texture Repeat
    pCaps->MaxTextureRepeat         = 8192;
    // Max Texture Aspect Ratio
    pCaps->MaxTextureAspectRatio    = 8192;
    // Max Anisotropy
    pCaps->MaxAnisotropy            = 16;
    // Max Vertex W
    pCaps->MaxVertexW               = 1e10f;
    // Guard Bands
    pCaps->GuardBandLeft            = -32768.0f;
    pCaps->GuardBandTop             = -32768.0f;
    pCaps->GuardBandRight           =  32768.0f;
    pCaps->GuardBandBottom          =  32768.0f;
    // Extents Adjust
    pCaps->ExtentsAdjust            = 0.0f;
    // Stencil Caps
    pCaps->StencilCaps              = D3DSTENCILCAPS_KEEP
                                    | D3DSTENCILCAPS_ZERO
                                    | D3DSTENCILCAPS_REPLACE
                                    | D3DSTENCILCAPS_INCRSAT
                                    | D3DSTENCILCAPS_DECRSAT
                                    | D3DSTENCILCAPS_INVERT
                                    | D3DSTENCILCAPS_INCR
                                    | D3DSTENCILCAPS_DECR
                                    | D3DSTENCILCAPS_TWOSIDED;
    // FVF Caps
    pCaps->FVFCaps                  = (MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK)
                                 /* | D3DFVFCAPS_DONOTSTRIPELEMENTS */
                                    | D3DFVFCAPS_PSIZE;
    // Texture Op Caps
    pCaps->TextureOpCaps            = D3DTEXOPCAPS_DISABLE
                                    | D3DTEXOPCAPS_SELECTARG1
                                    | D3DTEXOPCAPS_SELECTARG2
                                    | D3DTEXOPCAPS_MODULATE
                                    | D3DTEXOPCAPS_MODULATE2X
                                    | D3DTEXOPCAPS_MODULATE4X
                                    | D3DTEXOPCAPS_ADD
                                    | D3DTEXOPCAPS_ADDSIGNED
                                    | D3DTEXOPCAPS_ADDSIGNED2X
                                    | D3DTEXOPCAPS_SUBTRACT
                                    | D3DTEXOPCAPS_ADDSMOOTH
                                    | D3DTEXOPCAPS_BLENDDIFFUSEALPHA
                                    | D3DTEXOPCAPS_BLENDTEXTUREALPHA
                                    | D3DTEXOPCAPS_BLENDFACTORALPHA
                                    | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
                                    | D3DTEXOPCAPS_BLENDCURRENTALPHA
                                    | D3DTEXOPCAPS_PREMODULATE
                                    | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR
                                    | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
                                    | D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR
                                    | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
                                    | D3DTEXOPCAPS_BUMPENVMAP
                                    | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
                                    | D3DTEXOPCAPS_DOTPRODUCT3
                                    | D3DTEXOPCAPS_MULTIPLYADD
                                    | D3DTEXOPCAPS_LERP;
    // Max Texture Blend Stages
    pCaps->MaxTextureBlendStages    = MaxTextureBlendStages;
    // Max Simultaneous Textures
    pCaps->MaxSimultaneousTextures  = MaxSimultaneousTextures;
    // Vertex Processing Caps
    pCaps->VertexProcessingCaps      = D3DVTXPCAPS_TEXGEN
                                     | D3DVTXPCAPS_MATERIALSOURCE7
                                     | D3DVTXPCAPS_DIRECTIONALLIGHTS
                                     | D3DVTXPCAPS_POSITIONALLIGHTS
                                     | D3DVTXPCAPS_LOCALVIEWER
                                     | D3DVTXPCAPS_TWEENING
                                     | D3DVTXPCAPS_TEXGEN_SPHEREMAP
                                  /* | D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER*/;
    // Max Active Lights
    pCaps->MaxActiveLights           = caps::MaxEnabledLights;
    // Max User Clip Planes
    pCaps->MaxUserClipPlanes         = MaxClipPlanes;
    // Max Vertex Blend Matrices
    pCaps->MaxVertexBlendMatrices    = 4;
    // Max Vertex Blend Matrix Index
    pCaps->MaxVertexBlendMatrixIndex = 0;
    // Max Point Size
    pCaps->MaxPointSize              = 256.0f;
    // Max Primitive Count
    pCaps->MaxPrimitiveCount         = 0x00555555;
    // Max Vertex Index
    pCaps->MaxVertexIndex            = 0x00ffffff;
    // Max Streams
    pCaps->MaxStreams                = MaxStreams;
    // Max Stream Stride
    pCaps->MaxStreamStride           = 508; // bytes

    const uint32_t majorVersion = options.shaderModel;
    const uint32_t minorVersion = options.shaderModel != 1 ? 0 : 4;

    // Shader Versions
    pCaps->VertexShaderVersion = D3DVS_VERSION(majorVersion, minorVersion);
    pCaps->PixelShaderVersion  = D3DPS_VERSION(majorVersion, minorVersion);

    // Max Vertex Shader Const
    pCaps->MaxVertexShaderConst       = MaxFloatConstantsVS;
    // Max PS1 Value
    pCaps->PixelShader1xMaxValue      = FLT_MAX;
    // Dev Caps 2
    pCaps->DevCaps2                   = D3DDEVCAPS2_STREAMOFFSET
                                   /* | D3DDEVCAPS2_DMAPNPATCH */
                                   /* | D3DDEVCAPS2_ADAPTIVETESSRTPATCH */
                                   /* | D3DDEVCAPS2_ADAPTIVETESSNPATCH */
                                      | D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES
                                   /* | D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH */
                                      | D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET;
    // Max N Patch Tesselation Level
    pCaps->MaxNpatchTessellationLevel = 0.0f;
    // Reserved for... something
    pCaps->Reserved5                  = 0;
    // Master adapter for us is adapter 0, atm... 
    pCaps->MasterAdapterOrdinal       = 0;
    // The group of adapters this one is in
    pCaps->AdapterOrdinalInGroup      = 0;
    // Number of adapters in current group
    pCaps->NumberOfAdaptersInGroup    = 1;
    // Decl Type Caps
    pCaps->DeclTypes                  = D3DDTCAPS_UBYTE4
                                      | D3DDTCAPS_UBYTE4N
                                      | D3DDTCAPS_SHORT2N
                                      | D3DDTCAPS_SHORT4N
                                      | D3DDTCAPS_USHORT2N
                                      | D3DDTCAPS_USHORT4N
                                      | D3DDTCAPS_UDEC3
                                      | D3DDTCAPS_DEC3N
                                      | D3DDTCAPS_FLOAT16_2
                                      | D3DDTCAPS_FLOAT16_4;
    // Number of simultaneous RTs
    pCaps->NumSimultaneousRTs         = MaxSimultaneousRenderTargets;
    // Possible StretchRect filters
    pCaps->StretchRectFilterCaps      = D3DPTFILTERCAPS_MINFPOINT
                                      | D3DPTFILTERCAPS_MINFLINEAR
                                   /* | D3DPTFILTERCAPS_MINFANISOTROPIC */
                                   /* | D3DPTFILTERCAPS_MINFPYRAMIDALQUAD */
                                   /* | D3DPTFILTERCAPS_MINFGAUSSIANQUAD */
                                   /* | D3DPTFILTERCAPS_MIPFPOINT */
                                   /* | D3DPTFILTERCAPS_MIPFLINEAR */
                                   /* | D3DPTFILTERCAPS_CONVOLUTIONMONO */
                                      | D3DPTFILTERCAPS_MAGFPOINT
                                      | D3DPTFILTERCAPS_MAGFLINEAR
                                   /* | D3DPTFILTERCAPS_MAGFANISOTROPIC */
                                   /* | D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD */
                                   /* | D3DPTFILTERCAPS_MAGFGAUSSIANQUAD */;

    // Not too bothered about doing these longhand
    // We should match whatever my AMD hardware reports here
    // methinks for the best chance of stuff working.
    pCaps->VS20Caps.Caps                     = 1;
    pCaps->VS20Caps.DynamicFlowControlDepth  = 24;
    pCaps->VS20Caps.NumTemps                 = 32;
    pCaps->VS20Caps.StaticFlowControlDepth   = 4;

    pCaps->PS20Caps.Caps                     = 31;
    pCaps->PS20Caps.DynamicFlowControlDepth  = 24;
    pCaps->PS20Caps.NumTemps                 = 32;
    pCaps->PS20Caps.StaticFlowControlDepth   = 4;

    pCaps->PS20Caps.NumInstructionSlots      = options.shaderModel >= 2 ? 512 : 256;

    pCaps->VertexTextureFilterCaps           = 50332416;
    pCaps->MaxVShaderInstructionsExecuted    = 4294967295;
    pCaps->MaxPShaderInstructionsExecuted    = 4294967295;

    pCaps->MaxVertexShader30InstructionSlots = options.shaderModel == 3 ? 32768 : 0;
    pCaps->MaxPixelShader30InstructionSlots  = options.shaderModel == 3 ? 32768 : 0;

    return D3D_OK;
  }


  HMONITOR D3D9Adapter::GetMonitor() {
    return wsi::getDefaultMonitor();
  }


  UINT D3D9Adapter::GetAdapterModeCountEx(const D3DDISPLAYMODEFILTER* pFilter) {
    if (pFilter == nullptr)
      return 0;

    // We don't offer any interlaced formats here so early out and avoid destroying mode cache.
    if (pFilter->ScanLineOrdering == D3DSCANLINEORDERING_INTERLACED)
      return 0;

    CacheModes(EnumerateFormat(pFilter->Format));
    return m_modes.size();
  }


  HRESULT D3D9Adapter::EnumAdapterModesEx(
    const D3DDISPLAYMODEFILTER* pFilter,
          UINT                  Mode,
          D3DDISPLAYMODEEX*     pMode) {
    if (pMode == nullptr || pFilter == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9Format format =
      EnumerateFormat(pFilter->Format);

    if (FAILED(CheckDeviceFormat(
      D3DDEVTYPE_HAL, EnumerateFormat(pFilter->Format),
      D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE,
      EnumerateFormat(pFilter->Format))))
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


  HRESULT D3D9Adapter::GetAdapterDisplayModeEx(
          D3DDISPLAYMODEEX*   pMode,
          D3DDISPLAYROTATION* pRotation) {
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    if (pRotation != nullptr)
      *pRotation = D3DDISPLAYROTATION_IDENTITY;

    wsi::WsiMode mode = { };

    if (!wsi::getCurrentDisplayMode(wsi::getDefaultMonitor(), &mode)) {
      Logger::err("D3D9Adapter::GetAdapterDisplayModeEx: Failed to enum display settings");
      return D3DERR_INVALIDCALL;
    }

    *pMode = ConvertDisplayMode(mode);
    return D3D_OK;
  }


  HRESULT D3D9Adapter::GetAdapterLUID(LUID* pLUID) {
    if (pLUID == nullptr)
      return D3DERR_INVALIDCALL;

    auto& vk11 = m_adapter->devicePropertiesExt().vk11;

    if (vk11.deviceLUIDValid)
      *pLUID = bit::cast<LUID>(vk11.deviceLUID);
    else
      *pLUID = dxvk::GetAdapterLUID(m_ordinal);

    return D3D_OK;
  }


  HRESULT D3D9Adapter::CheckDeviceVkFormat(
          VkFormat        Format,
          DWORD           Usage,
          D3DRESOURCETYPE RType) {
    VkFormatFeatureFlags2 checkFlags = 0;

    if (RType != D3DRTYPE_SURFACE)
      checkFlags |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;

    if (Usage & D3DUSAGE_RENDERTARGET) {
      checkFlags |= VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;

      if (Usage & D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING)
        checkFlags |= VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (Usage & D3DUSAGE_DEPTHSTENCIL)
      checkFlags |= VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;
    else
      checkFlags |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;

    VkFormatFeatureFlags2 checkFlagsMipGen = checkFlags;

    if (Usage & D3DUSAGE_AUTOGENMIPMAP) {
      checkFlagsMipGen |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;
      checkFlagsMipGen |= VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;
    }

    DxvkFormatFeatures    fmtSupport  = m_adapter->getFormatFeatures(Format);
    VkFormatFeatureFlags2 imgFeatures = fmtSupport.optimal | fmtSupport.linear;

    if ((imgFeatures & checkFlags) != checkFlags)
      return D3DERR_NOTAVAILABLE;

    return ((imgFeatures & checkFlagsMipGen) != checkFlagsMipGen)
      ? D3DOK_NOAUTOGEN
      : D3D_OK;
  }


  void D3D9Adapter::CacheModes(D3D9Format Format) {
    if (!m_modes.empty() && m_modeCacheFormat == Format)
      return; // We already cached the modes for this format. No need to do it again.

    m_modes.clear();
    m_modeCacheFormat = Format;

    // Skip unsupported formats
    if (!IsSupportedAdapterFormat(Format))
      return;

    auto& options = m_parent->GetOptions();

    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    wsi::WsiMode devMode = { };

    uint32_t modeIndex = 0;

    const auto forcedRatio = Ratio<DWORD>(options.forceAspectRatio);

    while (wsi::getDisplayMode(wsi::getDefaultMonitor(), modeIndex++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.interlaced)
        continue;

      // Skip modes with incompatible formats
      if (devMode.bitsPerPixel != GetMonitorFormatBpp(Format))
        continue;

      if (!forcedRatio.undefined() && Ratio<DWORD>(devMode.width, devMode.height) != forcedRatio)
        continue;

      D3DDISPLAYMODEEX mode = ConvertDisplayMode(devMode);
      // Fix up the D3DFORMAT to match what we are enumerating
      mode.Format = static_cast<D3DFORMAT>(Format);

      if (std::count(m_modes.begin(), m_modes.end(), mode) == 0)
        m_modes.push_back(mode);
    }

    // Sort display modes by width, height and refresh rate,
    // in that order. Some games rely on correct ordering.
    std::sort(m_modes.begin(), m_modes.end(),
      [](const D3DDISPLAYMODEEX& a, const D3DDISPLAYMODEEX& b) {
        if (a.Width < b.Width)   return true;
        if (a.Width > b.Width)   return false;
        
        if (a.Height < b.Height) return true;
        if (a.Height > b.Height) return false;
        
        return a.RefreshRate < b.RefreshRate;
    });
  }

}
