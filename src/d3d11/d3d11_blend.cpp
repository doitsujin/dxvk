#include "d3d11_blend.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11BlendState::D3D11BlendState(
          D3D11Device*      device,
    const D3D11_BLEND_DESC& desc)
  : m_device(device), m_desc(desc) {
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
    m_msState.enableAlphaToOne      = VK_FALSE;
    m_msState.enableSampleShading   = VK_FALSE;
    m_msState.minSampleShading      = 0.0f;
    
    // In 11_0, there is no logic op state. Later versions
    // of D3D11 however put it into the blend state object.
    m_loState.enableLogicOp         = VK_FALSE;
    m_loState.logicOp               = VK_LOGIC_OP_NO_OP;
  }
  
  
  D3D11BlendState::~D3D11BlendState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11BlendState::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11BlendState);
    
    Logger::warn("D3D11BlendState::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDesc(D3D11_BLEND_DESC* pDesc) {
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
  
  
  DxvkBlendMode D3D11BlendState::DecodeBlendMode(
    const D3D11_RENDER_TARGET_BLEND_DESC& BlendDesc) {
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
    }
    
    if (BlendFactor != 0)  // prevent log spamming when apps use ZeroMemory
      Logger::err(str::format("D3D11: Invalid blend factor: ", BlendFactor));
    return VK_BLEND_FACTOR_ZERO;
  }
  
  
  VkBlendOp D3D11BlendState::DecodeBlendOp(D3D11_BLEND_OP BlendOp) {
    switch (BlendOp) {
      case D3D11_BLEND_OP_ADD:            return VK_BLEND_OP_ADD;
      case D3D11_BLEND_OP_SUBTRACT:       return VK_BLEND_OP_SUBTRACT;
      case D3D11_BLEND_OP_REV_SUBTRACT:   return VK_BLEND_OP_REVERSE_SUBTRACT;
      case D3D11_BLEND_OP_MIN:            return VK_BLEND_OP_MIN;
      case D3D11_BLEND_OP_MAX:            return VK_BLEND_OP_MAX;
    }
    
    if (BlendOp != 0)  // prevent log spamming when apps use ZeroMemory
      Logger::err(str::format("D3D11: Invalid blend op: ", BlendOp));
    return VK_BLEND_OP_ADD;
  }
  
}
