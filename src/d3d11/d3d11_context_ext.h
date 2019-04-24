#pragma once

#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11DeviceContext;

  class D3D11DeviceContextExt : public ID3D11VkExtContext {
    
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
    
  private:
    
    D3D11DeviceContext* m_ctx;
    
  };

}
