#pragma once

#include "d3d9_device.h"
#include "d3d9_surface.h"
#include "d3d9_volume.h"
#include "d3d9_util.h"

#include <vector>
#include <list>
#include <mutex>
#include <new>
#include <type_traits>

namespace dxvk {

  template <typename SubresourceType, typename... Base>
  class D3D9BaseTexture : public D3D9Resource<Base...> {

  public:

    using SubresourceData = std::aligned_storage_t<sizeof(SubresourceType), alignof(SubresourceType)>;

    D3D9BaseTexture(
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc,
            D3DRESOURCETYPE           ResourceType)
      : D3D9Resource<Base...> ( pDevice )
      , m_texture             ( pDevice, pDesc, ResourceType )
      , m_lod                 ( 0 )
      , m_autogenFilter       ( D3DTEXF_LINEAR ) {
      const uint32_t arraySlices = m_texture.Desc()->ArraySize;
      const uint32_t mipLevels   = m_texture.Desc()->MipLevels;

      m_subresources.resize(arraySlices * mipLevels);

      for (uint32_t i = 0; i < arraySlices; i++) {
        for (uint32_t j = 0; j < mipLevels; j++) {
          const uint32_t subresource = m_texture.CalcSubresource(i, j);

          SubresourceType* subObj = this->GetSubresource(subresource);

          new (subObj) SubresourceType(
            pDevice,
            &m_texture,
            i, j,
            this);
        }
      }
    }

    ~D3D9BaseTexture() {
      for (uint32_t i = 0; i < m_subresources.size(); i++) {
        SubresourceType* subObj = this->GetSubresource(i);
        subObj->~SubresourceType();
      }
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) final {
      DWORD oldLod = m_lod;
      m_lod = LODNew;

      m_texture.CreateSampleView(LODNew);
      if (this->GetPrivateRefCount() > 0)
        this->m_parent->MarkSamplersDirty();

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
      if (m_texture.IsAutomaticMip())
        this->m_parent->GenerateMips(&m_texture);
    }

    D3D9CommonTexture* GetCommonTexture() {
      return &m_texture;
    }

    SubresourceType* GetSubresource(UINT Subresource) {
      if (unlikely(Subresource >= m_subresources.size()))
        return nullptr;

      return reinterpret_cast<SubresourceType*>(&m_subresources[Subresource]);
    }

  protected:

    D3D9CommonTexture m_texture;

    DWORD m_lod;
    D3DTEXTUREFILTERTYPE m_autogenFilter;

    std::vector<SubresourceData> m_subresources;

  };

  using D3D9Texture2DBase = D3D9BaseTexture<D3D9Surface, IDirect3DTexture9>;
  class D3D9Texture2D final : public D3D9Texture2DBase {

  public:

    D3D9Texture2D(
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc);

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
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc);

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
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface9** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES Face, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES Face, UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES Face, CONST RECT* pDirtyRect);

  };

  inline D3D9CommonTexture* GetCommonTexture(IDirect3DBaseTexture9* ptr) {
    if (ptr == nullptr)
      return nullptr;

    D3DRESOURCETYPE type = ptr->GetType();
    if (type == D3DRTYPE_TEXTURE)
      return static_cast<D3D9Texture2D*>  (ptr)->GetCommonTexture();
    else if (type == D3DRTYPE_CUBETEXTURE)
      return static_cast<D3D9TextureCube*>(ptr)->GetCommonTexture();
    else //if(type == D3DRTYPE_VOLUMETEXTURE)
      return static_cast<D3D9Texture3D*>  (ptr)->GetCommonTexture();
  }

  inline D3D9CommonTexture* GetCommonTexture(D3D9Surface* ptr) {
    if (ptr == nullptr)
      return nullptr;

    return ptr->GetCommonTexture();
  }

  inline D3D9CommonTexture* GetCommonTexture(IDirect3DSurface9* ptr) {
    return GetCommonTexture(static_cast<D3D9Surface*>(ptr));
  }

  inline void TextureRefPrivate(IDirect3DBaseTexture9* tex, bool AddRef) {
    if (tex == nullptr)
      return;

    D3DRESOURCETYPE type = tex->GetType();
    if (type == D3DRTYPE_TEXTURE)
      return CastRefPrivate<D3D9Texture2D>  (tex, AddRef);
    else if (type == D3DRTYPE_CUBETEXTURE)
      return CastRefPrivate<D3D9TextureCube>(tex, AddRef);
    else //if(type == D3DRTYPE_VOLUMETEXTURE)
      return CastRefPrivate<D3D9Texture3D>  (tex, AddRef);
  }

  inline void TextureChangePrivate(IDirect3DBaseTexture9*& dst, IDirect3DBaseTexture9* src) {
    TextureRefPrivate(dst, false);
    TextureRefPrivate(src, true);
    dst = src;
  }

}