#include "d3d11_context.h"

namespace dxvk {
  
  D3D11DeviceContextExt::D3D11DeviceContextExt(
          D3D11DeviceContext*     pContext)
  : m_ctx(pContext) {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11DeviceContextExt::AddRef() {
    return m_ctx->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11DeviceContextExt::Release() {
    return m_ctx->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceContextExt::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_ctx->QueryInterface(riid, ppvObject);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::MultiDrawIndirect(
          UINT                    DrawCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) {
    D3D10DeviceLock lock = m_ctx->LockContext();
    m_ctx->SetDrawBuffers(pBufferForArgs, nullptr);
    
    m_ctx->EmitCs([
      cCount  = DrawCount,
      cOffset = ByteOffsetForArgs,
      cStride = ByteStrideForArgs
    ] (DxvkContext* ctx) {
      ctx->drawIndirect(cOffset, cCount, cStride);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::MultiDrawIndexedIndirect(
          UINT                    DrawCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) {
    D3D10DeviceLock lock = m_ctx->LockContext();
    m_ctx->SetDrawBuffers(pBufferForArgs, nullptr);
    
    m_ctx->EmitCs([
      cCount  = DrawCount,
      cOffset = ByteOffsetForArgs,
      cStride = ByteStrideForArgs
    ] (DxvkContext* ctx) {
      ctx->drawIndexedIndirect(cOffset, cCount, cStride);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::MultiDrawIndirectCount(
          UINT                    MaxDrawCount,
          ID3D11Buffer*           pBufferForCount,
          UINT                    ByteOffsetForCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) {
    D3D10DeviceLock lock = m_ctx->LockContext();
    m_ctx->SetDrawBuffers(pBufferForArgs, pBufferForCount);

    m_ctx->EmitCs([
      cMaxCount  = MaxDrawCount,
      cArgOffset = ByteOffsetForArgs,
      cCntOffset = ByteOffsetForCount,
      cStride    = ByteStrideForArgs
    ] (DxvkContext* ctx) {
      ctx->drawIndirectCount(cArgOffset, cCntOffset, cMaxCount, cStride);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::MultiDrawIndexedIndirectCount(
          UINT                    MaxDrawCount,
          ID3D11Buffer*           pBufferForCount,
          UINT                    ByteOffsetForCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) {
    D3D10DeviceLock lock = m_ctx->LockContext();
    m_ctx->SetDrawBuffers(pBufferForArgs, pBufferForCount);

    m_ctx->EmitCs([
      cMaxCount  = MaxDrawCount,
      cArgOffset = ByteOffsetForArgs,
      cCntOffset = ByteOffsetForCount,
      cStride    = ByteStrideForArgs
    ] (DxvkContext* ctx) {
      ctx->drawIndexedIndirectCount(cArgOffset, cCntOffset, cMaxCount, cStride);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::SetDepthBoundsTest(
          BOOL                    Enable,
          FLOAT                   MinDepthBounds,
          FLOAT                   MaxDepthBounds) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    DxvkDepthBounds db;
    db.enableDepthBounds  = Enable;
    db.minDepthBounds     = MinDepthBounds;
    db.maxDepthBounds     = MaxDepthBounds;
    
    m_ctx->EmitCs([cDepthBounds = db] (DxvkContext* ctx) {
      ctx->setDepthBounds(cDepthBounds);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContextExt::SetBarrierControl(
          UINT                    ControlFlags) {
    D3D10DeviceLock lock = m_ctx->LockContext();
    DxvkBarrierControlFlags flags;
    
    if (ControlFlags & D3D11_VK_BARRIER_CONTROL_IGNORE_WRITE_AFTER_WRITE)
      flags.set(DxvkBarrierControl::IgnoreWriteAfterWrite);
    
    m_ctx->EmitCs([cFlags = flags] (DxvkContext* ctx) {
      ctx->setBarrierControl(cFlags);
    });
  }
  
}
