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
  
  /**
   * \brief Per-stage state
   *
   * Stores an object of the given type for each shader stage.
   * \tparam Object type
   */
  template<typename T>
  class D3D11ShaderStageState {

  public:

          T& operator [] (DxbcProgramType type)       { return m_state[uint32_t(type)]; }
    const T& operator [] (DxbcProgramType type) const { return m_state[uint32_t(type)]; }

    /**
     * \brief Calls reset method on all objects
     */
    void reset() {
      for (auto& state : m_state)
        state.reset();
    }

  private:

    std::array<T, 6> m_state = { };

  };


  /**
   * \brief Constant buffer bindings
   *
   * Stores the bound buffer range from a runtime point of view,
   * as well as the range that is actually bound to the context.
   */
  struct D3D11ConstantBufferBinding {
    Com<D3D11Buffer, false> buffer  = nullptr;
    UINT             constantOffset = 0;
    UINT             constantCount  = 0;
    UINT             constantBound  = 0;
  };
  
  struct D3D11ShaderStageCbvBinding {
    std::array<D3D11ConstantBufferBinding, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> buffers = { };

    uint32_t maxCount = 0;

    void reset() {
      for (uint32_t i = 0; i < maxCount; i++)
        buffers[i] = D3D11ConstantBufferBinding();

      maxCount = 0;
    }
  };

  using D3D11CbvBindings = D3D11ShaderStageState<D3D11ShaderStageCbvBinding>;
  
  /**
   * \brief Shader resource bindings
   *
   * Stores bound shader resource views, as well as a bit
   * set of views that are potentially hazardous.
   */
  struct D3D11ShaderStageSrvBinding {
    std::array<Com<D3D11ShaderResourceView, false>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> views     = { };
    DxvkBindingSet<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>                           hazardous = { };

    uint32_t maxCount = 0;

    void reset() {
      for (uint32_t i = 0; i < maxCount; i++)
        views[i] = nullptr;

      hazardous.clear();
      maxCount = 0;
    }
  };
    
  using D3D11SrvBindings = D3D11ShaderStageState<D3D11ShaderStageSrvBinding>;

  /**
   * \brief Sampler bindings
   *
   * Stores bound samplers.
   */
  struct D3D11ShaderStageSamplerBinding {
    std::array<D3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> samplers = { };

    uint32_t maxCount = 0;

    void reset() {
      for (uint32_t i = 0; i < maxCount; i++)
        samplers[i] = nullptr;

      maxCount = 0;
    }
  };
    
  using D3D11SamplerBindings = D3D11ShaderStageState<D3D11ShaderStageSamplerBinding>;

  /**
   * \brief UAV bindings
   *
   * Stores bound UAVs. For compute shader UAVs,
   * we also store a bit mask of bound UAVs.
   */
  using D3D11ShaderStageUavBinding = std::array<Com<D3D11UnorderedAccessView, false>, D3D11_1_UAV_SLOT_COUNT>;
  
  struct D3D11UavBindings {
    D3D11ShaderStageUavBinding              views = { };
    DxvkBindingSet<D3D11_1_UAV_SLOT_COUNT>  mask  = { };

    uint32_t maxCount = 0;

    void reset() {
      for (uint32_t i = 0; i < maxCount; i++)
        views[i] = nullptr;

      mask.clear();
      maxCount = 0;
    }
  };

  /**
   * \brief Input assembly state
   *
   * Stores vertex buffers, the index buffer, the
   * input layout, and the dynamic primitive topology.
   */
  struct D3D11VertexBufferBinding {
    Com<D3D11Buffer, false> buffer = nullptr;
    UINT                    offset = 0;
    UINT                    stride = 0;
  };
  
  struct D3D11IndexBufferBinding {
    Com<D3D11Buffer, false> buffer = nullptr;
    UINT                    offset = 0;
    DXGI_FORMAT             format = DXGI_FORMAT_UNKNOWN;
  };

  struct D3D11ContextStateIA {
    Com<D3D11InputLayout, false> inputLayout       = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY     primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    
    std::array<D3D11VertexBufferBinding, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> vertexBuffers = { };
    D3D11IndexBufferBinding                                                         indexBuffer   = { };

    uint32_t maxVbCount = 0;

    void reset() {
      inputLayout = nullptr;

      primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

      for (uint32_t i = 0; i < maxVbCount; i++)
        vertexBuffers[i] = D3D11VertexBufferBinding();

      indexBuffer = D3D11IndexBufferBinding();
    }
  };
  
  /**
   * \brief Output merger state
   *
   * Stores RTV, DSV, and graphics UAV bindings, as well as related state.
   */
  using D3D11RenderTargetViewBinding = std::array<Com<D3D11RenderTargetView, false>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>;
  
  struct D3D11ContextStateOM {
    D3D11ShaderStageUavBinding        uavs  = { };
    D3D11RenderTargetViewBinding      rtvs  = { };
    Com<D3D11DepthStencilView, false> dsv   = { };
    
    D3D11BlendState*        cbState = nullptr;
    D3D11DepthStencilState* dsState = nullptr;
    
    FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    UINT  sampleCount    = 0u;
    UINT  sampleMask     = D3D11_DEFAULT_SAMPLE_MASK;
    UINT  stencilRef     = D3D11_DEFAULT_STENCIL_REFERENCE;

    UINT  maxRtv         = 0u;
    UINT  maxUav         = 0u;

    void reset() {
      for (uint32_t i = 0; i < maxUav; i++)
        uavs[i] = nullptr;

      for (uint32_t i = 0; i < maxRtv; i++)
        rtvs[i] = nullptr;

      dsv = nullptr;

      cbState = nullptr;
      dsState = nullptr;

      for (uint32_t i = 0; i < 4; i++)
        blendFactor[i] = 1.0f;

      sampleCount = 0u;
      sampleMask = D3D11_DEFAULT_SAMPLE_MASK;
      stencilRef = D3D11_DEFAULT_STENCIL_REFERENCE;

      maxRtv = 0;
      maxUav = 0;
    }
  };
  
  /**
   * \brief Indirect draw state
   *
   * Stores the current indirct draw
   * argument and draw count buffer.
   */
  struct D3D11ContextStateID {
    Com<D3D11Buffer, false> argBuffer = nullptr;
    Com<D3D11Buffer, false> cntBuffer = nullptr;

    void reset() {
      argBuffer = nullptr;
      cntBuffer = nullptr;
    }
  };

  /**
   * \brief Rasterizer state
   *
   * Stores viewport info and the rasterizer state object.
   */
  struct D3D11ContextStateRS {
    uint32_t numViewports = 0;
    uint32_t numScissors  = 0;
    
    std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports = { };
    std::array<D3D11_RECT,     D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors  = { };
    
    D3D11RasterizerState* state = nullptr;

    void reset() {
      for (uint32_t i = 0; i < numViewports; i++)
        viewports[i] = D3D11_VIEWPORT();

      for (uint32_t i = 0; i < numScissors; i++)
        scissors[i] = D3D11_RECT();

      numViewports = 0;
      numScissors = 0;

      state = nullptr;
    }
  };

  /**
   * \brief Stream output binding
   *
   * Stores stream output buffers with offset.
   */
  struct D3D11ContextSoTarget {
    Com<D3D11Buffer, false> buffer = nullptr;
    UINT                    offset = 0;
  };

  struct D3D11ContextStateSO {
    std::array<D3D11ContextSoTarget, D3D11_SO_BUFFER_SLOT_COUNT> targets = { };

    void reset() {
      for (uint32_t i = 0; i < targets.size(); i++)
        targets[i] = D3D11ContextSoTarget();
    }
  };
  
  /**
   * \brief Predication state
   *
   * Stores predication info.
   */
  struct D3D11ContextStatePR {
    Com<D3D11Query, false> predicateObject = nullptr;
    BOOL                   predicateValue  = false;

    void reset() {
      predicateObject = nullptr;
      predicateValue = false;
    }
  };
  
  /**
   * \brief Context state
   */
  struct D3D11ContextState {
    Com<D3D11VertexShader, false>    vs;
    Com<D3D11HullShader, false>      hs;
    Com<D3D11DomainShader, false>    ds;
    Com<D3D11GeometryShader, false>  gs;
    Com<D3D11PixelShader, false>     ps;
    Com<D3D11ComputeShader, false>   cs;

    D3D11ContextStateID id;
    D3D11ContextStateIA ia;
    D3D11ContextStateOM om;
    D3D11ContextStateRS rs;
    D3D11ContextStateSO so;
    D3D11ContextStatePR pr;

    D3D11CbvBindings    cbv;
    D3D11SrvBindings    srv;
    D3D11UavBindings    uav;
    D3D11SamplerBindings samplers;
  };

  /**
   * \brief Maximum used binding numbers in a shader stage
   */
  struct D3D11MaxUsedStageBindings {
    uint32_t cbvCount     : 5;
    uint32_t srvCount     : 9;
    uint32_t uavCount     : 7;
    uint32_t samplerCount : 5;
    uint32_t reserved     : 6;
  };

  /**
   * \brief Maximum used binding numbers for all context state
   */
  struct D3D11MaxUsedBindings {
    std::array<D3D11MaxUsedStageBindings, 6> stages;
    uint32_t  vbCount;
    uint32_t  soCount;
  };

}