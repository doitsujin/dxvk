#include "d3d9_format.h"

namespace dxvk {

  std::ostream& operator << (std::ostream& os, D3D9Format format) {
    switch (format) {
    case D3D9Format::Unknown: os << "Unknown"; break;

    case D3D9Format::R8G8B8: os << "R8G8B8"; break;
    case D3D9Format::A8R8G8B8: os << "A8R8G8B8"; break;
    case D3D9Format::X8R8G8B8: os << "X8R8G8B8"; break;
    case D3D9Format::R5G6B5: os << "R5G6B5"; break;
    case D3D9Format::X1R5G5B5: os << "X1R5G5B5"; break;
    case D3D9Format::A1R5G5B5: os << "A1R5G5B5"; break;
    case D3D9Format::A4R4G4B4: os << "A4R4G4B4"; break;
    case D3D9Format::R3G3B2: os << "R3G3B2"; break;
    case D3D9Format::A8: os << "A8"; break;
    case D3D9Format::A8R3G3B2: os << "A8R3G3B2"; break;
    case D3D9Format::X4R4G4B4: os << "X4R4G4B4"; break;
    case D3D9Format::A2B10G10R10: os << "A2B10G10R10"; break;
    case D3D9Format::A8B8G8R8: os << "A8B8G8R8"; break;
    case D3D9Format::X8B8G8R8: os << "X8B8G8R8"; break;
    case D3D9Format::G16R16: os << "G16R16"; break;
    case D3D9Format::A2R10G10B10: os << "A2R10G10B10"; break;
    case D3D9Format::A16B16G16R16: os << "A16B16G16R16"; break;
    case D3D9Format::A8P8: os << "A8P8"; break;
    case D3D9Format::P8: os << "P8"; break;
    case D3D9Format::L8: os << "L8"; break;
    case D3D9Format::A8L8: os << "A8L8"; break;
    case D3D9Format::A4L4: os << "A4L4"; break;
    case D3D9Format::V8U8: os << "V8U8"; break;
    case D3D9Format::L6V5U5: os << "L6V5U5"; break;
    case D3D9Format::X8L8V8U8: os << "X8L8V8U8"; break;
    case D3D9Format::Q8W8V8U8: os << "Q8W8V8U8"; break;
    case D3D9Format::V16U16: os << "V16U16"; break;
    case D3D9Format::A2W10V10U10: os << "A2W10V10U10"; break;
    case D3D9Format::UYVY: os << "UYVY"; break;
    case D3D9Format::R8G8_B8G8: os << "R8G8_B8G8"; break;
    case D3D9Format::YUY2: os << "YUY2"; break;
    case D3D9Format::G8R8_G8B8: os << "G8R8_G8B8"; break;
    case D3D9Format::DXT1: os << "DXT1"; break;
    case D3D9Format::DXT2: os << "DXT2"; break;
    case D3D9Format::DXT3: os << "DXT3"; break;
    case D3D9Format::DXT4: os << "DXT4"; break;
    case D3D9Format::DXT5: os << "DXT5"; break;
    case D3D9Format::D16_LOCKABLE: os << "D16_LOCKABLE"; break;
    case D3D9Format::D32: os << "D32"; break;
    case D3D9Format::D15S1: os << "D15S1"; break;
    case D3D9Format::D24S8: os << "D24S8"; break;
    case D3D9Format::D24X8: os << "D24X8"; break;
    case D3D9Format::D24X4S4: os << "D24X4S4"; break;
    case D3D9Format::D16: os << "D16"; break;
    case D3D9Format::D32F_LOCKABLE: os << "D32F_LOCKABLE"; break;
    case D3D9Format::D24FS8: os << "D24FS8"; break;
    case D3D9Format::D32_LOCKABLE: os << "D32_LOCKABLE"; break;
    case D3D9Format::S8_LOCKABLE: os << "S8_LOCKABLE"; break;
    case D3D9Format::L16: os << "L16"; break;
    case D3D9Format::VERTEXDATA: os << "VERTEXDATA"; break;
    case D3D9Format::INDEX16: os << "INDEX16"; break;
    case D3D9Format::INDEX32: os << "INDEX32"; break;
    case D3D9Format::Q16W16V16U16: os << "Q16W16V16U16"; break;
    case D3D9Format::MULTI2_ARGB8: os << "MULTI2_ARGB8"; break;
    case D3D9Format::R16F: os << "R16F"; break;
    case D3D9Format::G16R16F: os << "G16R16F"; break;
    case D3D9Format::A16B16G16R16F: os << "A16B16G16R16F"; break;
    case D3D9Format::R32F: os << "R32F"; break;
    case D3D9Format::G32R32F: os << "G32R32F"; break;
    case D3D9Format::A32B32G32R32F: os << "A32B32G32R32F"; break;
    case D3D9Format::CxV8U8: os << "CxV8U8"; break;
    case D3D9Format::A1: os << "A1"; break;
    case D3D9Format::A2B10G10R10_XR_BIAS: os << "A2B10G10R10_XR_BIAS"; break;
    case D3D9Format::BINARYBUFFER: os << "BINARYBUFFER"; break;

    // Driver Hacks / Unofficial Formats
    case D3D9Format::ATI1: os << "ATI1"; break;
    case D3D9Format::ATI2: os << "ATI2"; break;
    case D3D9Format::INST: os << "INST"; break;
    case D3D9Format::DF24: os << "DF24"; break;
    case D3D9Format::DF16: os << "DF16"; break;
    case D3D9Format::NULL_FORMAT: os << "NULL_FORMAT"; break;
    case D3D9Format::GET4: os << "GET4"; break;
    case D3D9Format::GET1: os << "GET1"; break;
    case D3D9Format::NVDB: os << "NVDB"; break;
    case D3D9Format::A2M1: os << "A2M1"; break;
    case D3D9Format::A2M0: os << "A2M0"; break;
    case D3D9Format::ATOC: os << "ATOC"; break;
    case D3D9Format::INTZ: os << "INTZ"; break;
    default:
      os << "Invalid Format (" << static_cast<uint32_t>(format) << ")"; break;
    }

    return os;
  }

