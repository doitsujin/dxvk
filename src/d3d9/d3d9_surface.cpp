#include "d3d9_surface.h"

namespace dxvk {

  D3D9Surface::D3D9Surface(
          D3D9DeviceEx*             pDevice,
    const D3D9TextureDesc*          pDesc)
    : D3D9SurfaceBase(
        pDevice,
        new D3D9CommonTexture( pDevice, pDesc ),
        0,
        0,
        pDevice,
        true) { }

  D3D9Surface::D3D9Surface(
          D3D9DeviceEx*             pDevice,
          D3D9CommonTexture*        pTexture,
          UINT                      Face,
          UINT                      MipLevel,
          IUnknown*                 pContainer)
    : D3D9SurfaceBase(
        pDevice,
        pTexture,
        Face,
        MipLevel,
        pContainer,
        false) { }

  HRESULT STDMETHODCALLTYPE D3D9Surface::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DSurface9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Surface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9Surface::GetType() {
    return D3DRTYPE_SURFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::GetDesc(D3DSURFACE_DESC *pDesc) {
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

  HRESULT STDMETHODCALLTYPE D3D9Surface::LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
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

  HRESULT STDMETHODCALLTYPE D3D9Surface::UnlockRect() {
    return m_texture->Unlock(
      m_face,
      m_mipLevel);
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::GetDC(HDC *phdc) {
    Logger::warn("D3D9Surface::GetDC: Stub");
    return D3DERR_INVALIDCALL;
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::ReleaseDC(HDC hdc) {
    Logger::warn("D3D9Surface::ReleaseDC: Stub");
    return D3DERR_INVALIDCALL;
  }

}