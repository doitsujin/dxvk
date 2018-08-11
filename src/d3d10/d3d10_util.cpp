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


  void GetD3D10ResourceFromView(
          ID3D11View*           pSrcView,
          ID3D10Resource**      ppDstResource) {
    Com<ID3D11Resource> d3d11Resource;
    pSrcView->GetResource(&d3d11Resource);
    GetD3D10Resource(d3d11Resource.ptr(), ppDstResource);
  }


  void GetD3D11ResourceFromView(
          ID3D10View*           pSrcView,
          ID3D11Resource**      ppDstResource) {
    Com<ID3D10Resource> d3d10Resource;
    pSrcView->GetResource(&d3d10Resource);
    GetD3D11Resource(d3d10Resource.ptr(), ppDstResource);
  }


  void GetD3D10Resource(
          ID3D11Resource*       pSrcResource,
          ID3D10Resource**      ppDstResource) {
    pSrcResource->QueryInterface(
      __uuidof(ID3D10Resource),
      reinterpret_cast<void**>(ppDstResource));
  }


  void GetD3D11Resource(
          ID3D10Resource*       pSrcResource,
          ID3D11Resource**      ppDstResource) {
    pSrcResource->QueryInterface(
      __uuidof(ID3D11Resource),
      reinterpret_cast<void**>(ppDstResource));
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