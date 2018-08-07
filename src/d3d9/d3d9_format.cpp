#include "d3d9_format.h"

namespace dxvk {
  // This is the list of back buffer formats which D3D9 accepts.
  // These formats are supported on pretty much all modern GPUs.
  static std::unordered_map<_D3DFORMAT, DXGI_FORMAT> backBufferFormats {
    { D3DFMT_A1R5G5B5, DXGI_FORMAT_B5G5R5A1_UNORM },
    { D3DFMT_A2R10G10B10, DXGI_FORMAT_R10G10B10A2_UNORM },
    { D3DFMT_A8R8G8B8, DXGI_FORMAT_B8G8R8A8_UNORM },
    { D3DFMT_R5G6B5, DXGI_FORMAT_B5G6R5_UNORM },
    { D3DFMT_X1R5G5B5, DXGI_FORMAT_B5G5R5A1_UNORM },
    { D3DFMT_X8R8G8B8, DXGI_FORMAT_B8G8R8X8_UNORM },
  };

  bool SupportedBackBufferFormat(D3DFORMAT Format) {
    if (backBufferFormats.count(Format) == 1)
      return true;
    else {
        Logger::err(str::format("Unsupported display mode format: ", Format));
        return false;
    }
  }

  DXGI_FORMAT BackBufferFormatToDXGIFormat(D3DFORMAT Format) {
    const auto it = backBufferFormats.find(Format);
    if (it == backBufferFormats.end()) {
      Logger::err(str::format("Unsupported back buffer format: ", Format));
      return DXGI_FORMAT_UNKNOWN;
    } else {
      return it->second;
    }
  }
}
