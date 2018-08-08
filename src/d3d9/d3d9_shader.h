#pragma once

#include "d3d9_include.h"
#include "d3d9_resource.h"

namespace dxvk {
  /// Shader class.
  template <typename Base, typename I>
  class D3D9Shader final: public ComObject<Base> {
  public:
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) final override;
    HRESULT STDMETHODCALLTYPE GetFunction(void* pFunction, UINT* pSizeOfData) override;
  private:
    Com<IDirect3DDevice9> m_parent;
    std::vector<DWORD> m_func;
    Com<I> m_shader;
  };

  using D3D9VertexShader = D3D9Shader<IDirect3DVertexShader9, ID3D11VertexShader>;
  using D3D9PixelShader = D3D9Shader<IDirect3DPixelShader9, ID3D11PixelShader>;
}
