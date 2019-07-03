#include "d3d9_caps.h"

#include "d3d9_monitor.h"
#include "d3d9_format.h"
#include "d3d9_options.h"

#include <cfloat>

namespace dxvk::caps {

  bool IsDepthFormat(D3D9Format Format) {
    return Format == D3D9Format::D16_LOCKABLE
        || Format == D3D9Format::D32
        || Format == D3D9Format::D15S1
        || Format == D3D9Format::D24S8
        || Format == D3D9Format::D24X8
        || Format == D3D9Format::D24X4S4
        || Format == D3D9Format::D16
        || Format == D3D9Format::D32F_LOCKABLE
        || Format == D3D9Format::D24FS8
        || Format == D3D9Format::D32_LOCKABLE
        || Format == D3D9Format::DF16
        || Format == D3D9Format::DF24
        || Format == D3D9Format::INTZ;
  }


  HRESULT CheckDeviceFormat(
          D3D9Format      AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE ResourceType,
          D3D9Format      CheckFormat) {
    if (!IsSupportedAdapterFormat(AdapterFormat))
      return D3DERR_INVALIDCALL;

    if (!IsSupportedDisplayFormat(AdapterFormat, false))
      return D3DERR_NOTAVAILABLE;

    const bool dmap = Usage & D3DUSAGE_DMAP;
    const bool rt   = Usage & D3DUSAGE_RENDERTARGET;
    const bool ds   = Usage & D3DUSAGE_DEPTHSTENCIL;

    const bool surface = ResourceType == D3DRTYPE_SURFACE;
    const bool texture = ResourceType == D3DRTYPE_TEXTURE;

    const bool twoDimensional = surface || texture;

    const bool srgb = (Usage & (D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE)) != 0;

    if (CheckFormat == D3D9Format::INST)
      return D3D_OK;

    if (ds && !IsDepthFormat(CheckFormat))
      return D3DERR_NOTAVAILABLE;

    if (rt && CheckFormat == D3D9Format::NULL_FORMAT && twoDimensional)
      return D3D_OK;

    if (rt && CheckFormat == D3D9Format::RESZ && surface)
      return D3D_OK;

    if (CheckFormat == D3D9Format::ATOC && surface)
      return D3D_OK;

    // I really don't want to support this...
    if (dmap)
      return D3DERR_NOTAVAILABLE;

    auto mapping = ConvertFormatUnfixed(CheckFormat);
    if (mapping.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (mapping.FormatSrgb  == VK_FORMAT_UNDEFINED && srgb)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }


  HRESULT CheckDepthStencilMatch(
          D3D9Format AdapterFormat,
          D3D9Format RenderTargetFormat,
          D3D9Format DepthStencilFormat) {
    if (!IsSupportedAdapterFormat(AdapterFormat))
      return D3DERR_NOTAVAILABLE;

    if (!IsDepthFormat(DepthStencilFormat))
      return D3DERR_NOTAVAILABLE;

    auto mapping = ConvertFormatUnfixed(RenderTargetFormat);
    if (mapping.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }


  HRESULT CheckDeviceFormatConversion(
          D3D9Format SrcFormat,
          D3D9Format DstFormat) {
    return IsSupportedBackBufferFormat(DstFormat, SrcFormat, FALSE);
  }


  HRESULT CheckDeviceMultiSampleType(
        D3D9Format          SurfaceFormat,
        BOOL                Windowed,
        D3DMULTISAMPLE_TYPE MultiSampleType,
        DWORD*              pQualityLevels) {
    if (pQualityLevels != nullptr)
      *pQualityLevels = 1;

    auto dst = ConvertFormatUnfixed(SurfaceFormat);
    if (dst.FormatColor == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (SurfaceFormat == D3D9Format::D32_LOCKABLE
     || SurfaceFormat == D3D9Format::D32F_LOCKABLE
     || SurfaceFormat == D3D9Format::D16_LOCKABLE)
      return D3DERR_NOTAVAILABLE;

    // Not a multiple of 2
    // Not nonmaskable
    // Not greater than 8
    if ((MultiSampleType % 2 != 0 && MultiSampleType != 1)
      || MultiSampleType > 8)
      return D3DERR_NOTAVAILABLE;

    if (pQualityLevels != nullptr) {
      if (MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
        *pQualityLevels = 4;
      else
        *pQualityLevels = 1;
    }

    return D3D_OK;
  }


  HRESULT CheckDeviceType(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed) {
    if (!IsSupportedBackBufferFormat(
      AdapterFormat, BackBufferFormat, Windowed))
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }


  HRESULT GetDeviceCaps(
    const dxvk::D3D9Options& Options,
          UINT               Adapter,
          D3DDEVTYPE         Type,
          D3DCAPS9*          pCaps) {
    if (pCaps == nullptr)
      return D3DERR_INVALIDCALL;

    // TODO: Actually care about what the adapter supports here.
    // ^ For Intel and older cards most likely here.

    // Device Type
    pCaps->DeviceType               = Type;
    // Adapter Id
    pCaps->AdapterOrdinal           = Adapter;
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
                                 /* | D3DPMISCCAPS_CLIPTLVERTS */
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
                                 /* | D3DPRASTERCAPS_WFOG */
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
    pCaps->MaxActiveLights           = 8;
    // Max User Clip Planes
    pCaps->MaxUserClipPlanes         = MaxClipPlanes;
    // Max Vertex Blend Matrices
    pCaps->MaxVertexBlendMatrices    = 4;
    // Max Vertex Blend Matrix Index
    pCaps->MaxVertexBlendMatrixIndex = 8;
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

    const uint32_t majorVersion = Options.shaderModel;
    const uint32_t minorVersion = Options.shaderModel != 1 ? 0 : 4;

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

    pCaps->PS20Caps.NumInstructionSlots      = Options.shaderModel >= 2 ? 512 : 256;

    pCaps->VertexTextureFilterCaps           = 50332416;
    pCaps->MaxVShaderInstructionsExecuted    = 4294967295;
    pCaps->MaxPShaderInstructionsExecuted    = 4294967295;

    pCaps->MaxVertexShader30InstructionSlots = Options.shaderModel == 3 ? 32768 : 0;
    pCaps->MaxPixelShader30InstructionSlots  = Options.shaderModel == 3 ? 32768 : 0;

    return D3D_OK;
  }

}
