#include "d3d9_stateblock.h"

#include "d3d9_device.h"
#include "d3d9_vertex_declaration.h"
#include "d3d9_buffer.h"
#include "d3d9_shader.h"
#include "d3d9_texture.h"

#include "d3d9_util.h"

namespace dxvk {

  D3D9StateBlock::D3D9StateBlock(D3D9DeviceEx* pDevice, D3D9StateBlockType Type)
    : D3D9StateBlockBase(pDevice)
    , m_deviceState     (pDevice->GetRawState()) {
    CaptureType(Type);
  }


  HRESULT STDMETHODCALLTYPE D3D9StateBlock::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DStateBlock9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9StateBlock::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9StateBlock::Capture() {
    if (m_captures.flags.test(D3D9CapturedStateFlag::VertexDecl))
      SetVertexDeclaration(m_deviceState->vertexDecl.ptr());

    ApplyOrCapture<D3D9StateFunction::Capture>();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9StateBlock::Apply() {
    m_applying = true;

    if (m_captures.flags.test(D3D9CapturedStateFlag::VertexDecl) && m_state.vertexDecl != nullptr)
      m_parent->SetVertexDeclaration(m_state.vertexDecl.ptr());

    ApplyOrCapture<D3D9StateFunction::Apply>();
    m_applying = false;

    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetVertexDeclaration(D3D9VertexDecl* pDecl) {
    m_state.vertexDecl = pDecl;

    m_captures.flags.set(D3D9CapturedStateFlag::VertexDecl);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetIndices(D3D9IndexBuffer* pIndexData) {
    m_state.indices = pIndexData;

    m_captures.flags.set(D3D9CapturedStateFlag::Indices);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    m_state.renderStates[State] = Value;

    m_captures.flags.set(D3D9CapturedStateFlag::RenderStates);
    m_captures.renderStates.set(State, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStateSamplerState(
          DWORD               StateSampler,
          D3DSAMPLERSTATETYPE Type,
          DWORD               Value) {
    m_state.samplerStates[StateSampler][Type] = Value;

    m_captures.flags.set(D3D9CapturedStateFlag::SamplerStates);
    m_captures.samplers.set(StateSampler, true);
    m_captures.samplerStates[StateSampler].set(Type, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStreamSource(
          UINT                StreamNumber,
          D3D9VertexBuffer*   pStreamData,
          UINT                OffsetInBytes,
          UINT                Stride) {
    m_state.vertexBuffers[StreamNumber].vertexBuffer = pStreamData;

    m_state.vertexBuffers[StreamNumber].offset = OffsetInBytes;
    m_state.vertexBuffers[StreamNumber].stride = Stride;

    m_captures.flags.set(D3D9CapturedStateFlag::VertexBuffers);
    m_captures.vertexBuffers.set(StreamNumber, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    m_state.streamFreq[StreamNumber] = Setting;

    m_captures.flags.set(D3D9CapturedStateFlag::StreamFreq);
    m_captures.streamFreq.set(StreamNumber, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture) {
    TextureChangePrivate(m_state.textures[StateSampler], pTexture);

    m_captures.flags.set(D3D9CapturedStateFlag::Textures);
    m_captures.textures.set(StateSampler, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetVertexShader(D3D9VertexShader* pShader) {
    m_state.vertexShader = pShader;

    m_captures.flags.set(D3D9CapturedStateFlag::VertexShader);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetPixelShader(D3D9PixelShader* pShader) {
    m_state.pixelShader = pShader;

    m_captures.flags.set(D3D9CapturedStateFlag::PixelShader);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetMaterial(const D3DMATERIAL9* pMaterial) {
    m_state.material = *pMaterial;

    m_captures.flags.set(D3D9CapturedStateFlag::Material);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix) {
    m_state.transforms[idx] = ConvertMatrix(pMatrix);

    m_captures.flags.set(D3D9CapturedStateFlag::Transforms);
    m_captures.transforms.set(idx, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetStateTextureStageState(
          DWORD                      Stage,
          D3D9TextureStageStateTypes Type,
          DWORD                      Value) {
    m_state.textureStages[Stage][Type] = Value;

    m_captures.flags.set(D3D9CapturedStateFlag::TextureStages);
    m_captures.textureStages.set(Stage, true);
    m_captures.textureStageStates[Stage].set(Type, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::MultiplyStateTransform(uint32_t idx, const D3DMATRIX* pMatrix) {
    m_state.transforms[idx] = m_state.transforms[idx] * ConvertMatrix(pMatrix);

    m_captures.flags.set(D3D9CapturedStateFlag::Transforms);
    m_captures.transforms.set(idx, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetViewport(const D3DVIEWPORT9* pViewport) {
    m_state.viewport = *pViewport;

    m_captures.flags.set(D3D9CapturedStateFlag::Viewport);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetScissorRect(const RECT* pRect) {
    m_state.scissorRect = *pRect;

    m_captures.flags.set(D3D9CapturedStateFlag::ScissorRect);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetClipPlane(DWORD Index, const float* pPlane) {
    for (uint32_t i = 0; i < 4; i++)
      m_state.clipPlanes[Index].coeff[i] = pPlane[i];

    m_captures.flags.set(D3D9CapturedStateFlag::ClipPlanes);
    m_captures.clipPlanes.set(Index, true);
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetVertexShaderConstantF(
          UINT   StartRegister,
    const float* pConstantData,
          UINT   Vector4fCount) {
    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT D3D9StateBlock::SetVertexShaderConstantI(
          UINT StartRegister,
    const int* pConstantData,
          UINT Vector4iCount) {
    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT D3D9StateBlock::SetVertexShaderConstantB(
          UINT  StartRegister,
    const BOOL* pConstantData,
          UINT  BoolCount) {
    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT D3D9StateBlock::SetPixelShaderConstantF(
          UINT   StartRegister,
    const float* pConstantData,
          UINT   Vector4fCount) {
    return SetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT D3D9StateBlock::SetPixelShaderConstantI(
          UINT StartRegister,
    const int* pConstantData,
          UINT Vector4iCount) {
    return SetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT D3D9StateBlock::SetPixelShaderConstantB(
          UINT  StartRegister,
    const BOOL* pConstantData,
          UINT  BoolCount) {
    return SetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT D3D9StateBlock::SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.vsConsts.bConsts[idx] &= ~mask;
    m_state.vsConsts.bConsts[idx] |= bits & mask;
    return D3D_OK;
  }


  HRESULT D3D9StateBlock::SetPixelBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.psConsts.bConsts[idx] &= ~mask;
    m_state.psConsts.bConsts[idx] |= bits & mask;
    return D3D_OK;
  }


  void D3D9StateBlock::CapturePixelRenderStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::RenderStates);

    m_captures.renderStates.set(D3DRS_ZENABLE, true);
    m_captures.renderStates.set(D3DRS_FILLMODE, true);
    m_captures.renderStates.set(D3DRS_SHADEMODE, true);
    m_captures.renderStates.set(D3DRS_ZWRITEENABLE, true);
    m_captures.renderStates.set(D3DRS_ALPHATESTENABLE, true);
    m_captures.renderStates.set(D3DRS_LASTPIXEL, true);
    m_captures.renderStates.set(D3DRS_SRCBLEND, true);
    m_captures.renderStates.set(D3DRS_DESTBLEND, true);
    m_captures.renderStates.set(D3DRS_ZFUNC, true);
    m_captures.renderStates.set(D3DRS_ALPHAREF, true);
    m_captures.renderStates.set(D3DRS_ALPHAFUNC, true);
    m_captures.renderStates.set(D3DRS_DITHERENABLE, true);
    m_captures.renderStates.set(D3DRS_FOGSTART, true);
    m_captures.renderStates.set(D3DRS_FOGEND, true);
    m_captures.renderStates.set(D3DRS_FOGDENSITY, true);
    m_captures.renderStates.set(D3DRS_ALPHABLENDENABLE, true);
    m_captures.renderStates.set(D3DRS_DEPTHBIAS, true);
    m_captures.renderStates.set(D3DRS_STENCILENABLE, true);
    m_captures.renderStates.set(D3DRS_STENCILFAIL, true);
    m_captures.renderStates.set(D3DRS_STENCILZFAIL, true);
    m_captures.renderStates.set(D3DRS_STENCILPASS, true);
    m_captures.renderStates.set(D3DRS_STENCILFUNC, true);
    m_captures.renderStates.set(D3DRS_STENCILREF, true);
    m_captures.renderStates.set(D3DRS_STENCILMASK, true);
    m_captures.renderStates.set(D3DRS_STENCILWRITEMASK, true);
    m_captures.renderStates.set(D3DRS_TEXTUREFACTOR, true);
    m_captures.renderStates.set(D3DRS_WRAP0, true);
    m_captures.renderStates.set(D3DRS_WRAP1, true);
    m_captures.renderStates.set(D3DRS_WRAP2, true);
    m_captures.renderStates.set(D3DRS_WRAP3, true);
    m_captures.renderStates.set(D3DRS_WRAP4, true);
    m_captures.renderStates.set(D3DRS_WRAP5, true);
    m_captures.renderStates.set(D3DRS_WRAP6, true);
    m_captures.renderStates.set(D3DRS_WRAP7, true);
    m_captures.renderStates.set(D3DRS_WRAP8, true);
    m_captures.renderStates.set(D3DRS_WRAP9, true);
    m_captures.renderStates.set(D3DRS_WRAP10, true);
    m_captures.renderStates.set(D3DRS_WRAP11, true);
    m_captures.renderStates.set(D3DRS_WRAP12, true);
    m_captures.renderStates.set(D3DRS_WRAP13, true);
    m_captures.renderStates.set(D3DRS_WRAP14, true);
    m_captures.renderStates.set(D3DRS_WRAP15, true);
    m_captures.renderStates.set(D3DRS_COLORWRITEENABLE, true);
    m_captures.renderStates.set(D3DRS_BLENDOP, true);
    m_captures.renderStates.set(D3DRS_SCISSORTESTENABLE, true);
    m_captures.renderStates.set(D3DRS_SLOPESCALEDEPTHBIAS, true);
    m_captures.renderStates.set(D3DRS_ANTIALIASEDLINEENABLE, true);
    m_captures.renderStates.set(D3DRS_TWOSIDEDSTENCILMODE, true);
    m_captures.renderStates.set(D3DRS_CCW_STENCILFAIL, true);
    m_captures.renderStates.set(D3DRS_CCW_STENCILZFAIL, true);
    m_captures.renderStates.set(D3DRS_CCW_STENCILPASS, true);
    m_captures.renderStates.set(D3DRS_CCW_STENCILFUNC, true);
    m_captures.renderStates.set(D3DRS_COLORWRITEENABLE1, true);
    m_captures.renderStates.set(D3DRS_COLORWRITEENABLE2, true);
    m_captures.renderStates.set(D3DRS_COLORWRITEENABLE3, true);
    m_captures.renderStates.set(D3DRS_BLENDFACTOR, true);
    m_captures.renderStates.set(D3DRS_SRGBWRITEENABLE, true);
    m_captures.renderStates.set(D3DRS_SEPARATEALPHABLENDENABLE, true);
    m_captures.renderStates.set(D3DRS_SRCBLENDALPHA, true);
    m_captures.renderStates.set(D3DRS_DESTBLENDALPHA, true);
    m_captures.renderStates.set(D3DRS_BLENDOPALPHA, true);
  }


  void D3D9StateBlock::CapturePixelSamplerStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::SamplerStates);

    for (uint32_t i = 0; i < 17; i++) {
      m_captures.samplers.set(i, true);

      m_captures.samplerStates[i].set(D3DSAMP_ADDRESSU, true);
      m_captures.samplerStates[i].set(D3DSAMP_ADDRESSV, true);
      m_captures.samplerStates[i].set(D3DSAMP_ADDRESSW, true);
      m_captures.samplerStates[i].set(D3DSAMP_BORDERCOLOR, true);
      m_captures.samplerStates[i].set(D3DSAMP_MAGFILTER, true);
      m_captures.samplerStates[i].set(D3DSAMP_MINFILTER, true);
      m_captures.samplerStates[i].set(D3DSAMP_MIPFILTER, true);
      m_captures.samplerStates[i].set(D3DSAMP_MIPMAPLODBIAS, true);
      m_captures.samplerStates[i].set(D3DSAMP_MAXMIPLEVEL, true);
      m_captures.samplerStates[i].set(D3DSAMP_MAXANISOTROPY, true);
      m_captures.samplerStates[i].set(D3DSAMP_SRGBTEXTURE, true);
      m_captures.samplerStates[i].set(D3DSAMP_ELEMENTINDEX, true);
    }
  }


  void D3D9StateBlock::CapturePixelShaderStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::PixelShader);
    m_captures.flags.set(D3D9CapturedStateFlag::PsConstants);

    m_captures.psConsts.fConsts.setAll();
    m_captures.psConsts.iConsts.setAll();
    m_captures.psConsts.bConsts.setAll();
  }


  void D3D9StateBlock::CaptureVertexRenderStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::RenderStates);

    m_captures.renderStates.set(D3DRS_CULLMODE, true);
    m_captures.renderStates.set(D3DRS_FOGENABLE, true);
    m_captures.renderStates.set(D3DRS_FOGCOLOR, true);
    m_captures.renderStates.set(D3DRS_FOGTABLEMODE, true);
    m_captures.renderStates.set(D3DRS_FOGSTART, true);
    m_captures.renderStates.set(D3DRS_FOGEND, true);
    m_captures.renderStates.set(D3DRS_FOGDENSITY, true);
    m_captures.renderStates.set(D3DRS_RANGEFOGENABLE, true);
    m_captures.renderStates.set(D3DRS_AMBIENT, true);
    m_captures.renderStates.set(D3DRS_COLORVERTEX, true);
    m_captures.renderStates.set(D3DRS_FOGVERTEXMODE, true);
    m_captures.renderStates.set(D3DRS_CLIPPING, true);
    m_captures.renderStates.set(D3DRS_LIGHTING, true);
    m_captures.renderStates.set(D3DRS_LOCALVIEWER, true);
    m_captures.renderStates.set(D3DRS_EMISSIVEMATERIALSOURCE, true);
    m_captures.renderStates.set(D3DRS_AMBIENTMATERIALSOURCE, true);
    m_captures.renderStates.set(D3DRS_DIFFUSEMATERIALSOURCE, true);
    m_captures.renderStates.set(D3DRS_SPECULARMATERIALSOURCE, true);
    m_captures.renderStates.set(D3DRS_VERTEXBLEND, true);
    m_captures.renderStates.set(D3DRS_CLIPPLANEENABLE, true);
    m_captures.renderStates.set(D3DRS_POINTSIZE, true);
    m_captures.renderStates.set(D3DRS_POINTSIZE_MIN, true);
    m_captures.renderStates.set(D3DRS_POINTSPRITEENABLE, true);
    m_captures.renderStates.set(D3DRS_POINTSCALEENABLE, true);
    m_captures.renderStates.set(D3DRS_POINTSCALE_A, true);
    m_captures.renderStates.set(D3DRS_POINTSCALE_B, true);
    m_captures.renderStates.set(D3DRS_POINTSCALE_C, true);
    m_captures.renderStates.set(D3DRS_MULTISAMPLEANTIALIAS, true);
    m_captures.renderStates.set(D3DRS_MULTISAMPLEMASK, true);
    m_captures.renderStates.set(D3DRS_PATCHEDGESTYLE, true);
    m_captures.renderStates.set(D3DRS_POINTSIZE_MAX, true);
    m_captures.renderStates.set(D3DRS_INDEXEDVERTEXBLENDENABLE, true);
    m_captures.renderStates.set(D3DRS_TWEENFACTOR, true);
    m_captures.renderStates.set(D3DRS_POSITIONDEGREE, true);
    m_captures.renderStates.set(D3DRS_NORMALDEGREE, true);
    m_captures.renderStates.set(D3DRS_MINTESSELLATIONLEVEL, true);
    m_captures.renderStates.set(D3DRS_MAXTESSELLATIONLEVEL, true);
    m_captures.renderStates.set(D3DRS_ADAPTIVETESS_X, true);
    m_captures.renderStates.set(D3DRS_ADAPTIVETESS_Y, true);
    m_captures.renderStates.set(D3DRS_ADAPTIVETESS_Z, true);
    m_captures.renderStates.set(D3DRS_ADAPTIVETESS_W, true);
    m_captures.renderStates.set(D3DRS_ENABLEADAPTIVETESSELLATION, true);
    m_captures.renderStates.set(D3DRS_NORMALIZENORMALS, true);
    m_captures.renderStates.set(D3DRS_SPECULARENABLE, true);
    m_captures.renderStates.set(D3DRS_SHADEMODE, true);
  }


  void D3D9StateBlock::CaptureVertexSamplerStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::SamplerStates);

    for (uint32_t i = 17; i < SamplerCount; i++) {
      m_captures.samplers.set(i, true);
      m_captures.samplerStates[i].set(D3DSAMP_DMAPOFFSET, true);
    }
  }


  void D3D9StateBlock::CaptureVertexShaderStates() {
    m_captures.flags.set(D3D9CapturedStateFlag::VertexShader);
    m_captures.flags.set(D3D9CapturedStateFlag::VsConstants);

    for (uint32_t i = 0; i < m_parent->GetVertexConstantLayout().floatCount / 32; i++)
      m_captures.vsConsts.fConsts.dword(i) = std::numeric_limits<uint32_t>::max();

    for (uint32_t i = 0; i < m_parent->GetVertexConstantLayout().intCount / 32; i++)
      m_captures.vsConsts.iConsts.dword(i) = std::numeric_limits<uint32_t>::max();

    for (uint32_t i = 0; i < m_parent->GetVertexConstantLayout().bitmaskCount; i++)
      m_captures.vsConsts.bConsts.dword(i) = std::numeric_limits<uint32_t>::max();
  }


  void D3D9StateBlock::CaptureType(D3D9StateBlockType Type) {
    if (Type == D3D9StateBlockType::PixelState || Type == D3D9StateBlockType::All) {
      CapturePixelRenderStates();
      CapturePixelSamplerStates();
      CapturePixelShaderStates();

      m_captures.flags.set(D3D9CapturedStateFlag::TextureStages);
      m_captures.textureStages.setAll();
      for (auto& stage : m_captures.textureStageStates)
        stage.setAll();
    }

    if (Type == D3D9StateBlockType::VertexState || Type == D3D9StateBlockType::All) {
      CaptureVertexRenderStates();
      CaptureVertexSamplerStates();
      CaptureVertexShaderStates();

      m_captures.flags.set(D3D9CapturedStateFlag::VertexDecl);
      m_captures.flags.set(D3D9CapturedStateFlag::StreamFreq);

      for (uint32_t i = 0; i < caps::MaxStreams; i++)
        m_captures.streamFreq.set(i, true);
    }

    if (Type == D3D9StateBlockType::All) {
      m_captures.flags.set(D3D9CapturedStateFlag::Textures);
      m_captures.textures.setAll();

      m_captures.flags.set(D3D9CapturedStateFlag::VertexBuffers);
      m_captures.vertexBuffers.setAll();

      m_captures.flags.set(D3D9CapturedStateFlag::Indices);
      m_captures.flags.set(D3D9CapturedStateFlag::Viewport);
      m_captures.flags.set(D3D9CapturedStateFlag::ScissorRect);

      m_captures.flags.set(D3D9CapturedStateFlag::ClipPlanes);
      m_captures.clipPlanes.setAll();

      m_captures.flags.set(D3D9CapturedStateFlag::Transforms);
      m_captures.transforms.setAll();

      m_captures.flags.set(D3D9CapturedStateFlag::Material);
    }

    if (Type != D3D9StateBlockType::None)
      this->Capture();
  }

}