#pragma once

#include "d3d11_include.h"

namespace dxvk {

  /**
   * \brief Common resource description
   * 
   * Stores the usage and bind flags of a resource
   * Can be used to quickly determine whether it is
   * legal to create a view for a given resource.
   */
  struct D3D11_COMMON_RESOURCE_DESC {
    D3D11_RESOURCE_DIMENSION  Dim;
    DXGI_FORMAT               Format;
    D3D11_USAGE               Usage;
    UINT                      BindFlags;
    UINT                      CPUAccessFlags;
    UINT                      MiscFlags;
    UINT                      DxgiUsage;
  };
  

  /**
   * \brief IDXGIKeyedMutex implementation
   */
  class D3D11DXGIKeyedMutex : public IDXGIKeyedMutex {

  public:

    D3D11DXGIKeyedMutex(
            ID3D11Resource*         pResource,
            D3D11Device*            pDevice);

    ~D3D11DXGIKeyedMutex();

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

    HRESULT STDMETHODCALLTYPE AcquireSync(
            UINT64                  Key,
            DWORD                   dwMilliseconds);

    HRESULT STDMETHODCALLTYPE ReleaseSync(
            UINT64                  Key);

  private:

    ID3D11Resource* m_resource;
    D3D11Device* m_device;
    bool m_warned = false;
    bool m_supported = false;
  };


  /**
   * \brief IDXGIResource implementation for D3D11 resources
   */
  class D3D11DXGIResource : public IDXGIResource1 {

  public:
    
    D3D11DXGIResource(
            ID3D11Resource*         pResource,
            D3D11Device*            pDevice);

    ~D3D11DXGIResource();

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

    HRESULT STDMETHODCALLTYPE GetEvictionPriority(
            UINT*                   pEvictionPriority);

    HRESULT STDMETHODCALLTYPE GetSharedHandle(
            HANDLE*                 pSharedHandle);

    HRESULT STDMETHODCALLTYPE GetUsage(
            DXGI_USAGE*             pUsage);

    HRESULT STDMETHODCALLTYPE SetEvictionPriority(
            UINT                    EvictionPriority);

    HRESULT STDMETHODCALLTYPE CreateSharedHandle(
      const SECURITY_ATTRIBUTES*    pAttributes,
            DWORD                   dwAccess,
            LPCWSTR                 lpName,
            HANDLE*                 pHandle);

    HRESULT STDMETHODCALLTYPE CreateSubresourceSurface(
            UINT                    index,
            IDXGISurface2**         ppSurface);

    HRESULT GetKeyedMutex(void **ppvObject);

  private:

    ID3D11Resource* m_resource;
    D3D11DXGIKeyedMutex m_keyedMutex;

  };


  /**
   * \brief Queries D3D11on12 resource info
   *
   * \param [in] pResource The resource to query
   * \param [out] p11on12Info 11on12 info
   * \returns \c S_OK on success, or \c E_INVALIDARG
   */
  HRESULT GetResource11on12Info(
          ID3D11Resource*             pResource,
          D3D11_ON_12_RESOURCE_INFO*  p11on12Info);

  /**
   * \brief Queries common resource description
   * 
   * \param [in] pResource The resource to query
   * \param [out] pDesc Resource description
   * \returns \c S_OK on success, or \c E_INVALIDARG
   */
  HRESULT GetCommonResourceDesc(
          ID3D11Resource*             pResource,
          D3D11_COMMON_RESOURCE_DESC* pDesc);

  /**
   * \brief Checks whether a format can be used to view a resource
   * 
   * Depending on whether the resource is a buffer or a
   * texture, certain restrictions apply on which formats
   * can be used to view the resource.
   * \param [in] pResource The resource to check
   * \param [in] BindFlags Bind flags required for the view
   * \param [in] Format The desired view format
   * \param [in] Plane Plane slice for planar formats
   * \returns \c true if the format is compatible
   */
  BOOL CheckResourceViewCompatibility(
          ID3D11Resource*             pResource,
          UINT                        BindFlags,
          DXGI_FORMAT                 Format,
          UINT                        Plane);

  /**
   * \brief Queries paged resource from resource pointer
   *
   * \param [in] resource The resource
   * \returns Paged resource object
   */
  Rc<DxvkPagedResource> GetPagedResource(
          ID3D11Resource*             pResource);

