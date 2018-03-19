#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DeviceContext;
  
  
  /**
   * \brief Common buffer info
   * 
   * Stores where the buffer was last
   * mapped on the immediate context.
   */
  struct D3D11BufferInfo {
    DxvkPhysicalBufferSlice mappedSlice;
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
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_BUFFER_DESC *pDesc) final;
    
    Rc<DxvkBuffer> GetBuffer() const {
      return m_buffer;
    }
    
    DxvkBufferSlice GetBufferSlice() const {
      return DxvkBufferSlice(m_buffer, 0, m_buffer->info().size);
    }
    
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset) const {
      return DxvkBufferSlice(m_buffer, offset, m_buffer->info().size - offset);
    }
    
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset, VkDeviceSize length) const {
      return DxvkBufferSlice(m_buffer, offset, length);
    }
    
    VkDeviceSize GetSize() const {
      return m_buffer->info().size;
    }
    
    D3D11BufferInfo* GetBufferInfo() {
      return &m_bufferInfo;
    }
    
  private:
    
    const Com<D3D11Device>      m_device;
    const D3D11_BUFFER_DESC     m_desc;
    
    Rc<DxvkBuffer>              m_buffer;
    D3D11BufferInfo             m_bufferInfo;
    
    Rc<DxvkBuffer> CreateBuffer(
      const D3D11_BUFFER_DESC* pDesc) const;
    
    VkMemoryPropertyFlags GetMemoryFlags(
      const D3D11_BUFFER_DESC* pDesc) const;
    
  };
  
}
