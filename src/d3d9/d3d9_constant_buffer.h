#pragma once

#include "../dxvk/dxvk_buffer.h"
#include "../dxvk/dxvk_context.h"

#include "../dxso/dxso_util.h"

#include "../util/util_math.h"

#include "d3d9_include.h"

namespace dxvk {

  class D3D9DeviceEx;

  /**
   * \brief Constant buffer
   */
  class D3D9ConstantBuffer {

  public:

    D3D9ConstantBuffer();

    D3D9ConstantBuffer(
            D3D9DeviceEx*         pDevice,
            DxsoProgramType       ShaderStage,
            DxsoConstantBuffers   BufferType,
            VkDeviceSize          Size);

    D3D9ConstantBuffer(
            D3D9DeviceEx*         pDevice,
            VkBufferUsageFlags    Usage,
            VkShaderStageFlags    Stages,
            uint32_t              ResourceSlot,
            VkDeviceSize          Size);

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
     * \param [in] size Number of bytes to allocate
     * \returns Map pointer of the allocated region
     */
    void* Alloc(VkDeviceSize size);

    /**
     * \brief Allocates a full buffer slice
     *
     * This must not be called if \ref Alloc is used.
     * \returns Map pointer of the allocated region
     */
    void* AllocSlice();

  private:

    D3D9DeviceEx*         m_device  = nullptr;

    uint32_t              m_binding = 0u;
    VkBufferUsageFlags    m_usage   = 0u;
    VkShaderStageFlags    m_stages  = 0u;
    VkDeviceSize          m_size    = 0ull;
    VkDeviceSize          m_align   = 0ull;
    VkDeviceSize          m_offset  = 0ull;

    Rc<DxvkBuffer>        m_buffer  = nullptr;
    Rc<DxvkResourceAllocation> m_slice = nullptr;

    Rc<DxvkResourceAllocation> createBuffer();

    VkDeviceSize getAlignment(const Rc<DxvkDevice>& device) const;

  };



  /**
   * \brief Constant buffer living on the CS thread
   */
  class D3D9CSConstantBuffer {

  public:

    D3D9CSConstantBuffer();

    D3D9CSConstantBuffer(
      const Rc<DxvkDevice>&       Device,
            DxsoProgramType       ShaderStage,
            DxsoConstantBuffers   BufferType,
            VkDeviceSize          Size,
            bool                  UseDeviceLocalBuffer);

    D3D9CSConstantBuffer(
      const Rc<DxvkDevice>&       Device,
            VkBufferUsageFlags    Usage,
            VkShaderStageFlags    Stages,
            uint32_t              ResourceSlot,
            VkDeviceSize          Size,
            bool                  UseDeviceLocalBuffer);

    ~D3D9CSConstantBuffer();

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
     * \param [in] size Number of bytes to allocate
     * \returns Map pointer of the allocated region
     */
    void* Alloc(DxvkContext* ctx, VkDeviceSize size);

    /**
     * \brief Allocates a full buffer slice
     *
     * This must not be called if \ref Alloc is used.
     * \returns Map pointer of the allocated region
     */
    void* AllocSlice(DxvkContext* ctx);

  private:

    Rc<DxvkDevice>        m_device;

    uint32_t              m_binding = 0u;
    VkBufferUsageFlags    m_usage   = 0u;
    VkShaderStageFlags    m_stages  = 0u;
    VkDeviceSize          m_size    = 0ull;
    VkDeviceSize          m_align   = 0ull;
    VkDeviceSize          m_offset  = 0ull;

    bool                  m_useDeviceLocalBuffer = false;

    Rc<DxvkBuffer>        m_buffer  = nullptr;
    Rc<DxvkResourceAllocation> m_slice = nullptr;

    Rc<DxvkResourceAllocation> createBuffer(DxvkContext* ctx);

    VkDeviceSize getAlignment(const Rc<DxvkDevice>& device) const;

  };

}