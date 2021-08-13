#pragma once

#include "d3d9_device_child.h"
#include "d3d9_device.h"
#include "d3d9_state.h"

#include "../util/util_bit.h"

namespace dxvk {

  enum class D3D9CapturedStateFlag : uint32_t {
    VertexDecl,
    Indices,
    RenderStates,
    SamplerStates,
    VertexBuffers,
    Textures,
    VertexShader,
    PixelShader,
    Viewport,
    ScissorRect,
    ClipPlanes,
    VsConstants,
    PsConstants,
    StreamFreq,
    Transforms,
    TextureStages,
    Material
  };

  using D3D9CapturedStateFlags = Flags<D3D9CapturedStateFlag>;

  struct D3D9StateCaptures {
    D3D9CapturedStateFlags flags;

    bit::bitset<RenderStateCount>                       renderStates;

    bit::bitset<SamplerCount>                           samplers;
    std::array<
      bit::bitset<SamplerStateCount>,
      SamplerCount>                                     samplerStates;

    bit::bitset<caps::MaxStreams>                       vertexBuffers;
    bit::bitset<SamplerCount>                           textures;
    bit::bitset<caps::MaxClipPlanes>                    clipPlanes;
    bit::bitset<caps::MaxStreams>                       streamFreq;
    bit::bitset<caps::MaxTransforms>                    transforms;
    bit::bitset<caps::TextureStageCount>                textureStages;
    std::array<
      bit::bitset<TextureStageStateCount>,
      caps::TextureStageCount>                          textureStageStates;

    struct {
      bit::bitset<caps::MaxFloatConstantsSoftware>      fConsts;
      bit::bitset<caps::MaxOtherConstantsSoftware>      iConsts;
      bit::bitset<caps::MaxOtherConstantsSoftware>      bConsts;
    } vsConsts;

    struct {
      bit::bitset<caps::MaxFloatConstantsPS>            fConsts;
      bit::bitset<caps::MaxOtherConstants>              iConsts;
      bit::bitset<caps::MaxOtherConstants>              bConsts;
    } psConsts;
  };

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

