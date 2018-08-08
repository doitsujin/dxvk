#include "d3d9_shader.h"

#include <cstring>

namespace dxvk {
  template <typename B, typename I>
  HRESULT D3D9Shader<B, I>::GetDevice(IDirect3DDevice9** ppDevice) {
    return D3D9DeviceChild::GetDevice(ppDevice);
  }

  template <typename B, typename I>
  HRESULT D3D9Shader<B, I>::GetFunction(void* pFunction, UINT* pSizeOfData) {
    if (!pFunction) {
      CHECK_NOT_NULL(pSizeOfData);
      *pSizeOfData = m_func.size();
    } else {
      std::memcpy(pFunction, m_func.data(), m_func.size());
    }
    return D3D_OK;
  }

  template class D3D9Shader<IDirect3DVertexShader9, ID3D11VertexShader>;

  template class D3D9Shader<IDirect3DPixelShader9, ID3D11PixelShader>;
}
