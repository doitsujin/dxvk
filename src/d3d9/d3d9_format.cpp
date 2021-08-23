#include "d3d9_format.h"

namespace dxvk {

  // It is also worth noting that the msb/lsb-ness is flipped between VK and D3D9.
  D3D9_VK_FORMAT_MAPPING ConvertFormatUnfixed(D3D9Format Format) {
    switch (Format) {
      case D3D9Format::Unknown: return {};

      case D3D9Format::R8G8B8: return {}; // Unsupported

      case D3D9Format::A8R8G8B8: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::X8R8G8B8: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::R5G6B5: return {
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT};

      case D3D9Format::X1R5G5B5: return {
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A1R5G5B5: return {
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::A4R4G4B4: return {
        VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::R3G3B2: return {}; // Unsupported

      case D3D9Format::A8: return {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::A8R3G3B2: return {}; // Unsupported

      case D3D9Format::X4R4G4B4: return {
        VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::A2B10G10R10: return {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32, // The A2 is out of place here. This should be investigated.
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::A8B8G8R8: return {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::X8B8G8R8: return {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::G16R16: return {
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A2R10G10B10: return {
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::A16B16G16R16: return {
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::A8P8: return {}; // Unsupported

      case D3D9Format::P8: return {}; // Unsupported

      case D3D9Format::L8: return {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A8L8: return {
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G }};

      case D3D9Format::A4L4: return {
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::V8U8: return {
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::L6V5U5: return {
        // Any PACK16 format will do...
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        { D3D9ConversionFormat_L6V5U5, 1u,
        // Convert -> float (this is a mixed snorm and unorm type)
          VK_FORMAT_R16G16B16A16_SFLOAT } };

      case D3D9Format::X8L8V8U8: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE },
        { D3D9ConversionFormat_X8L8V8U8, 1u,
        // Convert -> float (this is a mixed snorm and unorm type)
          VK_FORMAT_R16G16B16A16_SFLOAT } };

      case D3D9Format::Q8W8V8U8: return {
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::V16U16: return {
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A2W10V10U10: return {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        { D3D9ConversionFormat_A2W10V10U10, 1u,
        // Convert -> float (this is a mixed snorm and unorm type)
          VK_FORMAT_R16G16B16A16_SFLOAT } };

      case D3D9Format::UYVY: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { D3D9ConversionFormat_UYVY, 1u }
      };

      case D3D9Format::R8G8_B8G8: return {
        VK_FORMAT_G8B8G8R8_422_UNORM, // This format may have been _SCALED in DX9.
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::YUY2: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { D3D9ConversionFormat_YUY2, 1u }
      };

      case D3D9Format::G8R8_G8B8: return {
        VK_FORMAT_B8G8R8G8_422_UNORM, // This format may have been _SCALED in DX9.
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT1: return {
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT2: return {
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT3: return {
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT4: return {
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT5: return {
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::D16_LOCKABLE: return {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D32: return {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D15S1: return {}; // Unsupported (everywhere)

      case D3D9Format::D24S8: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT };

      case D3D9Format::D24X8: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D24X4S4: return {}; // Unsupported (everywhere)

      case D3D9Format::D16: return {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D32F_LOCKABLE: return {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D24FS8: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT };

      case D3D9Format::D32_LOCKABLE: return {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::S8_LOCKABLE: return {
        VK_FORMAT_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_STENCIL_BIT };

      case D3D9Format::L16: return {
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::VERTEXDATA: return {
        VK_FORMAT_R8_UINT,
        VK_FORMAT_UNDEFINED,
        0 };

      case D3D9Format::INDEX16: return {
        VK_FORMAT_R16_UINT,
        VK_FORMAT_UNDEFINED,
        0 };

      case D3D9Format::INDEX32: return {
        VK_FORMAT_R32_UINT,
        VK_FORMAT_UNDEFINED,
        0 };

      case D3D9Format::Q16W16V16U16: return {
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::MULTI2_ARGB8: return {}; // Unsupported

      case D3D9Format::R16F: return {
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_ONE,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::G16R16F: return {
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A16B16G16R16F: return {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::R32F: return {
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_ONE,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::G32R32F: return {
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A32B32G32R32F: return {
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::CxV8U8: return {}; // Unsupported

      case D3D9Format::A1: return {}; // Unsupported

      case D3D9Format::A2B10G10R10_XR_BIAS: return {
        VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::BINARYBUFFER: return {
        VK_FORMAT_R8_UINT,
        VK_FORMAT_UNDEFINED,
        0 };

      case D3D9Format::ATI1: return {
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,    VK_COMPONENT_SWIZZLE_ZERO,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::ATI2: return {
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_G,   VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::INST: return {}; // Driver hack, handled elsewhere

      case D3D9Format::DF24: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        { VK_COMPONENT_SWIZZLE_R,    VK_COMPONENT_SWIZZLE_ZERO,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::DF16: return {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        { VK_COMPONENT_SWIZZLE_R,    VK_COMPONENT_SWIZZLE_ZERO,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::NULL_FORMAT: return {}; // Driver hack, handled elsewhere

      case D3D9Format::GET4: return {}; // Unsupported

      case D3D9Format::GET1: return {}; // Unsupported

      case D3D9Format::NVDB: return {}; // Driver hack, handled elsewhere

      case D3D9Format::A2M1: return {}; // Driver hack, handled elsewhere

      case D3D9Format::A2M0: return {}; // Driver hack, handled elsewhere

      case D3D9Format::ATOC: return {}; // Driver hack, handled elsewhere

      case D3D9Format::INTZ: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::NV12: return {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { D3D9ConversionFormat_NV12, 2u, VK_FORMAT_B8G8R8A8_UNORM }
      };

      case D3D9Format::YV12: return {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { D3D9ConversionFormat_YV12, 3u, VK_FORMAT_B8G8R8A8_UNORM }
      };

      case D3D9Format::RAWZ: return {}; // Unsupported

      default:
        Logger::warn(str::format("ConvertFormat: Unknown format encountered: ", Format));
        return {}; // Unsupported
    }
  }

  D3D9VkFormatTable::D3D9VkFormatTable(
    const Rc<DxvkAdapter>& adapter,
    const D3D9Options&     options) {
    m_dfSupport = options.supportDFFormats;
    m_x4r4g4b4Support = options.supportX4R4G4B4;
    m_d32supportFinal = options.supportD32;

    // AMD do not support 24-bit depth buffers on Vulkan,
    // so we have to fall back to a 32-bit depth format.
    m_d24s8Support = CheckImageFormatSupport(adapter, VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    // NVIDIA do not support 16-bit depth buffers with stencil on Vulkan,
    // so we have to fall back to a 32-bit depth format.
    m_d16s8Support = CheckImageFormatSupport(adapter, VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    // VK_EXT_4444_formats
    m_a4r4g4b4Support = CheckImageFormatSupport(adapter, VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    if (!m_d24s8Support)
      Logger::info("D3D9: VK_FORMAT_D24_UNORM_S8_UINT -> VK_FORMAT_D32_SFLOAT_S8_UINT");

    if (!m_d16s8Support) {
      if (m_d24s8Support)
        Logger::info("D3D9: VK_FORMAT_D16_UNORM_S8_UINT -> VK_FORMAT_D24_UNORM_S8_UINT");
      else
        Logger::info("D3D9: VK_FORMAT_D16_UNORM_S8_UINT -> VK_FORMAT_D32_SFLOAT_S8_UINT");
    }

    if (!m_a4r4g4b4Support)
      Logger::warn("D3D9: VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT -> VK_FORMAT_B4G4R4A4_UNORM_PACK16");
  }

  D3D9_VK_FORMAT_MAPPING D3D9VkFormatTable::GetFormatMapping(
          D3D9Format          Format) const {
    D3D9_VK_FORMAT_MAPPING mapping = ConvertFormatUnfixed(Format);

    if (Format == D3D9Format::X4R4G4B4 && !m_x4r4g4b4Support)
      return D3D9_VK_FORMAT_MAPPING();

    if (Format == D3D9Format::DF16 && !m_dfSupport)
      return D3D9_VK_FORMAT_MAPPING();

    if (Format == D3D9Format::DF24 && !m_dfSupport)
      return D3D9_VK_FORMAT_MAPPING();

    if (Format == D3D9Format::D32 && !m_d32supportFinal)
      return D3D9_VK_FORMAT_MAPPING();
    
    if (!m_d24s8Support && mapping.FormatColor == VK_FORMAT_D24_UNORM_S8_UINT)
      mapping.FormatColor = VK_FORMAT_D32_SFLOAT_S8_UINT;

    if (!m_d16s8Support && mapping.FormatColor == VK_FORMAT_D16_UNORM_S8_UINT)
      mapping.FormatColor = m_d24s8Support ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;

    if (!m_a4r4g4b4Support && mapping.FormatColor == VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT) {
      VkComponentSwizzle alphaSwizzle = Format == D3D9Format::A4R4G4B4
        ? VK_COMPONENT_SWIZZLE_B
        : VK_COMPONENT_SWIZZLE_ONE;

      mapping.FormatColor = VK_FORMAT_B4G4R4A4_UNORM_PACK16;
      mapping.Swizzle     = {
        VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_A, alphaSwizzle };
    }

    return mapping;
  }


  const DxvkFormatInfo* D3D9VkFormatTable::GetUnsupportedFormatInfo(
    D3D9Format            Format) const {
    static const DxvkFormatInfo r8b8g8      = { 3, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo r3g3b2      = { 1, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo a8r3g3b2    = { 2, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo a8p8        = { 2, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo p8          = { 1, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo l6v5u5      = { 2, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo x8l8v8u8    = { 4, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo a2w10v10u10 = { 4, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo cxv8u8      = { 2, VK_IMAGE_ASPECT_COLOR_BIT };
    static const DxvkFormatInfo unknown     = {};

    switch (Format) {
      case D3D9Format::R8G8B8:
        return &r8b8g8;

      case D3D9Format::R3G3B2:
        return &r3g3b2;

      case D3D9Format::A8R3G3B2:
        return &a8r3g3b2;

      case D3D9Format::A8P8:
        return &a8p8;

      case D3D9Format::P8:
        return &p8;

      case D3D9Format::L6V5U5:
        return &l6v5u5;

      case D3D9Format::X8L8V8U8:
        return &x8l8v8u8;

      case D3D9Format::A2W10V10U10:
        return &a2w10v10u10;

      // MULTI2_ARGB8 -> Don't have a clue what this is.

      case D3D9Format::CxV8U8:
        return &cxv8u8;

      // A1 -> Doesn't map nicely here cause it's not byte aligned.
      // Gonna just pretend that doesn't exist until something
      // depends on that.

      default:
        return &unknown;
    }
  }
  

  bool D3D9VkFormatTable::CheckImageFormatSupport(
    const Rc<DxvkAdapter>&      Adapter,
          VkFormat              Format,
          VkFormatFeatureFlags  Features) const {
    VkFormatProperties supported = Adapter->formatProperties(Format);
    
    return (supported.linearTilingFeatures  & Features) == Features
        || (supported.optimalTilingFeatures & Features) == Features;
  }

}