#include <cstring>

#include "d3d9_initializer.h"

namespace dxvk {

  D3D9Initializer::D3D9Initializer(
    const Rc<DxvkDevice>&             Device)
  : m_device(Device), m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());
  }

  
  D3D9Initializer::~D3D9Initializer() {

  }


  void D3D9Initializer::Flush() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_transferCommands != 0)
      FlushInternal();
  }


  void D3D9Initializer::InitBuffer(
          D3D9CommonBuffer*  pBuffer) {
    VkMemoryPropertyFlags memFlags = pBuffer->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>()->memFlags();

    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>())
      : InitDeviceLocalBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>());

    if (pBuffer->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      InitHostVisibleBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_STAGING>());
  }
  

  void D3D9Initializer::InitTexture(
          D3D9CommonTexture* pTexture,
          void*              pInitialData) {    
    if (pTexture->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_NONE)
      return;

    (pTexture->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED)
      ? InitDeviceLocalTexture(pTexture)
      : InitHostVisibleTexture(pTexture, pInitialData);
  }


  void D3D9Initializer::InitDeviceLocalBuffer(
          DxvkBufferSlice    Slice) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_transferCommands += 1;

    m_context->clearBuffer(
      Slice.buffer(),
      Slice.offset(),
      Slice.length(),
      0u);

    FlushImplicit();
  }


  void D3D9Initializer::InitHostVisibleBuffer(
          DxvkBufferSlice    Slice) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    std::memset(
      Slice.mapPtr(0), 0,
      Slice.length());
  }


  void D3D9Initializer::InitDeviceLocalTexture(
          D3D9CommonTexture* pTexture) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto InitImage = [&](Rc<DxvkImage> image) {
      if (image == nullptr)
        return;

      auto formatInfo = imageFormatInfo(image->info().format);

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
    };

    InitImage(pTexture->GetImage());

    FlushImplicit();
  }


  void D3D9Initializer::InitHostVisibleTexture(
          D3D9CommonTexture* pTexture,
          void*              pInitialData) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    for (uint32_t i = 0; i < pTexture->CountSubresources(); i++) {
      DxvkBufferSliceHandle mapSlice  = pTexture->GetBuffer(i)->getSliceHandle();

      if (pInitialData != nullptr) {
        std::memcpy(
          mapSlice.mapPtr,
          pInitialData,
          mapSlice.length);
      } else {
        std::memset(
          mapSlice.mapPtr, 0,
          mapSlice.length);
      }
    }
  }


  void D3D9Initializer::FlushImplicit() {
    if (m_transferCommands > MaxTransferCommands
     || m_transferMemory   > MaxTransferMemory)
      FlushInternal();
  }


  void D3D9Initializer::FlushInternal() {
    m_context->flushCommandList();
    
    m_transferCommands = 0;
    m_transferMemory   = 0;
  }

}