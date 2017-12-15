#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DeviceContext;
  
  
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
    
    /**
     * \brief Retrieves current buffer slice
     * 
     * When the buffer gets renamed, the slice that is
     * used for rendering and copy operations changes.
     * May only be called from the immediate context.
     * \returns Current buffer slice
     */
    DxvkBufferSlice GetBufferSlice() const;
    
    /**
     * \brief Maps buffer
     * 
     * Must only be called from the immediate context.
     * \param [in] pContext The immediate context
     * \param [in] MapType Map type
     * \param [in] MapFlags Map flags
     * \param [out] pMappedSubresource Map pointer
     * \return \c S_OK on success
     */
    HRESULT Map(
            D3D11DeviceContext*       pContext,
            D3D11_MAP                 MapType,
            UINT                      MapFlags,
            D3D11_MAPPED_SUBRESOURCE* pMappedSubresource);
    
  private:
    
    const Com<D3D11Device>      m_device;
    const D3D11_BUFFER_DESC     m_desc;
    
    Rc<DxvkBuffer>              m_buffer;
    
    Rc<DxvkBuffer> CreateBuffer(
      const D3D11_BUFFER_DESC* pDesc) const;
    
  };
  
}
