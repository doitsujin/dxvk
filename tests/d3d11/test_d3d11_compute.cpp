#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  Com<ID3D11Device>         device;
  Com<ID3D11DeviceContext>  context;
  
  if (FAILED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context))) {
    std::cerr << "Failed to create D3D11 device" << std::endl;
    return 1;
  }
  
  
  
  return 0;
}
