#include "d3d9_device_shader.h"

namespace dxvk {
  HRESULT D3D9DeviceShader::CreateVertexShader(const DWORD* pFunction,
    IDirect3DVertexShader9** ppShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceShader::SetVertexShader(IDirect3DVertexShader9* pShader) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
