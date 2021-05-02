#include "dxvk_format.h"

namespace dxvk {
  
  const std::array<DxvkFormatInfo, 152> g_formatInfos = {{
    // VK_FORMAT_UNDEFINED
    { },
    
    // VK_FORMAT_R4G4_UNORM_PACK8
    { 1, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R4G4B4A4_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B4G4R4A4_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R5G6B5_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B5G6R5_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R5G5B5A1_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B5G5R5A1_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A1R5G5B5_UNORM_PACK16
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8_UNORM
    { 1, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8_SNORM
    { 1, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8_USCALED
    { 1, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8_SSCALED
    { 1, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8_UINT
    { 1, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R8_SINT
    { 1, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R8_SRGB
    { 1, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_R8G8_UNORM
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8_SNORM
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8_USCALED
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8_SSCALED
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8_UINT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R8G8_SINT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R8G8_SRGB
    { 2, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_R8G8B8_UNORM
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8_SNORM
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8_USCALED
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8_SSCALED
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8_UINT
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R8G8B8_SINT
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R8G8B8_SRGB
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_B8G8R8_UNORM
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8_SNORM
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8_USCALED
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8_SSCALED
    { 3, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8_UINT
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_B8G8R8_SINT
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_B8G8R8_SRGB
    { 3, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_R8G8B8A8_UNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8A8_SNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8A8_USCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8A8_SSCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R8G8B8A8_UINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R8G8B8A8_SINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R8G8B8A8_SRGB
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_B8G8R8A8_UNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8A8_SNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8A8_USCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8A8_SSCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B8G8R8A8_UINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_B8G8R8A8_SINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_B8G8R8A8_SRGB
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_A8B8G8R8_UNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A8B8G8R8_SNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A8B8G8R8_USCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A8B8G8R8_SSCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A8B8G8R8_UINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_A8B8G8R8_SINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_A8B8G8R8_SRGB_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::ColorSpaceSrgb },
    
    // VK_FORMAT_A2R10G10B10_UNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2R10G10B10_SNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2R10G10B10_USCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2R10G10B10_SSCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2R10G10B10_UINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_A2R10G10B10_SINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_A2B10G10R10_UNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2B10G10R10_SNORM_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2B10G10R10_USCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2B10G10R10_SSCALED_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_A2B10G10R10_UINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_A2B10G10R10_SINT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R16_UNORM
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16_SNORM
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16_USCALED
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16_SSCALED
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16_UINT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R16_SINT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R16_SFLOAT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16_UNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16_SNORM
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16_USCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16_SSCALED
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16_UINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R16G16_SINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R16G16_SFLOAT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16_UNORM
    { 6, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16_SNORM
    { 6, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16_USCALED
    { 6, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16_SSCALED
    { 6, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16_UINT
    { 6, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R16G16B16_SINT
    { 6, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R16G16B16_SFLOAT
    { 6, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16A16_UNORM
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16A16_SNORM
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16A16_USCALED
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16A16_SSCALED
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R16G16B16A16_UINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R16G16B16A16_SINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R16G16B16A16_SFLOAT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R32_UINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R32_SINT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R32_SFLOAT
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R32G32_UINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R32G32_SINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R32G32_SFLOAT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R32G32B32_UINT
    { 12, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R32G32B32_SINT
    { 12, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R32G32B32_SFLOAT
    { 12, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R32G32B32A32_UINT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R32G32B32A32_SINT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R32G32B32A32_SFLOAT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R64_UINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R64_SINT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R64_SFLOAT
    { 8, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R64G64_UINT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R64G64_SINT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R64G64_SFLOAT
    { 16, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R64G64B64_UINT
    { 24, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R64G64B64_SINT
    { 24, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R64G64B64_SFLOAT
    { 24, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_R64G64B64A64_UINT
    { 32, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledUInt },
    
    // VK_FORMAT_R64G64B64A64_SINT
    { 32, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::SampledSInt },
    
    // VK_FORMAT_R64G64B64A64_SFLOAT
    { 32, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_B10G11R11_UFLOAT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
    { 4, VK_IMAGE_ASPECT_COLOR_BIT },
    
    // VK_FORMAT_D16_UNORM
    { 2, VK_IMAGE_ASPECT_DEPTH_BIT },
    
    // VK_FORMAT_X8_D24_UNORM_PACK32
    { 4, VK_IMAGE_ASPECT_DEPTH_BIT },
    
    // VK_FORMAT_D32_SFLOAT
    { 4, VK_IMAGE_ASPECT_DEPTH_BIT },
    
    // VK_FORMAT_S8_UINT
    { 1, VK_IMAGE_ASPECT_STENCIL_BIT },
    
    // VK_FORMAT_D16_UNORM_S8_UINT
    { 4, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT },
    
    // VK_FORMAT_D24_UNORM_S8_UINT
    { 4, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT },
    
    // VK_FORMAT_D32_SFLOAT_S8_UINT
    { 8, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT },
    
    // VK_FORMAT_BC1_RGB_UNORM_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC1_RGB_SRGB_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlags(
        DxvkFormatFlag::BlockCompressed,
        DxvkFormatFlag::ColorSpaceSrgb),
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC1_RGBA_UNORM_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC1_RGBA_SRGB_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlags(
        DxvkFormatFlag::BlockCompressed,
        DxvkFormatFlag::ColorSpaceSrgb),
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC2_UNORM_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC2_SRGB_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlags(
        DxvkFormatFlag::BlockCompressed,
        DxvkFormatFlag::ColorSpaceSrgb),
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC3_UNORM_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC3_SRGB_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlags(
        DxvkFormatFlag::BlockCompressed,
        DxvkFormatFlag::ColorSpaceSrgb),
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC4_UNORM_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC4_SNORM_BLOCK
    { 8, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC5_UNORM_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC5_SNORM_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC6H_UFLOAT_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC6H_SFLOAT_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC7_UNORM_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_BC7_SRGB_BLOCK
    { 16, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlags(
        DxvkFormatFlag::BlockCompressed,
        DxvkFormatFlag::ColorSpaceSrgb),
      VkExtent3D { 4, 4, 1 } },
    
    // VK_FORMAT_G8B8G8R8_422_UNORM_KHR
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 2, 1, 1 } },
    
    // VK_FORMAT_B8G8R8G8_422_UNORM_KHR
    { 4, VK_IMAGE_ASPECT_COLOR_BIT,
      DxvkFormatFlag::BlockCompressed,
      VkExtent3D { 2, 1, 1 } },

    // VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },

    // VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT
    { 2, VK_IMAGE_ASPECT_COLOR_BIT },

    // VK_FORMAT_G8_B8R8_2PLANE_420_UNORM
    { 6, VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT,
      DxvkFormatFlag::MultiPlane, VkExtent3D { 1, 1, 1 },
      { DxvkPlaneFormatInfo { 1, { 1, 1 } },
        DxvkPlaneFormatInfo { 2, { 2, 2 } } } },
  }};
  
  
  const std::array<std::pair<VkFormat, VkFormat>, 4> g_formatGroups = {{
    { VK_FORMAT_UNDEFINED,                  VK_FORMAT_BC7_SRGB_BLOCK            },
    { VK_FORMAT_G8B8G8R8_422_UNORM_KHR,     VK_FORMAT_B8G8R8G8_422_UNORM_KHR    },
    { VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,  VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT },
    { VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,   VK_FORMAT_G8_B8R8_2PLANE_420_UNORM  },
  }};
  
  
  const DxvkFormatInfo* imageFormatInfo(VkFormat format) {
    uint32_t indexOffset = 0;
    
    for (const auto& group : g_formatGroups) {
      if (format >= group.first && format <= group.second) {
        uint32_t index = uint32_t(format) - uint32_t(group.first);
        return &g_formatInfos[indexOffset + index];
      } else {
        indexOffset += uint32_t(group.second)
                     - uint32_t(group.first) + 1;
      }
    }
    
    return nullptr;
  }
  
}