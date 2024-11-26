#pragma once

#include <d3d9.h>
#include "d3d9_options.h"

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
    W11V11U10 = 65,
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

    NV11 = MAKEFOURCC('N', 'V', '1', '1'),
    NV12 = MAKEFOURCC('N', 'V', '1', '2'),
    P010 = MAKEFOURCC('P', '0', '1', '0'), // Same as NV12 but 10 bit
    P016 = MAKEFOURCC('P', '0', '1', '6'), // Same as NV12 but 16 bit
    Y210 = MAKEFOURCC('Y', '2', '1', '0'),
    Y216 = MAKEFOURCC('Y', '2', '1', '6'),
    Y410 = MAKEFOURCC('Y', '4', '1', '0'),
    AYUV = MAKEFOURCC('A', 'Y', 'U', 'V'),
    YV12 = MAKEFOURCC('Y', 'V', '1', '2'),
    OPAQUE_420 = MAKEFOURCC('4', '2', '0', 'O'),

    // Not supported but exist
    AI44 = MAKEFOURCC('A', 'I', '4', '4'),
    IA44 = MAKEFOURCC('I', 'A', '4', '4'),
    R2VB = MAKEFOURCC('R', '2', 'V', 'B'),
    COPM = MAKEFOURCC('C', 'O', 'P', 'M'),
    SSAA = MAKEFOURCC('S', 'S', 'A', 'A'),
    AL16 = MAKEFOURCC('A', 'L', '1', '6'),
    R16  = MAKEFOURCC(' ', 'R', '1', '6'),

    EXT1 = MAKEFOURCC('E', 'X', 'T', '1'),
    FXT1 = MAKEFOURCC('F', 'X', 'T', '1'),
    GXT1 = MAKEFOURCC('G', 'X', 'T', '1'),
    HXT1 = MAKEFOURCC('H', 'X', 'T', '1'),
  };

  inline D3D9Format EnumerateFormat(D3DFORMAT format) {
    return static_cast<D3D9Format>(format);
  }

  std::ostream& operator << (std::ostream& os, D3D9Format format);

  enum D3D9ConversionFormat : uint32_t {
    D3D9ConversionFormat_None = 0,
    D3D9ConversionFormat_YUY2 = 1,
    D3D9ConversionFormat_UYVY,
    D3D9ConversionFormat_L6V5U5,
    D3D9ConversionFormat_X8L8V8U8,
    D3D9ConversionFormat_A2W10V10U10,
    D3D9ConversionFormat_W11V11U10,
    D3D9ConversionFormat_NV12,
    D3D9ConversionFormat_YV12,
    D3D9ConversionFormat_Count
  };

  struct D3D9_CONVERSION_FORMAT_INFO {
    D3D9ConversionFormat FormatType     = D3D9ConversionFormat_None;
    VkFormat             FormatColor    = VK_FORMAT_UNDEFINED;
    VkFormat             FormatSrgb     = VK_FORMAT_UNDEFINED;
  };

  struct D3D9_FORMAT_BLOCK_SIZE {
    uint8_t            Width  = 0;
    uint8_t            Height = 0;
    uint8_t            Depth  = 0;
  };

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
    D3D9_CONVERSION_FORMAT_INFO ConversionFormatInfo = { };

    bool IsValid() const { return FormatColor != VK_FORMAT_UNDEFINED; }
  };

  D3D9_VK_FORMAT_MAPPING ConvertFormatUnfixed(D3D9Format Format);

  D3D9_FORMAT_BLOCK_SIZE GetFormatAlignedBlockSize(D3D9Format Format);

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
      const Rc<DxvkAdapter>& adapter,
      const D3D9Options&     options);

    /**
     * \brief Retrieves info for a given D3D9 format
     *
     * \param [in] Format The D3D9 format to look up
     * \param [in] Mode the format lookup mode
     * \returns Format info
     */
    D3D9_VK_FORMAT_MAPPING GetFormatMapping(
      D3D9Format            Format) const;

    /**
     * \brief Retrieves format info for unsupported
     * formats.
     *
     * \param [in] Format The D3D9 format to look up
     */
    const DxvkFormatInfo* GetUnsupportedFormatInfo(
      D3D9Format            Format) const;

  private:

    bool CheckImageFormatSupport(
      const Rc<DxvkAdapter>&      Adapter,
      VkFormat              Format,
      VkFormatFeatureFlags2 Features) const;

    bool m_d24s8Support;
    bool m_d16s8Support;

    bool m_dfSupport;
    bool m_x4r4g4b4Support;
    bool m_d16lockableSupport;
  };

  inline bool IsFourCCFormat(D3D9Format format) {
    // BINARYBUFFER is the largest non-fourcc format
    return format > D3D9Format::BINARYBUFFER;
  }

  inline bool IsVendorFormat(D3D9Format format) {
    return IsFourCCFormat(format)
      && format != D3D9Format::MULTI2_ARGB8
      && format != D3D9Format::UYVY
      && format != D3D9Format::R8G8_B8G8
      && format != D3D9Format::YUY2
      && format != D3D9Format::G8R8_G8B8
      && format != D3D9Format::DXT1
      && format != D3D9Format::DXT2
      && format != D3D9Format::DXT3
      && format != D3D9Format::DXT4
      && format != D3D9Format::DXT5;
  }

  inline bool IsDXTFormat(D3D9Format format) {
    return format == D3D9Format::DXT1
        || format == D3D9Format::DXT2
        || format == D3D9Format::DXT3
        || format == D3D9Format::DXT4
        || format == D3D9Format::DXT5;
  }

  // D3D9 documentation says: IDirect3DSurface9::GetDC is valid on the following formats only:
  // D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_R8G8B8, and D3DFMT_X8R8G8B8. However,
  // the equivalent formats of D3DFMT_A1R5G5B5 and D3DFMT_A8R8G8B8 are also supported.
  inline bool IsSurfaceGetDCCompatibleFormat(D3D9Format format) {
    return format == D3D9Format::R5G6B5
        || format == D3D9Format::X1R5G5B5
        || format == D3D9Format::A1R5G5B5
        || format == D3D9Format::R8G8B8
        || format == D3D9Format::X8R8G8B8
        || format == D3D9Format::A8R8G8B8;
  }

}
