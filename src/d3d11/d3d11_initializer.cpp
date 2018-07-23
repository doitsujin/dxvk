#include <cstring>

#include "d3d11_initializer.h"

namespace dxvk {

  D3D11Initializer::D3D11Initializer(
    const Rc<DxvkDevice>&             Device)
  : m_device(Device), m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());
  }

  
  D3D11Initializer::~D3D11Initializer() {

  }


  void D3D11Initializer::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

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
    VkMemoryPropertyFlags memFlags = pTexture->GetImage()->memFlags();
    
    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleTexture(pTexture, pInitialData)
      : InitDeviceLocalTexture(pTexture, pInitialData);
  }


  void D3D11Initializer::InitDeviceLocalBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice();

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      m_transferMemory   += bufferSlice.length();
      m_transferCommands += 1;
      
      m_context->updateBuffer(
        bufferSlice.buffer(),
        bufferSlice.offset(),
        bufferSlice.length(),
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
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Rc<DxvkImage> image = pTexture->GetImage();

    auto formatInfo = imageFormatInfo(image->info().format);

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      // pInitialData is an array that stores an entry for
      // every single subresource. Since we will define all
      // subresources, this counts as initialization.
      VkImageSubresourceLayers subresourceLayers;
      subresourceLayers.aspectMask     = formatInfo->aspectMask;
      subresourceLayers.mipLevel       = 0;
      subresourceLayers.baseArrayLayer = 0;
      subresourceLayers.layerCount     = 1;
      
      for (uint32_t layer = 0; layer < image->info().numLayers; layer++) {
        for (uint32_t level = 0; level < image->info().mipLevels; level++) {
          subresourceLayers.baseArrayLayer = layer;
          subresourceLayers.mipLevel       = level;
          
          const uint32_t id = D3D11CalcSubresource(
            level, layer, image->info().mipLevels);
          
          VkOffset3D mipLevelOffset = { 0, 0, 0 };
          VkExtent3D mipLevelExtent = image->mipLevelExtent(level);

          m_transferCommands += 1;
          m_transferMemory   += util::computeImageDataSize(
            image->info().format, mipLevelExtent);
          
          m_context->updateImage(
            image, subresourceLayers,
            mipLevelOffset,
            mipLevelExtent,
            pInitialData[id].pSysMem,
            pInitialData[id].SysMemPitch,
            pInitialData[id].SysMemSlicePitch);
        }
      }
    } else {
      m_transferCommands += 1;
      
      // While the Microsoft docs state that resource contents are
      // undefined if no initial data is provided, some applications
      // expect a resource to be pre-cleared. We can only do that
      // for non-compressed images, but that should be fine.
      VkImageSubresourceRange subresources;
      subresources.aspectMask     = formatInfo->aspectMask;
      subresources.baseMipLevel   = 0;
      subresources.levelCount     = image->info().mipLevels;
      subresources.baseArrayLayer = 0;
      subresources.layerCount     = image->info().numLayers;

      if (formatInfo->flags.test(DxvkFormatFlag::BlockCompressed)) {
        m_context->initImage(image, subresources);
      } else {
        if (subresources.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT) {
          VkClearColorValue value = { };

          m_context->clearColorImage(
            image, value, subresources);
        } else {
          VkClearDepthStencilValue value;
          value.depth   = 1.0f;
          value.stencil = 0;
          
          m_context->clearDepthStencilImage(
            image, value, subresources);
        }
      }
    }

    FlushImplicit();
  }


  void D3D11Initializer::InitHostVisibleTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    // TODO implement properly with memset/memcpy
    InitDeviceLocalTexture(pTexture, pInitialData);
  }


  void D3D11Initializer::FlushImplicit() {
    if (m_transferCommands > MaxTransferCommands
     || m_transferMemory   > MaxTransferMemory)
      FlushInternal();
  }


  void D3D11Initializer::FlushInternal() {
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);

    m_context->beginRecording(
      m_device->createCommandList());
    
    m_transferCommands = 0;
    m_transferMemory   = 0;
  }

}