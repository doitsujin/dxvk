#include "d3d9_volume.h"

namespace dxvk {

  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc)
    : D3D9VolumeBase(
        pDevice,
        new D3D9CommonTexture( pDevice, pDesc, D3DRTYPE_VOLUMETEXTURE ),
        0, 0,
        pDevice,
        true) { }

  D3D9Volume::D3D9Volume(
          D3D9DeviceEx*             pDevice,
          D3D9CommonTexture*        pTexture,
          UINT                      Face,
          UINT                      MipLevel,
          IUnknown*                 pContainer)
    : D3D9VolumeBase(
        pDevice,
        pTexture,
        Face, MipLevel,
        pContainer,
        false) { }

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