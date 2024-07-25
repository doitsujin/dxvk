#pragma once

#include "d3d8_resource.h"
#include "d3d8_surface.h"
#include "d3d8_volume.h"

#include "d3d8_d3d9_util.h"

#include <vector>
#include <new>

namespace dxvk {

  template <typename SubresourceType, typename D3D9, typename D3D8>
  class D3D8BaseTexture : public D3D8Resource<D3D9, D3D8> {

  public:

    constexpr static UINT CUBE_FACES = 6;

    using SubresourceType8 = typename SubresourceType::D3D8;
    using SubresourceType9 = typename SubresourceType::D3D9;

    D3D8BaseTexture(
            D3D8Device*                         pDevice,
            Com<D3D9>&&                         pBaseTexture,
            UINT                                SubresourceCount)
        : D3D8Resource<D3D9, D3D8> ( pDevice, std::move(pBaseTexture) ) {
      m_subresources.resize(SubresourceCount, nullptr);
    }

    ~D3D8BaseTexture() {
      for (size_t i = 0; i < m_subresources.size(); i++)
        if (m_subresources[i] != nullptr)
          m_subresources[i] = nullptr;
    }

    virtual IUnknown* GetInterface(REFIID riid) final override try {
      return D3D8Resource<D3D9, D3D8>::GetInterface(riid);
    } catch (HRESULT err) {
      if (riid == __uuidof(IDirect3DBaseTexture8))
        return this;
      
      throw err;
    }

    void STDMETHODCALLTYPE PreLoad() final {
      this->GetD3D9()->PreLoad();
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) final {
      return this->GetD3D9()->SetLOD(LODNew);
    }

    DWORD STDMETHODCALLTYPE GetLOD() final {
      return this->GetD3D9()->GetLOD();
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() final {
      return this->GetD3D9()->GetLevelCount();
    }

  protected:

    HRESULT STDMETHODCALLTYPE GetSubresource(UINT Index, SubresourceType8** ppSubresource) {
      InitReturnPtr(ppSubresource);

      if (unlikely(Index >= m_subresources.size()))
        return D3DERR_INVALIDCALL;
      
      if (m_subresources[Index] == nullptr) {
        try {
          Com<SubresourceType9> subresource = LookupSubresource(Index);

          // Cache the subresource
          m_subresources[Index] = new SubresourceType(this->m_parent, this, std::move(subresource));
        } catch (HRESULT res) {
          return res;
        }
      }

      *ppSubresource = m_subresources[Index].ref();
      return D3D_OK;
    }

  private:

    Com<SubresourceType9> LookupSubresource(UINT Index) {
      Com<SubresourceType9> ptr = nullptr;
      HRESULT res = D3DERR_INVALIDCALL;
      if constexpr (std::is_same_v<D3D8, IDirect3DTexture8>) {
        res = this->GetD3D9()->GetSurfaceLevel(Index, &ptr); 
      } else if constexpr (std::is_same_v<D3D8, IDirect3DVolume8>) {
        res = this->GetD3D9()->GetVolumeLevel(Index, &ptr);
      } else if constexpr (std::is_same_v<D3D8, IDirect3DCubeTexture8>) {
        res = this->GetD3D9()->GetCubeMapSurface(d3d9::D3DCUBEMAP_FACES(Index % CUBE_FACES), Index / CUBE_FACES, &ptr);
      }
      if (FAILED(res))
        throw res;
      return ptr;
    }

    std::vector<Com<SubresourceType, false>> m_subresources;

  };

  using D3D8Texture2DBase = D3D8BaseTexture<D3D8Surface, d3d9::IDirect3DTexture9, IDirect3DTexture8>;
  class D3D8Texture2D final : public D3D8Texture2DBase {

  public:

    D3D8Texture2D(
            D3D8Device*                    pDevice,
            Com<d3d9::IDirect3DTexture9>&& pTexture)
      : D3D8Texture2DBase(pDevice, std::move(pTexture), pTexture->GetLevelCount()) {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_TEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC surf;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);
      ConvertSurfaceDesc8(&surf, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
      return GetSubresource(Level, ppSurfaceLevel);
    }

    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
      return GetD3D9()->LockRect(Level, reinterpret_cast<d3d9::D3DLOCKED_RECT*>(pLockedRect), pRect, Flags);
    }

    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) {
      return GetD3D9()->UnlockRect(Level);
    }

    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect) {
      return GetD3D9()->AddDirtyRect(pDirtyRect);
    }

  };

  using D3D8Texture3DBase = D3D8BaseTexture<D3D8Volume, d3d9::IDirect3DVolumeTexture9, IDirect3DVolumeTexture8>;
  class D3D8Texture3D final : public D3D8Texture3DBase {

  public:

    D3D8Texture3D(
          D3D8Device*                           pDevice,
          Com<d3d9::IDirect3DVolumeTexture9>&&  pVolumeTexture)
      : D3D8Texture3DBase(pDevice, std::move(pVolumeTexture), pVolumeTexture->GetLevelCount()) {}

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_VOLUMETEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) {
      d3d9::D3DVOLUME_DESC vol;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &vol);
      ConvertVolumeDesc8(&vol, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume8** ppVolumeLevel) {
      return GetSubresource(Level, ppVolumeLevel);
    }

    HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
      return GetD3D9()->LockBox(
        Level,
        reinterpret_cast<d3d9::D3DLOCKED_BOX*>(pLockedBox),
        reinterpret_cast<const d3d9::D3DBOX*>(pBox),
        Flags
      );
    }

    HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level) {
      return GetD3D9()->UnlockBox(Level);
    }

    HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX* pDirtyBox) {
      return GetD3D9()->AddDirtyBox(reinterpret_cast<const d3d9::D3DBOX*>(pDirtyBox));
    }

  };

  using D3D8TextureCubeBase = D3D8BaseTexture<D3D8Surface, d3d9::IDirect3DCubeTexture9, IDirect3DCubeTexture8>;
  class D3D8TextureCube final : public D3D8TextureCubeBase {

  public:

    D3D8TextureCube(
            D3D8Device*                         pDevice,
            Com<d3d9::IDirect3DCubeTexture9>&&  pTexture)
      : D3D8TextureCubeBase(pDevice, std::move(pTexture), pTexture->GetLevelCount() * CUBE_FACES) {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_CUBETEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC surf;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);
      ConvertSurfaceDesc8(&surf, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
      return GetSubresource((Level * CUBE_FACES) + Face, ppSurfaceLevel);
    }

    HRESULT STDMETHODCALLTYPE LockRect(
            D3DCUBEMAP_FACES Face,
            UINT Level,
            D3DLOCKED_RECT* pLockedRect,
            const RECT* pRect,
            DWORD Flags) {
      return GetD3D9()->LockRect(
        d3d9::D3DCUBEMAP_FACES(Face),
        Level,
        reinterpret_cast<d3d9::D3DLOCKED_RECT*>(pLockedRect),
        pRect,
        Flags);
    }

    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES Face, UINT Level) {
      return GetD3D9()->UnlockRect(d3d9::D3DCUBEMAP_FACES(Face), Level);
    }

    HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES Face, const RECT* pDirtyRect) {
      return GetD3D9()->AddDirtyRect(d3d9::D3DCUBEMAP_FACES(Face), pDirtyRect);
    }
  };
}