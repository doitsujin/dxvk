#pragma once

#include "../util/util_small_vector.h"

#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_texture.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"
#include "d3d11_on_12.h"
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
   * \brief Region
   */
  struct D3D11_COMMON_TEXTURE_REGION {
    VkOffset3D      Offset;
    VkExtent3D      Extent;
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
            ID3D11Resource*             pInterface,
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
      const D3D11_ON_12_RESOURCE_INFO*  p11on12Info,
            D3D11_RESOURCE_DIMENSION    Dimension,
            DXGI_USAGE                  DxgiUsage,
            VkImage                     vkImage,
            HANDLE                      hSharedHandle);
    
    ~D3D11CommonTexture();
    
    /**
     * \brief Retrieves resource interface
     * \returns Resource interface
     */
    ID3D11Resource* GetInterface() const {
      return m_interface;
    }

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
     * \brief Retrieves D3D11 texture type
     * \returns D3D11 resource dimension
     */
    D3D11_RESOURCE_DIMENSION GetDimension() const {
      return m_dimension;
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
      return Subresource < m_mapInfo.size()
        ? D3D11_MAP(m_mapInfo[Subresource].mapType)
        : D3D11_MAP(~0u);
    }

    /**
     * \brief Sets map type for a given subresource
     * 
     * \param [in] Subresource The subresource
     * \param [in] MapType The map type
     */
    void SetMapType(UINT Subresource, D3D11_MAP MapType) {
      if (Subresource < m_mapInfo.size())
        m_mapInfo[Subresource].mapType = MapType;
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
    Rc<DxvkResourceAllocation> DiscardSlice(UINT Subresource) {
      if (Subresource < m_buffers.size()) {
        Rc<DxvkResourceAllocation> slice = m_buffers[Subresource].buffer->allocateStorage();
        m_buffers[Subresource].slice = slice;
        return slice;
      } else {
        return nullptr;
      }
    }

    /**
     * \brief Retrieves mapped buffer slice for a given subresource
     *
     * \param [in] Subresource Subresource index to query
     * \returns Currently mapped buffer slice
     */
    Rc<DxvkResourceAllocation> GetMappedSlice(UINT Subresource) const {
      return Subresource < m_buffers.size()
        ? m_buffers[Subresource].slice
        : nullptr;
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
     * \brief Checks whether the resource is eligible for tracking
     *
     * Mapped resources with no bind flags can be tracked so that
     * mapping them will not necessarily cause a CS thread sync.
     * \returns \c true if tracking is supported for this resource
     */
    bool HasSequenceNumber() const {
      if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_NONE)
        return false;

      // For buffer-mapped images we only need to track copies to
      // and from that buffer, so we can safely ignore bind flags
      if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER)
        return m_desc.Usage != D3D11_USAGE_DEFAULT;

      // Otherwise we can only do accurate tracking if the
      // image cannot be used in the rendering pipeline.
      return m_desc.BindFlags == 0;
    }

    /**
     * \brief Tracks sequence number for a given subresource
     *
     * Stores which CS chunk the resource was last used on.
     * \param [in] Subresource Subresource index
     * \param [in] Seq Sequence number
     */
    void TrackSequenceNumber(UINT Subresource, uint64_t Seq) {
      if (Subresource < m_mapInfo.size())
        m_mapInfo[Subresource].seq = Seq;
    }

    /**
     * \brief Queries sequence number for a given subresource
     *
     * Returns which CS chunk the resource was last used on.
     * \param [in] Subresource Subresource index
     * \returns Sequence number for the given subresource
     */
    uint64_t GetSequenceNumber(UINT Subresource) {
      if (HasSequenceNumber()) {
        return Subresource < m_buffers.size()
          ? m_mapInfo[Subresource].seq
          : 0ull;
      } else {
        return DxvkCsThread::SynchronizeAll;
      }
    }

    /**
     * \brief Allocates new backing storage
     * \returns New backing storage for the image
     */
    Rc<DxvkResourceAllocation> AllocStorage() {
      return m_image->allocateStorage();
    }

    /**
     * \brief Discards backing storage
     *
     * Also updates the mapped pointer if the image is mapped.
     * \returns New backing storage for the image
     */
    Rc<DxvkResourceAllocation> DiscardStorage() {
      auto storage = m_image->allocateStorage();
      m_mapPtr = storage->mapPtr();
      return storage;
    }

    /**
     * \brief Queries map pointer of the raw image
     *
     * If the image is mapped directly, the returned pointer will
     * point directly to the image, otherwise it will point to a
     * buffer that contains image data.
     * \param [in] Subresource Subresource index
     * \param [in] Offset Offset derived from the subresource layout
     */
    void* GetMapPtr(uint32_t Subresource, size_t Offset) const {
      switch (m_mapMode) {
        case D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT:
          return reinterpret_cast<char*>(m_mapPtr) + Offset;

        case D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER:
        case D3D11_COMMON_TEXTURE_MAP_MODE_STAGING:
          return reinterpret_cast<char*>(m_buffers[Subresource].slice->mapPtr()) + Offset;

        case D3D11_COMMON_TEXTURE_MAP_MODE_NONE:
          return nullptr;
      }

      return nullptr;
    }

    /**
     * \brief Adds a dirty region
     *
     * This region will be updated on Unmap.
     * \param [in] Subresource Subresource index
     * \param [in] Offset Region offset
     * \param [in] Extent Region extent
     */
    void AddDirtyRegion(UINT Subresource, VkOffset3D Offset, VkExtent3D Extent) {
      D3D11_COMMON_TEXTURE_REGION region;
      region.Offset = Offset;
      region.Extent = Extent;

      if (Subresource < m_buffers.size())
        m_buffers[Subresource].dirtyRegions.push_back(region);
    }

    /**
     * \brief Clears dirty regions
     *
     * Removes all dirty regions from the given subresource.
     * \param [in] Subresource Subresource index
     */
    void ClearDirtyRegions(UINT Subresource) {
      if (Subresource < m_buffers.size())
        m_buffers[Subresource].dirtyRegions.clear();
    }

    /**
     * \brief Counts dirty regions
     *
     * \param [in] Subresource Subresource index
     * \returns Dirty region count
     */
    UINT GetDirtyRegionCount(UINT Subresource) {
      return (Subresource < m_buffers.size())
        ? UINT(m_buffers[Subresource].dirtyRegions.size())
        : UINT(0);
    }

    /**
     * \brief Queries a dirty regions
     *
     * \param [in] Subresource Subresource index
     * \param [in] Region Region index
     * \returns Dirty region
     */
    D3D11_COMMON_TEXTURE_REGION GetDirtyRegion(UINT Subresource, UINT Region) {
      return m_buffers[Subresource].dirtyRegions[Region];
    }

    /**
     * \brief Checks whether or not to track dirty regions
     *
     * If this returns true, then any functions that update the
     * mapped staging buffer must also track dirty regions while
     * the image is mapped. Otherwise, the entire image is dirty.
     * \returns \c true if dirty regions must be tracked
     */
    bool NeedsDirtyRegionTracking() const {
      // Only set this for images where Map can't return a pointer
      return m_mapMode            == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER
          && m_desc.Usage         == D3D11_USAGE_DEFAULT
          && m_desc.TextureLayout == D3D11_TEXTURE_LAYOUT_UNDEFINED;
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
            UINT                  Subresource) const {
      VkImageSubresource result;
      result.aspectMask     = Aspect;
      result.mipLevel       = Subresource % m_desc.MipLevels;
      result.arrayLayer     = Subresource / m_desc.MipLevels;
      return result;
    }

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
     * \brief Retrieves D3D11on12 resource info
     * \returns 11on12 resource info
     */
    D3D11_ON_12_RESOURCE_INFO Get11on12Info() const {
      return m_11on12;
    }

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
    
    /**
     * \brief Initializes D3D11 texture description from D3D12
     *
     * \param [in] pResource D3D12 resource
     * \param [in] pResourceFlags D3D11 flag overrides
     * \param [out] pTextureDesc D3D11 buffer description
     * \returns \c S_OK if the parameters are valid
     */
    static HRESULT GetDescFromD3D12(
            ID3D12Resource*         pResource,
      const D3D11_RESOURCE_FLAGS*   pResourceFlags,
            D3D11_COMMON_TEXTURE_DESC* pTextureDesc);

  private:
    
    struct MappedBuffer {
      Rc<DxvkBuffer>              buffer;
      Rc<DxvkResourceAllocation>  slice;

      std::vector<D3D11_COMMON_TEXTURE_REGION> dirtyRegions;
    };

    struct MappedInfo {
      D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT layout = { };
      D3D11_MAP                   mapType = D3D11_MAP(~0u);
      uint64_t                    seq     = 0u;
    };

    ID3D11Resource*               m_interface;
    D3D11Device*                  m_device;
    D3D11_RESOURCE_DIMENSION      m_dimension;
    D3D11_COMMON_TEXTURE_DESC     m_desc;
    D3D11_ON_12_RESOURCE_INFO     m_11on12;
    D3D11_COMMON_TEXTURE_MAP_MODE m_mapMode;
    DXGI_USAGE                    m_dxgiUsage;
    VkFormat                      m_packedFormat;
    
    Rc<DxvkImage>                 m_image;
    small_vector<MappedBuffer, 6> m_buffers;
    small_vector<MappedInfo, 6>   m_mapInfo;

    void*                         m_mapPtr = nullptr;

    void CreateMappedBuffer(
            UINT                  Subresource);
    
    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
            VkImageTiling         Tiling) const;
    
    BOOL CheckFormatFeatureSupport(
            VkFormat              Format,
            VkFormatFeatureFlags2 Features) const;
    
    std::pair<D3D11_COMMON_TEXTURE_MAP_MODE, VkMemoryPropertyFlags> DetermineMapMode(
      const DxvkImageCreateInfo*  pImageInfo) const;

    D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT DetermineSubresourceLayout(
      const DxvkImageCreateInfo*  pImageInfo,
      const VkImageSubresource&   subresource) const;

    void ExportImageInfo();

    static BOOL IsR32UavCompatibleFormat(
            DXGI_FORMAT           Format);

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
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
      const D3D11_ON_12_RESOURCE_INFO*  p11on12Info);
    
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
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
      const D3D11_ON_12_RESOURCE_INFO*  p11on12Info,
            HANDLE                      hSharedHandle);

    D3D11Texture2D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
            DXGI_USAGE                  DxgiUsage,
            VkImage                     vkImage);
    
    D3D11Texture2D(
            D3D11Device*                pDevice,
            IUnknown*                   pSwapChain,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
            DXGI_USAGE                  DxgiUsage);
    
    ~D3D11Texture2D();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();

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
    IUnknown*             m_swapChain;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  class D3D11Texture3D : public D3D11DeviceChild<ID3D11Texture3D1> {
    
  public:
    
    D3D11Texture3D(
            D3D11Device*                pDevice,
      const D3D11_COMMON_TEXTURE_DESC*  pDesc,
      const D3D11_ON_12_RESOURCE_INFO*  p11on12Info);
    
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