  /**
   * \brief Increments private reference count of a resource
   * 
   * Helper method that figures out the exact type of
   * the resource and calls its \c AddRefPrivate method.
   * \param [in] pResource The resource to reference
   * \param [in] Type Resource type
   * \returns \c S_OK, or \c E_INVALIDARG for an invalid resource
   */
  HRESULT ResourceAddRefPrivate(
          ID3D11Resource*             pResource,
          D3D11_RESOURCE_DIMENSION    Type);
  
  HRESULT ResourceAddRefPrivate(
          ID3D11Resource*             pResource);

  /**
   * \brief Decrements private reference count of a resource
   * 
   * Helper method that figures out the exact type of
   * the resource and calls its \c ReleasePrivate method.
   * \param [in] pResource The resource to reference
   * \param [in] Type Resource type
   * \returns \c S_OK, or \c E_INVALIDARG for an invalid resource
   */
  HRESULT ResourceReleasePrivate(
          ID3D11Resource*             pResource,
          D3D11_RESOURCE_DIMENSION    Type);

  HRESULT ResourceReleasePrivate(
          ID3D11Resource*             pResource);

  /**
   * \brief Typed private resource pointer
   *
   * Stores a resource and its type, in order to avoid
   * unnecessary GetType calls. Also optionally stores
   * a subresource index to avoid struct padding.
   */
  class D3D11ResourceRef {

  public:

    D3D11ResourceRef()
    : m_type(D3D11_RESOURCE_DIMENSION_UNKNOWN),
      m_subresource(0), m_resource(nullptr) { }

    D3D11ResourceRef(ID3D11Resource* pResource)
    : D3D11ResourceRef(pResource, 0) { }

    D3D11ResourceRef(ID3D11Resource* pResource, UINT Subresource)
    : m_type(D3D11_RESOURCE_DIMENSION_UNKNOWN),
      m_subresource(Subresource), m_resource(pResource) {
      if (m_resource) {
        m_resource->GetType(&m_type);
        ResourceAddRefPrivate(m_resource, m_type);
      }
    }

    D3D11ResourceRef(ID3D11Resource* pResource, UINT Subresource, D3D11_RESOURCE_DIMENSION Type)
    : m_type(Type), m_subresource(Subresource), m_resource(pResource) {
      if (m_resource)
        ResourceAddRefPrivate(m_resource, m_type);
    }

    D3D11ResourceRef(D3D11ResourceRef&& other)
    : m_type(other.m_type), m_subresource(other.m_subresource), m_resource(other.m_resource) {
      other.m_type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      other.m_subresource = 0;
      other.m_resource = nullptr;
    }

    D3D11ResourceRef(const D3D11ResourceRef& other)
    : m_type(other.m_type), m_subresource(other.m_subresource), m_resource(other.m_resource) {
      if (m_resource)
        ResourceAddRefPrivate(m_resource, m_type);
    }

    ~D3D11ResourceRef() {
      if (m_resource)
        ResourceReleasePrivate(m_resource, m_type);
    }

    D3D11ResourceRef& operator = (D3D11ResourceRef&& other) {
      if (m_resource)
        ResourceReleasePrivate(m_resource, m_type);

      m_type = other.m_type;
      m_subresource = other.m_subresource;
      m_resource = other.m_resource;

      other.m_type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      other.m_subresource = 0;
      other.m_resource = nullptr;
      return *this;
    }

    D3D11ResourceRef& operator = (const D3D11ResourceRef& other) {
      if (other.m_resource)
        ResourceAddRefPrivate(other.m_resource, other.m_type);

      if (m_resource)
        ResourceReleasePrivate(m_resource, m_type);

      m_type = other.m_type;
      m_subresource = other.m_subresource;
      m_resource = other.m_resource;
      return *this;
    }

    D3D11_RESOURCE_DIMENSION GetType() const {
      return m_type;
    }

    UINT GetSubresource() const {
      return m_subresource;
    }

    ID3D11Resource* Get() const {
      return m_resource;
    }

  private:

    D3D11_RESOURCE_DIMENSION  m_type;
    UINT                      m_subresource;
    ID3D11Resource*           m_resource;

  };

}
