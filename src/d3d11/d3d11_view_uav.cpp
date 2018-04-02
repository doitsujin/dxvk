#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_texture.h"
#include "d3d11_view_uav.h"

namespace dxvk {
  
  D3D11UnorderedAccessView::D3D11UnorderedAccessView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc,
    const Rc<DxvkBufferView>&               bufferView,
    const DxvkBufferSlice&                  counterSlice)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_bufferView(bufferView),
    m_counterSlice(counterSlice) { }
  
  
  D3D11UnorderedAccessView::D3D11UnorderedAccessView(
          D3D11Device*                      device,
          ID3D11Resource*                   resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc,
    const Rc<DxvkImageView>&                imageView,
    const DxvkBufferSlice&                  counterSlice)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_imageView(imageView),
    m_counterSlice(counterSlice) { }
  
  
  D3D11UnorderedAccessView::~D3D11UnorderedAccessView() {
    if (m_counterSlice.defined())
      m_device->FreeCounterSlice(m_counterSlice);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11UnorderedAccessView::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11UnorderedAccessView)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11UnorderedAccessView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = m_resource.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11UnorderedAccessView::GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11UnorderedAccessView::GetDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        D3D11_BUFFER_DESC bufferDesc;
        static_cast<D3D11Buffer*>(pResource)->GetDesc(&bufferDesc);
        
        if (bufferDesc.MiscFlags == D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
          pDesc->Format              = DXGI_FORMAT_UNKNOWN;
          pDesc->ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
          pDesc->Buffer.FirstElement = 0;
          pDesc->Buffer.NumElements  = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;
          return S_OK;
        }
      } return E_INVALIDARG;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MipSlice        = 0;
          pDesc->Texture1DArray.FirstArraySlice = 0;
          pDesc->Texture1DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
          pDesc->Texture2D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
          pDesc->Texture2DArray.MipSlice        = 0;
          pDesc->Texture2DArray.FirstArraySlice = 0;
          pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format        = resourceDesc.Format;
        pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        pDesc->Texture3D.MipSlice = 0;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for unordered access view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D11UnorderedAccessView::NormalizeDesc(
          ID3D11Resource*                   pResource,
          D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
          Logger::err("D3D11: Incompatible view dimension for Buffer");
          return E_INVALIDARG;
        }
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE3D) {
          Logger::err("D3D11: Incompatible view dimension for Texture3D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.Depth >> pDesc->Texture3D.MipSlice;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_UAV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.WSize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture3D.WSize = numLayers - pDesc->Texture3D.FirstWSlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
}
