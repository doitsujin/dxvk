#pragma once

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_staging.h"

#include "../d3d10/d3d10_multithread.h"

#include "d3d11_annotation.h"
#include "d3d11_cmd.h"
#include "d3d11_context_ext.h"
#include "d3d11_context_state.h"
#include "d3d11_device_child.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11DeviceContext : public D3D11DeviceChild<ID3D11DeviceContext4> {
    template<typename T>
    friend class D3D11DeviceContextExt;
    // Needed in order to call EmitCs for pushing markers
    template<typename T>
    friend class D3D11UserDefinedAnnotation;

    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;
  public:

    D3D11DeviceContext(
            D3D11Device*            pParent,
      const Rc<DxvkDevice>&         Device);

    ~D3D11DeviceContext();

  protected:
    
    Rc<DxvkDevice>              m_device;
    Rc<DxvkDataBuffer>          m_updateBuffer;

    DxvkStagingBuffer           m_staging;

    D3D11ContextState           m_state;

    VkClearValue ConvertColorValue(
      const FLOAT                             Color[4],
      const DxvkFormatInfo*                   pFormatInfo);
    
    DxvkDataSlice AllocUpdateBufferSlice(size_t Size);
    
    DxvkBufferSlice AllocStagingBuffer(
            VkDeviceSize                      Size);

    void ResetStagingBuffer();

    static void InitDefaultPrimitiveTopology(
            DxvkInputAssemblyState*           pIaState);

    static void InitDefaultRasterizerState(
            DxvkRasterizerState*              pRsState);

    static void InitDefaultDepthStencilState(
            DxvkDepthStencilState*            pDsState);

    static void InitDefaultBlendState(
            DxvkBlendMode*                    pCbState,
            DxvkLogicOpState*                 pLoState,
            DxvkMultisampleState*             pMsState,
            UINT                              SampleMask);

    template<typename T>
    const D3D11CommonShader* GetCommonShader(T* pShader) const {
      return pShader != nullptr ? pShader->GetCommonShader() : nullptr;
    }

    static uint32_t GetIndirectCommandStride(const D3D11CmdDrawIndirectData* cmdData, uint32_t offset, uint32_t minStride) {
      if (likely(cmdData->stride))
        return cmdData->offset + cmdData->count * cmdData->stride == offset ? cmdData->stride : 0;

      uint32_t stride = offset - cmdData->offset;
      return stride >= minStride && stride <= 32 ? stride : 0;
    }

    static bool ValidateDrawBufferSize(ID3D11Buffer* pBuffer, UINT Offset, UINT Size) {
      UINT bufferSize = 0;

      if (likely(pBuffer != nullptr))
        bufferSize = static_cast<D3D11Buffer*>(pBuffer)->Desc()->ByteWidth;

      return bufferSize >= Offset + Size;
    }

  };
  
}
