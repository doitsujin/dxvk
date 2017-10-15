#include <d3d11_include.h>

#include <windows.h>
#include <windowsx.h>

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  Com<ID3D11Device>         device;
  Com<ID3D11DeviceContext>  context;
  
  if (FAILED(D3D11CreateDevice(nullptr,
        D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, 0, &device, nullptr, &context))) {
    Logger::err("Failed to create D3D11 device");
    return 1;
  }
  
  return 0;
}
