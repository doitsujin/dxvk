#include <vector>
#include <utility>
#include <cstring>

#include "d3d11_device.h"
#include "d3d11_context.h"
#include "d3d11_cuda.h"

#include "../util/log/log.h"

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

    if (ControlFlags & D3D11_VK_BARRIER_CONTROL_IGNORE_GRAPHICS_UAV)
      flags.set(DxvkBarrierControl::IgnoreGraphicsBarriers);

    m_ctx->EmitCs([cFlags = flags] (DxvkContext* ctx) {
      ctx->setBarrierControl(cFlags);
    });
  }


  bool STDMETHODCALLTYPE D3D11DeviceContextExt::LaunchCubinShaderNVX(IUnknown* hShader, uint32_t GridX, uint32_t GridY, uint32_t GridZ,
      const void* pParams, uint32_t ParamSize, void* const* pReadResources, uint32_t NumReadResources, void* const* pWriteResources, uint32_t NumWriteResources) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    CubinShaderWrapper* cubinShader = static_cast<CubinShaderWrapper*>(hShader);
    CubinShaderLaunchInfo launchInfo;

    const uint32_t maxResources = NumReadResources + NumWriteResources;
    launchInfo.buffers.reserve(maxResources);
    launchInfo.images.reserve(maxResources);

    for (uint32_t i = 0; i < NumReadResources; i++)
      launchInfo.insertResource(static_cast<ID3D11Resource*>(pReadResources[i]), DxvkAccess::Read);

    for (uint32_t i = 0; i < NumWriteResources; i++)
      launchInfo.insertResource(static_cast<ID3D11Resource*>(pWriteResources[i]), DxvkAccess::Write);

    launchInfo.paramSize = ParamSize;
    launchInfo.params.resize(launchInfo.paramSize);
    std::memcpy(launchInfo.params.data(), pParams, ParamSize);

    launchInfo.cuLaunchConfig[0] = reinterpret_cast<void*>(0x01); // CU_LAUNCH_PARAM_BUFFER_POINTER
    launchInfo.cuLaunchConfig[1] = launchInfo.params.data();
    launchInfo.cuLaunchConfig[2] = reinterpret_cast<void*>(0x02); // CU_LAUNCH_PARAM_BUFFER_SIZE
    launchInfo.cuLaunchConfig[3] = &launchInfo.paramSize; // yes, this actually requires a pointer to a size_t containing the parameter size
    launchInfo.cuLaunchConfig[4] = reinterpret_cast<void*>(0x00); // CU_LAUNCH_PARAM_END

    launchInfo.nvxLaunchInfo.function       = cubinShader->cuFunction();
    launchInfo.nvxLaunchInfo.gridDimX       = GridX;
    launchInfo.nvxLaunchInfo.gridDimY       = GridY;
    launchInfo.nvxLaunchInfo.gridDimZ       = GridZ;
    launchInfo.nvxLaunchInfo.blockDimX      = cubinShader->blockDim().width;
    launchInfo.nvxLaunchInfo.blockDimY      = cubinShader->blockDim().height;
    launchInfo.nvxLaunchInfo.blockDimZ      = cubinShader->blockDim().depth;
    launchInfo.nvxLaunchInfo.sharedMemBytes = 0;
    launchInfo.nvxLaunchInfo.paramCount     = 0;
    launchInfo.nvxLaunchInfo.pParams        = nullptr;
    launchInfo.nvxLaunchInfo.extraCount     = 1;
    launchInfo.nvxLaunchInfo.pExtras        = launchInfo.cuLaunchConfig.data();

    launchInfo.shader = cubinShader;

    /* Need to capture by value in case this gets called from a deferred context */
    m_ctx->EmitCs([cLaunchInfo = std::move(launchInfo)] (DxvkContext* ctx) {
      ctx->launchCuKernelNVX(cLaunchInfo.nvxLaunchInfo, cLaunchInfo.buffers, cLaunchInfo.images);
    });

    return true;
  }
}
