#include "d3d9_monitor.h"

#include "d3d9_format.h"

namespace dxvk {

  uint32_t GetMonitorFormatBpp(D3D9Format Format) {
    switch (Format) {
    case D3D9Format::A8R8G8B8:
    case D3D9Format::X8R8G8B8: // This is still 32 bit even though the alpha is unspecified.
    case D3D9Format::A2R10G10B10:
      return 32;

    case D3D9Format::A1R5G5B5:
    case D3D9Format::X1R5G5B5:
    case D3D9Format::R5G6B5:
      return 16;

    default:
      Logger::warn(str::format(
        "GetMonitorFormatBpp: Unknown format: ",
        Format));
      return 32;
    }
  }


  bool IsSupportedAdapterFormat(
          D3D9Format Format,
          BOOL       Windowed) {
    return (Format == D3D9Format::A2R10G10B10 && !Windowed)
         || Format == D3D9Format::X8R8G8B8
         || Format == D3D9Format::X1R5G5B5
         || Format == D3D9Format::R5G6B5;
  }


  bool IsSupportedBackBufferFormat(
          D3D9Format BackBufferFormat,
          BOOL       Windowed) {
    return (BackBufferFormat == D3D9Format::A2R10G10B10 && !Windowed)
         || BackBufferFormat == D3D9Format::A8R8G8B8
         || BackBufferFormat == D3D9Format::X8R8G8B8
         || BackBufferFormat == D3D9Format::A1R5G5B5
         || BackBufferFormat == D3D9Format::X1R5G5B5
         || BackBufferFormat == D3D9Format::R5G6B5;
  }

}