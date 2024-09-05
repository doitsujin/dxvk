#include <cstring>

#include "d3d11_device.h"
#include "d3d11_texture.h"
#include "d3d11_initializer.h"

namespace dxvk {

  D3D11Initializer::D3D11Initializer(
          D3D11Device*                pParent)
  : m_parent (pParent),
    m_device (pParent->GetDXVKDevice()),
    m_staging(pParent->GetDXVKDevice(), StagingBufferSize),
    m_chunk (AllocCsChunk()) {
  }


  D3D11Initializer::~D3D11Initializer() {

  }


  void D3D11Initializer::EmitToCsThread(const D3D11InitChunkDispatchProc& DispatchProc) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_chunks.push_back(std::move(m_chunk));
    m_chunk = AllocCsChunk();

    for (size_t i = 0; i < m_chunks.size(); i++) {
      DispatchProc(std::move(m_chunks[i]));
    }

    for (auto texture : m_texturesUpdateSeqNum) {
      const D3D11_COMMON_TEXTURE_DESC* desc = texture->Desc();

      for (uint32_t layer = 0; layer < desc->ArraySize; layer++) {
        for (uint32_t level = 0; level < desc->MipLevels; level++) {
          const uint32_t id = D3D11CalcSubresource(
            level, layer, desc->MipLevels);

          texture->TrackSequenceNumber(id, DxvkCsThread::SynchronizeAll);
        }
      }
    }
    m_texturesUpdateSeqNum.clear();

    m_chunks.clear();
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

