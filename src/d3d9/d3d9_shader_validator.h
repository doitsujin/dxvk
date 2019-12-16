#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class IDirect3DShaderValidator9 : public IUnknown {

  public:

    virtual HRESULT STDMETHODCALLTYPE Begin(
            void* pCallback,
            void* pUserParam,
            DWORD Unknown) = 0;

    virtual HRESULT STDMETHODCALLTYPE Instruction(
      const char*  pUnknown1,
            UINT   Unknown2,
      const DWORD* pInstruction,
            DWORD  InstructionLength) = 0;

    virtual HRESULT STDMETHODCALLTYPE End() = 0;

  };

  class D3D9ShaderValidator final : public ComObjectClamp<IDirect3DShaderValidator9> {

  public:

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = ref(this);
      return S_OK;
    }


    HRESULT STDMETHODCALLTYPE Begin(
            void* pCallback,
            void* pUserParam,
            DWORD Unknown) {
      Logger::debug("D3D9ShaderValidator::Begin: Stub");

      return D3D_OK;
    }


    HRESULT STDMETHODCALLTYPE Instruction(
      const char*  pUnknown1,
            UINT   Unknown2,
      const DWORD* pInstruction,
            DWORD  InstructionLength) {
      Logger::debug("D3D9ShaderValidator::Instruction: Stub");

      return D3D_OK;
    }


    HRESULT STDMETHODCALLTYPE End() {
      Logger::debug("D3D9ShaderValidator::End: Stub");

      return D3D_OK;
    }

  };

}