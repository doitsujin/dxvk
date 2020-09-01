#pragma once

#include "d3d8_device.h"

#include "d3d8_resource.h"
#include "d3d8_surface.h"

#include "d3d8_d3d9_util.h"

#include <vector>
#include <list>
#include <mutex>
#include <new>
#include <type_traits>

namespace dxvk {

   /**
   * \brief Common texture description
   * 
   * Contains all members that can be
   * defined for 2D, Cube and 3D textures.
   */
  struct D3D8_COMMON_TEXTURE_DESC {
    UINT                Width;
    UINT                Height;
    UINT                Depth;
    UINT                ArraySize;
    UINT                MipLevels;
    DWORD               Usage;
    D3DFORMAT           Format; // TODO: D3D8Format
    D3DPOOL             Pool;
    BOOL                Discard;
    D3DMULTISAMPLE_TYPE MultiSample;
  };


  // Implements IDirect3DBaseTexture8 (Except GetType)
  template <typename SubresourceType, typename... Base>
  class D3D8BaseTexture : public D3D8Resource<Base...> {

  public:

    using SubresourceData = std::aligned_storage_t<sizeof(SubresourceType), alignof(SubresourceType)>;

    D3D8BaseTexture(
            D3D8DeviceEx*                 pDevice,
            d3d9::IDirect3DBaseTexture9*  pBaseTexture,
      //const D3D8_COMMON_TEXTURE_DESC*     pDesc,
            D3DRESOURCETYPE               ResourceType)
        : D3D8Resource<Base...> ( pDevice )
        , m_pBaseTexture        ( pBaseTexture ) {
      // TODO: set up subresource
    }

    ~D3D8BaseTexture() {
    }

    // TODO: all these methods should probably be final

    void STDMETHODCALLTYPE PreLoad() {
      m_pBaseTexture->PreLoad();
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) {
      return m_pBaseTexture->SetLOD(LODNew);
    }

    DWORD STDMETHODCALLTYPE GetLOD() {
      return m_pBaseTexture->GetLOD();
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() {
      return m_pBaseTexture->GetLevelCount();
    }

    inline operator d3d9::IDirect3DBaseTexture9*() const { return m_pBaseTexture; }

    SubresourceType* GetSubresource(UINT Subresource) {
      return reinterpret_cast<SubresourceType*>(&m_subresources[Subresource]);
    }

  protected:

    d3d9::IDirect3DBaseTexture9* m_pBaseTexture;

    std::vector<SubresourceData> m_subresources;

  };

  // Implements IDirect3DTexture8
  using D3D8Texture2DBase = D3D8BaseTexture<D3D8Surface, IDirect3DTexture8>;
  class D3D8Texture2D final : public D3D8Texture2DBase {

  public:

    D3D8Texture2D(
            D3D8DeviceEx*             pDevice,
            d3d9::IDirect3DTexture9*  pTexture)
      //const D3D8_COMMON_TEXTURE_DESC* pDesc)
        : D3D8Texture2DBase( pDevice, pTexture, D3DRTYPE_TEXTURE )
        , m_pTexture(pTexture) { }

    // TODO: QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return D3DRTYPE_TEXTURE; }

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC surf;
      HRESULT res = m_pTexture->GetLevelDesc(Level, &surf);
      ConvertSurfaceDesc8(&surf, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
      d3d9::IDirect3DSurface9* pSurface = nullptr;
      HRESULT res = m_pTexture->GetSurfaceLevel(Level, &pSurface);
      D3D8Surface* surface = new D3D8Surface(m_parent, pSurface);
      *ppSurfaceLevel = surface;
      return res;
    }

    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
      return m_pTexture->LockRect(Level, (d3d9::D3DLOCKED_RECT*)pLockedRect, pRect, Flags);
    }

    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) {
      return m_pTexture->UnlockRect(Level);
    }

    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect) {
      return m_pTexture->AddDirtyRect(pDirtyRect);
    }

  private:
    d3d9::IDirect3DTexture9* m_pTexture;

  };

/*
  using D3D8Texture3DBase = D3D8BaseTexture<D3D8Volume, IDirect3DVolumeTexture8>;
  class D3D8Texture3D final : public D3D8Texture3DBase {

  public:

    D3D8Texture3D(
            D3D8DeviceEx*             pDevice,
      const D3D8_COMMON_TEXTURE_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume8** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX* pDirtyBox);

  };

  using D3D8TextureCubeBase = D3D8BaseTexture<D3D8Surface, IDirect3DCubeTexture8>;
  class D3D8TextureCube final : public D3D8TextureCubeBase {

  public:

    D3D8TextureCube(
            D3D8DeviceEx*             pDevice,
      const D3D8_COMMON_TEXTURE_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface8** ppSurfaceLevel);

    HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES Face, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES Face, UINT Level);

    HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES Face, CONST RECT* pDirtyRect);

  };

  inline D3D8CommonTexture* GetCommonTexture(IDirect3DBaseTexture8* ptr) {
    if (ptr == nullptr)
      return nullptr;

    D3DRESOURCETYPE type = ptr->GetType();
    if (type == D3DRTYPE_TEXTURE)
      return static_cast<D3D8Texture2D*>  (ptr)->GetCommonTexture();
    else if (type == D3DRTYPE_CUBETEXTURE)
      return static_cast<D3D8TextureCube*>(ptr)->GetCommonTexture();
    else //if(type == D3DRTYPE_VOLUMETEXTURE)
      return static_cast<D3D8Texture3D*>  (ptr)->GetCommonTexture();
  }

  inline D3D8CommonTexture* GetCommonTexture(D3D8Surface* ptr) {
    if (ptr == nullptr)
      return nullptr;

    return ptr->GetCommonTexture();
  }

  inline D3D8CommonTexture* GetCommonTexture(IDirect3DSurface8* ptr) {
    return GetCommonTexture(static_cast<D3D8Surface*>(ptr));
  }

  inline void TextureRefPrivate(IDirect3DBaseTexture8* tex, bool AddRef) {
    if (tex == nullptr)
      return;

    D3DRESOURCETYPE type = tex->GetType();
    if (type == D3DRTYPE_TEXTURE)
      return CastRefPrivate<D3D8Texture2D>  (tex, AddRef);
    else if (type == D3DRTYPE_CUBETEXTURE)
      return CastRefPrivate<D3D8TextureCube>(tex, AddRef);
    else //if(type == D3DRTYPE_VOLUMETEXTURE)
      return CastRefPrivate<D3D8Texture3D>  (tex, AddRef);
  }

  inline void TextureChangePrivate(IDirect3DBaseTexture8*& dst, IDirect3DBaseTexture8* src) {
    TextureRefPrivate(dst, false);
    TextureRefPrivate(src, true);
    dst = src;
  }
*/

}