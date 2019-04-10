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
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_transferCommands != 0)
      FlushInternal();
  }

  void D3D9Initializer::InitBuffer(
          Direct3DCommonBuffer9*  pBuffer) {
    VkMemoryPropertyFlags memFlags = pBuffer->GetBuffer(D3D9_COMMON_BUFFER_TYPE_REAL)->memFlags();

    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleBuffer(pBuffer)
      : InitDeviceLocalBuffer(pBuffer);
  }
  

  void D3D9Initializer::InitTexture(
          Direct3DCommonTexture9* pTexture) {
    if (pTexture->GetImage() == nullptr)
      return;

    VkMemoryPropertyFlags memFlags = pTexture->GetImage()->memFlags();
    
    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleTexture(pTexture)
      : InitDeviceLocalTexture(pTexture);
  }


  void D3D9Initializer::InitDeviceLocalBuffer(
          Direct3DCommonBuffer9*  pBuffer) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL);

    m_transferCommands += 1;

    m_context->clearBuffer(
      bufferSlice.buffer(),
      bufferSlice.offset(),
      bufferSlice.length(),
      0u);

    FlushImplicit();
  }


  void D3D9Initializer::InitHostVisibleBuffer(
          Direct3DCommonBuffer9*  pBuffer) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL);

    std::memset(
      bufferSlice.mapPtr(0), 0,
      bufferSlice.length());
  }


  void D3D9Initializer::InitDeviceLocalTexture(
          Direct3DCommonTexture9* pTexture) {
    std::lock_guard<std::mutex> lock(m_mutex);

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
          value.depth   = 1.0f;
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
          Direct3DCommonTexture9* pTexture) {
    // TODO implement properly with memset/memcpy
    InitDeviceLocalTexture(pTexture);
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