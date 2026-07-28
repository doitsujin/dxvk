
#include "d3d9_device.h"
#include "d3d9_interface.h"
#include "d3d9_bridge.h"
#include "d3d9_swapchain.h"
#include "d3d9_surface.h"
#include "d3d9_format.h"

namespace dxvk {

  DxvkLegacyD3DDeviceBridge::DxvkLegacyD3DDeviceBridge(D3D9DeviceEx* pDevice)
    : m_device(pDevice) {
  }

  DxvkLegacyD3DDeviceBridge::~DxvkLegacyD3DDeviceBridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  HRESULT DxvkLegacyD3DDeviceBridge::UpdateTextureFromBuffer(
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

  bool DxvkLegacyD3DDeviceBridge::IsSupportedSurfaceFormat(D3DFORMAT Format) {
    auto mapping = m_device->LookupFormat(EnumerateFormat(Format));
    return mapping.IsValid();
  }

  DxvkLegacyD3DInterfaceBridge::DxvkLegacyD3DInterfaceBridge(D3D9InterfaceEx* pObject)
    : m_interface(pObject) {
  }

  DxvkLegacyD3DInterfaceBridge::~DxvkLegacyD3DInterfaceBridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::AddRef() {
    return m_interface->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::Release() {
    return m_interface->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void DxvkLegacyD3DInterfaceBridge::SetD3DCompatibility(D3DCompatibility d3dCompatibility) const {
    m_interface->SetD3DCompatibility(d3dCompatibility);
  }

  const Config* DxvkLegacyD3DInterfaceBridge::GetConfig() const {
    return &m_interface->GetInstance()->config();
  }

}
