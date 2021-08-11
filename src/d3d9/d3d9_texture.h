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
      , m_lod                 ( 0 ) {
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
        this->m_parent->MarkTextureBindingDirty(this);

      return oldLod;
    }

    DWORD STDMETHODCALLTYPE GetLOD() final {
      return m_lod;
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() final {
      return m_texture.ExposedMipLevels();
    }

    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) final {
      if (unlikely(FilterType == D3DTEXF_NONE))
        return D3DERR_INVALIDCALL;

      auto lock = this->m_parent->LockDevice();

      m_texture.SetMipFilter(FilterType);
      if (m_texture.IsAutomaticMip())
        this->m_parent->MarkTextureMipsDirty(&m_texture);
      return D3D_OK;
    }

    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() final {
      return m_texture.GetMipFilter();
    }

    void STDMETHODCALLTYPE GenerateMipSubLevels() final {
      if (!m_texture.NeedsMipGen())
        return;

      auto lock = this->m_parent->LockDevice();

      this->m_parent->MarkTextureMipsUnDirty(&m_texture);
      this->m_parent->EmitGenerateMips(&m_texture);
    }

    void STDMETHODCALLTYPE PreLoad() final {
      m_texture.PreLoadAll();
    }

    D3D9CommonTexture* GetCommonTexture() {
      return &m_texture;
    }

    SubresourceType* GetSubresource(UINT Subresource) {
      return reinterpret_cast<SubresourceType*>(&m_subresources[Subresource]);
    }

  protected:

    D3D9CommonTexture m_texture;

    std::vector<SubresourceData> m_subresources;

    DWORD m_lod;

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

  static_assert(sizeof(D3D9Texture2D) == sizeof(D3D9Texture3D) &&
                sizeof(D3D9Texture2D) == sizeof(D3D9TextureCube));

  inline D3D9CommonTexture* GetCommonTexture(IDirect3DBaseTexture9* ptr) {
    if (ptr == nullptr)
      return nullptr;

    // We can avoid needing to get the type as m_texture has the same offset
    // no matter the texture type.
    // The compiler is not smart enough to eliminate the call to GetType as it is
    // not marked const.
    return static_cast<D3D9Texture2D*>(ptr)->GetCommonTexture();
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

    // We can avoid needing to get the type as m_refCount has the same offset
    // no matter the texture type.
    // The compiler is not smart enough to eliminate the call to GetType as it is
    // not marked const.
    return CastRefPrivate<D3D9Texture2D>(tex, AddRef);
  }

  inline void TextureChangePrivate(IDirect3DBaseTexture9*& dst, IDirect3DBaseTexture9* src) {
    TextureRefPrivate(dst, false);
    TextureRefPrivate(src, true);
    dst = src;
  }

}