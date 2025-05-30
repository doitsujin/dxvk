#pragma once

#include <array>

#include "dxvk_descriptor_heap.h"
#include "dxvk_pipelayout.h"

#include "../util/thread.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkDescriptorCopyWorker;

  /**
   * \brief Buffer descriptor update
   *
   * Stores the address range and descriptor
   * metadata for a uniform buffer update.
   */
  struct DxvkDescriptorCopyBuffer {
    VkDeviceAddress gpuAddress      = 0u;
    uint32_t        size            = 0u;
    uint16_t        indexInSet      = 0u;
    uint16_t        descriptorType  = 0u;
  };


  /**
   * \brief Descriptor copy metadata
   *
   * Stores info for a single descriptor set update.
   */
  struct DxvkDescriptorCopyRange {
    const DxvkDescriptorSetLayout* layout = nullptr;
    void*    descriptorMemory = nullptr;
    uint32_t descriptorCount = 0u;
    uint32_t bufferCount = 0u;
  };


  /**
   * \brief Allocated descriptor copy
   *
   * Stores pointers to the allocated descriptor and
   * buffer arrays. Both arrays are tightly packed.
   */
  struct DxvkDescriptorCopy {
    const DxvkDescriptor** descriptors = nullptr;
    DxvkDescriptorCopyBuffer* buffers = nullptr;
  };


  /** Function type to process buffer descriptors */
  using WriteBufferDescriptorsFn = void (const DxvkDescriptorCopyWorker*, DxvkDescriptor*, uint32_t, const DxvkDescriptorCopyBuffer*);


  /**
   * \brief Descriptor copy worker
   *
   * Off-loads descriptor uploads to a worker thread using a small
   * ring buffer. This is useful for moving the API call overhead
   * from uniform buffer updates away from the main worker thread,
   * without adding much latency to the command submission.
   */
  class DxvkDescriptorCopyWorker {
    constexpr static size_t DescriptorCount = 4096u;
    constexpr static size_t RangeCount      = 256u;
    constexpr static size_t BlockCount      = 4u;
  public:

    DxvkDescriptorCopyWorker(const Rc<DxvkDevice>& device);

    ~DxvkDescriptorCopyWorker();

    /**
     * \brief Allocates ring buffer entry for a descriptor update
     *
     * \param [in] layout Descriptor set layout
     * \param [in] descriptorMemory Allocated descriptor storage
     * \param [in] descriptorCount Total number of descriptors
     * \param [in] bufferCount Number of uniform buffer descriptors
     * \returns Allocated descriptor update range
     */
    DxvkDescriptorCopy allocEntry(
      const DxvkDescriptorSetLayout*  layout,
            void*                     descriptorMemory,
            uint32_t                  descriptorCount,
            uint32_t                  bufferCount) {
      auto* block = getBlock();

      if (block->descriptorCount + descriptorCount > DescriptorCount || block->rangeCount == RangeCount)
        block = flushBlock();

      DxvkDescriptorCopy result = { };
      result.descriptors = &block->descriptors[block->descriptorCount];
      result.buffers = &block->buffers[block->bufferCount];

      block->descriptorCount += descriptorCount;
      block->bufferCount += bufferCount;

      auto& range = block->ranges[block->rangeCount++];
      range.layout = layout;
      range.descriptorMemory = descriptorMemory;
      range.descriptorCount = descriptorCount;
      range.bufferCount = bufferCount;
      return result;
    }

    /**
     * \brief Flushes pending copies and retrieves sync handle
     *
     * The sync point can be used to ensure that all previously recorded
     * descriptor updates complete before submitting a command list.
     * \returns Sync handle
     */
    sync::SyncPoint getSyncHandle() {
      flushBlock();

      return sync::SyncPoint(
        m_consumeFence,
        m_appendFence->value());
    }

  private:

    Rc<DxvkDevice>    m_device;
    Rc<vk::DeviceFn>  m_vkd;

    Rc<sync::Fence> m_appendFence;
    Rc<sync::Fence> m_consumeFence;

    WriteBufferDescriptorsFn* m_writeBufferDescriptorsFn = nullptr;

    struct alignas(CACHE_LINE_SIZE) Block {
      size_t descriptorCount  = 0u;
      size_t bufferCount      = 0u;
      size_t rangeCount       = 0u;

      std::array<const DxvkDescriptor*,     DescriptorCount> descriptors  = { };
      std::array<DxvkDescriptorCopyBuffer,  DescriptorCount> buffers      = { };
      std::array<DxvkDescriptorCopyRange,   RangeCount>      ranges       = { };
    };

    std::array<Block, BlockCount> m_blocks = { };
    size_t                        m_blockIndex = 0u;

    std::thread m_thread;

    Block* getBlock() {
      return &m_blocks[m_blockIndex];
    }

    Block* flushBlock();

    WriteBufferDescriptorsFn* getWriteBufferDescriptorFn() const;

    void processBlock(Block& block);

    void runWorker();

    static void writeBufferDescriptorsGeneric(
      const DxvkDescriptorCopyWorker* worker,
            DxvkDescriptor*           descriptors,
            uint32_t                  bufferCount,
      const DxvkDescriptorCopyBuffer* bufferInfos);

    static void writeBufferDescriptorsGetDescriptorExt(
      const DxvkDescriptorCopyWorker* worker,
            DxvkDescriptor*           descriptors,
            uint32_t                  bufferCount,
      const DxvkDescriptorCopyBuffer* bufferInfos);

    static void writeBufferDescriptorsSteamDeck(
      const DxvkDescriptorCopyWorker* worker,
            DxvkDescriptor*           descriptors,
            uint32_t                  bufferCount,
      const DxvkDescriptorCopyBuffer* bufferInfos);

  };

}
