#pragma once

// Implements IDirect3DDevice8

#include "d3d8_include.h"
#include "d3d8_texture.h"
#include "d3d8_buffer.h"
#include "d3d8_d3d9_util.h"

#include <vector>
#include <type_traits>
#include <unordered_map>

// so we dont have to write 100 stubs in development
// TODO: eliminate use of this by implementing stubs
#define D3D8_DEVICE_STUB(...) \
(__VA_ARGS__) { \
  Logger::warn("D3D8DeviceEx: STUB (" #__VA_ARGS__ ") -> HRESULT"); \
  return D3DERR_INVALIDCALL;\
}

#define D3D8_DEVICE_STUB_(Name, ...) \
(__VA_ARGS__) { \
  Logger::warn("D3D8DeviceEx::" #Name " STUB (" #__VA_ARGS__ ") -> HRESULT"); \
  return D3DERR_INVALIDCALL;\
}


#define D3D8_DEVICE_STUB_VOID(...) \
(__VA_ARGS__) { \
  Logger::warn("D3D8DeviceEx: STUB (" #__VA_ARGS__ ") -> void"); \
  return;\
}

namespace dxvk {

  class D3D8InterfaceEx;
  class D3D8SwapChainEx;
  class D3D8CommonTexture;
  class D3D8CommonBuffer;
  class D3D8CommonShader;
  class D3D8ShaderModuleSet;
  class D3D8Initializer;
  class D3D8Query;
  class D3D8StateBlock;
  class D3D8FormatHelper;

  enum class D3D8DeviceFlag : uint32_t {
    DirtyFramebuffer,
    DirtyClipPlanes,
    DirtyDepthStencilState,
    DirtyBlendState,
    DirtyRasterizerState,
    DirtyDepthBias,
    DirtyAlphaTestState,
    DirtyInputLayout,
    DirtyViewportScissor,
    DirtyMultiSampleState,

    DirtyFogState,
    DirtyFogColor,
    DirtyFogDensity,
    DirtyFogScale,
    DirtyFogEnd,

    DirtyFFVertexData,
    DirtyFFVertexBlend,
    DirtyFFVertexShader,
    DirtyFFPixelShader,
    DirtyFFViewport,
    DirtyFFPixelData,
    DirtyProgVertexShader,
    DirtySharedPixelShaderData,
    ValidSampleMask,
    DirtyDepthBounds,
    DirtyPointScale,
  };

  using D3D8DeviceFlags = Flags<D3D8DeviceFlag>;

  /*struct D3D8DrawInfo {
    uint32_t vertexCount;
    uint32_t instanceCount;
  };

  struct D3D8SamplerPair {
    Rc<DxvkSampler> color;
    Rc<DxvkSampler> depth;
  };

  struct D3D8UPBufferSlice {
    DxvkBufferSlice slice = {};
    void*           mapPtr = nullptr;
  };*/

  using D3D8DeviceBase = D3D8WrappedObject<d3d9::IDirect3DDevice9, IDirect3DDevice8>;
  class D3D8DeviceEx final : public D3D8DeviceBase {
    /*constexpr static uint32_t DefaultFrameLatency = 3;
    constexpr static uint32_t MaxFrameLatency     = 20;

    constexpr static uint32_t MinFlushIntervalUs = 750;
    constexpr static uint32_t IncFlushIntervalUs = 250;
    constexpr static uint32_t MaxPendingSubmits = 6;

    constexpr static uint32_t NullStreamIdx = caps::MaxStreams;*/

    friend class D3D8SwapChainEx;
  public:

    D3D8DeviceEx(
      D3D8InterfaceEx*              pParent,
      Com<d3d9::IDirect3DDevice9>&& pDevice,
      //D3D8Adapter*                    pAdapter,
      D3DDEVTYPE                    DeviceType,
      HWND                          hFocusWindow,
      DWORD                         BehaviorFlags);

    ~D3D8DeviceEx();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    /* Direct3D 8 Exclusive Methods */
    STDMETHOD(CopyRects) D3D8_DEVICE_STUB(THIS_ IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects, IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);
    STDMETHOD(CreateImageSurface) D3D8_DEVICE_STUB(THIS_ UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface);
    STDMETHOD(DeletePixelShader) D3D8_DEVICE_STUB(THIS_ DWORD Handle);
    STDMETHOD(DeleteStateBlock) D3D8_DEVICE_STUB(THIS_ DWORD Token);
    STDMETHOD(DeleteVertexShader) D3D8_DEVICE_STUB(THIS_ DWORD Handle);
    STDMETHOD(GetPixelShaderConstant) D3D8_DEVICE_STUB(THIS_ DWORD Register, void* pConstantData, DWORD ConstantCount);
    STDMETHOD(GetPixelShaderFunction) D3D8_DEVICE_STUB(THIS_ DWORD Handle, void* pData, DWORD* pSizeOfData);
    STDMETHOD(GetVertexShaderConstant) D3D8_DEVICE_STUB(THIS_ DWORD Register, void* pConstantData, DWORD ConstantCount);
    STDMETHOD(GetVertexShaderDeclaration) D3D8_DEVICE_STUB(THIS_ DWORD Handle, void* pData, DWORD* pSizeOfData);
    STDMETHOD(GetVertexShaderFunction) D3D8_DEVICE_STUB(THIS_ DWORD Handle, void* pData, DWORD* pSizeOfData);

    STDMETHOD(GetInfo) D3D8_DEVICE_STUB(THIS_ DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize);


    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();

    UINT    STDMETHODCALLTYPE GetAvailableTextureMem() { return GetD3D9()->GetAvailableTextureMem(); }

    // TODO?
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD bytes) { return D3D_OK; }

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** ppD3D8);

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* pCaps) {
      d3d9::D3DCAPS9 caps9;
      HRESULT res = GetD3D9()->GetDeviceCaps(&caps9);
      dxvk::ConvertCaps8(caps9, pCaps);
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode) {
      // swap chain 0
      return GetD3D9()->GetDisplayMode(0, (d3d9::D3DDISPLAYMODE*)pMode);
    }

    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
      return GetD3D9()->GetCreationParameters((d3d9::D3DDEVICE_CREATION_PARAMETERS*)pParameters);
    }

    // TODO: SetCursorProperties
    HRESULT STDMETHODCALLTYPE SetCursorProperties D3D8_DEVICE_STUB(
      UINT               XHotSpot,
      UINT               YHotSpot,
      IDirect3DSurface8* pCursorBitmap);

    void    STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags) {
      // TODO: do we need to convert from screenspace?
      //GetD3D9()->SetCursorPosition(XScreenSpace, YScreenSpace, Flags);
    }

    BOOL    STDMETHODCALLTYPE ShowCursor(BOOL bShow) { return GetD3D9()->ShowCursor(bShow); }

    // TODO: CreateAdditionalSwapChain
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain D3D8_DEVICE_STUB(
      D3DPRESENT_PARAMETERS* pPresentationParameters,
      IDirect3DSwapChain8** ppSwapChain);


    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
      d3d9::D3DPRESENT_PARAMETERS params = ConvertPresentParameters9(pPresentationParameters);
      return GetD3D9()->Reset(&params);
    }

    HRESULT STDMETHODCALLTYPE Present(
      const RECT* pSourceRect,
      const RECT* pDestRect,
            HWND hDestWindowOverride,
      const RGNDATA* pDirtyRegion) {
      return GetD3D9()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
            UINT iBackBuffer,
            D3DBACKBUFFER_TYPE Type,
            IDirect3DSurface8** ppBackBuffer) {
      d3d9::IDirect3DSurface9* pBackBuffer;
      HRESULT res = GetD3D9()->GetBackBuffer(0, iBackBuffer, (d3d9::D3DBACKBUFFER_TYPE)Type, &pBackBuffer);

      (*ppBackBuffer) = (IDirect3DSurface8*)pBackBuffer;
      return res;
    }

    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
      return GetD3D9()->GetRasterStatus(0, (d3d9::D3DRASTER_STATUS*)pRasterStatus);
    }

    void    STDMETHODCALLTYPE SetGammaRamp D3D8_DEVICE_STUB_VOID(DWORD Flags, const D3DGAMMARAMP* pRamp);

    void    STDMETHODCALLTYPE SetGammaRamp D3D8_DEVICE_STUB_VOID(const D3DGAMMARAMP* pRamp);


    void    STDMETHODCALLTYPE GetGammaRamp D3D8_DEVICE_STUB_VOID(D3DGAMMARAMP* pRamp);

    HRESULT STDMETHODCALLTYPE CreateTexture (
            UINT                Width,
            UINT                Height,
            UINT                Levels,
            DWORD               Usage,
            D3DFORMAT           Format,
            D3DPOOL             Pool,
            IDirect3DTexture8** ppTexture) {
      Com<d3d9::IDirect3DTexture9> pTex9 = nullptr;
      HRESULT res = GetD3D9()->CreateTexture(
        Width,
        Height,
        Levels,
        Usage,
        (d3d9::D3DFORMAT)Format,
        (d3d9::D3DPOOL)Pool,
        &pTex9,
        NULL);

      *ppTexture = ref(new D3D8Texture2D(this, std::move(pTex9)));

      return res;
    }

    HRESULT STDMETHODCALLTYPE CreateVolumeTexture D3D8_DEVICE_STUB(
            UINT                      Width,
            UINT                      Height,
            UINT                      Depth,
            UINT                      Levels,
            DWORD                     Usage,
            D3DFORMAT                 Format,
            D3DPOOL                   Pool,
            IDirect3DVolumeTexture8** ppVolumeTexture);

    HRESULT STDMETHODCALLTYPE CreateCubeTexture D3D8_DEVICE_STUB(
          UINT                      EdgeLength,
            UINT                    Levels,
            DWORD                   Usage,
            D3DFORMAT               Format,
            D3DPOOL                 Pool,
            IDirect3DCubeTexture8** ppCubeTexture);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(
            UINT                     Length,
            DWORD                    Usage,
            DWORD                    FVF,
            D3DPOOL                  Pool,
            IDirect3DVertexBuffer8** ppVertexBuffer) {
      
      Com<d3d9::IDirect3DVertexBuffer9> pVertexBuffer9 = nullptr;
      HRESULT res = GetD3D9()->CreateVertexBuffer(Length, Usage, FVF, d3d9::D3DPOOL(Pool), &pVertexBuffer9, NULL);
      *ppVertexBuffer = ref(new D3D8VertexBuffer(this, std::move(pVertexBuffer9)));
      return res;
    }

    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(
            UINT                    Length,
            DWORD                   Usage,
            D3DFORMAT               Format,
            D3DPOOL                 Pool,
            IDirect3DIndexBuffer8** ppIndexBuffer) {
      Com<d3d9::IDirect3DIndexBuffer9> pIndexBuffer9 = nullptr;
      HRESULT res = GetD3D9()->CreateIndexBuffer(Length, Usage, d3d9::D3DFORMAT(Format), d3d9::D3DPOOL(Pool), &pIndexBuffer9, NULL);
      *ppIndexBuffer = ref(new D3D8IndexBuffer(this, std::move(pIndexBuffer9)));
      return res;

    }

    HRESULT STDMETHODCALLTYPE CreateRenderTarget D3D8_DEVICE_STUB(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            BOOL                Lockable,
            IDirect3DSurface8** ppSurface);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface D3D8_DEVICE_STUB(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            IDirect3DSurface8** ppSurface);

    HRESULT STDMETHODCALLTYPE UpdateTexture D3D8_DEVICE_STUB_(UpdateTexture,
            IDirect3DBaseTexture8* pSourceTexture,
            IDirect3DBaseTexture8* pDestinationTexture);

    HRESULT STDMETHODCALLTYPE GetFrontBuffer D3D8_DEVICE_STUB(IDirect3DSurface8* pDestSurface);


    HRESULT STDMETHODCALLTYPE SetRenderTarget D3D8_DEVICE_STUB(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil);

    HRESULT STDMETHODCALLTYPE GetRenderTarget D3D8_DEVICE_STUB(IDirect3DSurface8** ppRenderTarget);

    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface D3D8_DEVICE_STUB(IDirect3DSurface8** ppZStencilSurface);

    HRESULT STDMETHODCALLTYPE BeginScene() { return GetD3D9()->BeginScene(); }

    HRESULT STDMETHODCALLTYPE EndScene() { return GetD3D9()->EndScene(); }

    HRESULT STDMETHODCALLTYPE Clear(
            DWORD    Count,
      const D3DRECT* pRects,
            DWORD    Flags,
            D3DCOLOR Color,
            float    Z,
            DWORD    Stencil) {
      return GetD3D9()->Clear(Count, pRects, Flags, Color, Z, Stencil);
    }

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
      return GetD3D9()->SetTransform((d3d9::D3DTRANSFORMSTATETYPE)State, pMatrix);
    }

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
      return GetD3D9()->GetTransform((d3d9::D3DTRANSFORMSTATETYPE)State, pMatrix);
    }

    HRESULT STDMETHODCALLTYPE MultiplyTransform D3D8_DEVICE_STUB(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix);

    HRESULT STDMETHODCALLTYPE SetViewport D3D8_DEVICE_STUB_(SetViewport, const D3DVIEWPORT8* pViewport);

    HRESULT STDMETHODCALLTYPE GetViewport D3D8_DEVICE_STUB_(GetViewport, D3DVIEWPORT8* pViewport);

    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL8* pMaterial) {
      return GetD3D9()->SetMaterial((const d3d9::D3DMATERIAL9*)pMaterial);
    }

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* pMaterial) {
      return GetD3D9()->GetMaterial((d3d9::D3DMATERIAL9*)pMaterial);
    }

    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT8* pLight) {
      return GetD3D9()->SetLight(Index, (const d3d9::D3DLIGHT9*)pLight);
    }

    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT8* pLight) {
      return GetD3D9()->GetLight(Index, (d3d9::D3DLIGHT9*)pLight);
    }

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable) {
      return GetD3D9()->LightEnable(Index, Enable);
    }

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL* pEnable) {
      return GetD3D9()->GetLightEnable(Index, pEnable);
    }

    HRESULT STDMETHODCALLTYPE SetClipPlane D3D8_DEVICE_STUB(DWORD Index, const float* pPlane);

    HRESULT STDMETHODCALLTYPE GetClipPlane D3D8_DEVICE_STUB(DWORD Index, float* pPlane);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
      return GetD3D9()->SetRenderState((d3d9::D3DRENDERSTATETYPE)State, Value);
    }

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
      return GetD3D9()->GetRenderState((d3d9::D3DRENDERSTATETYPE)State, pValue);
    }

    HRESULT STDMETHODCALLTYPE CreateStateBlock D3D8_DEVICE_STUB(
            D3DSTATEBLOCKTYPE     Type,
            DWORD*                pToken);

    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token) {
      return reinterpret_cast<d3d9::IDirect3DStateBlock9*>(Token)->Capture();
    }

    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token) {
      return reinterpret_cast<d3d9::IDirect3DStateBlock9*>(Token)->Apply();
    }


    HRESULT STDMETHODCALLTYPE BeginStateBlock() { 
      return GetD3D9()->BeginStateBlock();
    }

    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* pToken) {
      return GetD3D9()->EndStateBlock((d3d9::IDirect3DStateBlock9**)pToken);
    }

    HRESULT STDMETHODCALLTYPE SetClipStatus D3D8_DEVICE_STUB(const D3DCLIPSTATUS8* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetClipStatus D3D8_DEVICE_STUB(D3DCLIPSTATUS8* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetTexture D3D8_DEVICE_STUB_(GetTexture, DWORD Stage, IDirect3DBaseTexture8** ppTexture);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) {
      D3D8Texture2D* tex = static_cast<D3D8Texture2D*>(pTexture);
      return GetD3D9()->SetTexture(Stage, tex != nullptr ? tex->GetD3D9() : nullptr);
    }

    HRESULT STDMETHODCALLTYPE GetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD*                   pValue) {
      d3d9::D3DSAMPLERSTATETYPE stateType = GetSamplerStateType9(Type);

      if (stateType != -1) {
        // if the type has been remapped to a sampler state type:
        return GetD3D9()->GetSamplerState(Stage, stateType, pValue);
      }
      else {
        return GetD3D9()->GetTextureStageState(Stage, d3d9::D3DTEXTURESTAGESTATETYPE(Type), pValue);
      }
    }

    HRESULT STDMETHODCALLTYPE SetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD                    Value) {

      d3d9::D3DSAMPLERSTATETYPE stateType = GetSamplerStateType9(Type);

      if (stateType != -1) {
        // if the type has been remapped to a sampler state type:
        return GetD3D9()->SetSamplerState(Stage, stateType, Value);
      } else {
        return GetD3D9()->SetTextureStageState(Stage, d3d9::D3DTEXTURESTAGESTATETYPE(Type), Value);
      }
    }

    HRESULT STDMETHODCALLTYPE ValidateDevice D3D8_DEVICE_STUB(DWORD* pNumPasses);

    HRESULT STDMETHODCALLTYPE SetPaletteEntries D3D8_DEVICE_STUB(UINT PaletteNumber, const PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE GetPaletteEntries D3D8_DEVICE_STUB(UINT PaletteNumber, PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette D3D8_DEVICE_STUB(UINT PaletteNumber);

    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette D3D8_DEVICE_STUB(UINT *PaletteNumber);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             StartVertex,
            UINT             PrimitiveCount) {
      return GetD3D9()->DrawPrimitive(d3d9::D3DPRIMITIVETYPE(PrimitiveType), StartVertex, PrimitiveCount);
    }

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             MinVertexIndex,
            UINT             NumVertices,
            UINT             StartIndex,
            UINT             PrimitiveCount) {
      return GetD3D9()->DrawIndexedPrimitive(d3d9::D3DPRIMITIVETYPE(PrimitiveType), 0, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount);
    }

    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP D3D8_DEVICE_STUB(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             PrimitiveCount,
      const void*            pVertexStreamZeroData,
            UINT             VertexStreamZeroStride);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP D3D8_DEVICE_STUB(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             MinVertexIndex,
            UINT             NumVertices,
            UINT             PrimitiveCount,
      const void*            pIndexData,
            D3DFORMAT        IndexDataFormat,
      const void*            pVertexStreamZeroData,
            UINT             VertexStreamZeroStride);

    HRESULT STDMETHODCALLTYPE ProcessVertices D3D8_DEVICE_STUB(
            UINT                         SrcStartIndex,
            UINT                         DestIndex,
            UINT                         VertexCount,
            IDirect3DVertexBuffer8*      pDestBuffer,
            DWORD                        Flags);


    HRESULT STDMETHODCALLTYPE CreateVertexShader D3D8_DEVICE_STUB(
      const DWORD*  pDeclaration,
      const DWORD*  pFunction,
            DWORD*  pHandle,
            DWORD   Usage);

    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle) {
      // TODO: determine if Handle is an FVF or a shader ptr
      // (may need to set a bit on ptrs)
      return GetD3D9()->SetFVF(Handle);
    }
    HRESULT STDMETHODCALLTYPE SetVertexShader D3D8_DEVICE_STUB_(SetVertexShader, DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE GetVertexShader D3D8_DEVICE_STUB_(GetVertexShader, DWORD Handle);
    HRESULT STDMETHODCALLTYPE GetVertexShader D3D8_DEVICE_STUB_(GetVertexShader, DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant D3D8_DEVICE_STUB(
            DWORD StartRegister,
      const void* pConstantData,
            DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE SetStreamSource(
            UINT                    StreamNumber,
            IDirect3DVertexBuffer8* pStreamData,
            UINT                    Stride) {
      D3D8VertexBuffer* buffer = static_cast<D3D8VertexBuffer*>(pStreamData);

      return GetD3D9()->SetStreamSource(StreamNumber, buffer->GetD3D9(), 0, Stride);
    }

    HRESULT STDMETHODCALLTYPE GetStreamSource D3D8_DEVICE_STUB(
            UINT                     StreamNumber,
            IDirect3DVertexBuffer8** ppStreamData,
            UINT*                    pStride);

    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) {
      D3D8IndexBuffer* buffer = static_cast<D3D8IndexBuffer*>(pIndexData);
      return GetD3D9()->SetIndices(buffer->GetD3D9());
    }

    HRESULT STDMETHODCALLTYPE GetIndices D3D8_DEVICE_STUB(
            IDirect3DIndexBuffer8** ppIndexData,
            UINT* pBaseVertexIndex);

    HRESULT STDMETHODCALLTYPE CreatePixelShader D3D8_DEVICE_STUB(
      const DWORD* pFunction, 
            DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE SetPixelShader D3D8_DEVICE_STUB(DWORD Handle);
    HRESULT STDMETHODCALLTYPE SetPixelShader D3D8_DEVICE_STUB(DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE GetPixelShader D3D8_DEVICE_STUB(DWORD Handle);
    HRESULT STDMETHODCALLTYPE GetPixelShader D3D8_DEVICE_STUB(DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant D3D8_DEVICE_STUB(
            DWORD StartRegister,
      const void* pConstantData,
            DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE DrawRectPatch D3D8_DEVICE_STUB(
            UINT               Handle,
      const float*             pNumSegs,
      const D3DRECTPATCH_INFO* pRectPatchInfo);

    HRESULT STDMETHODCALLTYPE DrawTriPatch  D3D8_DEVICE_STUB(
            UINT              Handle,
      const float*            pNumSegs,
      const D3DTRIPATCH_INFO* pTriPatchInfo);

    HRESULT STDMETHODCALLTYPE DeletePatch  D3D8_DEVICE_STUB(UINT Handle);

    /*
    VkPipelineStageFlags GetEnabledShaderStages() const {
      return m_dxvkDevice->getShaderPipelineStages();
    }

    static DxvkDeviceFeatures GetDeviceFeatures(const Rc<DxvkAdapter>& adapter);

    bool SupportsSWVP();

    bool IsExtended();

    HWND GetWindow();

    Rc<DxvkDevice> GetDXVKDevice() {
      return m_dxvkDevice;
    }

    D3D8_VK_FORMAT_MAPPING LookupFormat(
      D3D8Format            Format) const;

    DxvkFormatInfo UnsupportedFormatInfo(
      D3D8Format            Format) const;

    bool WaitForResource(
      const Rc<DxvkResource>&                 Resource,
            DWORD                             MapFlags);

    */

    /**
     * \brief Locks a subresource of an image
     * 
     * \param [in] Subresource The subresource of the image to lock
     * \param [out] pLockedBox The returned locked box of the image, containing data ptr and strides
     * \param [in] pBox The region of the subresource to lock. This offsets the returned data ptr
     * \param [in] Flags The D3DLOCK_* flags to lock the image with
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    /*
    HRESULT LockImage(
            D3D8CommonTexture* pResource,
            UINT                    Face,
            UINT                    Mip,
            D3DLOCKED_BOX*          pLockedBox,
      const D3DBOX*                 pBox,
            DWORD                   Flags);

    uint32_t CalcImageLockOffset(
            uint32_t                SlicePitch,
            uint32_t                RowPitch,
      const DxvkFormatInfo*         FormatInfo,
      const D3DBOX*                 pBox);
    */

    /**
     * \brief Unlocks a subresource of an image
     * 
     * Passthrough to device unlock.
     * \param [in] Subresource The subresource of the image to unlock
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    /*
    HRESULT UnlockImage(
            D3D8CommonTexture*      pResource,
            UINT                    Face,
            UINT                    MipLevel);

    HRESULT FlushImage(
            D3D8CommonTexture*      pResource,
            UINT                    Subresource);

    void EmitGenerateMips(
            D3D8CommonTexture* pResource);

    HRESULT LockBuffer(
            D3D8CommonBuffer*       pResource,
            UINT                    OffsetToLock,
            UINT                    SizeToLock,
            void**                  ppbData,
            DWORD                   Flags);

    HRESULT FlushBuffer(
            D3D8CommonBuffer*       pResource);

    HRESULT UnlockBuffer(
            D3D8CommonBuffer*       pResource);

    void SetupFPU();

    int64_t DetermineInitialTextureMemory();

    Rc<DxvkBuffer> CreateConstantBuffer(
            bool                SSBO,
            VkDeviceSize        Size,
            DxsoProgramType     ShaderStage,
            DxsoConstantBuffers BufferType);

    void CreateConstantBuffers();

    void SynchronizeCsThread();

    void Flush();

    void UpdateActiveRTs(uint32_t index);

    void UpdateActiveTextures(uint32_t index, DWORD combinedUsage);

    void UpdateActiveHazardsRT(uint32_t rtMask);

    void UpdateActiveHazardsDS(uint32_t texMask);

    void MarkRenderHazards();

    void UploadManagedTexture(D3D8CommonTexture* pResource);

    void UploadManagedTextures(uint32_t mask);

    void GenerateTextureMips(uint32_t mask);

    void MarkTextureMipsDirty(D3D8CommonTexture* pResource);

    void MarkTextureMipsUnDirty(D3D8CommonTexture* pResource);

    void MarkTextureUploaded(D3D8CommonTexture* pResource);

    template <bool Points>
    void UpdatePointMode();

    void UpdateFog();

    void BindFramebuffer();

    void BindViewportAndScissor();

    inline bool IsAlphaToCoverageEnabled() {
      const bool alphaTest = m_state.renderStates[D3DRS_ALPHATESTENABLE] != 0;

      return m_amdATOC || (m_nvATOC && alphaTest);
    }

    inline bool IsDepthBiasEnabled() {
      const auto& rs = m_state.renderStates;

      float depthBias            = bit::cast<float>(rs[D3DRS_DEPTHBIAS]);
      float slopeScaledDepthBias = bit::cast<float>(rs[D3DRS_SLOPESCALEDEPTHBIAS]);

      return depthBias != 0.0f || slopeScaledDepthBias != 0.0f;
    }

    inline bool IsAlphaTestEnabled() {
      return m_state.renderStates[D3DRS_ALPHATESTENABLE] && !IsAlphaToCoverageEnabled();
    }

    inline bool IsZTestEnabled() {
      return m_state.renderStates[D3DRS_ZENABLE] && m_state.depthStencil != nullptr;
    }

    inline bool IsClipPlaneEnabled() {
      return m_state.renderStates[D3DRS_CLIPPLANEENABLE] != 0;
    }

    void BindMultiSampleState();
    
    void BindBlendState();

    void BindBlendFactor();

    void BindDepthStencilState();

    void BindDepthStencilRefrence();

    void BindRasterizerState();

    void BindDepthBias();

    void BindAlphaTestState();

    template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
    inline void UploadHardwareConstantSet(void* pData, const SoftwareLayoutType& Src, const ShaderType& Shader);

    template <typename SoftwareLayoutType, typename ShaderType>
    inline void UploadSoftwareConstantSet(void* pData, const SoftwareLayoutType& Src, const D3D8ConstantLayout& Layout, const ShaderType& Shader);

    template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
    inline void UploadConstantSet(const SoftwareLayoutType& Src, const D3D8ConstantLayout& Layout, const ShaderType& Shader);
    
    template <DxsoProgramType ShaderStage>
    void UploadConstants();
    
    void UpdateClipPlanes();
    
    template <uint32_t Offset, uint32_t Length>
    void UpdatePushConstant(const void* pData);

    template <D3D8RenderStateItem Item>
    void UpdatePushConstant();

    void BindSampler(DWORD Sampler);

    void BindTexture(DWORD SamplerSampler);

    void UndirtySamplers();

    void MarkSamplersDirty();

    D3D8DrawInfo GenerateDrawInfo(
      D3DPRIMITIVETYPE PrimitiveType,
      UINT             PrimitiveCount,
      UINT             InstanceCount);
    
    uint32_t GetInstanceCount() const;

    void PrepareDraw(D3DPRIMITIVETYPE PrimitiveType);

    template <DxsoProgramType ShaderStage>
    void BindShader(
      const D3D8CommonShader*                 pShaderModule,
            D3D8ShaderPermutation             Permutation);

    void BindInputLayout();

    void BindVertexBuffer(
            UINT                              Slot,
            D3D8VertexBuffer*                 pBuffer,
            UINT                              Offset,
            UINT                              Stride);

    void BindIndices();

    D3D8DeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    const D3D8Options* GetOptions() const {
      return &m_d3d8Options;
    }

    Direct3DState9* GetRawState() {
      return &m_state;
    }

    void Begin(D3D8Query* pQuery);
    void End(D3D8Query* pQuery);

    void SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits);
    void SetPixelBoolBitfield (uint32_t idx, uint32_t mask, uint32_t bits);

    void FlushImplicit(BOOL StrongHint);

    bool ChangeReportedMemory(int64_t delta) {
      if (IsExtended())
        return true;

      int64_t availableMemory = m_availableMemory.fetch_add(delta);

      return !m_d3d8Options.memoryTrackTest || availableMemory >= delta;
    }

    void ResolveZ();

    void TransitionImage(D3D8CommonTexture* pResource, VkImageLayout NewLayout);

    void TransformImage(
            D3D8CommonTexture*       pResource,
      const VkImageSubresourceRange* pSubresources,
            VkImageLayout            OldLayout,
            VkImageLayout            NewLayout);

    const D3D8ConstantLayout& GetVertexConstantLayout() { return m_vsLayout; }
    const D3D8ConstantLayout& GetPixelConstantLayout()  { return m_psLayout; }

    HRESULT ResetState(D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT ResetSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    HRESULT InitialReset(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    UINT GetSamplerCount() const {
      return m_samplerCount.load();
    }
    */

  private:

    /*D3D8DeviceFlags                 m_flags;
    uint32_t                        m_dirtySamplerStates = 0;

    D3D8Adapter*                    m_adapter;
    Rc<DxvkDevice>                  m_dxvkDevice;

    Rc<DxvkDataBuffer>              m_updateBuffer;
    DxvkCsChunkPool                 m_csChunkPool;
    dxvk::high_resolution_clock::time_point m_lastFlush
      = dxvk::high_resolution_clock::now();
    DxvkCsThread                    m_csThread;
    bool                            m_csIsBusy = false;

    uint32_t                        m_frameLatency = DefaultFrameLatency;

    D3D8Initializer*                m_initializer = nullptr;
    D3D8FormatHelper*               m_converter   = nullptr;

    DxvkCsChunkRef                  m_csChunk;

    D3D8FFShaderModuleSet           m_ffModules;
    D3D8SWVPEmulator                m_swvpEmulator;

    DxvkCsChunkRef AllocCsChunk() {
      DxvkCsChunk* chunk = m_csChunkPool.allocChunk(DxvkCsChunkFlag::SingleUse);
      return DxvkCsChunkRef(chunk, &m_csChunkPool);
    }

    template<typename Cmd>
    void EmitCs(Cmd&& command) {
      if (unlikely(!m_csChunk->push(command))) {
        EmitCsChunk(std::move(m_csChunk));

        m_csChunk = AllocCsChunk();
        m_csChunk->push(command);
      }
    }

    void EmitCsChunk(DxvkCsChunkRef&& chunk);

    void FlushCsChunk() {
      if (likely(!m_csChunk->empty())) {
        EmitCsChunk(std::move(m_csChunk));
        m_csChunk = AllocCsChunk();
      }
    }

    bool CanSWVP() {
      return m_behaviorFlags & (D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_SOFTWARE_VERTEXPROCESSING);
    }

    inline constexpr D3D8ShaderPermutation GetVertexShaderPermutation() {
      return D3D8ShaderPermutations::None;
    }

    inline D3D8ShaderPermutation GetPixelShaderPermutation() {
      if (unlikely(m_state.renderStates[D3DRS_SHADEMODE] == D3DSHADE_FLAT))
        return D3D8ShaderPermutations::FlatShade;

      return D3D8ShaderPermutations::None;
    }*/

    Com<D3D8InterfaceEx>            m_parent;
    D3DDEVTYPE                      m_deviceType;
    HWND                            m_window;

    DWORD                           m_behaviorFlags;

    /*
    Direct3DState9                  m_state;
    Com<D3D8StateBlock>             m_recorder;
    D3D8Multithread                 m_multithread;

    Rc<D3D8ShaderModuleSet>         m_shaderModules;

    D3D8ConstantSets                m_consts[DxsoProgramTypes::Count];

    Rc<DxvkBuffer>                  m_vsClipPlanes;

    Rc<DxvkBuffer>                  m_vsFixedFunction;
    Rc<DxvkBuffer>                  m_vsVertexBlend;
    Rc<DxvkBuffer>                  m_psFixedFunction;
    Rc<DxvkBuffer>                  m_psShared;

    D3D8UPBufferSlice               m_upBuffer;

    const D3D8Options               m_d3d8Options;
    DxsoOptions                     m_dxsoOptions;

    BOOL                            m_isSWVP;

    D3DPRESENT_PARAMETERS           m_presentParams;

    D3D8Cursor                      m_cursor;

    Com<D3D8Surface, false>         m_autoDepthStencil;

    Com<D3D8SwapChainEx, false>     m_implicitSwapchain;

    std::unordered_map<
      D3D8SamplerKey,
      D3D8SamplerPair,
      D3D8SamplerKeyHash,
      D3D8SamplerKeyEq>             m_samplers;

    std::unordered_map<
      DWORD,
      Com<D3D8VertexDecl,
      false>>                       m_fvfTable;

    D3D8InputAssemblyState          m_iaState;

    uint32_t                        m_instancedData   = 0;
    uint32_t                        m_lastSamplerTypeBitfield = 0;
    uint32_t                        m_samplerTypeBitfield = 0;
    uint32_t                        m_lastProjectionBitfield = 0;
    uint32_t                        m_projectionBitfield = 0;

    uint32_t                        m_lastBoolSpecConstantVertex = 0;
    uint32_t                        m_lastBoolSpecConstantPixel  = 0;

    uint32_t                        m_lastPointMode = 0;

    uint32_t                        m_activeRTs        = 0;
    uint32_t                        m_activeRTTextures = 0;
    uint32_t                        m_activeDSTextures = 0;
    uint32_t                        m_activeHazardsRT  = 0;
    uint32_t                        m_alphaSwizzleRTs  = 0;
    uint32_t                        m_activeTextures   = 0;
    uint32_t                        m_activeTexturesToUpload = 0;
    uint32_t                        m_activeTexturesToGen    = 0;

    uint32_t                        m_fetch4Enabled = 0;
    uint32_t                        m_fetch4        = 0;
    uint32_t                        m_lastFetch4    = 0;

    uint32_t                        m_activeHazardsDS = 0;
    uint32_t                        m_lastHazardsDS   = 0;

    D3D8ShaderMasks                 m_vsShaderMasks = D3D8ShaderMasks();
    D3D8ShaderMasks                 m_psShaderMasks = FixedFunctionMask;

    D3D8ViewportInfo                m_viewportInfo;

    std::atomic<int64_t>            m_availableMemory = 0;
    std::atomic<int32_t>            m_samplerCount = 0;

    bool                            m_amdATOC         = false;
    bool                            m_nvATOC          = false;
    bool                            m_ffZTest         = false;

    float                           m_depthBiasScale  = 0.0f;

    D3D8ConstantLayout              m_vsLayout;
    D3D8ConstantLayout              m_psLayout;

    void DetermineConstantLayouts(bool canSWVP);

    D3D8UPBufferSlice AllocUpBuffer(VkDeviceSize size);

    bool ShouldRecord();

    HRESULT               CreateShaderModule(
            D3D8CommonShader*     pShaderModule,
            VkShaderStageFlagBits ShaderStage,
      const DWORD*                pShaderBytecode,
      const DxsoModuleInfo*       pModuleInfo);

    // So we don't do OOB.
    template <DxsoProgramType  ProgramType,
              D3D8ConstantType ConstantType>
    inline static constexpr uint32_t DetermineSoftwareRegCount() {
      constexpr bool isVS = ProgramType == DxsoProgramType::VertexShader;

      switch (ConstantType) {
        default:
        case D3D8ConstantType::Float:  return isVS ? caps::MaxFloatConstantsSoftware : caps::MaxFloatConstantsPS;
        case D3D8ConstantType::Int:    return isVS ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
        case D3D8ConstantType::Bool:   return isVS ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
      }
    }

    // So we don't copy more than we need.
    template <DxsoProgramType  ProgramType,
              D3D8ConstantType ConstantType>
    inline uint32_t DetermineHardwareRegCount() const {
      const auto& layout = ProgramType == DxsoProgramType::VertexShader
        ? m_vsLayout : m_psLayout;

      switch (ConstantType) {
        default:
        case D3D8ConstantType::Float:  return layout.floatCount;
        case D3D8ConstantType::Int:    return layout.intCount;
        case D3D8ConstantType::Bool:   return layout.boolCount;
      }
    }

    inline uint32_t GetFrameLatency() {
      return m_frameLatency;
    }

    template <
      DxsoProgramType  ProgramType,
      D3D8ConstantType ConstantType,
      typename         T>
      HRESULT SetShaderConstants(
              UINT  StartRegister,
        const T*    pConstantData,
              UINT  Count);

    template <
      DxsoProgramType  ProgramType,
      D3D8ConstantType ConstantType,
      typename         T>
    HRESULT GetShaderConstants(
            UINT StartRegister,
            T*   pConstantData,
            UINT Count) {
      auto GetHelper = [&] (const auto& set) {
        const     uint32_t regCountHardware = DetermineHardwareRegCount<ProgramType, ConstantType>();
        constexpr uint32_t regCountSoftware = DetermineSoftwareRegCount<ProgramType, ConstantType>();

        if (StartRegister + Count > regCountSoftware)
          return D3DERR_INVALIDCALL;

        Count = UINT(
          std::max<INT>(
            std::clamp<INT>(Count + StartRegister, 0, regCountHardware) - INT(StartRegister),
            0));

        if (Count == 0)
          return D3D_OK;

        if (pConstantData == nullptr)
          return D3DERR_INVALIDCALL;

        if constexpr (ConstantType == D3D8ConstantType::Float) {
          auto begin = &set.fConsts[StartRegister];
          auto end = &begin[Count];

          std::copy(begin, end, reinterpret_cast<Vector4*>(pConstantData));
        }
        else if constexpr (ConstantType == D3D8ConstantType::Int) {
          auto begin = &set.iConsts[StartRegister];
          auto end = &begin[Count];

          std::copy(begin, end, reinterpret_cast<Vector4i*>(pConstantData));
        }
        else {
          for (uint32_t i = 0; i < Count; i++) {
            const uint32_t constantIdx = StartRegister + i;
            const uint32_t arrayIdx    = constantIdx / 32;
            const uint32_t bitIdx      = constantIdx % 32;

            const uint32_t bit         = (1u << bitIdx);

            bool constValue = set.bConsts[arrayIdx] & bit;
            pConstantData[i] = constValue ? TRUE : FALSE;
          }
        }

        return D3D_OK;
      };

      return ProgramType == DxsoProgramTypes::VertexShader
        ? GetHelper(m_state.vsConsts)
        : GetHelper(m_state.psConsts);
    }

    void UpdateFixedFunctionVS();

    void UpdateFixedFunctionPS();

    void ApplyPrimitiveType(
      DxvkContext*      pContext,
      D3DPRIMITIVETYPE  PrimType);

    bool UseProgrammableVS();

    bool UseProgrammablePS();

    void UpdateBoolSpecConstantVertex(uint32_t value);

    void UpdateBoolSpecConstantPixel(uint32_t value);

    void UpdateSamplerSpecConsant(uint32_t value);

    void UpdateProjectionSpecConstant(uint32_t value);

    void UpdateFetch4SpecConstant(uint32_t value);*/

  };

}
