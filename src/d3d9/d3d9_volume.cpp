#include "d3d9_volume.h"

namespace dxvk {

  Direct3DVolume9::Direct3DVolume9(
          Direct3DDevice9Ex*        device,
    const D3D9TextureDesc*          desc)
    : Direct3DVolume9Base(
        device,
        new Direct3DCommonTexture9( device, desc ),
        0,
        0,
        device,
        true) { }

  Direct3DVolume9::Direct3DVolume9(
          Direct3DDevice9Ex*         pDevice,
          Direct3DCommonTexture9*    pTexture,
          UINT                       Face,
          UINT                       MipLevel,
          IUnknown*                  pContainer)
    : Direct3DVolume9Base(
        pDevice,
        pTexture,
        Face,
        MipLevel,
        pContainer,
        false) { }

  HRESULT STDMETHODCALLTYPE Direct3DVolume9::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DVolume9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DVolume9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DVolume9::GetDesc(D3DVOLUME_DESC *pDesc) {
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

  HRESULT STDMETHODCALLTYPE Direct3DVolume9::LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
    return m_texture->Lock(
      m_face,
      m_mipLevel,
      pLockedBox,
      pBox,
      Flags);
  }

  HRESULT STDMETHODCALLTYPE Direct3DVolume9::UnlockBox() {
    return m_texture->Unlock(
      m_face,
      m_mipLevel);
  }

}