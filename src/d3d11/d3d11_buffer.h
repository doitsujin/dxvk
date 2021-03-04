#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_buffer.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"
#include "d3d11_resource.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DeviceContext;


  /**
   * \brief Buffer map mode
   */
  enum D3D11_COMMON_BUFFER_MAP_MODE {
    D3D11_COMMON_BUFFER_MAP_MODE_NONE,
    D3D11_COMMON_BUFFER_MAP_MODE_DIRECT,
  };


  /**
   * \brief Stream output buffer offset
   *
   * A byte offset into the buffer that
   * stores the byte offset where new
   * data will be written to.
   */
  struct D3D11SOCounter {
    uint32_t byteOffset;
  };
  
  
  class D3D11Buffer : public D3D11DeviceChild<ID3D11Buffer> {
    static constexpr VkDeviceSize BufferSliceAlignment = 64;
  public:
    
    D3D11Buffer(
            D3D11Device*                pDevice,
      const D3D11_BUFFER_DESC*          pDesc);
    ~D3D11Buffer();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_BUFFER_DESC *pDesc) final;
    
    bool CheckViewCompatibility(
            UINT                BindFlags,
            DXGI_FORMAT         Format) const;

    const D3D11_BUFFER_DESC* Desc() const {
      return &m_desc;
    }

    D3D11_COMMON_BUFFER_MAP_MODE GetMapMode() const {
      return (m_buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        ? D3D11_COMMON_BUFFER_MAP_MODE_DIRECT
        : D3D11_COMMON_BUFFER_MAP_MODE_NONE;
    }

    Rc<DxvkBuffer> GetBuffer() const {
      return m_buffer;
    }
    
    DxvkBufferSlice GetBufferSlice() const {
      return DxvkBufferSlice(m_buffer, 0, m_desc.ByteWidth);
    }
    
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset) const {
      VkDeviceSize size = m_desc.ByteWidth;

      return likely(offset < size)
        ? DxvkBufferSlice(m_buffer, offset, size - offset)
        : DxvkBufferSlice();
    }
    
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset, VkDeviceSize length) const {
      VkDeviceSize size = m_desc.ByteWidth;

      return likely(offset < size)
        ? DxvkBufferSlice(m_buffer, offset, std::min(length, size - offset))
        : DxvkBufferSlice();
    }

    DxvkBufferSlice GetSOCounter() {
      return m_soCounter != nullptr
        ? DxvkBufferSlice(m_soCounter)
        : DxvkBufferSlice();
    }
    
    DxvkBufferSliceHandle AllocSlice() {
      return m_buffer->allocSlice();
    }
    
    DxvkBufferSliceHandle DiscardSlice() {
      m_mapped = m_buffer->allocSlice();
      return m_mapped;
    }

    DxvkBufferSliceHandle GetMappedSlice() const {
      return m_mapped;
    }

    D3D10Buffer* GetD3D10Iface() {
      return &m_d3d10;
    }

    /**
     * \brief Normalizes buffer description
     * 
     * \param [in] pDesc Buffer description
     * \returns \c S_OK if the parameters are valid
     */
    static HRESULT NormalizeBufferProperties(
            D3D11_BUFFER_DESC*      pDesc);

  private:
    
    const D3D11_BUFFER_DESC     m_desc;
    
    Rc<DxvkBuffer>              m_buffer;
    Rc<DxvkBuffer>              m_soCounter;
    DxvkBufferSliceHandle       m_mapped;

    D3D11DXGIResource           m_resource;
    D3D10Buffer                 m_d3d10;

    BOOL CheckFormatFeatureSupport(
            VkFormat              Format,
            VkFormatFeatureFlags  Features) const;
    
    VkMemoryPropertyFlags GetMemoryFlags() const;

    Rc<DxvkBuffer> CreateSoCounterBuffer();

  };


  /**
   * \brief Retrieves buffer from resource pointer
   * 
   * \param [in] pResource The resource to query
   * \returns Pointer to buffer, or \c nullptr
   */
  D3D11Buffer* GetCommonBuffer(
          ID3D11Resource*       pResource);
  
}
