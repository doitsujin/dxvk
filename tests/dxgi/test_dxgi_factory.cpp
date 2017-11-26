#include <vector>

#include <dxgi_include.h>

#include <windows.h>
#include <windowsx.h>

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  Com<IDXGIFactory> factory;
  
  if (CreateDXGIFactory(__uuidof(IDXGIFactory),
      reinterpret_cast<void**>(&factory)) != S_OK) {
    Logger::err("Failed to create DXGI factory");
    return 1;
  }
  
  Com<IDXGIAdapter> adapter;
  
  for (UINT i = 0; factory->EnumAdapters(i, &adapter) == S_OK; i++) {
    DXGI_ADAPTER_DESC adapterDesc;
    
    if (adapter->GetDesc(&adapterDesc) != S_OK) {
      Logger::err("Failed to get DXGI adapter info");
      return 1;
    }
    
    Com<IDXGIOutput> output;
    
    for (UINT j = 0; adapter->EnumOutputs(j, &output) == S_OK; j++) {
      std::vector<DXGI_MODE_DESC> modes;
      
      HRESULT status = S_OK;
      UINT    displayModeCount = 0;
      
      Logger::info(str::format("Adapter ", i, ":"));
      
      DXGI_ADAPTER_DESC desc;
      
      if (adapter->GetDesc(&desc) != S_OK) {
        Logger::err("Failed to get DXGI adapter info");
        return 1;
      }
      
      std::array<char, 257> chars;
      std::wcstombs(chars.data(), desc.Description, chars.size() - 1);
      Logger::info(str::format(" ", chars.data()));
      Logger::info(str::format(" Vendor: ", desc.VendorId));
      Logger::info(str::format(" Device: ", desc.DeviceId));
      Logger::info(str::format(" Dedicated RAM: ", desc.DedicatedVideoMemory));
      Logger::info(str::format(" Shared RAM: ", desc.SharedSystemMemory));
      
      do {
        if (output->GetDisplayModeList(
          DXGI_FORMAT_R8G8B8A8_UNORM,
          DXGI_ENUM_MODES_SCALING,
          &displayModeCount, nullptr) != S_OK) {
          Logger::err("Failed to get DXGI output display mode count");
          return 1;
        }
        
        modes.resize(displayModeCount);
        
        status = output->GetDisplayModeList(
          DXGI_FORMAT_R8G8B8A8_UNORM,
          DXGI_ENUM_MODES_SCALING,
          &displayModeCount, modes.data());
      } while (status == DXGI_ERROR_MORE_DATA);
      
      if (status != S_OK) {
        Logger::err("Failed to get DXGI output display mode list");
        return 1;
      }
      
      Logger::info(str::format(" Output ", j, ":"));
      for (auto mode : modes) {
        Logger::info(str::format("  ",
          mode.Width, "x", mode.Height, " @ ",
          mode.RefreshRate.Numerator / mode.RefreshRate.Denominator));
      }
    }
  }
  
  return 0;
}

