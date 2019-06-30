#pragma once

#include "d3d9_device_child.h"
#include "d3d9_state.h"

namespace dxvk {

  enum class D3D9StateBlockType :uint32_t {
    None,
    VertexState,
    PixelState,
    All
  };

  inline D3D9StateBlockType ConvertStateBlockType(D3DSTATEBLOCKTYPE type) {
    switch (type) {
      case D3DSBT_PIXELSTATE:  return D3D9StateBlockType::PixelState;
      case D3DSBT_VERTEXSTATE: return D3D9StateBlockType::VertexState;
      default:
      case D3DSBT_ALL:         return D3D9StateBlockType::All;
    }
  }

  using D3D9StateBlockBase = D3D9DeviceChild<IDirect3DStateBlock9>;
  class D3D9StateBlock : public D3D9StateBlockBase {

  public:

    D3D9StateBlock(D3D9DeviceEx* pDevice, D3D9StateBlockType Type);

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID  riid,
        void** ppvObject) final;

    HRESULT STDMETHODCALLTYPE Capture() final;
    HRESULT STDMETHODCALLTYPE Apply() final;

    HRESULT SetVertexDeclaration(D3D9VertexDecl* pDecl);

    HRESULT SetIndices(D3D9IndexBuffer* pIndexData);

    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);

    HRESULT SetStateSamplerState(
            DWORD               StateSampler,
            D3DSAMPLERSTATETYPE Type,
            DWORD               Value);

    HRESULT SetStreamSource(
            UINT               StreamNumber,
            D3D9VertexBuffer*  pStreamData,
            UINT               OffsetInBytes,
            UINT               Stride);

    HRESULT SetStreamSourceFreq(UINT StreamNumber, UINT Setting);

    HRESULT SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture);

    HRESULT SetVertexShader(D3D9VertexShader* pShader);

    HRESULT SetPixelShader(D3D9PixelShader* pShader);

    HRESULT SetMaterial(const D3DMATERIAL9* pMaterial);

    HRESULT SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix);

    HRESULT SetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD                    Value);

    HRESULT MultiplyStateTransform(uint32_t idx, const D3DMATRIX* pMatrix);

    HRESULT SetViewport(const D3DVIEWPORT9* pViewport);

    HRESULT SetScissorRect(const RECT* pRect);

    HRESULT SetClipPlane(DWORD Index, const float* pPlane);


    HRESULT SetVertexShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT SetVertexShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT SetVertexShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);


    HRESULT SetPixelShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT SetPixelShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT SetPixelShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);

    enum class D3D9StateFunction {
      Apply,
      Capture
    };

    template <typename Dst, typename Src>
    void ApplyOrCapture(Dst* dst, const Src* src) {
      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexDecl))
        dst->SetVertexDeclaration(src->vertexDecl);

      if (m_captures.flags.test(D3D9CapturedStateFlag::StreamFreq)) {
        for (uint32_t i = 0; i < caps::MaxStreams; i++) {
          if (m_captures.streamFreq[i])
            dst->SetStreamSourceFreq(i, src->streamFreq[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Indices))
        dst->SetIndices(src->indices);

      if (m_captures.flags.test(D3D9CapturedStateFlag::RenderStates)) {
        for (uint32_t i = 0; i < m_captures.renderStates.size(); i++) {
          if (m_captures.renderStates[i])
            dst->SetRenderState(D3DRENDERSTATETYPE(i), src->renderStates[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::SamplerStates)) {
        for (uint32_t i = 0; i < m_captures.samplerStates.size(); i++) {
          if (m_captures.samplers[i]) {
            for (uint32_t j = 0; j < m_captures.samplerStates[i].size(); j++) {
              if (m_captures.samplerStates[i][j])
                dst->SetStateSamplerState(i, D3DSAMPLERSTATETYPE(j), src->samplerStates[i][j]);
            }
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexBuffers)) {
        for (uint32_t i = 0; i < m_captures.vertexBuffers.size(); i++) {
          if (m_captures.vertexBuffers[i]) {
            const auto& vbo = src->vertexBuffers[i];
            dst->SetStreamSource(
              i,
              vbo.vertexBuffer,
              vbo.offset,
              vbo.stride);
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Material))
        dst->SetMaterial(&src->material);

      if (m_captures.flags.test(D3D9CapturedStateFlag::Textures)) {
        for (uint32_t i = 0; i < m_captures.textures.size(); i++) {
          if (m_captures.textures[i])
            dst->SetStateTexture(i, src->textures[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexShader))
        dst->SetVertexShader(src->vertexShader);

      if (m_captures.flags.test(D3D9CapturedStateFlag::PixelShader))
        dst->SetPixelShader(src->pixelShader);

      if (m_captures.flags.test(D3D9CapturedStateFlag::Transforms)) {
        for (uint32_t i = 0; i < m_captures.transforms.size(); i++) {
          if (m_captures.transforms[i])
            dst->SetStateTransform(i, reinterpret_cast<const D3DMATRIX*>(&src->transforms[i]));
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::TextureStages)) {
        for (uint32_t i = 0; i < m_captures.textureStages.size(); i++) {
          if (m_captures.textureStages[i]) {
            for (uint32_t j = 0; j < m_captures.textureStageStates[i].size(); j++) {
              if (m_captures.textureStageStates[i][j])
                dst->SetTextureStageState(i, (D3DTEXTURESTAGESTATETYPE)j, src->textureStages[i][j]);
            }
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Viewport))
        dst->SetViewport(&src->viewport);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ScissorRect))
        dst->SetScissorRect(&src->scissorRect);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ClipPlanes)) {
        for (uint32_t i = 0; i < m_captures.clipPlanes.size(); i++) {
          if (m_captures.clipPlanes[i])
            dst->SetClipPlane(i, src->clipPlanes[i].coeff);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VsConstants)) {
        for (uint32_t i = 0; i < m_captures.vsConsts.fConsts.size(); i++) {
          if (m_captures.vsConsts.fConsts[i])
            dst->SetVertexShaderConstantF(i, (float*)&src->consts[DxsoProgramTypes::VertexShader].hardware.fConsts[i], 1);
        }

        for (uint32_t i = 0; i < m_captures.vsConsts.iConsts.size(); i++) {
          if (m_captures.vsConsts.iConsts[i])
            dst->SetVertexShaderConstantI(i, (int*)&src->consts[DxsoProgramTypes::VertexShader].hardware.iConsts[i], 1);
        }

        uint32_t boolMask = 0;
        for (uint32_t i = 0; i < m_captures.vsConsts.bConsts.size(); i++) {
          if (m_captures.vsConsts.bConsts[i])
            boolMask |= 1u << i;
        }

        dst->SetVertexBoolBitfield(boolMask, src->consts[DxsoProgramTypes::VertexShader].hardware.boolBitfield);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::PsConstants)) {
        for (uint32_t i = 0; i < m_captures.psConsts.fConsts.size(); i++) {
          if (m_captures.psConsts.fConsts[i])
            dst->SetPixelShaderConstantF(i, (float*)&src->consts[DxsoProgramTypes::PixelShader].hardware.fConsts[i], 1);
        }

        for (uint32_t i = 0; i < m_captures.psConsts.iConsts.size(); i++) {
          if (m_captures.psConsts.iConsts[i])
            dst->SetPixelShaderConstantI(i, (int*)&src->consts[DxsoProgramTypes::PixelShader].hardware.iConsts[i], 1);
        }

        uint32_t boolMask = 0;
        for (uint32_t i = 0; i < m_captures.psConsts.bConsts.size(); i++) {
          if (m_captures.psConsts.bConsts[i])
            boolMask |= 1u << i;
        }

        dst->SetPixelBoolBitfield(boolMask, src->consts[DxsoProgramTypes::PixelShader].hardware.boolBitfield);
      }
    }

    template <D3D9StateFunction Func>
    void ApplyOrCapture() {
      if      constexpr (Func == D3D9StateFunction::Apply)
        ApplyOrCapture(m_parent, &m_state);
      else if constexpr (Func == D3D9StateFunction::Capture)
        ApplyOrCapture(this, m_deviceState);
    }

    template <
      DxsoProgramType  ProgramType,
      D3D9ConstantType ConstantType,
      typename         T>
    HRESULT SetShaderConstants(
            UINT  StartRegister,
      const T*    pConstantData,
            UINT  Count) {
      if constexpr (ProgramType == DxsoProgramTypes::VertexShader)
        m_captures.flags.set(D3D9CapturedStateFlag::VsConstants);
      else
        m_captures.flags.set(D3D9CapturedStateFlag::PsConstants);

      auto& captureSet = ProgramType == DxsoProgramTypes::VertexShader
        ? m_captures.vsConsts
        : m_captures.psConsts;

      for (uint32_t i = 0; i < Count; i++) {
        uint32_t reg = StartRegister + i;
        if      constexpr (ConstantType == D3D9ConstantType::Float)
          captureSet.fConsts[reg] = true;
        else if constexpr (ConstantType == D3D9ConstantType::Int)
          captureSet.iConsts[reg] = true;
        else if constexpr (ConstantType == D3D9ConstantType::Bool)
          captureSet.bConsts[reg] = true;
      }

      UpdateStateConstants<
        ProgramType,
        ConstantType,
        T>(
        &m_state,
        StartRegister,
        pConstantData,
        Count);

      return D3D_OK;
    }

    HRESULT SetVertexBoolBitfield(uint32_t mask, uint32_t bits);
    HRESULT SetPixelBoolBitfield(uint32_t mask, uint32_t bits);

    inline bool IsApplying() {
      return m_applying;
    }

  private:

    void CapturePixelRenderStates();
    void CapturePixelSamplerStates();
    void CapturePixelShaderStates();

    void CaptureVertexRenderStates();
    void CaptureVertexSamplerStates();
    void CaptureVertexShaderStates();

    void CaptureType(D3D9StateBlockType State);

    D3D9CapturableState  m_state;
    D3D9StateCaptures    m_captures;

    D3D9CapturableState* m_deviceState;

    bool                 m_applying;

  };

}