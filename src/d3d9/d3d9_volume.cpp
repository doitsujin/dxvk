#include "d3d9_volume.h"

#include "d3d9_device.h"
#include "d3d9_texture.h"

namespace dxvk {

  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc)
    : D3D9VolumeBase(
        pDevice,
        new D3D9CommonTexture( pDevice, pDesc, D3DRTYPE_VOLUMETEXTURE ),
        0, 0,
        nullptr,
        nullptr) { }


  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
          D3D9CommonTexture*        pTexture,
          UINT                      Face,
          UINT                      MipLevel,
          IDirect3DBaseTexture9*    pContainer)
    : D3D9VolumeBase(
        pDevice,
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

    Logger::warn("D3D9Volume::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
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

    return m_parent->LockImage(
      m_texture,
      m_face, m_mipLevel,
      pLockedBox,
      pBox,
      Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9Volume::UnlockBox() {
    return m_parent->UnlockImage(
      m_texture,
      m_face, m_mipLevel);
  }

}