    SyncKeyedMutex(pTexture->GetInterface());
  }


  void D3D11Initializer::InitUavCounter(
          D3D11UnorderedAccessView*   pUav) {
    auto counterView = pUav->GetCounterView();

    if (counterView == nullptr)
      return;

    auto counterSlice = counterView->slice();

    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_transferCommands += 1;

    EmitCs([
      cCounterSlice = std::move(counterSlice)
    ](DxvkContext* context) {
      const uint32_t zero = 0;
      context->updateBuffer(
        cCounterSlice.buffer(),
        cCounterSlice.offset(),
        sizeof(zero), &zero, true);
    });
  }


  void D3D11Initializer::InitDeviceLocalBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkBufferSlice bufferSlice = pBuffer->GetBufferSlice();

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      m_transferMemory   += bufferSlice.length();
      m_transferCommands += 1;

      DxvkBufferSlice srcSlice = m_staging.alloc(CACHE_LINE_SIZE, bufferSlice.length());
      std::memcpy(srcSlice.mapPtr(0), pInitialData->pSysMem, bufferSlice.length());

      EmitCs([
        cBuffer   = bufferSlice.buffer(),
        cSrcSlice = std::move(srcSlice)
      ](DxvkContext* context) {
        context->uploadBufferFromBuffer(
          cBuffer, cSrcSlice.buffer(), cSrcSlice.offset()
        );
      });
    } else {
      m_transferCommands += 1;

      EmitCs([
        cBuffer = bufferSlice.buffer()
      ](DxvkContext* context) {
        context->initBuffer(cBuffer);
      });
    }
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
    auto formatInfo = lookupFormatInfo(packedFormat);

    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      // pInitialData is an array that stores an entry for
      // every single subresource. Since we will define all
      // subresources, this counts as initialization.
      for (uint32_t layer = 0; layer < desc->ArraySize; layer++) {
        for (uint32_t level = 0; level < desc->MipLevels; level++) {
          const uint32_t id = D3D11CalcSubresource(
            level, layer, desc->MipLevels);

          VkExtent3D mipLevelExtent = pTexture->MipLevelExtent(level);
          VkExtent3D blockCount = util::computeBlockCount(mipLevelExtent, formatInfo->blockSize);
          VkDeviceSize bytesPerRow   = blockCount.width  * formatInfo->elementSize;
          VkDeviceSize bytesPerSlice = blockCount.height * bytesPerRow;

          DxvkBufferSlice slice;
          if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
            slice = DxvkBufferSlice(pTexture->GetMappedBuffer(id));

            if (pTexture->HasSequenceNumber()) {
              m_texturesUpdateSeqNum.push_back(pTexture);
            }
          } else {
            slice = m_staging.alloc(CACHE_LINE_SIZE, util::computeImageDataSize(packedFormat, mipLevelExtent, formatInfo->aspectMask));
          }

          util::packImageData(slice.mapPtr(0),
            pInitialData[id].pSysMem, pInitialData[id].SysMemPitch, pInitialData[id].SysMemSlicePitch,
            bytesPerRow, bytesPerSlice, pTexture->GetVkImageType(), mipLevelExtent, 1, formatInfo, formatInfo->aspectMask);

          if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING) {
            m_transferCommands += 1;
            m_transferMemory   += pTexture->GetSubresourceLayout(formatInfo->aspectMask, id).Size;

            VkImageSubresourceLayers subresourceLayers;
            subresourceLayers.aspectMask     = formatInfo->aspectMask;
            subresourceLayers.mipLevel       = level;
            subresourceLayers.baseArrayLayer = layer;
            subresourceLayers.layerCount     = 1;

            if (formatInfo->aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
              EmitCs([
                cImage = image,
                cSubresources = subresourceLayers,
                cBuffer = slice.buffer(),
                cBufferOffset = slice.offset(),
                cBytesPerRow = bytesPerRow,
                cBytesPerSlice = bytesPerSlice
              ](DxvkContext* context) {
                context->uploadImageFromBuffer(
                  cImage, cSubresources,
                  cBuffer,
                  cBufferOffset,
                  cBytesPerRow,
                  cBytesPerSlice);
              });
            } else {
              EmitCs([
                cImage = image,
                cSubresources = subresourceLayers,
                cExtent = mipLevelExtent,
                cBuffer = slice.buffer(),
                cBufferOffset = slice.offset(),
                cFormat = packedFormat
              ](DxvkContext* context) {
                context->copyPackedBufferToDepthStencilImage(
                  cImage, cSubresources, VkOffset2D { 0, 0 },
                  VkExtent2D { cExtent.width, cExtent.height },
                  cBuffer, cBufferOffset, VkOffset2D { 0, 0 },
                  VkExtent2D { cExtent.width, cExtent.height },
                  cFormat, true
                );
              });
            }
          }
        }
      }
    } else {
      if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING) {
        m_transferCommands += 1;

        // While the Microsoft docs state that resource contents are
        // undefined if no initial data is provided, some applications
        // expect a resource to be pre-cleared.
        VkImageSubresourceRange subresources;
        subresources.aspectMask     = formatInfo->aspectMask;
        subresources.baseMipLevel   = 0;
        subresources.levelCount     = desc->MipLevels;
        subresources.baseArrayLayer = 0;
        subresources.layerCount     = desc->ArraySize;

        EmitCs([
          cImage        = std::move(image),
          cSubresources = subresources
        ](DxvkContext* context) {
          context->initImage(cImage, cSubresources, VK_IMAGE_LAYOUT_UNDEFINED);
        });
      }

      if (mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
        for (uint32_t i = 0; i < pTexture->CountSubresources(); i++) {
          auto buffer = pTexture->GetMappedBuffer(i);
          std::memset(buffer->mapPtr(0), 0, buffer->info().size);
        }
      }
    }
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

    VkImageSubresourceRange subresources = image->getAvailableSubresources();

    EmitCs([
      cImage        = std::move(image),
      cSubresources = subresources
    ](DxvkContext* context) {
      context->initImage(cImage, cSubresources, VK_IMAGE_LAYOUT_PREINITIALIZED);
    });

    m_transferCommands += 1;
  }


  void D3D11Initializer::InitTiledTexture(
          D3D11CommonTexture*         pTexture) {

    EmitCs([
      cImage = pTexture->GetImage()
    ](DxvkContext* context) {
      context->initSparseImage(cImage);
    });

    m_transferCommands += 1;
  }


  void D3D11Initializer::SyncKeyedMutex(ID3D11Resource *pResource) {
    Com<IDXGIKeyedMutex> keyedMutex;
    if (pResource->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&keyedMutex)) != S_OK)
      return;

    keyedMutex->AcquireSync(0, 0);
    keyedMutex->ReleaseSync(0);
  }

  DxvkCsChunkRef D3D11Initializer::AllocCsChunk() {
    return m_parent->AllocCsChunk(DxvkCsChunkFlag::SingleUse);
  }

}
