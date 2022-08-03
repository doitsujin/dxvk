#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_query.h"
#include "d3d11_texture.h"
#include "d3d11_video.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {

  D3D11DeviceContext::D3D11DeviceContext(
          D3D11Device*            pParent)
  : D3D11DeviceChild<ID3D11DeviceContext4>(pParent) {

  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
}
