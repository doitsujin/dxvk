#pragma once

#include "d3d10_include.h"
#include "d3d10_interfaces.h"

namespace dxvk {

  struct D3D10_STATE_BLOCK_STATE {
    Com<ID3D10VertexShader>       vs                  = { };
    Com<ID3D10SamplerState>       vsSso[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT]              = { };
    Com<ID3D10ShaderResourceView> vsSrv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]       = { };
    Com<ID3D10Buffer>             vsCbo[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]  = { };
    Com<ID3D10GeometryShader>     gs                  = { };
    Com<ID3D10SamplerState>       gsSso[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT]              = { };
    Com<ID3D10ShaderResourceView> gsSrv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]       = { };
    Com<ID3D10Buffer>             gsCbo[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]  = { };
    Com<ID3D10PixelShader>        ps                  = { };
    Com<ID3D10SamplerState>       psSso[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT]              = { };
    Com<ID3D10ShaderResourceView> psSrv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]       = { };
    Com<ID3D10Buffer>             psCbo[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]  = { };
    Com<ID3D10Buffer>             iaVertexBuffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { };
    UINT                          iaVertexOffsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { };
    UINT                          iaVertexStrides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { };
    Com<ID3D10Buffer>             iaIndexBuffer       = { };
    DXGI_FORMAT                   iaIndexFormat       = DXGI_FORMAT_UNKNOWN;
    UINT                          iaIndexOffset       = 0;
    Com<ID3D10InputLayout>        iaInputLayout       = nullptr;
    D3D10_PRIMITIVE_TOPOLOGY      iaTopology          = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
    Com<ID3D10RenderTargetView>   omRtv[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { };
    Com<ID3D10DepthStencilView>   omDsv               = { };
    Com<ID3D10DepthStencilState>  omDepthStencilState = { };
    UINT                          omStencilRef        = 0;
    Com<ID3D10BlendState>         omBlendState        = { };
    FLOAT                         omBlendFactor[4]    = { };
    UINT                          omSampleMask        = 0;
    UINT                          rsViewportCount     = 0;
    D3D10_VIEWPORT                rsViewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = { };
    UINT                          rsScissorCount      = 0;
    RECT                          rsScissors [D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = { };
    Com<ID3D10RasterizerState>    rsState             = { };
    Com<ID3D10Buffer>             soBuffers[D3D10_SO_BUFFER_SLOT_COUNT] = { };
    UINT                          soOffsets[D3D10_SO_BUFFER_SLOT_COUNT] = { };
    Com<ID3D10Predicate>          predicate           = { };
    BOOL                          predicateInvert     = FALSE;
  };


  class D3D10StateBlock : public ComObject<ID3D10StateBlock> {

  public:
    D3D10StateBlock(
            ID3D10Device*             pDevice,
      const D3D10_STATE_BLOCK_MASK*   pMask);
    
    ~D3D10StateBlock();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    HRESULT STDMETHODCALLTYPE Capture();

    HRESULT STDMETHODCALLTYPE Apply();

    HRESULT STDMETHODCALLTYPE GetDevice(
            ID3D10Device**            ppDevice);

    HRESULT STDMETHODCALLTYPE ReleaseAllDeviceObjects();
  
  private:

    Com<ID3D10Device>       m_device;
    D3D10_STATE_BLOCK_MASK  m_mask;
    D3D10_STATE_BLOCK_STATE m_state;

    static BOOL TestBit(
      const BYTE*                     pMask,
            UINT                      Idx);

  };

}
