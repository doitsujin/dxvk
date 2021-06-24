#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_texture.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"
#include "d3d11_resource.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11GDISurface;
  
  /**
   * \brief Image memory mapping mode
   * 
   * Determines how exactly \c Map will
   * behave when mapping an image.
   */
  enum D3D11_COMMON_TEXTURE_MAP_MODE {
    D3D11_COMMON_TEXTURE_MAP_MODE_NONE,     ///< Not mapped
    D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER,   ///< Mapped through buffer
    D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT,   ///< Directly mapped to host mem
    D3D11_COMMON_TEXTURE_MAP_MODE_STAGING,  ///< Buffer only, no image
  };
  
  
  /**
   * \brief Common texture description
   * 
   * Contains all members that can be
   * defined for 1D, 2D and 3D textures.
   */
  struct D3D11_COMMON_TEXTURE_DESC {
    UINT             Width;
    UINT             Height;
    UINT             Depth;
    UINT             MipLevels;
    UINT             ArraySize;
    DXGI_FORMAT      Format;
    DXGI_SAMPLE_DESC SampleDesc;
    D3D11_USAGE      Usage;
    UINT             BindFlags;
    UINT             CPUAccessFlags;
    UINT             MiscFlags;
    D3D11_TEXTURE_LAYOUT TextureLayout;
  };


  /**
   * \brief Packed subresource layout
   */
  struct D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT {
    UINT64          Offset;
    UINT64          Size;
    UINT            RowPitch;
    UINT            DepthPitch;
  };
  
  
  /**
   * \brief D3D11 common texture object
   * 
   * This class implements common texture methods and
   * aims to work around the issue that there are three
   * different interfaces for basically the same thing.
   */
  class D3D11CommonTexture {
    
  public:
    
    D3D11CommonTexture(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
            D3D11_RESOURCE_DIMENSION    Dimension,
            DXGI_USAGE                  DxgiUsage,
            VkImage                     vkImage);
    
    ~D3D11CommonTexture();
    
    /**
     * \brief Texture properties
     * 
     * The returned data can be used to fill in
     * \c D3D11_TEXTURE2D_DESC and similar structs.
     * \returns Pointer to texture description
     */
    const D3D11_COMMON_TEXTURE_DESC* Desc() const {
      return &m_desc;
    }

    /**
     * \brief Retrieves Vulkan image type
     *
     * Returns the image type based on the D3D11 resource
     * dimension. Also works if there is no actual image.
     * \returns Vulkan image type
     */
    VkImageType GetVkImageType() const {
      return GetImageTypeFromResourceDim(m_dimension);
    }

    /**
     * \brief Computes extent of a given mip level
     *
     * This also works for staging resources that have no image.
     * \param [in] Level Mip level to compute the size of
     */
    VkExtent3D MipLevelExtent(uint32_t Level) const {
      return util::computeMipLevelExtent(
        VkExtent3D { m_desc.Width, m_desc.Height, m_desc.Depth }, Level);
    }

    /**
     * \brief Special DXGI usage flags
     *
     * Flags that are set in addition to the bind
     * flags. Zero for application-created textures.
     * \returns DXGI usage flags
     */
    DXGI_USAGE GetDxgiUsage() const {
      return m_dxgiUsage;
    }

    /**
     * \brief Counts number of subresources
     * \returns Number of subresources
     */
    UINT CountSubresources() const {
      return m_desc.ArraySize * m_desc.MipLevels;
    }
    
    /**
     * \brief Map mode
     * \returns Map mode
     */
    D3D11_COMMON_TEXTURE_MAP_MODE GetMapMode() const {
      return m_mapMode;
    }

    /**
     * \brief Map type of a given subresource
     * 
     * \param [in] Subresource Subresource index
     * \returns Current map mode of that subresource
     */
    D3D11_MAP GetMapType(UINT Subresource) const {
      return Subresource < m_mapTypes.size()
        ? D3D11_MAP(m_mapTypes[Subresource])
        : D3D11_MAP(~0u);
    }

    /**
     * \brief Sets map type for a given subresource
     * 
     * \param [in] Subresource The subresource
     * \param [in] MapType The map type
     */
    void SetMapType(UINT Subresource, D3D11_MAP MapType) {
      if (Subresource < m_mapTypes.size())
        m_mapTypes[Subresource] = MapType;
    }
    
    /**
     * \brief The DXVK image
     * \returns The DXVK image
     */
    Rc<DxvkImage> GetImage() const {
      return m_image;
    }
    
    /**
     * \brief Mapped subresource buffer
     * 
     * \param [in] Subresource Subresource index
     * \returns Mapped subresource buffer
     */
    Rc<DxvkBuffer> GetMappedBuffer(UINT Subresource) const {
      return Subresource < m_buffers.size()
        ? m_buffers[Subresource].buffer
        : Rc<DxvkBuffer>();
    }

    /**
     * \brief Discards mapped buffer slice for a given subresource
     *
     * \param [in] Subresource Subresource to discard
     * \returns Newly allocated mapped buffer slice
     */
    DxvkBufferSliceHandle DiscardSlice(UINT Subresource) {
      if (Subresource < m_buffers.size()) {
        DxvkBufferSliceHandle slice = m_buffers[Subresource].buffer->allocSlice();
        m_buffers[Subresource].slice = slice;
        return slice;
      } else {
        return DxvkBufferSliceHandle();
      }
    }

    /**
     * \brief Retrieves mapped buffer slice for a given subresource
     *
     * \param [in] Subresource Subresource index to query
     * \returns Currently mapped buffer slice
     */
    DxvkBufferSliceHandle GetMappedSlice(UINT Subresource) const {
      return Subresource < m_buffers.size()
        ? m_buffers[Subresource].slice
        : DxvkBufferSliceHandle();
    }

    /**
     * \brief Returns underlying packed Vulkan format
     *
     * This works even for staging resources that have no image.
     * Note that for depth-stencil resources, the returned format
     * may be different from the image format on some systems.
     * \returns Packed Vulkan format
     */
    VkFormat GetPackedFormat() const {
      return m_packedFormat;
    }
    
    /**
     * \brief Computes pixel offset into mapped buffer
     *
     * \param [in] Subresource Subresource index
     * \param [in] Subresource Plane index
     * \param [in] Offset Pixel coordinate to compute offset for
     * \returns Offset into mapped subresource buffer, in pixels
     */
    VkDeviceSize ComputeMappedOffset(UINT Subresource, UINT Plane, VkOffset3D Offset) const;
    
    /**
     * \brief Computes subresource from the subresource index
     * 
     * Used by some functions that operate on only
     * one subresource, such as \c UpdateSubresource.
     * \param [in] Aspect The image aspect
     * \param [in] Subresource Subresource index
     * \returns The Vulkan image subresource
     */
    VkImageSubresource GetSubresourceFromIndex(
            VkImageAspectFlags    Aspect,
            UINT                  Subresource) const;

    /**
     * \brief Computes subresource layout for the given subresource
     *
     * \param [in] AspectMask The image aspect
     * \param [in] Subresource Subresource index
     * \returns Memory layout of the mapped subresource
     */
    D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT GetSubresourceLayout(
            VkImageAspectFlags    AspectMask,
            UINT                  Subresource) const;

    /**
     * \brief Format mode
     * 
     * Determines whether the image is going to
     * be used as a color image or a depth image.
     * \returns Format mode
     */
    DXGI_VK_FORMAT_MODE GetFormatMode() const;

    /**
     * \brief Computes plane count
     *
     * For non-planar formats, the plane count will be 1.
     * \returns Image plane count
     */
    uint32_t GetPlaneCount() const;

    /**
     * \brief Checks whether a view can be created for this textue
     * 
     * View formats are only compatible if they are either identical
     * or from the same family of typeless formats, where the resource
     * format must be typeless and the view format must be typed. This
     * will also check whether the required bind flags are supported.
     * \param [in] BindFlags Bind flags for the view
     * \param [in] Format The desired view format
     * \param [in] Plane Plane slice for planar formats
     * \returns \c true if the format is compatible
     */
    bool CheckViewCompatibility(
            UINT                BindFlags,
            DXGI_FORMAT         Format,
            UINT                Plane) const;
    
    /**
     * \brief Normalizes and validates texture description
     * 
     * Fills in undefined values and validates the texture
     * parameters. Any error returned by this method should
     * be forwarded to the application.
     * \param [in,out] pDesc Texture description
     * \returns \c S_OK if the parameters are valid
     */
    static HRESULT NormalizeTextureProperties(
            D3D11_COMMON_TEXTURE_DESC* pDesc);
    
  private:
    
    struct MappedBuffer {
      Rc<DxvkBuffer>        buffer;
      DxvkBufferSliceHandle slice;
    };

    D3D11Device* const            m_device;
    D3D11_RESOURCE_DIMENSION      m_dimension;
    D3D11_COMMON_TEXTURE_DESC     m_desc;
    D3D11_COMMON_TEXTURE_MAP_MODE m_mapMode;
    DXGI_USAGE                    m_dxgiUsage;
    VkFormat                      m_packedFormat;
    
    Rc<DxvkImage>                 m_image;
    std::vector<MappedBuffer>     m_buffers;
    std::vector<D3D11_MAP>        m_mapTypes;
    
    MappedBuffer CreateMappedBuffer(
            UINT                  MipLevel) const;
    
    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
            VkImageTiling         Tiling) const;
    
    BOOL CheckFormatFeatureSupport(
            VkFormat              Format,
            VkFormatFeatureFlags  Features) const;
    
    VkImageUsageFlags EnableMetaCopyUsage(
            VkFormat              Format,
            VkImageTiling         Tiling) const;
    
    VkImageUsageFlags EnableMetaPackUsage(
            VkFormat              Format,
            UINT                  CpuAccess) const;
    
    D3D11_COMMON_TEXTURE_MAP_MODE DetermineMapMode(
      const DxvkImageCreateInfo*  pImageInfo) const;
    
    static VkImageType GetImageTypeFromResourceDim(
            D3D11_RESOURCE_DIMENSION  Dimension);
    
    static VkImageLayout OptimizeLayout(
            VkImageUsageFlags         Usage);
    
  };


  /**
   * \brief IDXGISurface implementation for D3D11 textures
   *
   * Provides an implementation for 2D textures that
   * have only one array layer and one mip level.
   */
  class D3D11DXGISurface : public IDXGISurface2 {

  public:

    D3D11DXGISurface(
            ID3D11Resource*     pResource,
            D3D11CommonTexture* pTexture);
    
    ~D3D11DXGISurface();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID                 Name,
            UINT*                   pDataSize,
            void*                   pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                 Name,
            UINT                    DataSize,
      const void*                   pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                 Name,
      const IUnknown*               pUnknown);
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                  riid,
            void**                  ppParent);
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID                  riid,
            void**                  ppDevice);
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_SURFACE_DESC*      pDesc);
    
    HRESULT STDMETHODCALLTYPE Map(
            DXGI_MAPPED_RECT*       pLockedRect,
            UINT                    MapFlags);
    
    HRESULT STDMETHODCALLTYPE Unmap();

    HRESULT STDMETHODCALLTYPE GetDC(
            BOOL                    Discard,
            HDC*                    phdc);

    HRESULT STDMETHODCALLTYPE ReleaseDC(
            RECT*                   pDirtyRect);
    
    HRESULT STDMETHODCALLTYPE GetResource(
            REFIID                  riid,
            void**                  ppParentResource,
            UINT*                   pSubresourceIndex);
    
    bool isSurfaceCompatible() const;

  private:
    
    ID3D11Resource*     m_resource;
    D3D11CommonTexture* m_texture;
    D3D11GDISurface*    m_gdiSurface;

  };
  
  
  /**
   * \brief Common texture interop class
   * 
   * Provides access to the underlying Vulkan
   * texture to external Vulkan libraries.
   */
  class D3D11VkInteropSurface : public IDXGIVkInteropSurface {
    
  public:
    
    D3D11VkInteropSurface(
            ID3D11Resource*     pResource,
            D3D11CommonTexture* pTexture);
    
    ~D3D11VkInteropSurface();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            IDXGIVkInteropDevice**  ppDevice);
    
    HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
            VkImage*              pHandle,
            VkImageLayout*        pLayout,
            VkImageCreateInfo*    pInfo);
    
  private:
    
    ID3D11Resource*     m_resource;
    D3D11CommonTexture* m_texture;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 1 D
  class D3D11Texture1D : public D3D11DeviceChild<ID3D11Texture1D> {
    
  public:
    
    D3D11Texture1D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc);
    
    ~D3D11Texture1D();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_TEXTURE1D_DESC *pDesc) final;
    
    D3D11CommonTexture* GetCommonTexture() {
      return &m_texture;
    }

    D3D10Texture1D* GetD3D10Iface() {
      return &m_d3d10;
    }
    
  private:
    
    D3D11CommonTexture    m_texture;
    D3D11VkInteropSurface m_interop;
    D3D11DXGISurface      m_surface;
    D3D11DXGIResource     m_resource;
    D3D10Texture1D        m_d3d10;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 2 D
  class D3D11Texture2D : public D3D11DeviceChild<ID3D11Texture2D1> {
    
  public:
    
    D3D11Texture2D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc);

    D3D11Texture2D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
            DXGI_USAGE                  DxgiUsage,
            VkImage                     vkImage);
    
    ~D3D11Texture2D();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_TEXTURE2D_DESC* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc1(
            D3D11_TEXTURE2D_DESC1* pDesc) final;
    
    D3D11CommonTexture* GetCommonTexture() {
      return &m_texture;
    }
    
    D3D10Texture2D* GetD3D10Iface() {
      return &m_d3d10;
    }

  private:
    
    D3D11CommonTexture    m_texture;
    D3D11VkInteropSurface m_interop;
    D3D11DXGISurface      m_surface;
    D3D11DXGIResource     m_resource;
    D3D10Texture2D        m_d3d10;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  class D3D11Texture3D : public D3D11DeviceChild<ID3D11Texture3D1> {
    
  public:
    
    D3D11Texture3D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc);
    
    ~D3D11Texture3D();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_TEXTURE3D_DESC* pDesc) final;
    
    void STDMETHODCALLTYPE GetDesc1(
            D3D11_TEXTURE3D_DESC1* pDesc) final;
    
    D3D11CommonTexture* GetCommonTexture() {
      return &m_texture;
    }
    
    D3D10Texture3D* GetD3D10Iface() {
      return &m_d3d10;
    }

  private:
    
    D3D11CommonTexture    m_texture;
    D3D11VkInteropSurface m_interop;
    D3D11DXGIResource     m_resource;
    D3D10Texture3D        m_d3d10;
    
  };
  
  
  /**
   * \brief Retrieves texture from resource pointer
   * 
   * \param [in] pResource The resource to query
   * \returns Pointer to texture info, or \c nullptr
   */
  D3D11CommonTexture* GetCommonTexture(
          ID3D11Resource*       pResource);
  
}
