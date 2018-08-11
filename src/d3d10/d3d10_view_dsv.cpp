#include "d3d10_view_dsv.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_view_dsv.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10DepthStencilView::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10DepthStencilView::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10DepthStencilView::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10DepthStencilView::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilView::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilView::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilView::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10DepthStencilView::GetResource(
          ID3D10Resource**          ppResource) {
    GetD3D10ResourceFromView(m_d3d11, ppResource);
  }

  
  void STDMETHODCALLTYPE D3D10DepthStencilView::GetDesc(
          D3D10_DEPTH_STENCIL_VIEW_DESC* pDesc) {
    D3D11_DEPTH_STENCIL_VIEW_DESC d3d11Desc;
    m_d3d11->GetDesc(&d3d11Desc);

    pDesc->ViewDimension = D3D10_DSV_DIMENSION(d3d11Desc.ViewDimension);
    pDesc->Format        = d3d11Desc.Format;

    switch (d3d11Desc.ViewDimension) {
      case D3D11_DSV_DIMENSION_UNKNOWN:
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE1D:
        pDesc->Texture1D.MipSlice               = d3d11Desc.Texture1D.MipSlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        pDesc->Texture1DArray.MipSlice          = d3d11Desc.Texture1DArray.MipSlice;
        pDesc->Texture1DArray.FirstArraySlice   = d3d11Desc.Texture1DArray.FirstArraySlice;
        pDesc->Texture1DArray.ArraySize         = d3d11Desc.Texture1DArray.ArraySize;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2D:
        pDesc->Texture2D.MipSlice               = d3d11Desc.Texture2D.MipSlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        pDesc->Texture2DArray.MipSlice          = d3d11Desc.Texture2DArray.MipSlice;
        pDesc->Texture2DArray.FirstArraySlice   = d3d11Desc.Texture2DArray.FirstArraySlice;
        pDesc->Texture2DArray.ArraySize         = d3d11Desc.Texture2DArray.ArraySize;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DMS:
        break;

      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        pDesc->Texture2DMSArray.FirstArraySlice = d3d11Desc.Texture2DMSArray.FirstArraySlice;
        pDesc->Texture2DMSArray.ArraySize       = d3d11Desc.Texture2DMSArray.ArraySize;
        break;
    }
  }

}