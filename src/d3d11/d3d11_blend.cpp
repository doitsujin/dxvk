#include "d3d11_blend.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11BlendState::D3D11BlendState(
          D3D11Device*        device,
    const D3D11_BLEND_DESC1&  desc)
  : D3D11StateObject<ID3D11BlendState1>(device),
    m_desc(desc), m_d3d10(this) {
    // If Independent Blend is disabled, we must ignore the
    // blend modes for render target 1 to 7. In Vulkan, all
    // blend modes need to be identical in that case.
    for (uint32_t i = 0; i < m_blendModes.size(); i++) {
      m_blendModes.at(i) = DecodeBlendMode(
        desc.IndependentBlendEnable
          ? desc.RenderTarget[i]
          : desc.RenderTarget[0]);
    }
    
    // Multisample state is part of the blend state in D3D11
    m_msState.sampleMask            = 0; // Set during bind
    m_msState.enableAlphaToCoverage = desc.AlphaToCoverageEnable;
    
    // Vulkan only supports a global logic op for the blend
    // state, which might be problematic in some cases.
    if (desc.IndependentBlendEnable && desc.RenderTarget[0].LogicOpEnable)
      Logger::warn("D3D11: Per-target logic ops not supported");
    
    m_loState.enableLogicOp         = desc.RenderTarget[0].LogicOpEnable;
    m_loState.logicOp               = DecodeLogicOp(desc.RenderTarget[0].LogicOp);
  }
  
  
  D3D11BlendState::~D3D11BlendState() {
    
  }


  HRESULT STDMETHODCALLTYPE D3D11BlendState::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11BlendState)
     || riid == __uuidof(ID3D11BlendState1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10BlendState)
     || riid == __uuidof(ID3D10BlendState1)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    Logger::warn("D3D11BlendState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDesc(D3D11_BLEND_DESC* pDesc) {
    pDesc->AlphaToCoverageEnable  = m_desc.AlphaToCoverageEnable;
    pDesc->IndependentBlendEnable = m_desc.IndependentBlendEnable;
    
    for (uint32_t i = 0; i < 8; i++) {
      pDesc->RenderTarget[i].BlendEnable           = m_desc.RenderTarget[i].BlendEnable;
      pDesc->RenderTarget[i].SrcBlend              = m_desc.RenderTarget[i].SrcBlend;
      pDesc->RenderTarget[i].DestBlend             = m_desc.RenderTarget[i].DestBlend;
      pDesc->RenderTarget[i].BlendOp               = m_desc.RenderTarget[i].BlendOp;
      pDesc->RenderTarget[i].SrcBlendAlpha         = m_desc.RenderTarget[i].SrcBlendAlpha;
      pDesc->RenderTarget[i].DestBlendAlpha        = m_desc.RenderTarget[i].DestBlendAlpha;
      pDesc->RenderTarget[i].BlendOpAlpha          = m_desc.RenderTarget[i].BlendOpAlpha;
      pDesc->RenderTarget[i].RenderTargetWriteMask = m_desc.RenderTarget[i].RenderTargetWriteMask;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDesc1(D3D11_BLEND_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  void D3D11BlendState::BindToContext(
    const Rc<DxvkContext>&  ctx,
          uint32_t          sampleMask) const {
    // We handled Independent Blend during object creation
    // already, so if it is disabled, all elements in the
    // blend mode array will be identical
    for (uint32_t i = 0; i < m_blendModes.size(); i++)
      ctx->setBlendMode(i, m_blendModes.at(i));
    
    // The sample mask is dynamic state in D3D11
    DxvkMultisampleState msState = m_msState;
    msState.sampleMask = sampleMask;
    ctx->setMultisampleState(msState);
    
    // Set up logic op state as well
    ctx->setLogicOpState(m_loState);
  }
  
  
  D3D11_BLEND_DESC1 D3D11BlendState::PromoteDesc(const D3D11_BLEND_DESC* pSrcDesc) {
    D3D11_BLEND_DESC1 dstDesc;
    dstDesc.AlphaToCoverageEnable  = pSrcDesc->AlphaToCoverageEnable;
    dstDesc.IndependentBlendEnable = pSrcDesc->IndependentBlendEnable;
    
    for (uint32_t i = 0; i < 8; i++) {
      dstDesc.RenderTarget[i].BlendEnable           = pSrcDesc->RenderTarget[i].BlendEnable;
      dstDesc.RenderTarget[i].LogicOpEnable         = FALSE;
      dstDesc.RenderTarget[i].SrcBlend              = pSrcDesc->RenderTarget[i].SrcBlend;
      dstDesc.RenderTarget[i].DestBlend             = pSrcDesc->RenderTarget[i].DestBlend;
      dstDesc.RenderTarget[i].BlendOp               = pSrcDesc->RenderTarget[i].BlendOp;
      dstDesc.RenderTarget[i].SrcBlendAlpha         = pSrcDesc->RenderTarget[i].SrcBlendAlpha;
      dstDesc.RenderTarget[i].DestBlendAlpha        = pSrcDesc->RenderTarget[i].DestBlendAlpha;
      dstDesc.RenderTarget[i].BlendOpAlpha          = pSrcDesc->RenderTarget[i].BlendOpAlpha;
      dstDesc.RenderTarget[i].LogicOp               = D3D11_LOGIC_OP_NOOP;
      dstDesc.RenderTarget[i].RenderTargetWriteMask = pSrcDesc->RenderTarget[i].RenderTargetWriteMask;
    }
    
    return dstDesc;
  }
  
  
  HRESULT D3D11BlendState::NormalizeDesc(D3D11_BLEND_DESC1* pDesc) {
    if (pDesc->AlphaToCoverageEnable)
      pDesc->AlphaToCoverageEnable = TRUE;
    
    if (pDesc->IndependentBlendEnable)
      pDesc->IndependentBlendEnable = TRUE;
    
    const uint32_t numRenderTargets = pDesc->IndependentBlendEnable ? 8 : 1;
    
    for (uint32_t i = 0; i < numRenderTargets; i++) {
      D3D11_RENDER_TARGET_BLEND_DESC1* rt = &pDesc->RenderTarget[i];
      
      if (rt->BlendEnable) {
        rt->BlendEnable = TRUE;
        
        if (rt->LogicOpEnable)
          return E_INVALIDARG;
        
        if (!ValidateBlendOperations(
         rt->SrcBlend, rt->SrcBlendAlpha,
         rt->DestBlend, rt->DestBlendAlpha,
         rt->BlendOp, rt->BlendOpAlpha))
          return E_INVALIDARG;
      } else {
        rt->SrcBlend       = D3D11_BLEND_ONE;
        rt->DestBlend      = D3D11_BLEND_ZERO;
        rt->BlendOp        = D3D11_BLEND_OP_ADD;
        rt->SrcBlendAlpha  = D3D11_BLEND_ONE;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        rt->BlendOpAlpha   = D3D11_BLEND_OP_ADD;
      }
      
      if (rt->LogicOpEnable) {
        rt->LogicOpEnable = TRUE;
        
        // Blending must be disabled
        // if the logic op is enabled
        if (rt->BlendEnable
         || pDesc->IndependentBlendEnable
         || !ValidateLogicOp(rt->LogicOp))
          return E_INVALIDARG;
      } else {
        rt->LogicOp = D3D11_LOGIC_OP_NOOP;
      }
      
      if (rt->RenderTargetWriteMask > D3D11_COLOR_WRITE_ENABLE_ALL)
        return E_INVALIDARG;
    }
    
    for (uint32_t i = numRenderTargets; i < 8; i++) {
      // Render targets blend operations are the same
      // across all render targets when blend is enabled
      // on rendertarget[0] with independent blend disabled
      pDesc->RenderTarget[i] = pDesc->RenderTarget[0];
    }
    
    return S_OK;
  }
  
  
  DxvkBlendMode D3D11BlendState::DecodeBlendMode(
    const D3D11_RENDER_TARGET_BLEND_DESC1& BlendDesc) {
    DxvkBlendMode mode;
    mode.enableBlending   = BlendDesc.BlendEnable;
    mode.colorSrcFactor   = DecodeBlendFactor(BlendDesc.SrcBlend, false);
    mode.colorDstFactor   = DecodeBlendFactor(BlendDesc.DestBlend, false);
    mode.colorBlendOp     = DecodeBlendOp(BlendDesc.BlendOp);
    mode.alphaSrcFactor   = DecodeBlendFactor(BlendDesc.SrcBlendAlpha, true);
    mode.alphaDstFactor   = DecodeBlendFactor(BlendDesc.DestBlendAlpha, true);
    mode.alphaBlendOp     = DecodeBlendOp(BlendDesc.BlendOpAlpha);
    mode.writeMask        = BlendDesc.RenderTargetWriteMask;
    return mode;
  }
  
  
  VkBlendFactor D3D11BlendState::DecodeBlendFactor(D3D11_BLEND BlendFactor, bool IsAlpha) {
    switch (BlendFactor) {
      case D3D11_BLEND_ZERO:              return VK_BLEND_FACTOR_ZERO;
      case D3D11_BLEND_ONE:               return VK_BLEND_FACTOR_ONE;
      case D3D11_BLEND_SRC_COLOR:         return VK_BLEND_FACTOR_SRC_COLOR;
      case D3D11_BLEND_INV_SRC_COLOR:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      case D3D11_BLEND_SRC_ALPHA:         return VK_BLEND_FACTOR_SRC_ALPHA;
      case D3D11_BLEND_INV_SRC_ALPHA:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      case D3D11_BLEND_DEST_ALPHA:        return VK_BLEND_FACTOR_DST_ALPHA;
      case D3D11_BLEND_INV_DEST_ALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
      case D3D11_BLEND_DEST_COLOR:        return VK_BLEND_FACTOR_DST_COLOR;
      case D3D11_BLEND_INV_DEST_COLOR:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
      case D3D11_BLEND_SRC_ALPHA_SAT:     return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
      case D3D11_BLEND_BLEND_FACTOR:      return IsAlpha ? VK_BLEND_FACTOR_CONSTANT_ALPHA : VK_BLEND_FACTOR_CONSTANT_COLOR;
      case D3D11_BLEND_INV_BLEND_FACTOR:  return IsAlpha ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
      case D3D11_BLEND_SRC1_COLOR:        return VK_BLEND_FACTOR_SRC1_COLOR;
      case D3D11_BLEND_INV_SRC1_COLOR:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
      case D3D11_BLEND_SRC1_ALPHA:        return VK_BLEND_FACTOR_SRC1_ALPHA;
      case D3D11_BLEND_INV_SRC1_ALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
      default:                            return VK_BLEND_FACTOR_ZERO;
    }
  }
  
  
  VkBlendOp D3D11BlendState::DecodeBlendOp(D3D11_BLEND_OP BlendOp) {
    switch (BlendOp) {
      case D3D11_BLEND_OP_ADD:            return VK_BLEND_OP_ADD;
      case D3D11_BLEND_OP_SUBTRACT:       return VK_BLEND_OP_SUBTRACT;
      case D3D11_BLEND_OP_REV_SUBTRACT:   return VK_BLEND_OP_REVERSE_SUBTRACT;
      case D3D11_BLEND_OP_MIN:            return VK_BLEND_OP_MIN;
      case D3D11_BLEND_OP_MAX:            return VK_BLEND_OP_MAX;
      default:                            return VK_BLEND_OP_ADD;
    }
  }
  
  
  VkLogicOp D3D11BlendState::DecodeLogicOp(D3D11_LOGIC_OP LogicOp) {
    switch (LogicOp) {
      case D3D11_LOGIC_OP_CLEAR:          return VK_LOGIC_OP_CLEAR;
      case D3D11_LOGIC_OP_SET:            return VK_LOGIC_OP_SET;
      case D3D11_LOGIC_OP_COPY:           return VK_LOGIC_OP_COPY;
      case D3D11_LOGIC_OP_COPY_INVERTED:  return VK_LOGIC_OP_COPY_INVERTED;
      case D3D11_LOGIC_OP_NOOP:           return VK_LOGIC_OP_NO_OP;
      case D3D11_LOGIC_OP_INVERT:         return VK_LOGIC_OP_INVERT;
      case D3D11_LOGIC_OP_AND:            return VK_LOGIC_OP_AND;
      case D3D11_LOGIC_OP_NAND:           return VK_LOGIC_OP_NAND;
      case D3D11_LOGIC_OP_OR:             return VK_LOGIC_OP_OR;
      case D3D11_LOGIC_OP_NOR:            return VK_LOGIC_OP_NOR;
      case D3D11_LOGIC_OP_XOR:            return VK_LOGIC_OP_XOR;
      case D3D11_LOGIC_OP_EQUIV:          return VK_LOGIC_OP_EQUIVALENT;
      case D3D11_LOGIC_OP_AND_REVERSE:    return VK_LOGIC_OP_AND_REVERSE;
      case D3D11_LOGIC_OP_AND_INVERTED:   return VK_LOGIC_OP_AND_INVERTED;
      case D3D11_LOGIC_OP_OR_REVERSE:     return VK_LOGIC_OP_OR_REVERSE;
      case D3D11_LOGIC_OP_OR_INVERTED:    return VK_LOGIC_OP_OR_INVERTED;
      default:                            return VK_LOGIC_OP_NO_OP;
    }
  }
  
  
  bool D3D11BlendState::ValidateBlendFactor(D3D11_BLEND Blend) {
    return Blend >= D3D11_BLEND_ZERO
        && Blend <= D3D11_BLEND_INV_SRC1_ALPHA;
  }
  
  
  bool D3D11BlendState::ValidateBlendFactorAlpha(D3D11_BLEND BlendAlpha) {
    return BlendAlpha >= D3D11_BLEND_ZERO
        && BlendAlpha <= D3D11_BLEND_INV_SRC1_ALPHA
        && BlendAlpha != D3D11_BLEND_SRC_COLOR
        && BlendAlpha != D3D11_BLEND_INV_SRC_COLOR
        && BlendAlpha != D3D11_BLEND_DEST_COLOR
        && BlendAlpha != D3D11_BLEND_INV_DEST_COLOR
        && BlendAlpha != D3D11_BLEND_SRC1_COLOR
        && BlendAlpha != D3D11_BLEND_INV_SRC1_COLOR;
  }
  
  
  bool D3D11BlendState::ValidateBlendOp(D3D11_BLEND_OP BlendOp) {
    return BlendOp >= D3D11_BLEND_OP_ADD
        && BlendOp <= D3D11_BLEND_OP_MAX;
  }
  
  
  bool D3D11BlendState::ValidateLogicOp(D3D11_LOGIC_OP LogicOp) {
    return LogicOp >= D3D11_LOGIC_OP_CLEAR
        && LogicOp <= D3D11_LOGIC_OP_OR_INVERTED;
  }
  
  
  bool D3D11BlendState::ValidateBlendOperations(
          D3D11_BLEND     SrcBlend, 
          D3D11_BLEND     SrcBlendAlpha, 
          D3D11_BLEND     DestBlend, 
          D3D11_BLEND     DestBlendAlpha, 
          D3D11_BLEND_OP  BlendOp, 
          D3D11_BLEND_OP  BlendOpAlpha) {
    return ValidateBlendOp(BlendOp)
        && ValidateBlendOp(BlendOpAlpha)
        && ValidateBlendFactor(SrcBlend)
        && ValidateBlendFactor(DestBlend)
        && ValidateBlendFactorAlpha(SrcBlendAlpha)
        && ValidateBlendFactorAlpha(DestBlendAlpha);
  }
  
}
