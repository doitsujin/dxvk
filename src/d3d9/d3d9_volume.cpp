#include "d3d9_volume.h"

#include "d3d9_device.h"
#include "d3d9_texture.h"

namespace dxvk {

  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc,
    const bool                      Extended)
    : D3D9VolumeBase(
        pDevice,
        Extended,
        new D3D9CommonTexture( pDevice, this, pDesc, D3DRTYPE_VOLUMETEXTURE, nullptr ),
        0, 0,
        nullptr,
        nullptr) { }


  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
    const bool                      Extended,
          D3D9CommonTexture*        pTexture,
          UINT                      Face,
          UINT                      MipLevel,
          IDirect3DBaseTexture9*    pContainer)
    : D3D9VolumeBase(
        pDevice,
        Extended,
        pTexture,
        Face, MipLevel,
        pContainer,
        pContainer) { }


  void D3D9Volume::AddRefPrivate() {
    // Can't have a swapchain container for a volume.
    if (m_baseTexture != nullptr) {
      static_cast<D3D9Texture3D*>(m_baseTexture)->AddRefPrivate();
      return;
    }

    D3D9VolumeBase::AddRefPrivate();
  }


  void D3D9Volume::ReleasePrivate() {
    // Can't have a swapchain container for a volume.
    if (m_baseTexture != nullptr) {
      static_cast<D3D9Texture3D*>(m_baseTexture)->ReleasePrivate();
      return;
    }

    D3D9VolumeBase::ReleasePrivate();
  }


  HRESULT STDMETHODCALLTYPE D3D9Volume::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DVolume9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D9VkInteropTexture)) {
      *ppvObject = ref(m_texture->GetVkInterop());
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDirect3DVolume9), riid)) {
      Logger::warn("D3D9Volume::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9Volume::GetDesc(D3DVOLUME_DESC *pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    auto& desc = *(m_texture->Desc());

    pDesc->Format = static_cast<D3DFORMAT>(desc.Format);
    pDesc->Type   = D3DRTYPE_VOLUME;
    pDesc->Usage  = desc.Usage;
    pDesc->Pool   = desc.Pool;

    pDesc->Width  = std::max(1u, desc.Width  >> m_mipLevel);
    pDesc->Height = std::max(1u, desc.Height >> m_mipLevel);
    pDesc->Depth  = std::max(1u, desc.Depth  >> m_mipLevel);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Volume::LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
    if (unlikely(pLockedBox == nullptr))
      return D3DERR_INVALIDCALL;

    // LockBox clears any existing content present in pLockedBox
    pLockedBox->pBits = nullptr;
    pLockedBox->RowPitch = 0;
    pLockedBox->SlicePitch = 0;

    if (unlikely(pBox != nullptr)) {
      auto& desc = *(m_texture->Desc());

      // Negative or zero length dimensions
      if ( static_cast<LONG>(pBox->Right)  - static_cast<LONG>(pBox->Left)  <= 0
        || static_cast<LONG>(pBox->Bottom) - static_cast<LONG>(pBox->Top)   <= 0
        || static_cast<LONG>(pBox->Back)   - static_cast<LONG>(pBox->Front) <= 0
      // Exceeding surface dimensions
        || pBox->Right  > std::max(1u, desc.Width  >> m_mipLevel)
        || pBox->Bottom > std::max(1u, desc.Height >> m_mipLevel)
        || pBox->Back   > std::max(1u, desc.Depth  >> m_mipLevel))
        return D3DERR_INVALIDCALL;
    }

    D3DLOCKED_BOX lockedBox;

    HRESULT hr = m_parent->LockImage(
      m_texture,
      m_face, m_mipLevel,
      &lockedBox,
      pBox,
      Flags);

    if (FAILED(hr)) return hr;

    pLockedBox->pBits      = lockedBox.pBits;
    pLockedBox->RowPitch   = lockedBox.RowPitch;
    pLockedBox->SlicePitch = lockedBox.SlicePitch;

    return hr;
  }


  HRESULT STDMETHODCALLTYPE D3D9Volume::UnlockBox() {
    return m_parent->UnlockImage(
      m_texture,
      m_face, m_mipLevel);
  }

}
