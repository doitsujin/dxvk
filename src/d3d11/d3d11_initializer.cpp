#include <cstring>

#include "d3d11_device.h"
#include "d3d11_initializer.h"

namespace dxvk {

  D3D11Initializer::D3D11Initializer(
          D3D11Device*                pParent)
  : m_parent(pParent),
    m_device(pParent->GetDXVKDevice()),
    m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());
  }

  
  D3D11Initializer::~D3D11Initializer() {

  }


  void D3D11Initializer::Flush() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_transferCommands != 0)
      FlushInternal();
  }

  void D3D11Initializer::InitBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    VkMemoryPropertyFlags memFlags = pBuffer->GetBuffer()->memFlags();

    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleBuffer(pBuffer, pInitialData)
      : InitDeviceLocalBuffer(pBuffer, pInitialData);
  }
  

  void D3D11Initializer::InitTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    (pTexture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
      ? InitHostVisibleTexture(pTexture, pInitialData)
      : InitDeviceLocalTexture(pTexture, pInitialData);
  }


  void D3D11Initializer::InitUavCounter(
          D3D11UnorderedAccessView*   pUav) {
    auto counterBuffer = pUav->GetCounterSlice();

    if (!counterBuffer.defined())
      return;

    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_transferCommands += 1;

    const uint32_t zero = 0;
    m_context->updateBuffer(
      counterBuffer.buffer(),
      0, sizeof(zero), &zero);

    FlushImplicit();
  }


  void D3D11Initializer::InitDeviceLocalBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice();

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      m_transferMemory   += bufferSlice.length();
      m_transferCommands += 1;
      
      m_context->uploadBuffer(
        bufferSlice.buffer(),
        pInitialData->pSysMem);
    } else {
      m_transferCommands += 1;

      m_context->clearBuffer(
        bufferSlice.buffer(),
        bufferSlice.offset(),
        bufferSlice.length(),
        0u);
    }

    FlushImplicit();
  }


  void D3D11Initializer::InitHostVisibleBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice();

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      std::memcpy(
        bufferSlice.mapPtr(0),
        pInitialData->pSysMem,
        bufferSlice.length());
    } else {
      std::memset(
        bufferSlice.mapPtr(0), 0,
        bufferSlice.length());
    }
  }


  void D3D11Initializer::InitDeviceLocalTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    Rc<DxvkImage> image = pTexture->GetImage();

    auto mapMode = pTexture->GetMapMode();
    auto desc = pTexture->Desc();

    VkFormat packedFormat = m_parent->LookupPackedFormat(desc->Format, pTexture->GetFormatMode()).Format;
    auto formatInfo = imageFormatInfo(packedFormat);

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      // pInitialData is an array that stores an entry for
      // every single subresource. Since we will define all
      // subresources, this counts as initialization.
      for (uint32_t layer = 0; layer < desc->ArraySize; layer++) {
        for (uint32_t level = 0; level < desc->MipLevels; level++) {
          const uint32_t id = D3D11CalcSubresource(
            level, layer, desc->MipLevels);

          VkOffset3D mipLevelOffset = { 0, 0, 0 };
          VkExtent3D mipLevelExtent = pTexture->MipLevelExtent(level);

          if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING) {
            m_transferCommands += 1;
            m_transferMemory   += pTexture->GetSubresourceLayout(formatInfo->aspectMask, id).Size;
            
            VkImageSubresourceLayers subresourceLayers;
            subresourceLayers.aspectMask     = formatInfo->aspectMask;
            subresourceLayers.mipLevel       = level;
            subresourceLayers.baseArrayLayer = layer;
            subresourceLayers.layerCount     = 1;
            
            if (formatInfo->aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
              m_context->uploadImage(
                image, subresourceLayers,
                pInitialData[id].pSysMem,
                pInitialData[id].SysMemPitch,
                pInitialData[id].SysMemSlicePitch);
            } else {
              m_context->updateDepthStencilImage(
                image, subresourceLayers,
                VkOffset2D { mipLevelOffset.x,     mipLevelOffset.y      },
                VkExtent2D { mipLevelExtent.width, mipLevelExtent.height },
                pInitialData[id].pSysMem,
                pInitialData[id].SysMemPitch,
                pInitialData[id].SysMemSlicePitch,
                packedFormat);
            }
          }

          if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
            util::packImageData(pTexture->GetMappedBuffer(id)->mapPtr(0),
              pInitialData[id].pSysMem, pInitialData[id].SysMemPitch, pInitialData[id].SysMemSlicePitch,
              pTexture->GetVkImageType(), mipLevelExtent, 1, formatInfo, formatInfo->aspectMask);
          }
        }
      }
    } else {
      if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING) {
        m_transferCommands += 1;
        
        // While the Microsoft docs state that resource contents are
        // undefined if no initial data is provided, some applications
        // expect a resource to be pre-cleared. We can only do that
        // for non-compressed images, but that should be fine.
        VkImageSubresourceRange subresources;
        subresources.aspectMask     = formatInfo->aspectMask;
        subresources.baseMipLevel   = 0;
        subresources.levelCount     = desc->MipLevels;
        subresources.baseArrayLayer = 0;
        subresources.layerCount     = desc->ArraySize;

        if (formatInfo->flags.any(DxvkFormatFlag::BlockCompressed, DxvkFormatFlag::MultiPlane)) {
          m_context->clearCompressedColorImage(image, subresources);
        } else {
          if (subresources.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT) {
            VkClearColorValue value = { };

            m_context->clearColorImage(
              image, value, subresources);
          } else {
            VkClearDepthStencilValue value;
            value.depth   = 0.0f;
            value.stencil = 0;
            
            m_context->clearDepthStencilImage(
              image, value, subresources);
          }
        }
      }

      if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
        for (uint32_t i = 0; i < pTexture->CountSubresources(); i++) {
          auto buffer = pTexture->GetMappedBuffer(i);
          std::memset(buffer->mapPtr(0), 0, buffer->info().size);
        }
      }
    }

    FlushImplicit();
  }


  void D3D11Initializer::InitHostVisibleTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    Rc<DxvkImage> image = pTexture->GetImage();

    for (uint32_t layer = 0; layer < image->info().numLayers; layer++) {
      for (uint32_t level = 0; level < image->info().mipLevels; level++) {
        VkImageSubresource subresource;
        subresource.aspectMask = image->formatInfo()->aspectMask;
        subresource.mipLevel   = level;
        subresource.arrayLayer = layer;

        VkExtent3D blockCount = util::computeBlockCount(
          image->mipLevelExtent(level),
          image->formatInfo()->blockSize);

        VkSubresourceLayout layout = image->querySubresourceLayout(subresource);

        auto initialData = pInitialData
          ? &pInitialData[D3D11CalcSubresource(level, layer, image->info().mipLevels)]
          : nullptr;

        for (uint32_t z = 0; z < blockCount.depth; z++) {
          for (uint32_t y = 0; y < blockCount.height; y++) {
            auto size = blockCount.width * image->formatInfo()->elementSize;
            auto dst = image->mapPtr(layout.offset + y * layout.rowPitch + z * layout.depthPitch);

            if (initialData) {
              auto src = reinterpret_cast<const char*>(initialData->pSysMem)
                       + y * initialData->SysMemPitch
                       + z * initialData->SysMemSlicePitch;
              std::memcpy(dst, src, size);
            } else {
              std::memset(dst, 0, size);
            }
          }
        }
      }
    }

    // Initialize the image on the GPU
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    VkImageSubresourceRange subresources;
    subresources.aspectMask     = image->formatInfo()->aspectMask;
    subresources.baseMipLevel   = 0;
    subresources.levelCount     = image->info().mipLevels;
    subresources.baseArrayLayer = 0;
    subresources.layerCount     = image->info().numLayers;
    
    m_context->initImage(image, subresources, VK_IMAGE_LAYOUT_PREINITIALIZED);

    m_transferCommands += 1;
    FlushImplicit();
  }


  void D3D11Initializer::FlushImplicit() {
    if (m_transferCommands > MaxTransferCommands
     || m_transferMemory   > MaxTransferMemory)
      FlushInternal();
  }


  void D3D11Initializer::FlushInternal() {
    m_context->flushCommandList();
    
    m_transferCommands = 0;
    m_transferMemory   = 0;
  }

}