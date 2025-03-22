
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

    // CopyRects will not pass a null pSrcRect, but check anyway
    if (unlikely(pSrcRect == nullptr))
      return D3DERR_INVALIDCALL;

    // validate dimensions to ensure we calculate a meaningful srcOffset & extent
    if (unlikely(pSrcRect->left < 0
              || pSrcRect->top  < 0
              || pSrcRect->right  <= pSrcRect->left
              || pSrcRect->bottom <= pSrcRect->top))
      return D3DERR_INVALIDCALL;

    // CopyRects will not pass a null pDestPoint, but check anyway
    if (unlikely(pDestPoint == nullptr))
      return D3DERR_INVALIDCALL;

    // validate dimensions to ensure we caculate a meaningful dstOffset
    if (unlikely(pDestPoint->x < 0
              || pDestPoint->y < 0))
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* srcTextureInfo = src->GetCommonTexture();
    D3D9CommonTexture* dstTextureInfo = dst->GetCommonTexture();

    VkOffset3D srcOffset = { pSrcRect->left,
                             pSrcRect->top,
                             0u };

    VkExtent3D extent = { uint32_t(pSrcRect->right - pSrcRect->left), uint32_t(pSrcRect->bottom - pSrcRect->top), 1 };

    VkOffset3D dstOffset = { pDestPoint->x,
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

  void DxvkD3D8InterfaceBridge::EnableD3D8CompatibilityMode() {
    m_interface->EnableD3D8CompatibilityMode();
  }

  const Config* DxvkD3D8InterfaceBridge::GetConfig() const {
    return &m_interface->GetInstance()->config();
  }

}
