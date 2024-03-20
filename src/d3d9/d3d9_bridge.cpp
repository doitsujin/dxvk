
#include "d3d9_device.h"
#include "d3d9_interface.h"
#include "d3d9_bridge.h"
#include "d3d9_swapchain.h"
#include "d3d9_surface.h"

namespace dxvk {

  DxvkD3D8Bridge::DxvkD3D8Bridge(D3D9DeviceEx* pDevice)
    : m_device(pDevice) {
  }

  DxvkD3D8Bridge::~DxvkD3D8Bridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkD3D8Bridge::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkD3D8Bridge::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkD3D8Bridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  void DxvkD3D8Bridge::SetAPIName(const char* name) {
    m_device->m_implicitSwapchain->SetApiName(name);
  }

  void DxvkD3D8Bridge::SetD3D8CompatibilityMode(const bool compatMode) {
    m_device->SetD3D8CompatibilityMode(compatMode);
  }

  HRESULT DxvkD3D8Bridge::UpdateTextureFromBuffer(
        IDirect3DSurface9*  pDestSurface,
        IDirect3DSurface9*  pSrcSurface,
        const RECT*         pSrcRect,
        const POINT*        pDestPoint) {
    auto lock = m_device->LockDevice();

    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestSurface);
    D3D9Surface* src = static_cast<D3D9Surface*>(pSrcSurface);

    if (unlikely(dst == nullptr || src == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* srcTextureInfo = src->GetCommonTexture();
    D3D9CommonTexture* dstTextureInfo = dst->GetCommonTexture();

    VkOffset3D srcOffset = { 0u, 0u, 0u };
    VkOffset3D dstOffset = { 0u, 0u, 0u };
    VkExtent3D texLevelExtent = srcTextureInfo->GetExtentMip(src->GetSubresource());
    VkExtent3D extent = texLevelExtent;

    srcOffset = { pSrcRect->left,
                  pSrcRect->top,
                  0u };

    extent = { uint32_t(pSrcRect->right - pSrcRect->left), uint32_t(pSrcRect->bottom - pSrcRect->top), 1 };

    // TODO: Validate extents like in D3D9DeviceEx::UpdateSurface

    dstOffset = { pDestPoint->x,
                  pDestPoint->y,
                  0u };


    m_device->UpdateTextureFromBuffer(
      srcTextureInfo, dstTextureInfo,
      src->GetSubresource(), dst->GetSubresource(),
      srcOffset, extent, dstOffset
    );

    dstTextureInfo->SetNeedsReadback(dst->GetSubresource(), true);

    if (dstTextureInfo->IsAutomaticMip())
      m_device->MarkTextureMipsDirty(dstTextureInfo);
    
    return D3D_OK;
  }

  DxvkD3D8InterfaceBridge::DxvkD3D8InterfaceBridge(D3D9InterfaceEx* pObject)
    : m_interface(pObject) {
  }

  DxvkD3D8InterfaceBridge::~DxvkD3D8InterfaceBridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkD3D8InterfaceBridge::AddRef() {
    return m_interface->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkD3D8InterfaceBridge::Release() {
    return m_interface->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkD3D8InterfaceBridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  const Config* DxvkD3D8InterfaceBridge::GetConfig() const {
    return &m_interface->GetInstance()->config();
  }
}
