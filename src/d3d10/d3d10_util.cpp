#include "d3d10_util.h"
#include "d3d10_device.h"

#include "../d3d11/d3d11_device.h"

namespace dxvk {

  UINT ConvertD3D10ResourceFlags(UINT MiscFlags) {
    UINT result = 0;
    if (MiscFlags & D3D10_RESOURCE_MISC_GENERATE_MIPS)
      result |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (MiscFlags & D3D10_RESOURCE_MISC_SHARED)
      result |= D3D11_RESOURCE_MISC_SHARED;
    if (MiscFlags & D3D10_RESOURCE_MISC_TEXTURECUBE)
      result |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (MiscFlags & D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX)
      result |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    if (MiscFlags & D3D10_RESOURCE_MISC_GDI_COMPATIBLE)
      result |= D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    return result;
  }


  UINT ConvertD3D11ResourceFlags(UINT MiscFlags) {
    UINT result = 0;
    if (MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      result |= D3D10_RESOURCE_MISC_GENERATE_MIPS;
    if (MiscFlags & D3D11_RESOURCE_MISC_SHARED)
      result |= D3D10_RESOURCE_MISC_SHARED;
    if (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      result |= D3D10_RESOURCE_MISC_TEXTURECUBE;
    if (MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
      result |= D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    if (MiscFlags & D3D11_RESOURCE_MISC_GDI_COMPATIBLE)
      result |= D3D10_RESOURCE_MISC_GDI_COMPATIBLE;
    return result;
  }


  void GetD3D10Device(
          ID3D11DeviceChild*    pObject,
          ID3D10Device**        ppDevice) {
    ID3D11Device* d3d11Device = nullptr;
    pObject->GetDevice(&d3d11Device);
    *ppDevice = static_cast<D3D11Device*>(d3d11Device)->GetD3D10Interface();
  }


  void GetD3D11Device(
          ID3D11DeviceChild*    pObject,
          ID3D11Device**        ppDevice) {
    pObject->GetDevice(ppDevice);
  }

  
  void GetD3D11Context(
          ID3D11DeviceChild*    pObject,
          ID3D11DeviceContext** ppContext) {
    Com<ID3D11Device> d3d11Device;
    pObject->GetDevice(&d3d11Device);
    d3d11Device->GetImmediateContext(ppContext);
  }

}