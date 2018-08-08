#include "d3d9_format.h"

namespace dxvk {
  static std::unordered_map<_D3DFORMAT, DXGI_FORMAT> surfaceFormats {
    { D3DFMT_A1R5G5B5, DXGI_FORMAT_B5G5R5A1_UNORM },
    { D3DFMT_A2R10G10B10, DXGI_FORMAT_R10G10B10A2_UNORM },
    { D3DFMT_A8R8G8B8, DXGI_FORMAT_B8G8R8A8_UNORM },
    { D3DFMT_R5G6B5, DXGI_FORMAT_B5G6R5_UNORM },
    { D3DFMT_X1R5G5B5, DXGI_FORMAT_B5G5R5A1_UNORM },
    { D3DFMT_X8R8G8B8, DXGI_FORMAT_B8G8R8X8_UNORM },

    // Depth/stencil formats.
    { D3DFMT_D24S8, DXGI_FORMAT_D24_UNORM_S8_UINT },
  };

  static std::unordered_map<DXGI_FORMAT, _D3DFORMAT> formatMapping {
    { DXGI_FORMAT_UNKNOWN, D3DFMT_UNKNOWN },
    { DXGI_FORMAT_B8G8R8X8_UNORM, D3DFMT_X8B8G8R8 },
  };

  DXGI_FORMAT SurfaceFormatToDXGIFormat(D3DFORMAT Format) {
    const auto it = surfaceFormats.find(Format);
    if (it == surfaceFormats.end()) {
      Logger::err(str::format("Unsupported surface format: ", Format));
      return DXGI_FORMAT_UNKNOWN;
    } else {
      return it->second;
    }
  }

  D3DFORMAT DXGIFormatToSurfaceFormat(DXGI_FORMAT Format) {
    const auto it = formatMapping.find(Format);

    if (it == formatMapping.end()) {
      Logger::err(str::format("Unsupported D3D9 format: ", Format));
      return D3DFMT_UNKNOWN;
    } else {
      return it->second;
    }
  }
}
