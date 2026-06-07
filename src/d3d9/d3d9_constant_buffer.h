#pragma once

#include "../dxvk/dxvk_buffer.h"

#include "../util/util_math.h"

#include "d3d9_include.h"
#include "d3d9_shader.h"

namespace dxvk {

  class D3D9DeviceEx;

  /**
   * \brief Constant buffer
   */
  class D3D9ConstantBuffer {
    static constexpr VkDeviceSize ShaderConstSize = 1024ull << 10;
    static constexpr VkDeviceSize MiscSize        =   64ull << 10;

    using Kind = D3D9ShaderResourceMapping::CbvIndex;
  public:

    struct StreamCommand {
      VkDeviceSize offset = 0u;
      VkDeviceSize size = 0u;
    };

    D3D9ConstantBuffer();

    D3D9ConstantBuffer(
            D3D9DeviceEx*         pDevice,
            Kind                  CbvType);

    ~D3D9ConstantBuffer();

    /**
     * \brief Queries alignment
     *
     * Useful to pad copies with initialized data.
     * \returns Data alignment
     */
    VkDeviceSize GetAlignment() const {
      return m_align;
    }

    /**
     * \brief Allocates a given amount of memory
     *
     * \note: Requires that all data is written to the buffer before
     * submitting the next set of commands to the CS chunk, otherwise
     * the GPU may read stale data.
     * \param [in] size Number of bytes to allocate
     * \returns Map pointer of the allocated region
     */
    void* Alloc(VkDeviceSize size);

    /**
     * \brief Allocates typed data
     *
     * \param [in] count Number of items to allocate
     * \returns Allocated data slice
     */
    template<typename T>
    T* AllocTyped(size_t Count) {
      return reinterpret_cast<T*>(Alloc(Count * sizeof(T)));
    }

    /**
     * \brief Resets current stream command
     *
     * This \e must unconditionally be called when the
     * device's current CS chunk gets submitted to the
     * worker to avoid accessing stale data.
     */
    void ResetStreamCommand() {
      m_streamCmd = nullptr;
    }

  private:

    D3D9DeviceEx*         m_device  = nullptr;

    Kind                  m_kind    = {};
    VkDeviceSize          m_size    = 0ull;
    VkBufferUsageFlags    m_usage   = 0u;
    VkShaderStageFlags    m_stages  = 0u;
    VkDeviceSize          m_align   = 0ull;
    VkMemoryPropertyFlags m_memType = 0u;
    VkDeviceSize          m_offset  = 0ull;
    bool                  m_useDma  = false;

    Rc<DxvkBuffer>        m_cpuBuffer = nullptr;
    Rc<DxvkBuffer>        m_gpuBuffer = nullptr;

    Rc<DxvkResourceAllocation> m_cpuSlice = nullptr;

    StreamCommand*        m_streamCmd = nullptr;

    Rc<DxvkResourceAllocation> CreateBuffer();

    void SetupStreamCommand(
            VkDeviceSize        Offset,
            VkDeviceSize        Size,
            bool                Discard);

    static VkDeviceSize DetermineSize(
            Kind                CbvType);

    static VkBufferUsageFlags DetermineUsage(
            D3D9DeviceEx*       pDevice,
            Kind                CbvType,
            VkDeviceSize        Size);

    static VkShaderStageFlags DetermineStages(
            Kind                CbvType);

    static VkDeviceSize DetermineAlignment(
            D3D9DeviceEx*       pDevice,
            VkBufferUsageFlags  Usage);

    static VkMemoryPropertyFlags DetermineMemoryType(
            Kind                CbvType);

    static bool DetermineDmaUsage(
            D3D9DeviceEx*       pDevice,
            Kind                CbvType);

  };

}
