#include "d3d9_surface.h"
#include "d3d9_texture.h"
#include "d3d9_swapchain.h"

#include "d3d9_device.h"

#include "../util/util_win32_compat.h"

namespace dxvk {

  D3D9Surface::D3D9Surface(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc,
          IUnknown*                 pContainer,
          HANDLE*                   pSharedHandle)
    : D3D9SurfaceBase(
        pDevice,
        new D3D9CommonTexture( pDevice, this, pDesc, D3DRTYPE_SURFACE, pSharedHandle),
        0, 0,
        nullptr,
        pContainer) { }

  D3D9Surface::D3D9Surface(
          D3D9DeviceEx*             pDevice,
          D3D9CommonTexture*        pTexture,
          UINT                      Face,
          UINT                      MipLevel,
          IDirect3DBaseTexture9*    pBaseTexture)
    : D3D9SurfaceBase(
        pDevice,
        pTexture,
        Face, MipLevel,
        pBaseTexture,
        pBaseTexture) { }

  void D3D9Surface::AddRefPrivate() {
    if (m_baseTexture != nullptr) {
      D3DRESOURCETYPE type = m_baseTexture->GetType();
      if (type == D3DRTYPE_TEXTURE)
        static_cast<D3D9Texture2D*>  (m_baseTexture)->AddRefPrivate();
      else //if (type == D3DRTYPE_CUBETEXTURE)
        static_cast<D3D9TextureCube*>(m_baseTexture)->AddRefPrivate();

      return;
    }

    D3D9SurfaceBase::AddRefPrivate();
  }

  void D3D9Surface::ReleasePrivate() {
    if (m_baseTexture != nullptr) {
      D3DRESOURCETYPE type = m_baseTexture->GetType();
      if (type == D3DRTYPE_TEXTURE)
        static_cast<D3D9Texture2D*>  (m_baseTexture)->ReleasePrivate();
      else //if (type == D3DRTYPE_CUBETEXTURE)
        static_cast<D3D9TextureCube*>(m_baseTexture)->ReleasePrivate();

      return;
    }

    D3D9SurfaceBase::ReleasePrivate();
  }

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

    if (logQueryInterfaceError(__uuidof(IDirect3DSurface9), riid)) {
      Logger::warn("D3D9Surface::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
    if (unlikely(pLockedRect == nullptr))
      return D3DERR_INVALIDCALL;

    D3DBOX box;
    if (pRect != nullptr) {
      box.Left   = pRect->left;
      box.Right  = pRect->right;
      box.Top    = pRect->top;
      box.Bottom = pRect->bottom;
      box.Front  = 0;
      box.Back   = 1;
    }

    D3DLOCKED_BOX lockedBox;

    HRESULT hr = m_parent->LockImage(
      m_texture,
      m_face, m_mipLevel,
      &lockedBox,
      pRect != nullptr ? &box : nullptr,
      Flags);

    pLockedRect->pBits = lockedBox.pBits;
    pLockedRect->Pitch = lockedBox.RowPitch;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::UnlockRect() {
    return m_parent->UnlockImage(
      m_texture,
      m_face, m_mipLevel);
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::GetDC(HDC *phDC) {
    if (phDC == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9_COMMON_TEXTURE_DESC& desc = *m_texture->Desc();

    D3DLOCKED_RECT lockedRect;
    HRESULT hr = LockRect(&lockedRect, nullptr, 0);
    if (FAILED(hr))
      return hr;

    D3DKMT_CREATEDCFROMMEMORY createInfo;
    // In...
    createInfo.pMemory     = lockedRect.pBits;
    createInfo.Format      = static_cast<D3DFORMAT>(desc.Format);
    createInfo.Width       = desc.Width;
    createInfo.Height      = desc.Height;
    createInfo.Pitch       = lockedRect.Pitch;
    createInfo.hDeviceDc   = CreateCompatibleDC(NULL);
    createInfo.pColorTable = nullptr;

    // Out...
    createInfo.hBitmap     = nullptr;
    createInfo.hDc         = nullptr;

    if (D3DKMTCreateDCFromMemory(&createInfo))
      Logger::err("D3D9: Failed to create GDI DC");

    DeleteDC(createInfo.hDeviceDc);

    // These should now be set...
    m_dcDesc.hDC     = createInfo.hDc;
    m_dcDesc.hBitmap = createInfo.hBitmap;

    *phDC = m_dcDesc.hDC;
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9Surface::ReleaseDC(HDC hDC) {
    if (m_dcDesc.hDC == nullptr || m_dcDesc.hDC != hDC)
      return D3DERR_INVALIDCALL;

    D3DKMTDestroyDCFromMemory(&m_dcDesc);

    HRESULT hr = UnlockRect();
    if (FAILED(hr))
      return hr;

    return D3D_OK;
  }


  void D3D9Surface::ClearContainer() {
    m_container = nullptr;
  }

}
