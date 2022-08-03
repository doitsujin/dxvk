#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_query.h"
#include "d3d11_texture.h"
#include "d3d11_video.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {

  D3D11DeviceContext::D3D11DeviceContext(
          D3D11Device*            pParent,
    const Rc<DxvkDevice>&         Device,
          DxvkCsChunkFlags        CsFlags)
  : D3D11DeviceChild<ID3D11DeviceContext4>(pParent),
    m_multithread(this, false),
    m_device    (Device),
    m_staging   (Device, StagingBufferSize),
    m_csFlags   (CsFlags),
    m_csChunk   (AllocCsChunk()),
    m_cmdData   (nullptr) {

  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  VkClearValue D3D11DeviceContext::ConvertColorValue(
    const FLOAT                             Color[4],
    const DxvkFormatInfo*                   pFormatInfo) {
    VkClearValue result;

    if (pFormatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      for (uint32_t i = 0; i < 4; i++) {
        if (pFormatInfo->flags.test(DxvkFormatFlag::SampledUInt))
          result.color.uint32[i] = uint32_t(std::max(0.0f, Color[i]));
        else if (pFormatInfo->flags.test(DxvkFormatFlag::SampledSInt))
          result.color.int32[i] = int32_t(Color[i]);
        else
          result.color.float32[i] = Color[i];
      }
    } else {
      result.depthStencil.depth = Color[0];
      result.depthStencil.stencil = 0;
    }

    return result;
  }


  DxvkDataSlice D3D11DeviceContext::AllocUpdateBufferSlice(size_t Size) {
    constexpr size_t UpdateBufferSize = 1 * 1024 * 1024;
    
    if (Size >= UpdateBufferSize) {
      Rc<DxvkDataBuffer> buffer = new DxvkDataBuffer(Size);
      return buffer->alloc(Size);
    } else {
      if (m_updateBuffer == nullptr)
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
      
      DxvkDataSlice slice = m_updateBuffer->alloc(Size);
      
      if (slice.ptr() == nullptr) {
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
        slice = m_updateBuffer->alloc(Size);
      }
      
      return slice;
    }
  }
  
  
  DxvkBufferSlice D3D11DeviceContext::AllocStagingBuffer(
          VkDeviceSize                      Size) {
    return m_staging.alloc(256, Size);
  }


  void D3D11DeviceContext::ResetStagingBuffer() {
    m_staging.reset();
  }
  

  DxvkCsChunkRef D3D11DeviceContext::AllocCsChunk() {
    return m_parent->AllocCsChunk(m_csFlags);
  }
  

  void D3D11DeviceContext::InitDefaultPrimitiveTopology(
          DxvkInputAssemblyState*           pIaState) {
    pIaState->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    pIaState->primitiveRestart  = VK_FALSE;
    pIaState->patchVertexCount  = 0;
  }


  void D3D11DeviceContext::InitDefaultRasterizerState(
          DxvkRasterizerState*              pRsState) {
    pRsState->polygonMode     = VK_POLYGON_MODE_FILL;
    pRsState->cullMode        = VK_CULL_MODE_BACK_BIT;
    pRsState->frontFace       = VK_FRONT_FACE_CLOCKWISE;
    pRsState->depthClipEnable = VK_TRUE;
    pRsState->depthBiasEnable = VK_FALSE;
    pRsState->conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    pRsState->sampleCount     = 0;
  }


  void D3D11DeviceContext::InitDefaultDepthStencilState(
          DxvkDepthStencilState*            pDsState) {
    VkStencilOpState stencilOp;
    stencilOp.failOp            = VK_STENCIL_OP_KEEP;
    stencilOp.passOp            = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp       = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = D3D11_DEFAULT_STENCIL_READ_MASK;
    stencilOp.writeMask         = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    stencilOp.reference         = 0;

    pDsState->enableDepthTest   = VK_TRUE;
    pDsState->enableDepthWrite  = VK_TRUE;
    pDsState->enableStencilTest = VK_FALSE;
    pDsState->depthCompareOp    = VK_COMPARE_OP_LESS;
    pDsState->stencilOpFront    = stencilOp;
    pDsState->stencilOpBack     = stencilOp;
  }

  
  void D3D11DeviceContext::InitDefaultBlendState(
          DxvkBlendMode*                    pCbState,
          DxvkLogicOpState*                 pLoState,
          DxvkMultisampleState*             pMsState,
          UINT                              SampleMask) {
    pCbState->enableBlending    = VK_FALSE;
    pCbState->colorSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->colorDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->colorBlendOp      = VK_BLEND_OP_ADD;
    pCbState->alphaSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->alphaDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->alphaBlendOp      = VK_BLEND_OP_ADD;
    pCbState->writeMask         = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    pLoState->enableLogicOp     = VK_FALSE;
    pLoState->logicOp           = VK_LOGIC_OP_NO_OP;

    pMsState->sampleMask            = SampleMask;
    pMsState->enableAlphaToCoverage = VK_FALSE;
  }


  void D3D11DeviceContext::TrackResourceSequenceNumber(
          ID3D11Resource*             pResource) {
    if (!pResource)
      return;

    D3D11CommonTexture* texture = GetCommonTexture(pResource);

    if (texture) {
      if (texture->HasSequenceNumber()) {
        for (uint32_t i = 0; i < texture->CountSubresources(); i++)
          TrackTextureSequenceNumber(texture, i);
      }
    } else {
      D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pResource);

      if (buffer->HasSequenceNumber())
        TrackBufferSequenceNumber(buffer);
    }
  }

}
