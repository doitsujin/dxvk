#pragma once

#include <array>

#include "d3d11_buffer.h"
#include "d3d11_input_layout.h"
#include "d3d11_query.h"
#include "d3d11_sampler.h"
#include "d3d11_shader.h"
#include "d3d11_state.h"
#include "d3d11_view_dsv.h"
#include "d3d11_view_rtv.h"
#include "d3d11_view_srv.h"
#include "d3d11_view_uav.h"

namespace dxvk {
  
  struct D3D11ConstantBufferBinding {
    Com<D3D11Buffer> buffer         = nullptr;
    UINT             constantOffset = 0;
    UINT             constantCount  = 0;
    UINT             constantBound  = 0;
  };
  
  using D3D11ConstantBufferBindings = std::array<
    D3D11ConstantBufferBinding, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>;
  
  
  using D3D11SamplerBindings = std::array<
    D3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>;
    
  
  struct D3D11ShaderResourceBindings {
    std::array<Com<D3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> views     = { };
    DxvkBindingSet<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>                           hazardous = { };
  };
    
    
  using D3D11UnorderedAccessBindings = std::array<
    Com<D3D11UnorderedAccessView>, D3D11_1_UAV_SLOT_COUNT>;
  
  
  struct D3D11ContextStateVS {
    Com<D3D11VertexShader>        shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
  };
  
  
  struct D3D11ContextStateHS {
    Com<D3D11HullShader>          shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
  };
  
  
  struct D3D11ContextStateDS {
    Com<D3D11DomainShader>        shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
  };
  
  
  struct D3D11ContextStateGS {
    Com<D3D11GeometryShader>      shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
  };
  
  
  struct D3D11ContextStatePS {
    Com<D3D11PixelShader>         shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
    D3D11UnorderedAccessBindings  unorderedAccessViews = { };
  };
  
  
  struct D3D11ContextStateCS {
    Com<D3D11ComputeShader>       shader = nullptr;
    D3D11ConstantBufferBindings   constantBuffers = { };
    D3D11SamplerBindings          samplers        = { };
    D3D11ShaderResourceBindings   shaderResources = { };
    D3D11UnorderedAccessBindings  unorderedAccessViews = { };

    DxvkBindingSet<D3D11_1_UAV_SLOT_COUNT> uavMask = { };
  };
  
  
  struct D3D11VertexBufferBinding {
    Com<D3D11Buffer> buffer = nullptr;
    UINT             offset = 0;
    UINT             stride = 0;
  };
  
  
  struct D3D11IndexBufferBinding {
    Com<D3D11Buffer> buffer = nullptr;
    UINT             offset = 0;
    DXGI_FORMAT      format = DXGI_FORMAT_UNKNOWN;
  };


  struct D3D11ContextStateID {
    Com<D3D11Buffer> argBuffer = nullptr;
    Com<D3D11Buffer> cntBuffer = nullptr;
  };
  
  
  struct D3D11ContextStateIA {
    Com<D3D11InputLayout>    inputLayout       = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    
    std::array<D3D11VertexBufferBinding, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> vertexBuffers = { };
    D3D11IndexBufferBinding                                                         indexBuffer   = { };
  };
  
  
  struct D3D11ContextStateOM {
    std::array<Com<D3D11RenderTargetView, false>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> renderTargetViews = { };
    Com<D3D11DepthStencilView, false>                                                     depthStencilView  = { };
    
    D3D11BlendState*        cbState = nullptr;
    D3D11DepthStencilState* dsState = nullptr;
    
    FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    UINT  sampleMask     = 0xFFFFFFFFu;
    UINT  stencilRef     = 0u;

    UINT  maxRtv         = 0u;
    UINT  maxUav         = 0u;
  };
  
  
  struct D3D11ContextStateRS {
    uint32_t numViewports = 0;
    uint32_t numScissors  = 0;
    
    std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports = { };
    std::array<D3D11_RECT,     D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors  = { };
    
    D3D11RasterizerState* state = nullptr;
  };


  struct D3D11ContextSoTarget {
    Com<D3D11Buffer> buffer = nullptr;
    UINT             offset = 0;
  };
  

  struct D3D11ContextStateSO {
    std::array<D3D11ContextSoTarget, D3D11_SO_BUFFER_SLOT_COUNT> targets = { };
  };
  
  
  struct D3D11ContextStatePR {
    Com<D3D11Query> predicateObject = nullptr;
    BOOL            predicateValue  = FALSE;
  };
  
  
  /**
   * \brief Context state
   */
  struct D3D11ContextState {
    D3D11ContextStateCS cs;
    D3D11ContextStateDS ds;
    D3D11ContextStateGS gs;
    D3D11ContextStateHS hs;
    D3D11ContextStatePS ps;
    D3D11ContextStateVS vs;
    
    D3D11ContextStateID id;
    D3D11ContextStateIA ia;
    D3D11ContextStateOM om;
    D3D11ContextStateRS rs;
    D3D11ContextStateSO so;
    D3D11ContextStatePR pr;
  };
  
}