#include "d3d11_cmdlist.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11ImmediateContext::D3D11ImmediateContext(
    D3D11Device*    pParent,
    Rc<DxvkDevice>  Device)
  : D3D11DeviceContext(pParent, Device),
    m_csThread(Device->createContext()) {
    
  }
  
  
  D3D11ImmediateContext::~D3D11ImmediateContext() {
    Flush();
    SynchronizeCsThread();
    SynchronizeDevice();
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
    
    if (m_csChunk->commandCount() != 0) {
      m_drawCount = 0;
      
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([dev = m_device] (DxvkContext* ctx) {
        dev->submitCommandList(
          ctx->endRecording(),
          nullptr, nullptr);
        
        ctx->beginRecording(
          dev->createCommandList());
      });
      
      FlushCsChunk();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          BOOL                RestoreContextState) {
    static_cast<D3D11CommandList*>(pCommandList)->EmitToCsThread(&m_csThread);
    
    if (RestoreContextState)
      RestoreState();
    else
      ClearState();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::FinishCommandList(
          BOOL                RestoreDeferredContextState,
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
    if (pResource == nullptr) {
      Logger::warn("D3D11ImmediateContext::Map() application tried to map a nullptr resource");
      return DXGI_ERROR_INVALID_CALL;
    }

    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      D3D11Buffer* resource = static_cast<D3D11Buffer*>(pResource);
      Rc<DxvkBuffer> buffer = resource->GetBufferSlice().buffer();
      
      if (!(buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        Logger::err("D3D11: Cannot map a device-local buffer");
        return E_INVALIDARG;
      }
      
      if (pMappedResource == nullptr)
        return S_FALSE;
      
      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        // Allocate a new backing slice for the buffer and set
        // it as the 'new' mapped slice. This assumes that the
        // only way to invalidate a buffer is by mapping it.
        auto physicalSlice = buffer->allocPhysicalSlice();
        physicalSlice.resource()->acquire();
        
        resource->GetBufferInfo()->mappedSlice = physicalSlice;
        
        EmitCs([
          cBuffer        = buffer,
          cPhysicalSlice = physicalSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cBuffer, cPhysicalSlice);
          cPhysicalSlice.resource()->release();
        });
      } else if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
        if (!WaitForResource(buffer->resource(), MapFlags))
          return DXGI_ERROR_WAS_STILL_DRAWING;
      }
      
      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3D11_MAP_WRITE_NO_OVERWRITE.
      const DxvkPhysicalBufferSlice physicalSlice
        = resource->GetBufferInfo()->mappedSlice;
      
      pMappedResource->pData      = physicalSlice.mapPtr(0);
      pMappedResource->RowPitch   = physicalSlice.length();
      pMappedResource->DepthPitch = physicalSlice.length();
      return S_OK;
    } else {
      // Depending on whether the image has been allocated on a
      // host-visible memory type, we can either use the mapped
      // memory region directly, or we map a linear buffer.
      D3D11TextureInfo* textureInfo = GetCommonTextureInfo(pResource);
      const DxvkImageCreateInfo& imageInfo = textureInfo->image->info();
      
      textureInfo->mappedSubresource = GetSubresourceFromIndex(
        VK_IMAGE_ASPECT_COLOR_BIT, imageInfo.mipLevels, Subresource);
      
      if (textureInfo->image->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkSubresourceLayout subresourceLayout =
          textureInfo->image->querySubresourceLayout(
            textureInfo->mappedSubresource);
        
        if (!WaitForResource(textureInfo->image, MapFlags))
          return DXGI_ERROR_WAS_STILL_DRAWING;
        
        pMappedResource->pData      = textureInfo->image->mapPtr(subresourceLayout.offset);
        pMappedResource->RowPitch   = subresourceLayout.rowPitch;
        pMappedResource->DepthPitch = subresourceLayout.rowPitch * imageInfo.extent.height;
        
        if (imageInfo.type == VK_IMAGE_TYPE_3D)
          pMappedResource->DepthPitch = subresourceLayout.depthPitch;
        else if (imageInfo.numLayers > 1)
          pMappedResource->DepthPitch = subresourceLayout.arrayPitch;
        
        return S_OK;
      } else {
        if (textureInfo->imageBuffer == nullptr) {
          Logger::err("D3D11: Cannot map a device-local image");
          return E_INVALIDARG;
        }
        
        if (pMappedResource == nullptr)
          return S_FALSE;
        
        // Query format info in order to compute
        // the row pitch and layer pitch properly.
        const DxvkFormatInfo* formatInfo = imageFormatInfo(imageInfo.format);
        
        const VkExtent3D levelExtent = textureInfo->image
          ->mipLevelExtent(textureInfo->mappedSubresource.mipLevel);
        
        const VkExtent3D blockCount = util::computeBlockCount(
          levelExtent, formatInfo->blockSize);
        
        DxvkPhysicalBufferSlice physicalSlice;
        
        // When using any map mode which requires the image contents
        // to be preserved, copy the image's contents into the buffer.
        if (MapType == D3D11_MAP_WRITE_DISCARD) {
          physicalSlice = textureInfo->imageBuffer->allocPhysicalSlice();
          physicalSlice.resource()->acquire();
          
          EmitCs([
            cImageBuffer   = textureInfo->imageBuffer,
            cPhysicalSlice = physicalSlice
          ] (DxvkContext* ctx) {
            ctx->invalidateBuffer(cImageBuffer, cPhysicalSlice);
            cPhysicalSlice.resource()->release();
          });
        } else {
          // We may have to copy the current image contents into the
          // mapped buffer if the GPU has write access to the image.
          const bool copyExistingData = textureInfo->usage == D3D11_USAGE_STAGING;
          
          if (copyExistingData) {
            const VkImageSubresourceLayers subresourceLayers = {
              textureInfo->mappedSubresource.aspectMask,
              textureInfo->mappedSubresource.mipLevel,
              textureInfo->mappedSubresource.arrayLayer, 1 };
            
            EmitCs([
              cImageBuffer  = textureInfo->imageBuffer,
              cImage        = textureInfo->image,
              cSubresources = subresourceLayers,
              cLevelExtent  = levelExtent
            ] (DxvkContext* ctx) {
              ctx->copyImageToBuffer(
                cImageBuffer, 0, VkExtent2D { 0u, 0u },
                cImage, cSubresources, VkOffset3D { 0, 0, 0 },
                cLevelExtent);
            });
          }
          
          if (!WaitForResource(textureInfo->imageBuffer->resource(), MapFlags))
            return DXGI_ERROR_WAS_STILL_DRAWING;
          
          physicalSlice = textureInfo->imageBuffer->slice();
        }
        
        // Set up map pointer. Data is tightly packed within the mapped buffer.
        pMappedResource->pData      = physicalSlice.mapPtr(0);
        pMappedResource->RowPitch   = formatInfo->elementSize * blockCount.width;
        pMappedResource->DepthPitch = formatInfo->elementSize * blockCount.width * blockCount.height;
        return S_OK;
      }
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
      
      if (!(textureInfo->image->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        const VkExtent3D levelExtent = textureInfo->image
          ->mipLevelExtent(textureInfo->mappedSubresource.mipLevel);
        
        const VkImageSubresourceLayers subresourceLayers = {
          textureInfo->mappedSubresource.aspectMask,
          textureInfo->mappedSubresource.mipLevel,
          textureInfo->mappedSubresource.arrayLayer, 1 };
        
        EmitCs([
          cSrcBuffer      = textureInfo->imageBuffer,
          cDstImage       = textureInfo->image,
          cDstLayers      = subresourceLayers,
          cDstLevelExtent = levelExtent
        ] (DxvkContext* ctx) {
          ctx->copyBufferToImage(cDstImage, cDstLayers,
            VkOffset3D { 0, 0, 0 }, cDstLevelExtent,
            cSrcBuffer, 0, { 0u, 0u });
        });
      }
    }
  }
  
  
  void D3D11ImmediateContext::SynchronizeCsThread() {
    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();
    
    m_csThread.synchronize();
  }
  
  
  void D3D11ImmediateContext::SynchronizeDevice() {
    m_device->waitForIdle();
  }
  
  
  bool D3D11ImmediateContext::WaitForResource(
    const Rc<DxvkResource>&                 Resource,
          UINT                              MapFlags) {
    // Wait for the any pending D3D11 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.
    Flush();
    SynchronizeCsThread();
    
    if (Resource->isInUse()) {
      // TODO implement properly in DxvkDevice
      while (Resource->isInUse())
        std::this_thread::yield();
    }
    
    return true;
  }
  
  
  void D3D11ImmediateContext::EmitCsChunk(Rc<DxvkCsChunk>&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
  }
  
}