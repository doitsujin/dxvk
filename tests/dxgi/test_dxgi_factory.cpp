#include <vector>

#include <dxgi.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  Com<IDXGIFactory> factory;
  
  if (CreateDXGIFactory(__uuidof(IDXGIFactory),
      reinterpret_cast<void**>(&factory)) != S_OK) {
    std::cerr << "Failed to create DXGI factory" << std::endl;
    return 1;
  }
  
  Com<IDXGIAdapter> adapter;
  
  for (UINT i = 0; factory->EnumAdapters(i, &adapter) == S_OK; i++) {
    DXGI_ADAPTER_DESC adapterDesc;
    
    if (adapter->GetDesc(&adapterDesc) != S_OK) {
      std::cerr << "Failed to get DXGI adapter info" << std::endl;
      return 1;
    }
    
    Com<IDXGIOutput> output;
    
    for (UINT j = 0; adapter->EnumOutputs(j, &output) == S_OK; j++) {
      std::vector<DXGI_MODE_DESC> modes;
      
      HRESULT status = S_OK;
      UINT    displayModeCount = 0;
      
      std::cout << str::format("Adapter ", i, ":") << std::endl;
      
      DXGI_ADAPTER_DESC desc;
      
      if (adapter->GetDesc(&desc) != S_OK) {
        std::cerr << "Failed to get DXGI adapter info" << std::endl;
        return 1;
      }
      
      std::array<char, 257> chars;
      std::wcstombs(chars.data(), desc.Description, chars.size() - 1);
      
      std::cout << str::format(" ", chars.data()) << std::endl;
      std::cout << str::format(" Vendor: ", desc.VendorId) << std::endl;
      std::cout << str::format(" Device: ", desc.DeviceId) << std::endl;
      std::cout << str::format(" Dedicated RAM: ", desc.DedicatedVideoMemory) << std::endl;
      std::cout << str::format(" Shared RAM: ", desc.SharedSystemMemory) << std::endl;
      
      do {
        if (output->GetDisplayModeList(
          DXGI_FORMAT_R8G8B8A8_UNORM,
          DXGI_ENUM_MODES_SCALING,
          &displayModeCount, nullptr) != S_OK) {
          std::cerr << "Failed to get DXGI output display mode count" << std::endl;
          return 1;
        }
        
        modes.resize(displayModeCount);
        
        status = output->GetDisplayModeList(
          DXGI_FORMAT_R8G8B8A8_UNORM,
          DXGI_ENUM_MODES_SCALING,
          &displayModeCount, modes.data());
      } while (status == DXGI_ERROR_MORE_DATA);
      
      if (status != S_OK) {
        std::cerr << "Failed to get DXGI output display mode list" << std::endl;
        return 1;
      }
      
      std::cout << str::format(" Output ", j, ":") << std::endl;
      for (auto mode : modes) {
        std::cout << str::format("  ",
          mode.Width, "x", mode.Height, " @ ",
          mode.RefreshRate.Numerator / mode.RefreshRate.Denominator) << std::endl;
      }
    }
  }
  
  return 0;
}

