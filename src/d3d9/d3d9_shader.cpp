#include "d3d9_shader.h"

#include "d3d9_device.h"

#include <cstring>

namespace dxvk {
  // Get a reference to the owning device.
  template <typename B, typename I>
  HRESULT D3D9Shader<B, I>::GetDevice(IDirect3DDevice9** ppDevice) {
    InitReturnPtr(ppDevice);
    CHECK_NOT_NULL(ppDevice);

    *ppDevice = m_parent.ref();

    return D3D_OK;
  }

  // Retrieve the compiled shader code.
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

  // Explicitly instantiate these classes here.
  template class D3D9Shader<IDirect3DVertexShader9, ID3D11VertexShader>;
  template class D3D9Shader<IDirect3DPixelShader9, ID3D11PixelShader>;

  HRESULT D3D9Device::CreateVertexShader(const DWORD* pFunction,
    IDirect3DVertexShader9** ppShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");

    return D3D_OK;
  }

  HRESULT D3D9Device::SetVertexShader(IDirect3DVertexShader9* pShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetVertexShaderConstantB(UINT StartRegister,
    const BOOL *pConstantData, UINT BoolCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetVertexShaderConstantF(UINT StartRegister,
    const float* pConstantData, UINT Vector4fCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetVertexShaderConstantI(UINT StartRegister,
    const int *pConstantData, UINT Vector4iCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }


  HRESULT D3D9Device::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetVertexShaderConstantB(UINT StartRegister,
    BOOL* pConstantData, UINT BoolCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetVertexShaderConstantF(UINT StartRegister,
    float* pConstantData, UINT Vector4fCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetVertexShaderConstantI(UINT StartRegister,
    int* pConstantData, UINT Vector4iCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
  HRESULT D3D9Device::CreatePixelShader(const DWORD* pFunction,
    IDirect3DPixelShader9** ppShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }


  HRESULT D3D9Device::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetPixelShaderConstantB(UINT StartRegister,
    BOOL* pConstantData, UINT BoolCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetPixelShaderConstantF(UINT StartRegister,
    float* pConstantData, UINT Vector4fCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetPixelShaderConstantI(UINT StartRegister,
    int* pConstantData, UINT Vector4iCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetPixelShader(IDirect3DPixelShader9* pShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetPixelShaderConstantB(UINT StartRegister,
    const BOOL* pConstantData, UINT BoolCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetPixelShaderConstantF(UINT StartRegister,
    const float* pConstantData, UINT Vector4fCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetPixelShaderConstantI(UINT StartRegister,
    const int* pConstantData, UINT Vector4iCount) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
