#pragma once

#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11DeviceContext;

  class D3D11DeviceContextExt : public ID3D11VkExtContext1 {
    
  public:
    
    D3D11DeviceContextExt(
            D3D11DeviceContext*     pContext);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    void STDMETHODCALLTYPE MultiDrawIndirect(
            UINT                    DrawCount,
            ID3D11Buffer*           pBufferForArgs,
            UINT                    ByteOffsetForArgs,
            UINT                    ByteStrideForArgs);
    
    void STDMETHODCALLTYPE MultiDrawIndexedIndirect(
            UINT                    DrawCount,
            ID3D11Buffer*           pBufferForArgs,
            UINT                    ByteOffsetForArgs,
            UINT                    ByteStrideForArgs);
    
    void STDMETHODCALLTYPE MultiDrawIndirectCount(
            UINT                    MaxDrawCount,
            ID3D11Buffer*           pBufferForCount,
            UINT                    ByteOffsetForCount,
            ID3D11Buffer*           pBufferForArgs,
            UINT                    ByteOffsetForArgs,
            UINT                    ByteStrideForArgs);
    
    void STDMETHODCALLTYPE MultiDrawIndexedIndirectCount(
            UINT                    MaxDrawCount,
            ID3D11Buffer*           pBufferForCount,
            UINT                    ByteOffsetForCount,
            ID3D11Buffer*           pBufferForArgs,
            UINT                    ByteOffsetForArgs,
            UINT                    ByteStrideForArgs);
    
    void STDMETHODCALLTYPE SetDepthBoundsTest(
            BOOL                    Enable,
            FLOAT                   MinDepthBounds,
            FLOAT                   MaxDepthBounds);
    
    void STDMETHODCALLTYPE SetBarrierControl(
            UINT                    ControlFlags);

    bool STDMETHODCALLTYPE LaunchCubinShaderNVX(
            IUnknown*               hShader,
            uint32_t                GridX,
            uint32_t                GridY,
            uint32_t                GridZ,
            const void*             pParams,
            uint32_t                paramSize,
            void* const*            pReadResources,
            uint32_t                NumReadResources,
            void* const*            pWriteResources,
            uint32_t                NumWriteResources);

  private:
    
    D3D11DeviceContext* m_ctx;

  };

}
