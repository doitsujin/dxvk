#include <vector>
#include <utility>
#include <cstring>

#include "d3d11_device.h"
#include "../util/log/log.h"

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

    if (ControlFlags & D3D11_VK_BARRIER_CONTROL_IGNORE_GRAPHICS_UAV)
      flags.set(DxvkBarrierControl::IgnoreGraphicsBarriers);

    m_ctx->EmitCs([cFlags = flags] (DxvkContext* ctx) {
      ctx->setBarrierControl(cFlags);
    });
  }


  class CubinShaderLaunchInfo {
  public:
    Com<CubinShaderWrapper> cuShader;
    std::vector<uint8_t> Params;
    size_t ParamSize;
    VkCuLaunchInfoNVX nvxLaunchInfo = { VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX };
    const void* CuLaunchConfig[5];
    std::vector<std::pair<Rc<DxvkBuffer>, DxvkAccessFlags>> buffers;
    std::vector<std::pair<Rc<DxvkImage>, DxvkAccessFlags>> images;
  };


  bool STDMETHODCALLTYPE D3D11DeviceContextExt::LaunchCubinShaderNVX(IUnknown* hShader, uint32_t GridX, uint32_t GridY, uint32_t GridZ, const void* pParams, uint32_t ParamSize, const void** pReadResources, uint32_t NumReadResources, const void** pWriteResources, uint32_t NumWriteResources) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    CubinShaderWrapper* cubinShader = (CubinShaderWrapper*)hShader;
    auto rcLaunchInfo = new CubinShaderLaunchInfo();

    const uint32_t maxResources = NumReadResources + NumWriteResources;
    rcLaunchInfo->buffers.reserve(maxResources);
    rcLaunchInfo->images.reserve(maxResources);

    auto InsertResource =
    [&images = rcLaunchInfo->images,
     &buffers = rcLaunchInfo->buffers](ID3D11Resource* res, DxvkAccessFlags access) {
      auto buffer = GetCommonBuffer(res);
      auto texture = GetCommonTexture(res);
      if (buffer) buffers.emplace_back(std::make_pair(buffer->GetBuffer(), access));
      else if (texture) images.emplace_back(std::make_pair(texture->GetImage(), access));
    };

    for (uint32_t i = 0; i < NumReadResources; i++) {
      InsertResource(const_cast<ID3D11Resource*>(static_cast<const ID3D11Resource*>(pReadResources[i])), DxvkAccess::Read);
    }
    for (uint32_t i = 0; i < NumWriteResources; i++) {
      InsertResource(const_cast<ID3D11Resource*>(static_cast<const ID3D11Resource*>(pWriteResources[i])), DxvkAccess::Write);
    }

    rcLaunchInfo->ParamSize = ParamSize;
    rcLaunchInfo->Params.resize(rcLaunchInfo->ParamSize);
    std::memcpy(rcLaunchInfo->Params.data(), pParams, ParamSize);

    rcLaunchInfo->CuLaunchConfig[0] = reinterpret_cast<void*>(0x01); // CU_LAUNCH_PARAM_BUFFER_POINTER
    rcLaunchInfo->CuLaunchConfig[1] = rcLaunchInfo->Params.data();
    rcLaunchInfo->CuLaunchConfig[2] = reinterpret_cast<void*>(0x02); // CU_LAUNCH_PARAM_BUFFER_SIZE
    rcLaunchInfo->CuLaunchConfig[3] = &rcLaunchInfo->ParamSize; // yes, this actually requires a pointer to a size_t containing the parameter size
    rcLaunchInfo->CuLaunchConfig[4] = reinterpret_cast<void*>(0x00); // CU_LAUNCH_PARAM_END

    rcLaunchInfo->nvxLaunchInfo.function       = cubinShader->Function;
    rcLaunchInfo->nvxLaunchInfo.gridDimX       = GridX;
    rcLaunchInfo->nvxLaunchInfo.gridDimY       = GridY;
    rcLaunchInfo->nvxLaunchInfo.gridDimZ       = GridZ;
    rcLaunchInfo->nvxLaunchInfo.blockDimX      = cubinShader->BlockDimX;
    rcLaunchInfo->nvxLaunchInfo.blockDimY      = cubinShader->BlockDimY;
    rcLaunchInfo->nvxLaunchInfo.blockDimZ      = cubinShader->BlockDimZ;
    rcLaunchInfo->nvxLaunchInfo.sharedMemBytes = 0;
    rcLaunchInfo->nvxLaunchInfo.paramCount     = 0;
    rcLaunchInfo->nvxLaunchInfo.pParams        = nullptr;
    rcLaunchInfo->nvxLaunchInfo.extraCount     = 1;
    rcLaunchInfo->nvxLaunchInfo.pExtras        = &rcLaunchInfo->CuLaunchConfig[0];

    rcLaunchInfo->cuShader = cubinShader;

    m_ctx->EmitCs([cRcLaunchInfo = rcLaunchInfo] (DxvkContext* ctx) {
        ctx->launchCuKernelNVX(cRcLaunchInfo->nvxLaunchInfo, cRcLaunchInfo->buffers, cRcLaunchInfo->images);
        delete cRcLaunchInfo;
    });

    return true;
  }
}
