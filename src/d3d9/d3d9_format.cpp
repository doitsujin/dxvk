#include "d3d9_format.h"

namespace dxvk {

  std::ostream& operator << (std::ostream& os, D3D9Format format) {
    switch (format) {
    case D3D9Format::Unknown: os << "Unknown";

    case D3D9Format::R8G8B8: os << "R8G8B8";
    case D3D9Format::A8R8G8B8: os << "A8R8G8B8";
    case D3D9Format::X8R8G8B8: os << "X8R8G8B8";
    case D3D9Format::R5G6B5: os << "R5G6B5";
    case D3D9Format::X1R5G5B5: os << "X1R5G5B5";
    case D3D9Format::A1R5G5B5: os << "A1R5G5B5";
    case D3D9Format::A4R4G4B4: os << "A4R4G4B4";
    case D3D9Format::R3G3B2: os << "R3G3B2";
    case D3D9Format::A8: os << "A8";
    case D3D9Format::A8R3G3B2: os << "A8R3G3B2";
    case D3D9Format::X4R4G4B4: os << "X4R4G4B4";
    case D3D9Format::A2B10G10R10: os << "A2B10G10R10";
    case D3D9Format::A8B8G8R8: os << "A8B8G8R8";
    case D3D9Format::X8B8G8R8: os << "X8B8G8R8";
    case D3D9Format::G16R16: os << "G16R16";
    case D3D9Format::A2R10G10B10: os << "A2R10G10B10";
    case D3D9Format::A16B16G16R16: os << "A16B16G16R16";
    case D3D9Format::A8P8: os << "A8P8";
    case D3D9Format::P8: os << "P8";
    case D3D9Format::L8: os << "L8";
    case D3D9Format::A8L8: os << "A8L8";
    case D3D9Format::A4L4: os << "A4L4";
    case D3D9Format::V8U8: os << "V8U8";
    case D3D9Format::L6V5U5: os << "L6V5U5";
    case D3D9Format::X8L8V8U8: os << "X8L8V8U8";
    case D3D9Format::Q8W8V8U8: os << "Q8W8V8U8";
    case D3D9Format::V16U16: os << "V16U16";
    case D3D9Format::A2W10V10U10: os << "A2W10V10U10";
    case D3D9Format::UYVY: os << "UYVY";
    case D3D9Format::R8G8_B8G8: os << "R8G8_B8G8";
    case D3D9Format::YUY2: os << "YUY2";
    case D3D9Format::G8R8_G8B8: os << "G8R8_G8B8";
    case D3D9Format::DXT1: os << "DXT1";
    case D3D9Format::DXT2: os << "DXT2";
    case D3D9Format::DXT3: os << "DXT3";
    case D3D9Format::DXT4: os << "DXT4";
    case D3D9Format::DXT5: os << "DXT5";
    case D3D9Format::D16_LOCKABLE: os << "D16_LOCKABLE";
    case D3D9Format::D32: os << "D32";
    case D3D9Format::D15S1: os << "D15S1";
    case D3D9Format::D24S8: os << "D24S8";
    case D3D9Format::D24X8: os << "D24X8";
    case D3D9Format::D24X4S4: os << "D24X4S4";
    case D3D9Format::D16: os << "D16";
    case D3D9Format::D32F_LOCKABLE: os << "D32F_LOCKABLE";
    case D3D9Format::D24FS8: os << "D24FS8";
    case D3D9Format::D32_LOCKABLE: os << "D32_LOCKABLE";
    case D3D9Format::S8_LOCKABLE: os << "S8_LOCKABLE";
    case D3D9Format::L16: os << "L16";
    case D3D9Format::VERTEXDATA: os << "VERTEXDATA";
    case D3D9Format::INDEX16: os << "INDEX16";
    case D3D9Format::INDEX32: os << "INDEX32";
    case D3D9Format::Q16W16V16U16: os << "Q16W16V16U16";
    case D3D9Format::MULTI2_ARGB8: os << "MULTI2_ARGB8";
    case D3D9Format::R16F: os << "R16F";
    case D3D9Format::G16R16F: os << "G16R16F";
    case D3D9Format::A16B16G16R16F: os << "A16B16G16R16F";
    case D3D9Format::R32F: os << "R32F";
    case D3D9Format::G32R32F: os << "G32R32F";
    case D3D9Format::A32B32G32R32F: os << "A32B32G32R32F";
    case D3D9Format::CxV8U8: os << "CxV8U8";
    case D3D9Format::A1: os << "A1";
    case D3D9Format::A2B10G10R10_XR_BIAS: os << "A2B10G10R10_XR_BIAS";
    case D3D9Format::BINARYBUFFER: os << "BINARYBUFFER";

    // Driver Hacks / Unofficial Formats
    case D3D9Format::ATI1: os << "ATI1";
    case D3D9Format::ATI2: os << "ATI2";
    case D3D9Format::INST: os << "INST";
    case D3D9Format::DF24: os << "DF24";
    case D3D9Format::DF16: os << "DF16";
    case D3D9Format::NULL_FORMAT: os << "NULL_FORMAT";
    case D3D9Format::GET4: os << "GET4";
    case D3D9Format::GET1: os << "GET1";
    case D3D9Format::NVDB: os << "NVDB";
    case D3D9Format::A2M1: os << "A2M1";
    case D3D9Format::A2M0: os << "A2M0";
    case D3D9Format::ATOC: os << "ATOC";
    case D3D9Format::INTZ: os << "INTZ";
    default:
      os << "Invalid Format (" << static_cast<uint32_t>(format) << ")";
    }

    return os;
  }

}