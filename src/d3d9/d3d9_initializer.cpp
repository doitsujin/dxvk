#include <cstring>

#include "d3d9_initializer.h"
#include "d3d9_device.h"

namespace dxvk {

  D3D9Initializer::D3D9Initializer(
    D3D9DeviceEx*             pParent)
  : m_parent(pParent),
    m_device(pParent->GetDXVKDevice()),
    m_csChunk(m_parent->AllocCsChunk()) {

  }


  D3D9Initializer::~D3D9Initializer() {

  }


  void D3D9Initializer::NotifyContextFlush() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_transferCommands = 0;
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

    void* mapPtr = nullptr;

    if (pTexture->Desc()->Pool != D3DPOOL_DEFAULT) {
      mapPtr = pTexture->GetData(0);
      if (mapPtr == nullptr)
        throw DxvkError("D3D9: InitTexture: map failed");
    }

    if (pTexture->GetImage() != nullptr)
      InitDeviceLocalTexture(pTexture);

    if (mapPtr != nullptr) {
      InitHostVisibleTexture(pTexture, pInitialData, mapPtr);
      pTexture->UnmapData();
    }

    SyncSharedTexture(pTexture);
  }


  void D3D9Initializer::InitDeviceLocalBuffer(
          DxvkBufferSlice    Slice) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_transferCommands += 1;

    EmitCs([
      cBuffer = Slice.buffer()
    ] (DxvkContext* ctx) {
      ctx->initBuffer(
        cBuffer);
    });

    ThrottleAllocationLocked();
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

    Rc<DxvkImage> image = pTexture->GetImage();

    EmitCs([
      cImage = std::move(image)
    ] (DxvkContext* ctx) {
      ctx->initImage(cImage, VK_IMAGE_LAYOUT_UNDEFINED);
    });

    ThrottleAllocationLocked();
  }


  void D3D9Initializer::InitHostVisibleTexture(
          D3D9CommonTexture* pTexture,
          void*              pInitialData,
          void*              mapPtr) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    if (pInitialData) {
      // Initial data is only supported for textures with 1 subresource
      VkExtent3D mipExtent = pTexture->GetExtentMip(0);
      const DxvkFormatInfo* formatInfo = lookupFormatInfo(pTexture->GetFormatMapping().FormatColor);
      VkExtent3D blockCount = util::computeBlockCount(mipExtent, formatInfo->blockSize);
      uint32_t pitch = blockCount.width * formatInfo->elementSize;
      uint32_t alignedPitch = align(pitch, 4);

      util::packImageData(
        mapPtr,
        pInitialData,
        pitch,
        pitch * blockCount.height,
        alignedPitch,
        alignedPitch * blockCount.height,
        D3D9CommonTexture::GetImageTypeFromResourceType(pTexture->GetType()),
        mipExtent,
        pTexture->Desc()->ArraySize,
        formatInfo,
        VK_IMAGE_ASPECT_COLOR_BIT);
    } else {
      // All subresources are allocated in one chunk of memory.
      // So we can just get the pointer for subresource 0 and memset all of them at once.
      std::memset(
        mapPtr, 0,
        pTexture->GetTotalSize());
    }
  }


  void D3D9Initializer::ThrottleAllocationLocked() {
    if (m_transferCommands > MaxTransferCommands)
      ExecuteFlushLocked();
  }


  void D3D9Initializer::ExecuteFlush() {
    std::lock_guard lock(m_mutex);

    ExecuteFlushLocked();
  }


  void D3D9Initializer::ExecuteFlushLocked() {
    EmitCs([] (DxvkContext* ctx) {
      ctx->flushCommandList(nullptr, nullptr);
    });

    FlushCsChunk();

    m_transferCommands = 0;
  }


  void D3D9Initializer::SyncSharedTexture(D3D9CommonTexture* pResource) {
    if (pResource->GetImage() == nullptr || pResource->GetImage()->info().sharing.mode == DxvkSharedHandleMode::None)
      return;

    // Ensure that initialization commands are submitted and waited on before
    // returning control to the application in order to avoid race conditions
    // in case the texture is used immediately on a secondary device.
    ExecuteFlush();

    m_device->waitForResource(*pResource->GetImage(), DxvkAccess::Write);
  }


  void D3D9Initializer::FlushCsChunkLocked() {
    m_parent->InjectCsChunk(std::move(m_csChunk), false);
    m_csChunk = m_parent->AllocCsChunk();
  }

}
