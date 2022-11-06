#pragma once

#include "d3d8_device.h"

#include "d3d8_resource.h"
#include "d3d8_surface.h"
#include "d3d8_volume.h"

#include "d3d8_d3d9_util.h"

#include <vector>
#include <new>
#include <type_traits>

namespace dxvk {


  // Implements IDirect3DBaseTexture8 (Except GetType)
  template <typename SubresourceType, typename D3D9, typename D3D8>
  class D3D8BaseTexture : public D3D8Resource<D3D9, D3D8> {

  public:

    using SubresourceData = std::aligned_storage_t<sizeof(SubresourceType), alignof(SubresourceType)>;

    D3D8BaseTexture(
            D3D8DeviceEx*                       pDevice,
            Com<D3D9>&&                         pBaseTexture,
      //const D3D8_COMMON_TEXTURE_DESC*         pDesc,
            D3DRESOURCETYPE                     ResourceType)
        : D3D8Resource<D3D9, D3D8> ( pDevice, std::move(pBaseTexture) ) {
      // TODO: set up subresource
    }

    ~D3D8BaseTexture() {
    }

    // TODO: all these methods should probably be final

    void STDMETHODCALLTYPE PreLoad() {
      this->GetD3D9()->PreLoad();
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) {
      return this->GetD3D9()->SetLOD(LODNew);
    }

    DWORD STDMETHODCALLTYPE GetLOD() {
      return this->GetD3D9()->GetLOD();
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() {
      return this->GetD3D9()->GetLevelCount();
    }

    SubresourceType* GetSubresource(UINT Subresource) {
      return reinterpret_cast<SubresourceType*>(&m_subresources[Subresource]);
    }

  protected:

    std::vector<SubresourceData> m_subresources;

  };

  // Implements IDirect3DTexture8 //

  using D3D8Texture2DBase = D3D8BaseTexture<D3D8Surface, d3d9::IDirect3DTexture9, IDirect3DTexture8>;
  class D3D8Texture2D final : public D3D8Texture2DBase {

  public:

    D3D8Texture2D(
            D3D8DeviceEx*                  pDevice,
            Com<d3d9::IDirect3DTexture9>&& pTexture)
      : D3D8Texture2DBase(pDevice, std::move(pTexture), D3DRTYPE_TEXTURE) {
    }

    // TODO: QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_TEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC surf;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);
      ConvertSurfaceDesc8(&surf, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
      // TODO: cache this
      Com<d3d9::IDirect3DSurface9> surface = nullptr;
      HRESULT res = GetD3D9()->GetSurfaceLevel(Level, &surface);
      *ppSurfaceLevel = ref(new D3D8Surface(m_parent, std::move(surface)));
      return res;
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

  // Implements IDirect3DVolumeTexture8 //

  using D3D8Texture3DBase = D3D8BaseTexture<D3D8Volume, d3d9::IDirect3DVolumeTexture9, IDirect3DVolumeTexture8>;
  class D3D8Texture3D final : public D3D8Texture3DBase {

  public:

    D3D8Texture3D(
          D3D8DeviceEx*                         pDevice,
          Com<d3d9::IDirect3DVolumeTexture9>&&  pVolumeTexture)
      : D3D8Texture3DBase(pDevice, std::move(pVolumeTexture), D3DRTYPE_VOLUMETEXTURE) {}

    // TODO: IDirect3DVolumeTexture8 QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_VOLUMETEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) {
      d3d9::D3DVOLUME_DESC vol;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &vol);
      ConvertVolumeDesc8(&vol, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume8** ppVolumeLevel) {
      // TODO: cache this
      Com<d3d9::IDirect3DVolume9> volume = nullptr;
      HRESULT res = GetD3D9()->GetVolumeLevel(Level, &volume);
      *ppVolumeLevel = ref(new D3D8Volume(m_parent, std::move(volume)));
      return res;
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


  // Implements IDirect3DCubeTexture8 //

  using D3D8TextureCubeBase = D3D8BaseTexture<D3D8Surface, d3d9::IDirect3DCubeTexture9, IDirect3DCubeTexture8>;
  class D3D8TextureCube final : public D3D8TextureCubeBase {

  public:

    D3D8TextureCube(
            D3D8DeviceEx*                       pDevice,
            Com<d3d9::IDirect3DCubeTexture9>&&  pTexture)
      : D3D8TextureCubeBase(pDevice, std::move(pTexture), D3DRTYPE_CUBETEXTURE) {
    }

    // TODO: IDirect3DCubeTexture8 QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_CUBETEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC surf;
      HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);
      ConvertSurfaceDesc8(&surf, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
      // TODO: cache this
      Com<d3d9::IDirect3DSurface9> surface = nullptr;
      HRESULT res = GetD3D9()->GetCubeMapSurface(d3d9::D3DCUBEMAP_FACES(Face), Level, &surface);
      *ppSurfaceLevel = ref(new D3D8Surface(m_parent, std::move(surface)));
      return res;
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