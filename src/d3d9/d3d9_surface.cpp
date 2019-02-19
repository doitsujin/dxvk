#include "d3d9_surface.h"

namespace dxvk {

  D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DSurface9::GetType() {
    return D3DRTYPE_SURFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    auto& desc = *(m_texture->Desc());

    pDesc->Format = static_cast<D3DFORMAT>(desc.Format);
    pDesc->Type = D3DRTYPE_SURFACE;
    pDesc->Usage = desc.Usage;
    pDesc->Pool = desc.Pool;
    
    pDesc->MultiSampleType = desc.MultiSample;
    pDesc->MultiSampleQuality = desc.MultisampleQuality;
    pDesc->Width = desc.Width;
    pDesc->Height = desc.Height;

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

    HRESULT hr = m_texture->Lock(m_subresource, &lockedBox, pRect != nullptr ? &box : nullptr, Flags);

    pLockedRect->pBits = lockedBox.pBits;
    pLockedRect->Pitch = lockedBox.RowPitch;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSurface9::UnlockRect() {
    return m_texture->Unlock(m_subresource);
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