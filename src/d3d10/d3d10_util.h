#pragma once

#include "d3d10_include.h"

namespace dxvk {

  /**
   * \brief Converts misc. resource flags
   * 
   * Converts the D3D11 misc. resource flags to
   * their D3D10 equivalents and vice versa.
   * \param [in] MiscFlags Original bit mask
   * \returns Converted bit mask
   */
  UINT ConvertD3D10ResourceFlags(UINT MiscFlags);
  UINT ConvertD3D11ResourceFlags(UINT MiscFlags);

  /**
   * \brief Retrieves D3D10 resource from D3D11 view
   * 
   * \param [in] pSrcView The D3D11 resource view
   * \param [out] ppDstResource The D3D10 resource
   */
  void GetD3D10ResourceFromView(
          ID3D11View*           pSrcView,
          ID3D10Resource**      ppDstResource);

  /**
   * \brief Retrieves D3D11 resource from D3D10 view
   * 
   * \param [in] pSrcView The D3D10 resource view
   * \param [out] ppDstResource The D3D11 resource
   */
  void GetD3D11ResourceFromView(
          ID3D10View*           pSrcView,
          ID3D11Resource**      ppDstResource);

  /**
   * \brief Retrieves D3D10 resource from D3D11 resource
   * 
   * \param [in] pSrcResource The D3D11 resource
   * \param [out] ppDstResource The D3D10 resource
   */
  void GetD3D10Resource(
          ID3D11Resource*       pSrcResource,
          ID3D10Resource**      ppDstResource);

  /**
   * \brief Retrieves D3D11 resource from D3D10 resource
   * 
   * \param [in] pSrcResource The D3D10 resource
   * \param [out] ppDstResource The D3D11 resource
   */
  void GetD3D11Resource(
          ID3D10Resource*       pSrcResource,
          ID3D11Resource**      ppDstResource);

  /**
   * \brief Retrieves D3D10 device from D3D11 object
   * 
   * \param [in] pObject The D3D11 device child
   * \param [out] ppDevice The D3D10 device pointer
   */
  void GetD3D10Device(
          ID3D11DeviceChild*    pObject,
          ID3D10Device**        ppDevice);

  /**
   * \brief Retrieves D3D11 device from D3D11 object
   * 
   * \param [in] pObject The D3D11 device child
   * \param [out] ppDevice The D3D11 device pointer
   */
  void GetD3D11Device(
          ID3D11DeviceChild*    pObject,
          ID3D11Device**        ppDevice);
  
  /**
   * \brief Retrieves D3D11 context from D3D11 object
   * 
   * \param [in] pObject The D3D11 device child
   * \param [out] ppContext The D3D11 immediate context
   */
  void GetD3D11Context(
          ID3D11DeviceChild*    pObject,
          ID3D11DeviceContext** ppContext);

}