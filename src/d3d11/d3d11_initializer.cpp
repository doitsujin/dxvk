#include <cstring>

#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_initializer.h"

namespace dxvk {

  D3D11Initializer::D3D11Initializer(
          D3D11Device*                pParent)
  : m_parent(pParent),
    m_device(pParent->GetDXVKDevice()),
    m_memorySignal(new sync::Fence(0)),
    m_csChunk(m_parent->AllocCsChunk(DxvkCsChunkFlag::SingleUse)) {

  }

  
  D3D11Initializer::~D3D11Initializer() {

  }


  void D3D11Initializer::NotifyContextFlush() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    NotifyContextFlushLocked();
  }


  void D3D11Initializer::InitBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    if (!(pBuffer->Desc()->MiscFlags & D3D11_RESOURCE_MISC_TILED)) {
      VkMemoryPropertyFlags memFlags = pBuffer->GetBuffer()->memFlags();

      (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        ? InitHostVisibleBuffer(pBuffer, pInitialData)
        : InitDeviceLocalBuffer(pBuffer, pInitialData);
    }
  }
  

  void D3D11Initializer::InitTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    if (pTexture->Desc()->MiscFlags & D3D11_RESOURCE_MISC_TILED)
      InitTiledTexture(pTexture);
    else if (pTexture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
      InitHostVisibleTexture(pTexture, pInitialData);
    else
      InitDeviceLocalTexture(pTexture, pInitialData);

    SyncSharedTexture(pTexture);
  }


  void D3D11Initializer::InitUavCounter(
          D3D11UnorderedAccessView*   pUav) {
    auto counterView = pUav->GetCounterView();

    if (!counterView)
      return;

    EmitCs(0u, [
      cCounterSlice = DxvkBufferSlice(counterView)
    ] (DxvkContext* ctx) {
      static const uint32_t zero = 0;

      ctx->updateBuffer(
        cCounterSlice.buffer(),
        cCounterSlice.offset(),
        sizeof(zero), &zero);
    });
  }


  void D3D11Initializer::InitRtvImage(
          D3D11RenderTargetView*      pRtv) {
    // Buffer RTVs own their associated image,
    // so we need to initialize it properly
    if (!pRtv->GetBufferView())
      return;

    EmitCs(0u, [
      cImageView = pRtv->GetImageView()
    ] (DxvkContext* ctx) {
      ctx->initImage(cImageView->image(), VK_IMAGE_LAYOUT_UNDEFINED);
    });
  }


  void D3D11Initializer::InitShaderIcb(
          D3D11CommonShader*          pShader,
          size_t                      IcbSize,
    const void*                       pIcbData) {
    auto icbSlice = pShader->GetIcb();
    auto srcSlice = AllocStagingBuffer(icbSlice.length());

    std::memcpy(srcSlice.mapPtr(0), pIcbData, IcbSize);

    if (IcbSize < icbSlice.length())
      std::memset(srcSlice.mapPtr(IcbSize), 0, icbSlice.length() - IcbSize);

    auto stagingSize = icbSlice.length();

    EmitCs(stagingSize, [
      cIcbSlice = std::move(icbSlice),
      cSrcSlice = std::move(srcSlice)
    ] (DxvkContext* ctx) {
      ctx->copyBuffer(cIcbSlice.buffer(), cIcbSlice.offset(),
        cSrcSlice.buffer(), cSrcSlice.offset(), cIcbSlice.length());
    });
  }


  void D3D11Initializer::InitDeviceLocalBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    Rc<DxvkBuffer> buffer = pBuffer->GetBuffer();

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      auto stagingSlice = AllocStagingBuffer(buffer->info().size);
      auto stagingSize = stagingSlice.length();

      std::memcpy(stagingSlice.mapPtr(0), pInitialData->pSysMem, stagingSlice.length());

      EmitCs(stagingSize, [
        cBuffer       = std::move(buffer),
        cStagingSlice = std::move(stagingSlice)
      ] (DxvkContext* ctx) {
        ctx->uploadBuffer(cBuffer, 0u,
          cStagingSlice.buffer(),
          cStagingSlice.offset(),
          cStagingSlice.length());
      });
    } else {
      EmitCs(0u, [
        cBuffer = std::move(buffer)
      ] (DxvkContext* ctx) {
        ctx->initBuffer(cBuffer);
      });
    }
  }


  void D3D11Initializer::InitHostVisibleBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    if (pInitialData && pInitialData->pSysMem)
      std::memcpy(pBuffer->GetMapPtr(), pInitialData->pSysMem, pBuffer->Desc()->ByteWidth);
    else
      std::memset(pBuffer->GetMapPtr(), 0, pBuffer->Desc()->ByteWidth);
  }


  void D3D11Initializer::InitDeviceLocalTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    // Image migt be null if this is a staging resource
    Rc<DxvkImage> image = pTexture->GetImage();
    auto desc = pTexture->Desc();

    VkFormat packedFormat = m_parent->LookupPackedFormat(desc->Format, pTexture->GetFormatMode()).Format;
    auto formatInfo = lookupFormatInfo(packedFormat);

    if (pInitialData && pInitialData->pSysMem) {
      // Compute data size for all subresources and allocate staging buffer memory
      DxvkBufferSlice stagingSlice;

      if (pTexture->HasImage()) {
        VkDeviceSize dataSize = 0u;

        for (uint32_t mip = 0; mip < image->info().mipLevels; mip++) {
          dataSize += image->info().numLayers * align(util::computeImageDataSize(
            packedFormat, image->mipLevelExtent(mip), formatInfo->aspectMask), CACHE_LINE_SIZE);
        }

        stagingSlice = AllocStagingBuffer(dataSize);
      }

      // Copy initial data for each subresource into the staging buffer,
      // as well as the mapped per-subresource buffers if available.
      VkDeviceSize dataOffset = 0u;

      for (uint32_t mip = 0; mip < desc->MipLevels; mip++) {
        for (uint32_t layer = 0; layer < desc->ArraySize; layer++) {
          uint32_t index = D3D11CalcSubresource(mip, layer, desc->MipLevels);
          VkExtent3D mipLevelExtent = pTexture->MipLevelExtent(mip);

          if (pTexture->HasImage()) {
            VkDeviceSize mipSizePerLayer = util::computeImageDataSize(
              packedFormat, image->mipLevelExtent(mip), formatInfo->aspectMask);

            util::packImageData(stagingSlice.mapPtr(dataOffset),
              pInitialData[index].pSysMem, pInitialData[index].SysMemPitch, pInitialData[index].SysMemSlicePitch,
              0, 0, pTexture->GetVkImageType(), mipLevelExtent, 1, formatInfo, formatInfo->aspectMask);

            dataOffset += align(mipSizePerLayer, CACHE_LINE_SIZE);
          }

          if (pTexture->HasPersistentBuffers()) {
            util::packImageData(pTexture->GetMapPtr(index, 0),
              pInitialData[index].pSysMem, pInitialData[index].SysMemPitch, pInitialData[index].SysMemSlicePitch,
              0, 0, pTexture->GetVkImageType(), mipLevelExtent, 1, formatInfo, formatInfo->aspectMask);
          }
        }
      }

      // Upload all subresources of the image in one go
      if (pTexture->HasImage()) {
        auto stagingSize = stagingSlice.length();

        EmitCs(stagingSize, [
          cImage        = std::move(image),
          cStagingSlice = std::move(stagingSlice),
          cFormat       = packedFormat
        ] (DxvkContext* ctx) {
          ctx->uploadImage(cImage,
            cStagingSlice.buffer(),
            cStagingSlice.offset(),
            CACHE_LINE_SIZE, cFormat);
        });
      }
    } else {
      if (pTexture->HasImage()) {
        // While the Microsoft docs state that resource contents are
        // undefined if no initial data is provided, some applications
        // expect a resource to be pre-cleared.
        EmitCs(0u, [
          cImage = std::move(image)
        ] (DxvkContext* ctx) {
          ctx->initImage(cImage, VK_IMAGE_LAYOUT_UNDEFINED);
        });
      }

      if (pTexture->HasPersistentBuffers()) {
        for (uint32_t i = 0; i < pTexture->CountSubresources(); i++) {
          auto layout = pTexture->GetSubresourceLayout(formatInfo->aspectMask, i);
          std::memset(pTexture->GetMapPtr(i, layout.Offset), 0, layout.Size);
        }
      }
    }
  }


  void D3D11Initializer::InitHostVisibleTexture(
          D3D11CommonTexture*         pTexture,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    Rc<DxvkImage> image = pTexture->GetImage();
    auto formatInfo = image->formatInfo();

    for (uint32_t layer = 0; layer < pTexture->Desc()->ArraySize; layer++) {
      for (uint32_t level = 0; level < pTexture->Desc()->MipLevels; level++) {
        uint32_t subresourceIndex = D3D11CalcSubresource(level, layer, pTexture->Desc()->MipLevels);

        VkImageSubresource subresource;
        subresource.aspectMask = formatInfo->aspectMask;
        subresource.mipLevel   = level;
        subresource.arrayLayer = layer;

        VkExtent3D blockCount = util::computeBlockCount(
          image->mipLevelExtent(level), formatInfo->blockSize);

        auto layout = pTexture->GetSubresourceLayout(
          subresource.aspectMask, subresourceIndex);

        if (pInitialData && pInitialData[subresourceIndex].pSysMem) {
          const auto& initialData = pInitialData[subresourceIndex];

          for (uint32_t z = 0; z < blockCount.depth; z++) {
            for (uint32_t y = 0; y < blockCount.height; y++) {
              auto size = blockCount.width * formatInfo->elementSize;

              auto dst = pTexture->GetMapPtr(subresourceIndex, layout.Offset
                      + y * layout.RowPitch
                      + z * layout.DepthPitch);

              auto src = reinterpret_cast<const char*>(initialData.pSysMem)
                      + y * initialData.SysMemPitch
                      + z * initialData.SysMemSlicePitch;

              std::memcpy(dst, src, size);

              if (size < layout.RowPitch)
                std::memset(reinterpret_cast<char*>(dst) + size, 0, layout.RowPitch - size);
            }
          }
        } else {
          void* dst = pTexture->GetMapPtr(subresourceIndex, layout.Offset);
          std::memset(dst, 0, layout.Size);
        }
      }
    }

    // Initialize the image on the GPU
    EmitCs(0u, [
      cImage = std::move(image)
    ] (DxvkContext* ctx) {
      ctx->initImage(cImage, VK_IMAGE_LAYOUT_PREINITIALIZED);
    });
  }


  void D3D11Initializer::InitTiledTexture(
          D3D11CommonTexture*         pTexture) {
    EmitCs(0u, [
      cImage = pTexture->GetImage()
    ] (DxvkContext* ctx) {
      ctx->initSparseImage(cImage);
    });
  }


  void D3D11Initializer::ExecuteFlush() {
    std::lock_guard lock(m_mutex);
    ExecuteFlushLocked();
  }


  void D3D11Initializer::ExecuteFlushLocked() {
    EmitCsLocked([] (DxvkContext* ctx) {
      ctx->flushCommandList(nullptr, 0u);
    });

    FlushCsChunkLocked();
    NotifyContextFlushLocked();
  }


  void D3D11Initializer::SyncSharedTexture(D3D11CommonTexture* pResource) {
    if (!(pResource->Desc()->MiscFlags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)))
      return;

    // Ensure that initialization commands are submitted and waited on before
    // returning control to the application in order to avoid race conditions
    // in case the texture is used immediately on a secondary device.
    if (pResource->HasImage()) {
      ExecuteFlush();

      m_device->waitForResource(*pResource->GetImage(), DxvkAccess::Write);
    }

    // If a keyed mutex is used, initialize that to the correct state as well.
    Com<IDXGIKeyedMutex> keyedMutex;

    if (SUCCEEDED(pResource->GetInterface()->QueryInterface(
        __uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&keyedMutex)))) {
      keyedMutex->AcquireSync(0, 0);
      keyedMutex->ReleaseSync(0);
    }
  }


  DxvkBufferSlice D3D11Initializer::AllocStagingBuffer(VkDeviceSize Size) {
    if (unlikely(!Size))
      return DxvkBufferSlice();

    VkDeviceSize alignedSize = dxvk::align(Size, StagingBufferAlignment);
    VkDeviceSize maxPending = std::max<VkDeviceSize>(alignedSize, MaxMemoryInFlight);

    { std::unique_lock lock(m_mutex);

      // Serialize allocation requests so that we don't end up starving large
      // allocations in case we have to throttle. Usually this will not wait.
      uint64_t ticket = ++m_ticketNext;

      m_ticketCond.wait(lock, [&] () {
        return m_ticketDone + 1u == ticket;
      });

      while (true) {
        // Flush pending commands to guarantee forward progress
        if (m_memoryRecorded - m_memorySignaled + alignedSize > MaxMemoryPerSubmission)
          ExecuteFlushLocked();

        // If necessary, wait for GPU to consume and release memory so that we
        // remain below the allocation threshold. Does not account for memory
        // fragmentation, but that should be fine.
        if (m_memoryRecorded - m_memorySignal->value() + alignedSize <= maxPending)
          break;

        // Unlock the initializer here so that we don't stall the immediate context.
        // Based on the above, we know that m_memoryRecorded + alignedSize > maxPending.
        uint64_t targetValue = m_memoryRecorded + alignedSize - maxPending;

        lock.unlock();
        m_memorySignal->wait(targetValue);
        lock.lock();
      }

      m_ticketDone = ticket;
      m_ticketCond.notify_all();
    }

    // Create temporary buffer. We can't really use the "normal" staging
    // buffer path here because memory is consumed and release out of order.
    DxvkBufferCreateInfo info;
    info.size   = Size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_SHADER_READ_BIT;
    info.debugName = "Staging buffer";

    return DxvkBufferSlice(m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
  }


  void D3D11Initializer::FlushCsChunkLocked() {
    m_parent->GetContext()->InjectCsChunk(DxvkCsQueue::HighPriority, std::move(m_csChunk), false);
    m_csChunk = m_parent->AllocCsChunk(DxvkCsChunkFlag::SingleUse);
  }


  void D3D11Initializer::NotifyContextFlushLocked() {
    m_csCommands = 0u;
    m_memorySignaled = m_memoryRecorded;
  }

}
