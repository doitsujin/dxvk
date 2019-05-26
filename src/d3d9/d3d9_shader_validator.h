#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class IDirect3DShaderValidator9 : public IUnknown {

  public:

    virtual LONG STDMETHODCALLTYPE Begin(void* pCallback, void* pUnknown1, ULONG Unknown2) = 0;
    virtual LONG STDMETHODCALLTYPE Instruction(const char* pUnknown1, UINT Unknown2, const UINT* Unknown3, UINT Unknown4) = 0;
    virtual LONG STDMETHODCALLTYPE End() = 0;
  };

  class D3D9ShaderValidator : public ComObject<IDirect3DShaderValidator9> {

  public:

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      // I don't know the UUID of this... It's all undocumented. :P
      *ppvObject = ref(this);
      return S_OK;
    }

    LONG STDMETHODCALLTYPE Begin(void* pCallback, void* pUnknown1, ULONG Unknown2) { return 1; }
    LONG STDMETHODCALLTYPE Instruction(const char* pUnknown1, UINT Unknown2, const UINT* Unknown3, UINT Unknown4) { return 1; }
    LONG STDMETHODCALLTYPE End() { return 1; }

  };

}