    HRESULT SetStateTextureStageState(
            DWORD                      Stage,
            D3D9TextureStageStateTypes Type,
            DWORD                      Value);

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
      if (m_captures.flags.test(D3D9CapturedStateFlag::StreamFreq)) {
        for (uint32_t idx : bit::BitMask(m_captures.streamFreq.dword(0)))
          dst->SetStreamSourceFreq(idx, src->streamFreq[idx]);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Indices))
        dst->SetIndices(src->indices.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::RenderStates)) {
        for (uint32_t i = 0; i < m_captures.renderStates.dwordCount(); i++) {
          for (uint32_t rs : bit::BitMask(m_captures.renderStates.dword(i))) {
            uint32_t idx = i * 32 + rs;

            dst->SetRenderState(D3DRENDERSTATETYPE(idx), src->renderStates[idx]);
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::SamplerStates)) {
        for (uint32_t samplerIdx : bit::BitMask(m_captures.samplers.dword(0))) {
          for (uint32_t stateIdx : bit::BitMask(m_captures.samplerStates[samplerIdx].dword(0)))
            dst->SetStateSamplerState(samplerIdx, D3DSAMPLERSTATETYPE(stateIdx), src->samplerStates[samplerIdx][stateIdx]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexBuffers)) {
        for (uint32_t idx : bit::BitMask(m_captures.vertexBuffers.dword(0))) {
          const auto& vbo = src->vertexBuffers[idx];
          dst->SetStreamSource(
            idx,
            vbo.vertexBuffer.ptr(),
            vbo.offset,
            vbo.stride);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Material))
        dst->SetMaterial(&src->material);

      if (m_captures.flags.test(D3D9CapturedStateFlag::Textures)) {
        for (uint32_t idx : bit::BitMask(m_captures.textures.dword(0)))
          dst->SetStateTexture(idx, src->textures[idx]);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VertexShader))
        dst->SetVertexShader(src->vertexShader.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::PixelShader))
        dst->SetPixelShader(src->pixelShader.ptr());

      if (m_captures.flags.test(D3D9CapturedStateFlag::Transforms)) {
        for (uint32_t i = 0; i < m_captures.transforms.dwordCount(); i++) {
          for (uint32_t trans : bit::BitMask(m_captures.transforms.dword(i))) {
            uint32_t idx = i * 32 + trans;

            dst->SetStateTransform(idx, reinterpret_cast<const D3DMATRIX*>(&src->transforms[idx]));
          }
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::TextureStages)) {
        for (uint32_t stageIdx : bit::BitMask(m_captures.textureStages.dword(0))) {
          for (uint32_t stateIdx : bit::BitMask(m_captures.textureStageStates[stageIdx].dword(0)))
            dst->SetStateTextureStageState(stageIdx, D3D9TextureStageStateTypes(stateIdx), src->textureStages[stageIdx][stateIdx]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::Viewport))
        dst->SetViewport(&src->viewport);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ScissorRect))
        dst->SetScissorRect(&src->scissorRect);

      if (m_captures.flags.test(D3D9CapturedStateFlag::ClipPlanes)) {
        for (uint32_t idx : bit::BitMask(m_captures.clipPlanes.dword(0)))
          dst->SetClipPlane(idx, src->clipPlanes[idx].coeff);
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::VsConstants)) {
        for (uint32_t i = 0; i < m_captures.vsConsts.fConsts.dwordCount(); i++) {
          for (uint32_t consts : bit::BitMask(m_captures.vsConsts.fConsts.dword(i))) {
            uint32_t idx = i * 32 + consts;

            dst->SetVertexShaderConstantF(idx, (float*)&src->vsConsts.fConsts[idx], 1);
          }
        }

        for (uint32_t i = 0; i < m_captures.vsConsts.iConsts.dwordCount(); i++) {
          for (uint32_t consts : bit::BitMask(m_captures.vsConsts.iConsts.dword(i))) {
            uint32_t idx = i * 32 + consts;

            dst->SetVertexShaderConstantI(idx, (int*)&src->vsConsts.iConsts[idx], 1);
          }
        }

        if (m_captures.vsConsts.bConsts.any()) {
          for (uint32_t i = 0; i < m_captures.vsConsts.bConsts.dwordCount(); i++)
            dst->SetVertexBoolBitfield(i, m_captures.vsConsts.bConsts.dword(i), src->vsConsts.bConsts[i]);
        }
      }

      if (m_captures.flags.test(D3D9CapturedStateFlag::PsConstants)) {
        for (uint32_t i = 0; i < m_captures.psConsts.fConsts.dwordCount(); i++) {
          for (uint32_t consts : bit::BitMask(m_captures.psConsts.fConsts.dword(i))) {
            uint32_t idx = i * 32 + consts;

            dst->SetPixelShaderConstantF(idx, (float*)&src->psConsts.fConsts[idx], 1);
          }
        }

        for (uint32_t i = 0; i < m_captures.psConsts.iConsts.dwordCount(); i++) {
          for (uint32_t consts : bit::BitMask(m_captures.psConsts.iConsts.dword(i))) {
            uint32_t idx = i * 32 + consts;

            dst->SetPixelShaderConstantI(idx, (int*)&src->psConsts.iConsts[idx], 1);
          }
        }

        if (m_captures.psConsts.bConsts.any()) {
          for (uint32_t i = 0; i < m_captures.psConsts.bConsts.dwordCount(); i++)
            dst->SetPixelBoolBitfield(i, m_captures.psConsts.bConsts.dword(i), src->psConsts.bConsts[i]);
        }
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
      auto SetHelper = [&](auto& setCaptures) {
        if constexpr (ProgramType == DxsoProgramTypes::VertexShader)
          m_captures.flags.set(D3D9CapturedStateFlag::VsConstants);
        else
          m_captures.flags.set(D3D9CapturedStateFlag::PsConstants);

        for (uint32_t i = 0; i < Count; i++) {
          uint32_t reg = StartRegister + i;
          if      constexpr (ConstantType == D3D9ConstantType::Float)
            setCaptures.fConsts.set(reg, true);
          else if constexpr (ConstantType == D3D9ConstantType::Int)
            setCaptures.iConsts.set(reg, true);
          else if constexpr (ConstantType == D3D9ConstantType::Bool)
            setCaptures.bConsts.set(reg, true);
        }

        UpdateStateConstants<
          ProgramType,
          ConstantType,
          T>(
            &m_state,
            StartRegister,
            pConstantData,
            Count,
            false);

        return D3D_OK;
      };

      return ProgramType == DxsoProgramTypes::VertexShader
        ? SetHelper(m_captures.vsConsts)
        : SetHelper(m_captures.psConsts);
    }

    HRESULT SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits);
    HRESULT SetPixelBoolBitfield (uint32_t idx, uint32_t mask, uint32_t bits);

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

    bool                 m_applying = false;

  };

}