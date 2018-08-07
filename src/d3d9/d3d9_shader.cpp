#include "d3d9_shader.h"

#include "d3d9_device_child.h"

#include <cstring>

namespace dxvk {
  /// Shader class.
  template <typename Base, typename I>
  class D3D9Shader final: public ComObject<Base>, D3D9DeviceChild {
  public:
    HRESULT GetDevice(IDirect3DDevice9** ppDevice) {
      return D3D9DeviceChild::GetDevice(ppDevice);
    }

    HRESULT GetFunction(void* pFunction, UINT* pSizeOfData) {
      if (!pFunction) {
        CHECK_NOT_NULL(pSizeOfData);
        *pSizeOfData = m_func.size();
      } else {
        std::memcpy(pFunction, m_func.data(), m_func.size());
      }
      return D3D_OK;
    }
  private:
    std::vector<DWORD> m_func;
    Com<I> m_shader;
  };

  using D3D9VertexShader = D3D9Shader<IDirect3DVertexShader9, ID3D11VertexShader>;
  using D3D9PixelShader = D3D9Shader<IDirect3DPixelShader9, ID3D11PixelShader>;
}
