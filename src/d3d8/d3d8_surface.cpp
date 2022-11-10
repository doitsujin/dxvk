
#include "d3d8_surface.h"
#include "d3d8_device.h"

namespace dxvk {

    Com<d3d9::IDirect3DSurface9> D3D8Surface::CreateBlitImage() {
      d3d9::D3DSURFACE_DESC desc;
      GetD3D9()->GetDesc(&desc);

      Com<d3d9::IDirect3DSurface9> image = nullptr;
      HRESULT res = GetParent()->GetD3D9()->CreateRenderTarget(
        desc.Width, desc.Height, desc.Format,
        d3d9::D3DMULTISAMPLE_NONE, 0,
        FALSE,
        &image,
        NULL);      
      
      if (FAILED(res))
        throw new DxvkError("D3D8: Failed to create blit image");
      
      image.ref();
      return image;
    }
}