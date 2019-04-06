#include "d3d9_surface.h"

namespace dxvk {

  Direct3DSurface9::Direct3DSurface9(
          Direct3DDevice9Ex*        device,
    const D3D9TextureDesc*          desc)
    : Direct3DSurface9Base(
        device,
        new Direct3DCommonTexture9( device, desc ),
        0,
        0,
        device,
        true) { }

  Direct3DSurface9::Direct3DSurface9(
          Direct3DDevice9Ex*         pDevice,
          Direct3DCommonTexture9*    pTexture,
          UINT                       Face,
          UINT                       MipLevel,
          IUnknown*                  pContainer)
    : Direct3DSurface9Base(
        pDevice,
        pTexture,
        Face,
        MipLevel,
        pContainer,
        false) { }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DSurface9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DSurface9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DSurface9::GetType() {
    return D3DRTYPE_SURFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    auto& desc = *(m_texture->Desc());

    pDesc->Format             = static_cast<D3DFORMAT>(desc.Format);
    pDesc->Type               = D3DRTYPE_SURFACE;
    pDesc->Usage              = desc.Usage;
    pDesc->Pool               = desc.Pool;
    
    pDesc->MultiSampleType    = desc.MultiSample;
    pDesc->MultiSampleQuality = desc.MultisampleQuality;
    pDesc->Width              = std::max(1u, desc.Width >> m_mipLevel);
    pDesc->Height             = std::max(1u, desc.Height >> m_mipLevel);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    D3DBOX box;
    if (pRect != nullptr) {
      box.Left = pRect->left;
      box.Right = pRect->right;
      box.Top = pRect->top;
      box.Bottom = pRect->bottom;
      box.Front = 0;
      box.Back = 1;
    }

    D3DLOCKED_BOX lockedBox;

    HRESULT hr = m_texture->Lock(
      m_face,
      m_mipLevel,
      &lockedBox,
      pRect != nullptr ? &box : nullptr,
      Flags);

    pLockedRect->pBits = lockedBox.pBits;
    pLockedRect->Pitch = lockedBox.RowPitch;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::UnlockRect() {
    return m_texture->Unlock(
      m_face,
      m_mipLevel);
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDC(HDC *phdc) {
    Logger::warn("Direct3DSurface9::GetDC: Stub");
    return D3DERR_INVALIDCALL;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::ReleaseDC(HDC hdc) {
    Logger::warn("Direct3DSurface9::ReleaseDC: Stub");
    return D3DERR_INVALIDCALL;
  }

}