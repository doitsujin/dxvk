#pragma once

#include "d3d9_surface.h"
#include "d3d9_volume.h"
#include "d3d9_util.h"

#include <vector>

namespace dxvk {

  template <typename SubresourceType, typename... Base>
  class Direct3DBaseTexture9 : public Direct3DResource9<Base...> {

  public:

    Direct3DBaseTexture9(
            Direct3DDevice9Ex*      pDevice,
      const D3D9TextureDesc*        pDesc)
      : Direct3DResource9<Base...> ( pDevice )
      , m_texture                  ( pDevice, pDesc )
      , m_lod                      ( 0 )
      , m_autogenFilter            ( D3DTEXF_LINEAR ) {
      const uint32_t arraySlices = m_texture.GetLayerCount();
      const uint32_t mipLevels   = m_texture.GetMipCount();

      m_subresources.resize(
        m_texture.GetSubresourceCount());

      for (uint32_t i = 0; i < arraySlices; i++) {
        for (uint32_t j = 0; j < mipLevels; j++) {

          uint32_t subresource = m_texture.CalcSubresource(i, j);

          SubresourceType* subObj = new SubresourceType(
            pDevice,
            &m_texture,
            i,
            j,
            this);
          subObj->AddRefPrivate();

          m_subresources[subresource] = subObj;
        }
      }
    }

    ~Direct3DBaseTexture9() {
      for (auto* subresource : m_subresources)
        subresource->ReleasePrivate();
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) final {
      DWORD oldLod = m_lod;
      m_lod = LODNew;

      m_texture.RecreateImageView(LODNew);

      return oldLod;
    }

    DWORD STDMETHODCALLTYPE GetLOD() final {
      return m_lod;
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() final {
      return m_texture.Desc()->MipLevels;
    }

    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) final {
      m_autogenFilter = FilterType;
      return D3D_OK;
    }

    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() final {
      return m_autogenFilter;
    }

    void STDMETHODCALLTYPE GenerateMipSubLevels() final {
      m_parent->GenerateMips(&m_texture);
    }

    Direct3DCommonTexture9* GetCommonTexture() {
      return &m_texture;
    }

    SubresourceType* GetSubresource(UINT Subresource) {
      if (Subresource >= m_subresources.size())
        return nullptr;

      return m_subresources[Subresource];
    }

  protected:

    Direct3DCommonTexture9 m_texture;

    DWORD m_lod;
    D3DTEXTUREFILTERTYPE m_autogenFilter;

    std::vector<SubresourceType*> m_subresources;

  };

  using Direct3DTexture9Base = Direct3DBaseTexture9<Direct3DSurface9, IDirect3DTexture9>;
  class Direct3DTexture9 final : public Direct3DTexture9Base {

  public:

    Direct3DTexture9(
          Direct3DDevice9Ex*      pDevice,
    const D3D9TextureDesc*        pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect);

  };

  using Direct3DVolumeTexture9Base = Direct3DBaseTexture9<Direct3DVolume9, IDirect3DVolumeTexture9>;
  class Direct3DVolumeTexture9 final : public Direct3DVolumeTexture9Base {

  public:

    Direct3DVolumeTexture9(
          Direct3DDevice9Ex*      pDevice,
    const D3D9TextureDesc*        pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX* pDirtyBox);

  };

  using Direct3DCubeTexture9Base = Direct3DBaseTexture9<Direct3DSurface9, IDirect3DCubeTexture9>;
  class Direct3DCubeTexture9 final : public Direct3DCubeTexture9Base {

  public:

    Direct3DCubeTexture9(
          Direct3DDevice9Ex*      pDevice,
    const D3D9TextureDesc*        pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES Face, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES Face, UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES Face, CONST RECT* pDirtyRect);

  };

  template <typename T>
  Direct3DCommonTexture9* GetCommonTexture(T* ptr) {
    if (ptr == nullptr)
      return nullptr;

    switch (ptr->GetType()) {
      case D3DRTYPE_TEXTURE:       return static_cast<Direct3DTexture9*>      (ptr)->GetCommonTexture();
      case D3DRTYPE_CUBETEXTURE:   return static_cast<Direct3DCubeTexture9*>  (ptr)->GetCommonTexture();
      case D3DRTYPE_VOLUMETEXTURE: return static_cast<Direct3DVolumeTexture9*>(ptr)->GetCommonTexture();
      default:
        Logger::warn("Unknown texture resource type."); break;
    }

    return nullptr;
  }

  template <typename T>
  void TextureRefPrivate(T* tex, bool AddRef) {
    if (tex == nullptr)
      return;

    switch (tex->GetType()) {
      case D3DRTYPE_TEXTURE:       CastRefPrivate<Direct3DTexture9>       (tex, AddRef); break;
      case D3DRTYPE_CUBETEXTURE:   CastRefPrivate<Direct3DCubeTexture9>  (tex, AddRef); break;
      case D3DRTYPE_VOLUMETEXTURE: CastRefPrivate<Direct3DVolumeTexture9>(tex, AddRef); break;
    default:
      Logger::warn("Unknown texture resource type."); break;
    }
  }

  template <typename T>
  void TextureChangePrivate(T*& dst, T* src) {
    TextureRefPrivate(dst, false);
    TextureRefPrivate(src, true);
    dst = src;
  }

}