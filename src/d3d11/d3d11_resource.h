#pragma once

#include "d3d11_buffer.h"
#include "d3d11_texture.h"

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
  };


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
   * \param [in] Format The desired view format
   * \returns \c true if the format is compatible
   */
  BOOL CheckResourceViewCompatibility(
          ID3D11Resource*             pResource,
          UINT                        BindFlags,
          DXGI_FORMAT                 Format);
  
  /**
   * \brief Increments private reference count of a resource
   * 
   * Helper method that figures out the exact type of
   * the resource and calls its \c AddRefPrivate method.
   * \param [in] pResource The resource to reference
   * \returns \c S_OK, or \c E_INVALIDARG for an invalid resource
   */
  HRESULT ResourceAddRefPrivate(
          ID3D11Resource*             pResource);
  
  /**
   * \brief Decrements private reference count of a resource
   * 
   * Helper method that figures out the exact type of
   * the resource and calls its \c ReleasePrivate method.
   * \param [in] pResource The resource to reference
   * \returns \c S_OK, or \c E_INVALIDARG for an invalid resource
   */
  HRESULT ResourceReleasePrivate(
          ID3D11Resource*             pResource);

}