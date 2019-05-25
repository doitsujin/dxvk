#pragma once

#include "d3d9_include.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_format.h"

#include <unordered_map>

namespace dxvk {

  enum class D3D9Format : uint32_t {
    Unknown = 0,

    R8G8B8 = 20,
    A8R8G8B8 = 21,
    X8R8G8B8 = 22,
    R5G6B5 = 23,
    X1R5G5B5 = 24,
    A1R5G5B5 = 25,
    A4R4G4B4 = 26,
    R3G3B2 = 27,
    A8 = 28,
    A8R3G3B2 = 29,
    X4R4G4B4 = 30,
    A2B10G10R10 = 31,
    A8B8G8R8 = 32,
    X8B8G8R8 = 33,
    G16R16 = 34,
    A2R10G10B10 = 35,
    A16B16G16R16 = 36,
    A8P8 = 40,
    P8 = 41,
    L8 = 50,
    A8L8 = 51,
    A4L4 = 52,
    V8U8 = 60,
    L6V5U5 = 61,
    X8L8V8U8 = 62,
    Q8W8V8U8 = 63,
    V16U16 = 64,
    A2W10V10U10 = 67,
    UYVY = MAKEFOURCC('U', 'Y', 'V', 'Y'),
    R8G8_B8G8 = MAKEFOURCC('R', 'G', 'B', 'G'),
    YUY2 = MAKEFOURCC('Y', 'U', 'Y', '2'),
    G8R8_G8B8 = MAKEFOURCC('G', 'R', 'G', 'B'),
    DXT1 = MAKEFOURCC('D', 'X', 'T', '1'),
    DXT2 = MAKEFOURCC('D', 'X', 'T', '2'),
    DXT3 = MAKEFOURCC('D', 'X', 'T', '3'),
    DXT4 = MAKEFOURCC('D', 'X', 'T', '4'),
    DXT5 = MAKEFOURCC('D', 'X', 'T', '5'),
    D16_LOCKABLE = 70,
    D32 = 71,
    D15S1 = 73,
    D24S8 = 75,
    D24X8 = 77,
    D24X4S4 = 79,
    D16 = 80,
    D32F_LOCKABLE = 82,
    D24FS8 = 83,
    D32_LOCKABLE = 84,
    S8_LOCKABLE = 85,
    L16 = 81,
    VERTEXDATA = 100,
    INDEX16 = 101,
    INDEX32 = 102,
    Q16W16V16U16 = 110,
    MULTI2_ARGB8 = MAKEFOURCC('M', 'E', 'T', '1'),
    R16F = 111,
    G16R16F = 112,
    A16B16G16R16F = 113,
    R32F = 114,
    G32R32F = 115,
    A32B32G32R32F = 116,
    CxV8U8 = 117,
    A1 = 118,
    A2B10G10R10_XR_BIAS = 119,
    BINARYBUFFER = 199,

    // Driver Hacks / Unofficial Formats
    ATI1 = MAKEFOURCC('A', 'T', 'I', '1'),
    ATI2 = MAKEFOURCC('A', 'T', 'I', '2'),
    INST = MAKEFOURCC('I', 'N', 'S', 'T'),
    DF24 = MAKEFOURCC('D', 'F', '2', '4'),
    DF16 = MAKEFOURCC('D', 'F', '1', '6'),
    NULL_FORMAT = MAKEFOURCC('N', 'U', 'L', 'L'),
    GET4 = MAKEFOURCC('G', 'E', 'T', '4'),
    GET1 = MAKEFOURCC('G', 'E', 'T', '1'),
    NVDB = MAKEFOURCC('N', 'V', 'D', 'B'),
    A2M1 = MAKEFOURCC('A', '2', 'M', '1'),
    A2M0 = MAKEFOURCC('A', '2', 'M', '0'),
    ATOC = MAKEFOURCC('A', 'T', 'O', 'C'),
    INTZ = MAKEFOURCC('I', 'N', 'T', 'Z'),
    RAWZ = MAKEFOURCC('R', 'A', 'W', 'Z'),
    RESZ = MAKEFOURCC('R', 'E', 'S', 'Z'),
  };

  inline D3D9Format EnumerateFormat(D3DFORMAT format) {
    return static_cast<D3D9Format>(format);
  }

  std::ostream& operator << (std::ostream& os, D3D9Format format);

  /**
   * \brief Format mapping
   * 
   * Maps a D3D9 format to a set of Vulkan formats.
   */
  struct D3D9_VK_FORMAT_MAPPING {
    union {
      struct {
        VkFormat          FormatColor;                          ///< Corresponding color format
        VkFormat          FormatSrgb;                           ///< Corresponding color format
      };
      VkFormat            Formats[2];
    };
    VkImageAspectFlags    Aspect        = 0;                    ///< Defined aspects for the color format
    VkComponentMapping    Swizzle       = {                     ///< Color component swizzle
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
  };

  D3D9_VK_FORMAT_MAPPING ConvertFormatUnfixed(D3D9Format Format);

  /**
   * \brief Format table
   *
   * Initializes a format table for a specific
   * device and provides methods to look up
   * formats.
   */
  class D3D9VkFormatTable {

  public:

    D3D9VkFormatTable(
      const Rc<DxvkAdapter>& adapter);

    /**
     * \brief Retrieves info for a given D3D9 format
     *
     * \param [in] Format The D3D9 format to look up
     * \param [in] Mode the format lookup mode
     * \returns Format info
     */
    D3D9_VK_FORMAT_MAPPING GetFormatMapping(
      D3D9Format            Format) const;

  private:

    bool CheckImageFormatSupport(
      const Rc<DxvkAdapter>&      Adapter,
      VkFormat              Format,
      VkFormatFeatureFlags  Features) const;

    bool m_d24s8Support;
    bool m_d16s8Support;

  };

}