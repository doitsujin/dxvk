#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11ImmediateContext::D3D11ImmediateContext(
    D3D11Device*    parent,
    Rc<DxvkDevice>  device)
  : D3D11DeviceContext(parent, device) {
    
  }
  
  
  D3D11ImmediateContext::~D3D11ImmediateContext() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11ImmediateContext::AddRef() {
    return m_parent->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11ImmediateContext::Release() {
    return m_parent->Release();
  }
  
  
  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11ImmediateContext::GetType() {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11ImmediateContext::GetContextFlags() {
    return 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush() {
    m_parent->FlushInitContext();
    m_drawCount = 0;
    
    EmitCs([dev = m_device] (DxvkContext* ctx) {
      dev->submitCommandList(
        ctx->endRecording(),
        nullptr, nullptr);
      
      ctx->beginRecording(
        dev->createCommandList());
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          WINBOOL             RestoreContextState) {
    Logger::err("D3D11ImmediateContext::ExecuteCommandList: Not implemented");
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::FinishCommandList(
          WINBOOL             RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    Logger::err("D3D11: FinishCommandList called on immediate context");
    return DXGI_ERROR_INVALID_CALL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      const D3D11Buffer* resource = static_cast<D3D11Buffer*>(pResource);
      const Rc<DxvkBuffer> buffer = resource->GetBufferSlice().buffer();
      
      if (!(buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        Logger::err("D3D11: Cannot map a device-local buffer");
        return E_INVALIDARG;
      }
      
      if (pMappedResource == nullptr)
        return S_OK;
      
      if (buffer->isInUse()) {
        // Don't wait if the application tells us not to
        if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT)
          return DXGI_ERROR_WAS_STILL_DRAWING;
        
        // Invalidate the buffer in order to avoid synchronization
        // if the application does not need the buffer contents to
        // be preserved. The No Overwrite mode does not require any
        // sort of synchronization, but should be used with care.
        if (MapType == D3D11_MAP_WRITE_DISCARD) {
          m_context->invalidateBuffer(buffer);
        } else if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
          this->Flush();
          this->Synchronize();
        }
      }
      
      pMappedResource->pData      = buffer->mapPtr(0);
      pMappedResource->RowPitch   = buffer->info().size;
      pMappedResource->DepthPitch = buffer->info().size;
      return S_OK;
    } else {
      // Mapping an image is sadly not as simple as mapping a buffer
      // because applications tend to ignore row and layer strides.
      // We use a buffer instead and then perform a copy.
      D3D11TextureInfo* textureInfo
        = GetCommonTextureInfo(pResource);
      
      if (textureInfo->imageBuffer == nullptr) {
        Logger::err("D3D11DeviceContext: Cannot map a device-local image");
        return E_INVALIDARG;
      }
      
      if (pMappedResource == nullptr)
        return S_FALSE;
      
      // Query format and subresource in order to compute
      // the row pitch and layer pitch properly.
      const DxvkImageCreateInfo& imageInfo = textureInfo->image->info();
      const DxvkFormatInfo* formatInfo = imageFormatInfo(imageInfo.format);
      
      textureInfo->mappedSubresource =
        GetSubresourceFromIndex(VK_IMAGE_ASPECT_COLOR_BIT,
          imageInfo.mipLevels, Subresource);
      
      const VkExtent3D levelExtent = textureInfo->image
        ->mipLevelExtent(textureInfo->mappedSubresource.mipLevel);
      
      const VkExtent3D blockCount = {
        levelExtent.width  / formatInfo->blockSize.width,
        levelExtent.height / formatInfo->blockSize.height,
        levelExtent.depth  / formatInfo->blockSize.depth };
      
      // When using any map mode which requires the image contents
      // to be preserved, copy image contents into the buffer.
      if (MapType != D3D11_MAP_WRITE_DISCARD) {
        const VkImageSubresourceLayers subresourceLayers = {
          textureInfo->mappedSubresource.aspectMask,
          textureInfo->mappedSubresource.mipLevel,
          textureInfo->mappedSubresource.arrayLayer, 1 };
        
        m_context->copyImageToBuffer(
          textureInfo->imageBuffer, 0, { 0u, 0u },
          textureInfo->image, subresourceLayers,
          VkOffset3D { 0, 0, 0 }, levelExtent);
      }
      
      if (textureInfo->imageBuffer->isInUse()) {
        if (MapType == D3D11_MAP_WRITE_DISCARD) {
          m_context->invalidateBuffer(textureInfo->imageBuffer);
        } else {
          this->Flush();
          this->Synchronize();
        }
      }
      
      // Set up map pointer. Data is tightly packed within the mapped buffer.
      pMappedResource->pData      = textureInfo->imageBuffer->mapPtr(0);
      pMappedResource->RowPitch   = formatInfo->elementSize * blockCount.width;
      pMappedResource->DepthPitch = formatInfo->elementSize * blockCount.width * blockCount.height;
      return S_OK;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim != D3D11_RESOURCE_DIMENSION_BUFFER) {
      // Now that data has been written into the buffer,
      // we need to copy its contents into the image
      const D3D11TextureInfo* textureInfo
        = GetCommonTextureInfo(pResource);
      
      const VkExtent3D levelExtent = textureInfo->image
        ->mipLevelExtent(textureInfo->mappedSubresource.mipLevel);
      
      const VkImageSubresourceLayers subresourceLayers = {
        textureInfo->mappedSubresource.aspectMask,
        textureInfo->mappedSubresource.mipLevel,
        textureInfo->mappedSubresource.arrayLayer, 1 };
      
      m_context->copyBufferToImage(
        textureInfo->image, subresourceLayers,
        VkOffset3D { 0, 0, 0 }, levelExtent,
        textureInfo->imageBuffer, 0, { 0u, 0u });
    }
  }
  
  
  void D3D11ImmediateContext::Synchronize() {
    // TODO sync with CS thread
    m_device->waitForIdle();
  }
  
}