  // It is also worth noting that the msb/lsb-ness is flipped between VK and D3D9.
  D3D9_VK_FORMAT_MAPPING ConvertFormatUnfixed(D3D9Format Format) {
    switch (Format) {
      case D3D9Format::Unknown: return {};

      case D3D9Format::R8G8B8: return {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }};

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
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
          VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::R3G3B2: return {}; // Unsupported

      case D3D9Format::A8: return {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::A8R3G3B2: return {}; // Unsupported

      case D3D9Format::X4R4G4B4: return {
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
          VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_ONE }};

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

      case D3D9Format::L6V5U5: return {}; // Unsupported

      case D3D9Format::X8L8V8U8: return {}; // Unsupported

      case D3D9Format::Q8W8V8U8: return {
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::V16U16: return {
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT,
        { VK_COMPONENT_SWIZZLE_R,   VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE }};

      case D3D9Format::A2W10V10U10: return {}; // Unsupported

      case D3D9Format::UYVY: return {}; // Unsupported

      case D3D9Format::R8G8_B8G8: return {
        VK_FORMAT_G8B8G8R8_422_UNORM, // This format may have been _SCALED in DX9.
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::YUY2: return {}; // Unsupported

      case D3D9Format::G8R8_G8B8: return {
        VK_FORMAT_B8G8R8G8_422_UNORM, // This format may have been _SCALED in DX9.
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT1: return {
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_IMAGE_ASPECT_COLOR_BIT };

      case D3D9Format::DXT2: return {
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
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

      case D3D9Format::D15S1: return {
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT };

      case D3D9Format::D24S8: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT };

      case D3D9Format::D24X8: return {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT };

      case D3D9Format::D24X4S4: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT };

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
        VK_FORMAT_D32_SFLOAT,
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

      case D3D9Format::NVDB: return {}; // Unsupported

      case D3D9Format::A2M1: return {}; // Unsupported

      case D3D9Format::A2M0: return {}; // Unsupported

      case D3D9Format::ATOC: return {}; // Unsupported

      case D3D9Format::INTZ: return {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R }};

      case D3D9Format::RAWZ: return {}; // Unsupported

      default:
        Logger::warn(str::format("ConvertFormat: Unknown format encountered: ", Format));
        return {}; // Unsupported
    }
  }

  D3D9VkFormatTable::D3D9VkFormatTable(const Rc<DxvkAdapter>& adapter) {
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

    if (!m_d24s8Support)
      Logger::warn("D3D9: VK_FORMAT_D24_UNORM_S8_UINT -> VK_FORMAT_D32_SFLOAT_S8_UINT");

    if (!m_d16s8Support)
      Logger::warn("D3D9: VK_FORMAT_D16_UNORM_S8_UINT -> VK_FORMAT_D32_SFLOAT_S8_UINT");
  }

  D3D9_VK_FORMAT_MAPPING D3D9VkFormatTable::GetFormatMapping(
          D3D9Format          Format) const {
    D3D9_VK_FORMAT_MAPPING mapping = ConvertFormatUnfixed(Format);
    
    if (!m_d24s8Support && mapping.FormatColor == VK_FORMAT_D24_UNORM_S8_UINT)
      mapping.FormatColor = VK_FORMAT_D32_SFLOAT_S8_UINT;

    if (!m_d16s8Support && mapping.FormatColor == VK_FORMAT_D16_UNORM_S8_UINT)
      mapping.FormatColor = VK_FORMAT_D32_SFLOAT_S8_UINT;

    return mapping;
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