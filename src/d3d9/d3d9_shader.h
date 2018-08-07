#pragma once

#include "d3d9_device.h"

namespace dxvk {
  /// Vertex shader device functions.
  class D3D9DeviceVertexShader: public virtual ComObject<D3D9Device> {
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* pFunction,
      IDirect3DVertexShader9** ppShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* pShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT StartRegister,
      const BOOL *pConstantData, UINT BoolCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT StartRegister,
      const float* pConstantData, UINT Vector4fCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT StartRegister,
      const int *pConstantData, UINT Vector4iCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** ppShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT StartRegister,
      BOOL* pConstantData, UINT BoolCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT StartRegister,
      float* pConstantData, UINT Vector4fCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT StartRegister,
      int* pConstantData, UINT Vector4iCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }
  };

  /// Pixel shader device functions.
  class D3D9DevicePixelShader: public virtual ComObject<D3D9Device> {
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* pFunction,
      IDirect3DPixelShader9** ppShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** ppShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT StartRegister,
      BOOL* pConstantData, UINT BoolCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT StartRegister,
      float* pConstantData, UINT Vector4fCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT StartRegister,
      int* pConstantData, UINT Vector4iCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9 *pShader) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT StartRegister,
      const BOOL* pConstantData, UINT BoolCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT StartRegister,
      const float* pConstantData, UINT Vector4fCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT StartRegister,
      const int* pConstantData, UINT Vector4iCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }
  };
}
