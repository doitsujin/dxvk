#include "d3d9_caps.h"

#include "d3d9_monitor.h"
#include "d3d9_format.h"

namespace dxvk::caps {

  HRESULT checkDeviceFormat(
    D3D9Format adapterFormat,
    DWORD usage,
    D3DRESOURCETYPE resourceType,
    D3D9Format checkFormat) {
    // TODO: Handle SRGB checks here.

    if (!IsSupportedMonitorFormat(adapterFormat) && adapterFormat != D3D9Format::Unknown)
      return D3DERR_NOTAVAILABLE;

    if (checkFormat == D3D9Format::INST)
      return D3DERR_NOTAVAILABLE;

      switch (resourceType) {
      case D3DRTYPE_SURFACE:
        if (usage & D3DUSAGE_RENDERTARGET) {
          switch (checkFormat) {
          case D3D9Format::NULL_FORMAT:
          case D3D9Format::R8G8B8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::G16R16:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else if (usage & D3DUSAGE_DEPTHSTENCIL) {
          switch (checkFormat) {
          case D3D9Format::D32:
          case D3D9Format::D24S8:
          case D3D9Format::D24X8:
          case D3D9Format::D16:
          case D3D9Format::D24FS8:
          case D3D9Format::D32F_LOCKABLE:
          case D3D9Format::DF24:
          case D3D9Format::DF16:
          case D3D9Format::INTZ:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else {
          switch (checkFormat) {
          case D3D9Format::A8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::P8:
          case D3D9Format::A8P8:
          case D3D9Format::G16R16:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::DXT1:
          case D3D9Format::DXT2:
          case D3D9Format::DXT3:
          case D3D9Format::DXT4:
          case D3D9Format::DXT5:
          case D3D9Format::ATI1:
          case D3D9Format::ATI2:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
          case D3D9Format::V8U8:
          case D3D9Format::L6V5U5:
          case D3D9Format::X8L8V8U8:
          case D3D9Format::Q8W8V8U8:
          case D3D9Format::V16U16:
          case D3D9Format::A2W10V10U10:
          case D3D9Format::Q16W16V16U16:
          case D3D9Format::L8:
          case D3D9Format::A4L4:
          case D3D9Format::L16:
          case D3D9Format::A8L8:
          case D3D9Format::NVDB:
          case D3D9Format::ATOC:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
      case D3DRTYPE_VOLUME:
        switch (checkFormat) {
        case D3D9Format::A8:
        case D3D9Format::R5G6B5:
        case D3D9Format::X1R5G5B5:
        case D3D9Format::A1R5G5B5:
        case D3D9Format::A4R4G4B4:
        case D3D9Format::R3G3B2:
        case D3D9Format::A8R3G3B2:
        case D3D9Format::X4R4G4B4:
        case D3D9Format::R8G8B8:
        case D3D9Format::X8R8G8B8:
        case D3D9Format::A8R8G8B8:
        case D3D9Format::X8B8G8R8:
        case D3D9Format::A8B8G8R8:
        case D3D9Format::P8:
        case D3D9Format::A8P8:
        case D3D9Format::G16R16:
        case D3D9Format::A2R10G10B10:
        case D3D9Format::A2B10G10R10:
        case D3D9Format::A16B16G16R16:
        case D3D9Format::DXT1:
        case D3D9Format::DXT2:
        case D3D9Format::DXT3:
        case D3D9Format::DXT4:
        case D3D9Format::DXT5:
        case D3D9Format::ATI1:
        case D3D9Format::ATI2:
        case D3D9Format::R16F:
        case D3D9Format::G16R16F:
        case D3D9Format::A16B16G16R16F:
        case D3D9Format::R32F:
        case D3D9Format::G32R32F:
        case D3D9Format::A32B32G32R32F:
        case D3D9Format::V8U8:
        case D3D9Format::L6V5U5:
        case D3D9Format::X8L8V8U8:
        case D3D9Format::Q8W8V8U8:
        case D3D9Format::V16U16:
        case D3D9Format::A2W10V10U10:
        case D3D9Format::Q16W16V16U16:
        case D3D9Format::L8:
        case D3D9Format::A4L4:
        case D3D9Format::L16:
        case D3D9Format::A8L8:
          return D3D_OK;
        default:
          return D3DERR_NOTAVAILABLE;
        }
      case D3DRTYPE_CUBETEXTURE:
        if (usage & D3DUSAGE_RENDERTARGET) {
          switch (checkFormat) {
          case D3D9Format::NULL_FORMAT:
          case D3D9Format::R8G8B8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::G16R16:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else if (usage & D3DUSAGE_DEPTHSTENCIL) {
          switch (checkFormat) {
          case D3D9Format::D32:
          case D3D9Format::D24S8:
          case D3D9Format::D24X8:
          case D3D9Format::D16:
          case D3D9Format::D24FS8:
          case D3D9Format::D32F_LOCKABLE:
          case D3D9Format::DF24:
          case D3D9Format::DF16:
          case D3D9Format::INTZ:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else {
          switch (checkFormat) {
          case D3D9Format::A8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::P8:
          case D3D9Format::A8P8:
          case D3D9Format::G16R16:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::DXT1:
          case D3D9Format::DXT2:
          case D3D9Format::DXT3:
          case D3D9Format::DXT4:
          case D3D9Format::DXT5:
          case D3D9Format::ATI1:
          case D3D9Format::ATI2:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
          case D3D9Format::V8U8:
          case D3D9Format::L6V5U5:
          case D3D9Format::X8L8V8U8:
          case D3D9Format::Q8W8V8U8:
          case D3D9Format::V16U16:
          case D3D9Format::A2W10V10U10:
          case D3D9Format::Q16W16V16U16:
          case D3D9Format::L8:
          case D3D9Format::A4L4:
          case D3D9Format::L16:
          case D3D9Format::A8L8:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
      case D3DRTYPE_VOLUMETEXTURE:
        switch (checkFormat) {
        case D3D9Format::A8:
        case D3D9Format::R5G6B5:
        case D3D9Format::X1R5G5B5:
        case D3D9Format::A1R5G5B5:
        case D3D9Format::A4R4G4B4:
        case D3D9Format::R3G3B2:
        case D3D9Format::A8R3G3B2:
        case D3D9Format::X4R4G4B4:
        case D3D9Format::R8G8B8:
        case D3D9Format::X8R8G8B8:
        case D3D9Format::A8R8G8B8:
        case D3D9Format::X8B8G8R8:
        case D3D9Format::A8B8G8R8:
        case D3D9Format::P8:
        case D3D9Format::A8P8:
        case D3D9Format::G16R16:
        case D3D9Format::A2R10G10B10:
        case D3D9Format::A2B10G10R10:
        case D3D9Format::A16B16G16R16:
        case D3D9Format::DXT1:
        case D3D9Format::DXT2:
        case D3D9Format::DXT3:
        case D3D9Format::DXT4:
        case D3D9Format::DXT5:
        case D3D9Format::ATI1:
        case D3D9Format::ATI2:
        case D3D9Format::R16F:
        case D3D9Format::G16R16F:
        case D3D9Format::A16B16G16R16F:
        case D3D9Format::R32F:
        case D3D9Format::G32R32F:
        case D3D9Format::A32B32G32R32F:
        case D3D9Format::V8U8:
        case D3D9Format::L6V5U5:
        case D3D9Format::X8L8V8U8:
        case D3D9Format::Q8W8V8U8:
        case D3D9Format::V16U16:
        case D3D9Format::A2W10V10U10:
        case D3D9Format::Q16W16V16U16:
        case D3D9Format::L8:
        case D3D9Format::A4L4:
        case D3D9Format::L16:
        case D3D9Format::A8L8:
          return D3D_OK;
        default:
          return D3DERR_NOTAVAILABLE;
        }
      case D3DRTYPE_TEXTURE:
        if (usage & D3DUSAGE_RENDERTARGET) {
          switch (checkFormat) {
          case D3D9Format::NULL_FORMAT:
          case D3D9Format::R8G8B8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::G16R16:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else if (usage & D3DUSAGE_DEPTHSTENCIL) {
          switch (checkFormat) {
          case D3D9Format::D32:
          case D3D9Format::D24S8:
          case D3D9Format::D24X8:
          case D3D9Format::D16:
          case D3D9Format::D24FS8:
          case D3D9Format::D32F_LOCKABLE:
          case D3D9Format::DF24:
          case D3D9Format::DF16:
          case D3D9Format::INTZ:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
        else {
          switch (checkFormat) {
          case D3D9Format::NULL_FORMAT:
          case D3D9Format::A8:
          case D3D9Format::R5G6B5:
          case D3D9Format::X1R5G5B5:
          case D3D9Format::A1R5G5B5:
          case D3D9Format::A4R4G4B4:
          case D3D9Format::R3G3B2:
          case D3D9Format::A8R3G3B2:
          case D3D9Format::X4R4G4B4:
          case D3D9Format::R8G8B8:
          case D3D9Format::X8R8G8B8:
          case D3D9Format::A8R8G8B8:
          case D3D9Format::X8B8G8R8:
          case D3D9Format::A8B8G8R8:
          case D3D9Format::P8:
          case D3D9Format::A8P8:
          case D3D9Format::G16R16:
          case D3D9Format::A2R10G10B10:
          case D3D9Format::A2B10G10R10:
          case D3D9Format::A16B16G16R16:
          case D3D9Format::DXT1:
          case D3D9Format::DXT2:
          case D3D9Format::DXT3:
          case D3D9Format::DXT4:
          case D3D9Format::DXT5:
          case D3D9Format::ATI1:
          case D3D9Format::ATI2:
          case D3D9Format::R16F:
          case D3D9Format::G16R16F:
          case D3D9Format::A16B16G16R16F:
          case D3D9Format::R32F:
          case D3D9Format::G32R32F:
          case D3D9Format::A32B32G32R32F:
          case D3D9Format::V8U8:
          case D3D9Format::L6V5U5:
          case D3D9Format::X8L8V8U8:
          case D3D9Format::Q8W8V8U8:
          case D3D9Format::V16U16:
          case D3D9Format::A2W10V10U10:
          case D3D9Format::Q16W16V16U16:
          case D3D9Format::L8:
          case D3D9Format::A4L4:
          case D3D9Format::L16:
          case D3D9Format::A8L8:
          case D3D9Format::D32:
          case D3D9Format::D24S8:
          case D3D9Format::D24X8:
          case D3D9Format::D16:
          case D3D9Format::D24FS8:
          case D3D9Format::D32F_LOCKABLE:
          case D3D9Format::DF24:
          case D3D9Format::DF16:
          case D3D9Format::INTZ:
            return D3D_OK;
          default:
            return D3DERR_NOTAVAILABLE;
          }
        }
      case D3DRTYPE_VERTEXBUFFER:
        if (checkFormat == D3D9Format::VERTEXDATA)
          return D3D_OK;
        else
          return D3DERR_NOTAVAILABLE;
      case D3DRTYPE_INDEXBUFFER:
        switch (checkFormat) {
        case D3D9Format::INDEX16:
        case D3D9Format::INDEX32:
          return D3D_OK;
        default:
          return D3DERR_NOTAVAILABLE;
        };
      default:
        return D3DERR_NOTAVAILABLE;
      }
    }

  HRESULT checkDepthStencilMatch(
    D3D9Format adapterFormat,
    D3D9Format renderTargetFormat,
    D3D9Format depthStencilFormat) {
    if (!IsSupportedMonitorFormat(adapterFormat) && adapterFormat != D3D9Format::Unknown)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK; // Any format combo is OK!
  }

  HRESULT checkDeviceFormatConversion(
    D3D9Format srcFormat,
    D3D9Format dstFormat) {
    return D3D_OK; // Any format combo is OK!
  }

  HRESULT checkDeviceMultiSampleType(
    D3D9Format surfaceFormat,
    BOOL windowed,
    D3DMULTISAMPLE_TYPE multiSampleType,
    DWORD* qualityLevels) {
    if (qualityLevels != nullptr) {
      if (multiSampleType == D3DMULTISAMPLE_NONMASKABLE)
        *qualityLevels = 4;
      else
        *qualityLevels = 1;
    }

    if (surfaceFormat == D3D9Format::D32F_LOCKABLE || surfaceFormat == D3D9Format::D16_LOCKABLE)
      return D3DERR_NOTAVAILABLE;

    if ((multiSampleType % 2 == 0 || multiSampleType == 1) && multiSampleType <= 16) {
      if (checkDeviceFormat(D3D9Format::X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, surfaceFormat) == D3D_OK
        || checkDeviceFormat(D3D9Format::X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, surfaceFormat) == D3D_OK) {
        return D3D_OK;
      }
    }

    return D3DERR_NOTAVAILABLE;
  }

  HRESULT checkDeviceType(
    D3D9Format adapterFormat,
    D3D9Format backBufferFormat,
    BOOL windowed) {
    if (adapterFormat == D3D9Format::Unknown) {
      if (windowed == FALSE)
        return D3DERR_INVALIDCALL;
      else
        return D3DERR_NOTAVAILABLE;
    }

    if (!IsSupportedMonitorFormat(adapterFormat))
      return D3DERR_NOTAVAILABLE;

    if (backBufferFormat == D3D9Format::Unknown) {
      if (windowed == FALSE)
        return D3DERR_INVALIDCALL;
      else
        return D3DERR_NOTAVAILABLE;
    }

    if (!IsSupportedMonitorFormat(backBufferFormat))
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }

  HRESULT getDeviceCaps(UINT adapter, D3DDEVTYPE type, D3DCAPS9* pCaps) {
    if (pCaps == nullptr)
      return D3DERR_INVALIDCALL;

    // Based off what I get from native D3D.
    // TODO: Make this more based in reality of the adapter.

    pCaps->DeviceType = type;
    pCaps->AdapterOrdinal = adapter;
    pCaps->Caps = 131072;
    pCaps->Caps2 = 3758227456;
    pCaps->Caps3 = 928;
    pCaps->PresentationIntervals = 2147483663;
    pCaps->CursorCaps = 1;
    pCaps->DevCaps = 1818352;
    pCaps->PrimitiveMiscCaps = 4181234;
    pCaps->RasterCaps = 259219857;
    pCaps->ZCmpCaps = 255;
    pCaps->SrcBlendCaps = 16383;
    pCaps->DestBlendCaps = 16383;
    pCaps->AlphaCmpCaps = 255;
    pCaps->ShadeCaps = 541192;
    pCaps->TextureCaps = 126021;
    pCaps->TextureFilterCaps = 117638912;
    pCaps->CubeTextureFilterCaps = 50529024;
    pCaps->VolumeTextureFilterCaps = 117638912;
    pCaps->TextureAddressCaps = 63;
    pCaps->VolumeTextureAddressCaps = 63;
    pCaps->LineCaps = 31;
    pCaps->MaxTextureWidth = 16384;
    pCaps->MaxTextureHeight = 16384;
    pCaps->MaxVolumeExtent = 8192;
    pCaps->MaxTextureRepeat = 8192;
    pCaps->MaxTextureAspectRatio = 8192;
    pCaps->MaxAnisotropy = 16;
    pCaps->MaxVertexW = 1e10f;
    pCaps->GuardBandLeft = -32768.0f;
    pCaps->GuardBandTop = -32768.0f;
    pCaps->GuardBandRight = 32768.0f;
    pCaps->GuardBandBottom = 32768.0f;
    pCaps->ExtentsAdjust = 0.0f;
    pCaps->StencilCaps = 511;
    pCaps->FVFCaps = 1048584;
    pCaps->TextureOpCaps = 67108863;
    pCaps->MaxTextureBlendStages = 8;

    pCaps->MaxSimultaneousTextures = 8;

    pCaps->VertexProcessingCaps = 379;
    pCaps->MaxActiveLights = 8;
    pCaps->MaxUserClipPlanes = 6;
    pCaps->MaxVertexBlendMatrices = 4;
    pCaps->MaxVertexBlendMatrixIndex = 0;
    pCaps->MaxPointSize = 256.0f;
    pCaps->MaxPrimitiveCount = 5592405;
    pCaps->MaxVertexIndex = 16777215;
    pCaps->MaxStreams = 16;
    pCaps->MaxStreamStride = 508;

    //if (shaderModel == "3") {
      pCaps->VertexShaderVersion = D3DVS_VERSION(3, 0);
      pCaps->PixelShaderVersion = D3DPS_VERSION(3, 0);
    //}
    //else if (shaderModel == "2b" || shaderModel == "2B" || shaderModel == "2") {
    //  pCaps->VertexShaderVersion = D3DVS_VERSION(2, 0);
    //  pCaps->PixelShaderVersion = D3DPS_VERSION(2, 0);
    //}
    //else if (shaderModel == "1") {
    //  pCaps->VertexShaderVersion = D3DVS_VERSION(1, 4);
    //  pCaps->PixelShaderVersion = D3DPS_VERSION(1, 4);
    //}

    pCaps->MaxVertexShaderConst = 256;
    pCaps->PixelShader1xMaxValue = FLT_MAX;
    pCaps->DevCaps2 = 113;
    pCaps->MaxNpatchTessellationLevel = 1.0f;
    pCaps->Reserved5 = 0;
    pCaps->MasterAdapterOrdinal = 0;
    pCaps->AdapterOrdinalInGroup = 0;
    pCaps->NumberOfAdaptersInGroup = 2;
    pCaps->DeclTypes = 1023;
    pCaps->NumSimultaneousRTs = 4;
    pCaps->StretchRectFilterCaps = 50332416;

    pCaps->VS20Caps.Caps = 1;
    pCaps->VS20Caps.DynamicFlowControlDepth = 24;
    pCaps->VS20Caps.NumTemps = 32;
    pCaps->VS20Caps.StaticFlowControlDepth = 4;

    pCaps->PS20Caps.Caps = 31;
    pCaps->PS20Caps.DynamicFlowControlDepth = 24;
    pCaps->PS20Caps.NumTemps = 32;
    pCaps->PS20Caps.StaticFlowControlDepth = 4;

    //if (shaderModel == "3" || shaderModel == "2b" || shaderModel == "2B")
      pCaps->PS20Caps.NumInstructionSlots = 512;
    //else
    //  pCaps->PS20Caps.NumInstructionSlots = 256;

    pCaps->VertexTextureFilterCaps = 50332416;
    pCaps->MaxVShaderInstructionsExecuted = 4294967295;
    pCaps->MaxPShaderInstructionsExecuted = 4294967295;

    //if (shaderModel == "3") {
      pCaps->MaxVertexShader30InstructionSlots = 32768;
      pCaps->MaxPixelShader30InstructionSlots = 32768;
    //}
    //else {
    //  pCaps->MaxVertexShader30InstructionSlots = 0;
    //  pCaps->MaxPixelShader30InstructionSlots = 0;
    //}

    return D3D_OK;
  }

}
