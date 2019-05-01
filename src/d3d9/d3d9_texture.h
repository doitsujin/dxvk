#pragma once

#include "d3d9_surface.h"
#include "d3d9_volume.h"
#include "d3d9_util.h"

#include <vector>
#include <list>
#include <mutex>

namespace dxvk {

  extern std::mutex g_managedTextureMutex;
  extern std::list<IDirect3DBaseTexture9*> g_managedTextures;

  template <typename SubresourceType, typename... Base>
  class D3D9BaseTexture : public D3D9Resource<Base...> {

  public:

    D3D9BaseTexture(
            D3D9DeviceEx*           pDevice,
      const D3D9TextureDesc*        pDesc)
      : D3D9Resource<Base...> ( pDevice )
      , m_texture             ( pDevice, pDesc )
      , m_lod                 ( 0 )
      , m_autogenFilter       ( D3DTEXF_LINEAR ) {
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

      if (pDesc->Pool == D3DPOOL_MANAGED) {
        auto lock = std::lock_guard(g_managedTextureMutex);
        g_managedTextures.push_back(this);
      }
    }

    ~D3D9BaseTexture() {
      if (m_texture.Desc()->Pool == D3DPOOL_MANAGED) {
        auto lock = std::lock_guard(g_managedTextureMutex);

        auto iter = std::find(g_managedTextures.begin(), g_managedTextures.end(), this);
        if (iter != g_managedTextures.end())
          g_managedTextures.erase(iter);
      }

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
      m_texture.GenerateMipSubLevels();
    }

    D3D9CommonTexture* GetCommonTexture() {
      return &m_texture;
    }

    SubresourceType* GetSubresource(UINT Subresource) {
      if (Subresource >= m_subresources.size())
        return nullptr;

      return m_subresources[Subresource];
    }

  protected:

    D3D9CommonTexture m_texture;

    DWORD m_lod;
    D3DTEXTUREFILTERTYPE m_autogenFilter;

    std::vector<SubresourceType*> m_subresources;

  };

  using D3D9Texture2DBase = D3D9BaseTexture<D3D9Surface, IDirect3DTexture9>;
  class D3D9Texture2D final : public D3D9Texture2DBase {

  public:

    D3D9Texture2D(
          D3D9DeviceEx*           pDevice,
    const D3D9TextureDesc*        pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect);

  };

  using D3D9Texture3DBase = D3D9BaseTexture<D3D9Volume, IDirect3DVolumeTexture9>;
  class D3D9Texture3D final : public D3D9Texture3DBase {

  public:

    D3D9Texture3D(
          D3D9DeviceEx*           pDevice,
    const D3D9TextureDesc*        pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX* pDirtyBox);

  };

  using D3D9TextureCubeBase = D3D9BaseTexture<D3D9Surface, IDirect3DCubeTexture9>;
  class D3D9TextureCube final : public D3D9TextureCubeBase {

  public:

    D3D9TextureCube(
          D3D9DeviceEx*           pDevice,
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
  D3D9CommonTexture* GetCommonTexture(T* ptr) {
    if (ptr == nullptr)
      return nullptr;

    switch (ptr->GetType()) {
      case D3DRTYPE_TEXTURE:       return static_cast<D3D9Texture2D*>  (ptr)->GetCommonTexture();
      case D3DRTYPE_CUBETEXTURE:   return static_cast<D3D9TextureCube*>(ptr)->GetCommonTexture();
      case D3DRTYPE_VOLUMETEXTURE: return static_cast<D3D9Texture3D*>  (ptr)->GetCommonTexture();
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
      case D3DRTYPE_TEXTURE:       CastRefPrivate<D3D9Texture2D>  (tex, AddRef); break;
      case D3DRTYPE_CUBETEXTURE:   CastRefPrivate<D3D9TextureCube>(tex, AddRef); break;
      case D3DRTYPE_VOLUMETEXTURE: CastRefPrivate<D3D9Texture3D>  (tex, AddRef); break;
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