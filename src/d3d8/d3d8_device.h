#pragma once

#include "d3d8_include.h"
#include "d3d8_multithread.h"
#include "d3d8_texture.h"
#include "d3d8_buffer.h"
#include "d3d8_swapchain.h"
#include "d3d8_state_block.h"
#include "d3d8_d3d9_util.h"
#include "d3d8_caps.h"
#include "d3d8_batch.h"

#include "../d3d9/d3d9_bridge.h"

#include <array>
#include <vector>
#include <type_traits>
#include <unordered_map>

namespace dxvk {

  class D3D8Interface;

  struct D3D8VertexShaderInfo;

  using D3D8DeviceBase = D3D8WrappedObject<d3d9::IDirect3DDevice9, IDirect3DDevice8>;
  class D3D8Device final : public D3D8DeviceBase {

    friend class D3D8StateBlock;

  public:

    D3D8Device(
            D3D8Interface*                pParent,
            Com<d3d9::IDirect3DDevice9>&& pDevice,
            D3DDEVTYPE                    DeviceType,
            HWND                          hFocusWindow,
            DWORD                         BehaviorFlags,
            D3DPRESENT_PARAMETERS*        pParams);

    ~D3D8Device();

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();

    UINT    STDMETHODCALLTYPE GetAvailableTextureMem();

    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD bytes);

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** ppD3D8);

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* pCaps);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters);

    HRESULT STDMETHODCALLTYPE SetCursorProperties(
      UINT               XHotSpot,
      UINT               YHotSpot,
      IDirect3DSurface8* pCursorBitmap);

    void    STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags);

    // Microsoft d3d8.h in the DirectX 9 SDK uses a different function signature...
    void    STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags);

    BOOL    STDMETHODCALLTYPE ShowCursor(BOOL bShow);

    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DSwapChain8** ppSwapChain);

    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters);

    HRESULT STDMETHODCALLTYPE Present(
      const RECT* pSourceRect,
      const RECT* pDestRect,
            HWND hDestWindowOverride,
      const RGNDATA* pDirtyRegion);

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
            UINT iBackBuffer,
            D3DBACKBUFFER_TYPE Type,
            IDirect3DSurface8** ppBackBuffer);

    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus);

    void STDMETHODCALLTYPE SetGammaRamp(DWORD Flags, const D3DGAMMARAMP* pRamp);

    void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* pRamp);

    HRESULT STDMETHODCALLTYPE CreateTexture(
            UINT                Width,
            UINT                Height,
            UINT                Levels,
            DWORD               Usage,
            D3DFORMAT           Format,
            D3DPOOL             Pool,
            IDirect3DTexture8** ppTexture);

    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(
            UINT                      Width,
            UINT                      Height,
            UINT                      Depth,
            UINT                      Levels,
            DWORD                     Usage,
            D3DFORMAT                 Format,
            D3DPOOL                   Pool,
            IDirect3DVolumeTexture8** ppVolumeTexture);

    HRESULT STDMETHODCALLTYPE CreateCubeTexture(
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
            IDirect3DVertexBuffer8** ppVertexBuffer);

    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(
            UINT                    Length,
            DWORD                   Usage,
            D3DFORMAT               Format,
            D3DPOOL                 Pool,
            IDirect3DIndexBuffer8** ppIndexBuffer);

    HRESULT STDMETHODCALLTYPE CreateRenderTarget(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            BOOL                Lockable,
            IDirect3DSurface8** ppSurface);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            IDirect3DSurface8** ppSurface);

    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface);

    HRESULT STDMETHODCALLTYPE CopyRects(
            IDirect3DSurface8*  pSourceSurface,
      const RECT*               pSourceRectsArray,
            UINT                cRects,
            IDirect3DSurface8*  pDestinationSurface,
      const POINT*              pDestPointsArray);

    HRESULT STDMETHODCALLTYPE UpdateTexture(
            IDirect3DBaseTexture8* pSourceTexture,
            IDirect3DBaseTexture8* pDestinationTexture);

    HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8* pDestSurface);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** ppRenderTarget);

    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE Clear(
            DWORD    Count,
      const D3DRECT* pRects,
            DWORD    Flags,
            D3DCOLOR Color,
            float    Z,
            DWORD    Stencil);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix);

    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT8* pViewport);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* pViewport);

    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL8* pMaterial);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* pMaterial);

    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT8* pLight);

    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT8* pLight);

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable);

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL* pEnable);

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, const float* pPlane);

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float* pPlane);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue);

    HRESULT STDMETHODCALLTYPE CreateStateBlock(
            D3DSTATEBLOCKTYPE     Type,
            DWORD*                pToken);

    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token);

    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token);

    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token);

    HRESULT STDMETHODCALLTYPE BeginStateBlock();

    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* pToken);

    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS8* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, IDirect3DBaseTexture8** ppTexture);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD*                   pValue);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD                    Value);

    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses);

    HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize);

    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber);

    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* PaletteNumber);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             StartVertex,
            UINT             PrimitiveCount);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             MinVertexIndex,
            UINT             NumVertices,
            UINT             StartIndex,
            UINT             PrimitiveCount);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             PrimitiveCount,
      const void*            pVertexStreamZeroData,
            UINT             VertexStreamZeroStride);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             MinVertexIndex,
            UINT             NumVertices,
            UINT             PrimitiveCount,
      const void*            pIndexData,
            D3DFORMAT        IndexDataFormat,
      const void*            pVertexStreamZeroData,
            UINT             VertexStreamZeroStride);

    HRESULT STDMETHODCALLTYPE ProcessVertices(
        UINT                         SrcStartIndex,
        UINT                         DestIndex,
        UINT                         VertexCount,
        IDirect3DVertexBuffer8*      pDestBuffer,
        DWORD                        Flags);

    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const DWORD*  pDeclaration,
      const DWORD*  pFunction,
            DWORD*  pHandle,
            DWORD   Usage);

    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle);

    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle);

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(
            DWORD StartRegister,
      const void* pConstantData,
            DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD Handle, void* pData, DWORD* pSizeOfData);

    HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData);

    HRESULT STDMETHODCALLTYPE SetStreamSource(
            UINT                    StreamNumber,
            IDirect3DVertexBuffer8* pStreamData,
            UINT                    Stride);

    HRESULT STDMETHODCALLTYPE GetStreamSource(
            UINT                     StreamNumber,
            IDirect3DVertexBuffer8** ppStreamData,
            UINT*                    pStride);

    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex);

    HRESULT STDMETHODCALLTYPE GetIndices(
            IDirect3DIndexBuffer8** ppIndexData,
            UINT* pBaseVertexIndex);

    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const DWORD* pFunction,
            DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle);

    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* pHandle);

    HRESULT STDMETHODCALLTYPE DeletePixelShader(THIS_ DWORD Handle);

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(
            DWORD StartRegister,
      const void* pConstantData,
            DWORD ConstantCount);

    HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData);

    HRESULT STDMETHODCALLTYPE DrawRectPatch(
            UINT               Handle,
      const float*             pNumSegs,
      const D3DRECTPATCH_INFO* pRectPatchInfo);

    HRESULT STDMETHODCALLTYPE DrawTriPatch(
            UINT              Handle,
      const float*            pNumSegs,
      const D3DTRIPATCH_INFO* pTriPatchInfo);

    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);

    const D3D8Options* GetOptions() const {
      return &m_d3d8Options;
    }

    inline bool ShouldRecord() { return m_recorder != nullptr; }
    inline bool ShouldBatch()  { return m_batcher  != nullptr; }

    D3D8DeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    /**
     * Marks any state change in the device, so we can signal
     * the batcher to emit draw calls. StateChange should be
     * called immediately before changing any D3D9 state.
     */
    inline void StateChange() {
      if (ShouldBatch())
        m_batcher->StateChange();
    }

    inline void ResetState() {
      // Mirrors how D3D9 handles the BackBufferCount
      m_presentParams.BackBufferCount = std::max(m_presentParams.BackBufferCount, 1u);

      // Purge cached objects
      m_textures.fill(nullptr);
      m_streams.fill(D3D8VBO());
      m_indices = nullptr;
      m_renderTarget = nullptr;
      m_depthStencil = nullptr;

      m_backBuffers.clear();
      m_backBuffers.resize(m_presentParams.BackBufferCount);

      m_autoDepthStencil = nullptr;

      m_shadowPerspectiveDivide = false;
    }

    inline void RecreateBackBuffersAndAutoDepthStencil() {
      for (UINT i = 0; i < m_presentParams.BackBufferCount; i++) {
        Com<d3d9::IDirect3DSurface9> pSurface9;
        GetD3D9()->GetBackBuffer(0, i, d3d9::D3DBACKBUFFER_TYPE_MONO, &pSurface9);
        m_backBuffers[i] = new D3D8Surface(this, D3DPOOL_DEFAULT, std::move(pSurface9));
      }

      Com<d3d9::IDirect3DSurface9> pStencil9;
      // This call will fail if the D3D9 device is created without
      // the EnableAutoDepthStencil presentation parameter set to TRUE.
      HRESULT res = GetD3D9()->GetDepthStencilSurface(&pStencil9);
      m_autoDepthStencil = FAILED(res) ? nullptr : new D3D8Surface(this, D3DPOOL_DEFAULT, std::move(pStencil9));

      m_renderTarget = m_backBuffers[0];
      m_depthStencil = m_autoDepthStencil;
    }

    friend d3d9::IDirect3DPixelShader9* getPixelShaderPtr(D3D8Device* device, DWORD Handle);
    friend D3D8VertexShaderInfo*        getVertexShaderInfo(D3D8Device* device, DWORD Handle);

  private:

    Com<IDxvkD3D8Bridge>  m_bridge;
    const D3D8Options&    m_d3d8Options;

    Com<D3D8Interface>    m_parent;

    D3DPRESENT_PARAMETERS m_presentParams;
    
    // Value of D3DRS_LINEPATTERN
    D3DLINEPATTERN        m_linePattern   = {};
    // Value of D3DRS_ZVISIBLE (although the RS is not supported, its value is stored)
    DWORD                 m_zVisible      = 0;
    // Value of D3DRS_PATCHSEGMENTS
    float                 m_patchSegments = 1.0f;

    // Controls fixed-function exclusive mode (no PS support)
    bool                  m_isFixedFunctionOnly = false;

    bool                  m_shadowPerspectiveDivide = false;

    D3D8StateBlock*                            m_recorder = nullptr;
    DWORD                                      m_recorderToken = 0;
    DWORD                                      m_token    = 0;
    std::unordered_map<DWORD, D3D8StateBlock>  m_stateBlocks;
    D3D8Batcher*                               m_batcher  = nullptr;

    struct D3D8VBO {
      Com<D3D8VertexBuffer, false>   buffer = nullptr;
      UINT                           stride = 0;
    };

    std::array<Com<D3D8Texture2D, false>, d8caps::MAX_TEXTURE_STAGES>  m_textures;
    std::array<D3D8VBO, d8caps::MAX_STREAMS>                           m_streams;

    Com<D3D8IndexBuffer, false>        m_indices;
    UINT                               m_baseVertexIndex = 0;

    std::vector<Com<D3D8Surface, false>> m_backBuffers;
    Com<D3D8Surface, false>              m_autoDepthStencil;

    Com<D3D8Surface, false>     m_renderTarget;
    Com<D3D8Surface, false>     m_depthStencil;

    std::vector<D3D8VertexShaderInfo>               m_vertexShaders;
    std::vector<Com<d3d9::IDirect3DPixelShader9>>   m_pixelShaders;
    DWORD                                           m_currentVertexShader  = 0; // can be FVF or vs index (marked by D3DFVF_RESERVED0)
    DWORD                                           m_currentPixelShader   = 0;

    D3DDEVTYPE            m_deviceType;
    HWND                  m_window;

    DWORD                 m_behaviorFlags;

    D3D8Multithread       m_multithread;

  };

}
