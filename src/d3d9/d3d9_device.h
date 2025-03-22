#pragma once

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_staging.h"

#include "d3d9_include.h"
#include "d3d9_cursor.h"
#include "d3d9_format.h"
#include "d3d9_multithread.h"
#include "d3d9_adapter.h"
#include "d3d9_constant_buffer.h"
#include "d3d9_constant_set.h"
#include "d3d9_mem.h"

#include "d3d9_state.h"

#include "d3d9_options.h"

#include "../dxso/dxso_module.h"
#include "../dxso/dxso_util.h"
#include "../dxso/dxso_options.h"
#include "../dxso/dxso_modinfo.h"

#include "d3d9_fixed_function.h"
#include "d3d9_swvp_emu.h"

#include "d3d9_spec_constants.h"
#include "d3d9_interop.h"
#include "d3d9_on_12.h"

#include <cstdint>
#include <unordered_set>
#include "d3d9_bridge.h"

#include <vector>
#include <type_traits>
#include <unordered_map>

#include "../util/util_flush.h"
#include "../util/util_lru.h"

namespace dxvk {

  class D3D9InterfaceEx;
  class D3D9SwapChainEx;
  class D3D9CommonTexture;
  class D3D9CommonBuffer;
  class D3D9CommonShader;
  class D3D9ShaderModuleSet;
  class D3D9Initializer;
  class D3D9Query;
  class D3D9StateBlock;
  class D3D9FormatHelper;
  class D3D9UserDefinedAnnotation;

  enum class D3D9DeviceFlag : uint32_t {
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
    DirtyVertexBuffers,
    DirtyIndexBuffer,

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

    InScene,

    DirtySpecializationEntries,
  };

  using D3D9DeviceFlags = Flags<D3D9DeviceFlag>;

  enum class D3D9DeviceLostState {
    Ok = 0,
    Lost = 1,
    NotReset = 2,
  };

  struct D3D9DrawInfo {
    uint32_t vertexCount;
    uint32_t instanceCount;
  };

  struct D3D9BufferSlice {
    DxvkBufferSlice slice = {};
    void*           mapPtr = nullptr;
  };

  class D3D9DeviceEx final : public ComObjectClamp<IDirect3DDevice9Ex> {
    constexpr static uint32_t DefaultFrameLatency = 3;
    constexpr static uint32_t MaxFrameLatency     = 20;

    constexpr static uint32_t MinFlushIntervalUs = 750;
    constexpr static uint32_t IncFlushIntervalUs = 250;
    constexpr static uint32_t MaxPendingSubmits = 6;

    constexpr static uint32_t NullStreamIdx = caps::MaxStreams;

    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;

    friend class D3D9SwapChainEx;
    friend struct D3D9WindowContext;
    friend class D3D9ConstantBuffer;
    friend class D3D9UserDefinedAnnotation;
    friend class DxvkD3D8Bridge;
    friend D3D9VkInteropDevice;
  public:

    D3D9DeviceEx(
            D3D9InterfaceEx*       pParent,
            D3D9Adapter*           pAdapter,
            D3DDEVTYPE             DeviceType,
            HWND                   hFocusWindow,
            DWORD                  BehaviorFlags,
            Rc<DxvkDevice>         dxvkDevice);

    ~D3D9DeviceEx();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();

    UINT    STDMETHODCALLTYPE GetAvailableTextureMem();

    HRESULT STDMETHODCALLTYPE EvictManagedResources();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** ppD3D9);

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* pCaps);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters);

    HRESULT STDMETHODCALLTYPE SetCursorProperties(
            UINT               XHotSpot,
            UINT               YHotSpot,
            IDirect3DSurface9* pCursorBitmap);

    void    STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags);

    BOOL    STDMETHODCALLTYPE ShowCursor(BOOL bShow);

    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(
            D3DPRESENT_PARAMETERS* pPresentationParameters,
            IDirect3DSwapChain9**  ppSwapChain);

    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain);

    UINT    STDMETHODCALLTYPE GetNumberOfSwapChains();

    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters);

    HRESULT STDMETHODCALLTYPE Present(
      const RECT* pSourceRect,
      const RECT* pDestRect, HWND hDestWindowOverride,
      const RGNDATA* pDirtyRegion);

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
      UINT iSwapChain,
      UINT iBackBuffer,
      D3DBACKBUFFER_TYPE Type,
      IDirect3DSurface9** ppBackBuffer);

    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus);

    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL bEnableDialogs);

    void    STDMETHODCALLTYPE SetGammaRamp(
      UINT iSwapChain,
      DWORD Flags,
      const D3DGAMMARAMP* pRamp);

    void    STDMETHODCALLTYPE GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp);

    HRESULT STDMETHODCALLTYPE CreateTexture(
            UINT                Width,
            UINT                Height,
            UINT                Levels,
            DWORD               Usage,
            D3DFORMAT           Format,
            D3DPOOL             Pool,
            IDirect3DTexture9** ppTexture,
            HANDLE*             pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(
            UINT                      Width,
            UINT                      Height,
            UINT                      Depth,
            UINT                      Levels,
            DWORD                     Usage,
            D3DFORMAT                 Format,
            D3DPOOL                   Pool,
            IDirect3DVolumeTexture9** ppVolumeTexture,
            HANDLE*                   pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateCubeTexture(
          UINT                      EdgeLength,
            UINT                    Levels,
            DWORD                   Usage,
            D3DFORMAT               Format,
            D3DPOOL                 Pool,
            IDirect3DCubeTexture9** ppCubeTexture,
            HANDLE*                 pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(
            UINT                     Length,
            DWORD                    Usage,
            DWORD                    FVF,
            D3DPOOL                  Pool,
            IDirect3DVertexBuffer9** ppVertexBuffer,
            HANDLE*                  pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(
            UINT                    Length,
            DWORD                   Usage,
            D3DFORMAT               Format,
            D3DPOOL                 Pool,
            IDirect3DIndexBuffer9** ppIndexBuffer,
            HANDLE*                 pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateRenderTarget(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            DWORD               MultisampleQuality,
            BOOL                Lockable,
            IDirect3DSurface9** ppSurface,
            HANDLE*             pSharedHandle);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            DWORD               MultisampleQuality,
            BOOL                Discard,
            IDirect3DSurface9** ppSurface,
            HANDLE*             pSharedHandle);

    HRESULT STDMETHODCALLTYPE UpdateSurface(
            IDirect3DSurface9* pSourceSurface,
      const RECT*              pSourceRect,
            IDirect3DSurface9* pDestinationSurface,
      const POINT*             pDestPoint);

    HRESULT STDMETHODCALLTYPE UpdateTexture(
            IDirect3DBaseTexture9* pSourceTexture,
            IDirect3DBaseTexture9* pDestinationTexture);

    HRESULT STDMETHODCALLTYPE GetRenderTargetData(
            IDirect3DSurface9* pRenderTarget,
            IDirect3DSurface9* pDestSurface);

    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface);

    HRESULT STDMETHODCALLTYPE StretchRect(
            IDirect3DSurface9*   pSourceSurface,
      const RECT*                pSourceRect,
            IDirect3DSurface9*   pDestSurface,
      const RECT*                pDestRect,
            D3DTEXTUREFILTERTYPE Filter);

    HRESULT STDMETHODCALLTYPE ColorFill(
            IDirect3DSurface9* pSurface,
      const RECT*              pRect,
            D3DCOLOR           Color);

    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(
      UINT Width,
      UINT Height,
      D3DFORMAT Format,
      D3DPOOL Pool,
      IDirect3DSurface9** ppSurface,
      HANDLE* pSharedHandle);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(
            DWORD              RenderTargetIndex,
            IDirect3DSurface9* pRenderTarget);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(
            DWORD               RenderTargetIndex,
            IDirect3DSurface9** ppRenderTarget);

    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil);

    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface);

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

    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* pViewport);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* pViewport);

    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* pMaterial);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* pMaterial);

    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT9* pLight);

    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT9* pLight);

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable);

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL* pEnable);

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, const float* pPlane);

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float* pPlane);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue);

    HRESULT STDMETHODCALLTYPE CreateStateBlock(
            D3DSTATEBLOCKTYPE      Type,
            IDirect3DStateBlock9** ppSB);

    HRESULT STDMETHODCALLTYPE BeginStateBlock();

    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** ppSB);

    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* pClipStatus);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD*                   pValue);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(
            DWORD                    Stage,
            D3DTEXTURESTAGESTATETYPE Type,
            DWORD                    Value);

    HRESULT STDMETHODCALLTYPE GetSamplerState(
            DWORD               Sampler,
            D3DSAMPLERSTATETYPE Type,
            DWORD*              pValue);

    HRESULT STDMETHODCALLTYPE SetSamplerState(
            DWORD               Sampler,
            D3DSAMPLERSTATETYPE Type,
            DWORD               Value);

    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses);

    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries);

    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber);

    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber);

    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* pRect);

    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* pRect);

    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware);

    BOOL    STDMETHODCALLTYPE GetSoftwareVertexProcessing();

    HRESULT STDMETHODCALLTYPE SetNPatchMode(float nSegments);

    float   STDMETHODCALLTYPE GetNPatchMode();

    HRESULT STDMETHODCALLTYPE DrawPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             StartVertex,
            UINT             PrimitiveCount);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            INT              BaseVertexIndex,
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
            IDirect3DVertexBuffer9*      pDestBuffer,
            IDirect3DVertexDeclaration9* pVertexDecl,
            DWORD                        Flags);

    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(
      const D3DVERTEXELEMENT9*            pVertexElements,
            IDirect3DVertexDeclaration9** ppDecl);

    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl);

    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl);

    HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF);

    HRESULT STDMETHODCALLTYPE GetFVF(DWORD* pFVF);

    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const DWORD*                   pFunction,
            IDirect3DVertexShader9** ppShader);

    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* pShader);

    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** ppShader);

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(
            UINT   StartRegister,
            float* pConstantData,
            UINT   Vector4fCount);

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(
            UINT StartRegister,
            int* pConstantData,
            UINT Vector4iCount);

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(
            UINT  StartRegister,
            BOOL* pConstantData,
            UINT  BoolCount);

    HRESULT STDMETHODCALLTYPE SetStreamSource(
            UINT                    StreamNumber,
            IDirect3DVertexBuffer9* pStreamData,
            UINT                    OffsetInBytes,
            UINT                    Stride);

    HRESULT STDMETHODCALLTYPE GetStreamSource(
            UINT                     StreamNumber,
            IDirect3DVertexBuffer9** ppStreamData,
            UINT*                    pOffsetInBytes,
            UINT*                    pStride);

    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT StreamNumber, UINT Setting);

    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting);

    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* pIndexData);

    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** ppIndexData);

    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const DWORD*                  pFunction, 
            IDirect3DPixelShader9** ppShader);

    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* pShader);

    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** ppShader);

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(
            UINT   StartRegister,
      const float* pConstantData,
            UINT   Vector4fCount);

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(
            UINT   StartRegister,
            float* pConstantData,
            UINT   Vector4fCount);

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(
            UINT StartRegister,
      const int* pConstantData,
            UINT Vector4iCount);

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(
            UINT StartRegister,
            int* pConstantData,
            UINT Vector4iCount);

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(
            UINT  StartRegister,
      const BOOL* pConstantData,
            UINT  BoolCount);

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(
            UINT  StartRegister,
            BOOL* pConstantData,
            UINT  BoolCount);

    HRESULT STDMETHODCALLTYPE DrawRectPatch(
            UINT               Handle,
      const float*             pNumSegs,
      const D3DRECTPATCH_INFO* pRectPatchInfo);

    HRESULT STDMETHODCALLTYPE DrawTriPatch(
            UINT              Handle,
      const float*            pNumSegs,
      const D3DTRIPATCH_INFO* pTriPatchInfo);

    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);

    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery);

    // Ex Methods

    HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(
            UINT   width,
            UINT   height,
            float* rows,
            float* columns);

    HRESULT STDMETHODCALLTYPE ComposeRects(
            IDirect3DSurface9*      pSrc,
            IDirect3DSurface9*      pDst,
            IDirect3DVertexBuffer9* pSrcRectDescs,
            UINT                    NumRects,
            IDirect3DVertexBuffer9* pDstRectDescs,
            D3DCOMPOSERECTSOP       Operation,
            int                     Xoffset,
            int                     Yoffset);

    HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT* pPriority);

    HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority);

    HRESULT STDMETHODCALLTYPE WaitForVBlank(UINT iSwapChain);

    HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources);

    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency);

    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency);

    HRESULT STDMETHODCALLTYPE CheckDeviceState(HWND hDestinationWindow);

    HRESULT STDMETHODCALLTYPE PresentEx(
      const RECT*    pSourceRect,
      const RECT*    pDestRect,
            HWND     hDestWindowOverride,
      const RGNDATA* pDirtyRegion,
            DWORD    dwFlags);

    HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            DWORD               MultisampleQuality,
            BOOL                Lockable,
            IDirect3DSurface9** ppSurface,
            HANDLE*             pSharedHandle,
            DWORD               Usage);

    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DPOOL             Pool,
            IDirect3DSurface9** ppSurface,
            HANDLE*             pSharedHandle,
            DWORD               Usage);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(
            UINT                Width,
            UINT                Height,
            D3DFORMAT           Format,
            D3DMULTISAMPLE_TYPE MultiSample,
            DWORD               MultisampleQuality,
            BOOL                Discard,
            IDirect3DSurface9** ppSurface,
            HANDLE*             pSharedHandle,
            DWORD               Usage);

    HRESULT STDMETHODCALLTYPE ResetEx(
            D3DPRESENT_PARAMETERS* pPresentationParameters,
            D3DDISPLAYMODEEX*      pFullscreenDisplayMode);

    HRESULT STDMETHODCALLTYPE GetDisplayModeEx(
            UINT                iSwapChain,
            D3DDISPLAYMODEEX*   pMode,
            D3DDISPLAYROTATION* pRotation);

    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChainEx(
            D3DPRESENT_PARAMETERS* pPresentationParameters,
      const D3DDISPLAYMODEEX*      pFullscreenDisplayMode,
            IDirect3DSwapChain9**  ppSwapChain);

    /**
     * @brief Sets the given sampler state
     *
     * @param StateSampler Sampler index (according to our internal way of storing samplers)
     * @param Type Sampler state type to change
     * @param Value State value
     */
    HRESULT SetStateSamplerState(
        DWORD               StateSampler,
        D3DSAMPLERSTATETYPE Type,
        DWORD               Value);

    /**
     * @brief Sets the given sampler texture
     *
     * @param StateSampler Sampler index (according to our internal way of storing samplers)
     * @param pTexture Texture to use
     */
    HRESULT SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture);

    /**
     * @brief Sets the transform for the given sampler
     *
     * @param idx Sampler index (according to our internal way of storing samplers)
     * @param pMatrix Transform matrix
     */
    HRESULT SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix);

    /**
     * @brief Sets the fixed function texture processing state
     *
     * @param Stage Sampler index (according to our internal way of storing samplers)
     * @param Type Fixed function texture stage type
     * @param Value Value for the state
     */
    HRESULT SetStateTextureStageState(
            DWORD                      Stage,
            D3D9TextureStageStateTypes Type,
            DWORD                      Value);

    VkPipelineStageFlags GetEnabledShaderStages() const {
      return m_dxvkDevice->getShaderPipelineStages();
    }

    static DxvkDeviceFeatures GetDeviceFeatures(const Rc<DxvkAdapter>& adapter);

    /**
     * \brief Returns whether the Vulkan device supports the required features for ProcessVertices
     */
    bool SupportsSWVP();

    bool IsExtended();

    HWND GetWindow();

    const Rc<DxvkDevice>& GetDXVKDevice() {
      return m_dxvkDevice;
    }

    D3D9_VK_FORMAT_MAPPING LookupFormat(
      D3D9Format            Format) const;

    const DxvkFormatInfo* UnsupportedFormatInfo(
      D3D9Format            Format) const;

    bool WaitForResource(
      const DxvkPagedResource&      Resource,
            uint64_t                SequenceNumber,
            DWORD                   MapFlags);

    /**
     * \brief Locks a subresource of an image
     * 
     * \param [in] Subresource The subresource of the image to lock
     * \param [out] pLockedBox The returned locked box of the image, containing data ptr and strides
     * \param [in] pBox The region of the subresource to lock. This offsets the returned data ptr
     * \param [in] Flags The D3DLOCK_* flags to lock the image with
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    HRESULT LockImage(
            D3D9CommonTexture* pResource,
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

    /**
     * \brief Unlocks a subresource of an image
     *
     * Passthrough to device unlock.
     * \param [in] Subresource The subresource of the image to unlock
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    HRESULT UnlockImage(
            D3D9CommonTexture*      pResource,
            UINT                    Face,
            UINT                    MipLevel);

    /**
     * \brief Uploads the given texture subresource from its local system memory copy.
     */
    HRESULT FlushImage(
            D3D9CommonTexture*      pResource,
            UINT                    Subresource);

    /**
     * \brief Copies the given part of a texture from the local system memory copy of the source texture
     * to the image of the destination texture.
     */
    void UpdateTextureFromBuffer(
            D3D9CommonTexture*      pDestTexture,
            D3D9CommonTexture*      pSrcTexture,
            UINT                    DestSubresource,
            UINT                    SrcSubresource,
            VkOffset3D              SrcOffset,
            VkExtent3D              SrcExtent,
            VkOffset3D              DestOffset);

    void EmitGenerateMips(
            D3D9CommonTexture* pResource);

    HRESULT LockBuffer(
            D3D9CommonBuffer*       pResource,
            UINT                    OffsetToLock,
            UINT                    SizeToLock,
            void**                  ppbData,
            DWORD                   Flags);

    /**
     * \brief Uploads the given buffer from its local system memory copy.
     */
    HRESULT FlushBuffer(
            D3D9CommonBuffer*       pResource);

    HRESULT UnlockBuffer(
            D3D9CommonBuffer*       pResource);

    /**
     * @brief Uploads data from D3DPOOL_SYSMEM + D3DUSAGE_DYNAMIC buffers and binds the temporary buffers.
     *
     * @param FirstVertexIndex The first vertex
     * @param NumVertices The number of vertices that are accessed. If this is 0, the vertex buffer binding will not be modified.
     * @param FirstIndex The first index
     * @param NumIndices The number of indices that will be drawn. If this is 0, the index buffer binding will not be modified.
     */
    void UploadPerDrawData(
            UINT&                   FirstVertexIndex,
            UINT                    NumVertices,
            UINT&                   FirstIndex,
            UINT                    NumIndices,
            INT&                    BaseVertexIndex,
            bool*                   pDynamicVBOs,
            bool*                   pDynamicIBO);


    void SetupFPU();

    int64_t DetermineInitialTextureMemory();

    void CreateConstantBuffers();

    void SynchronizeCsThread(uint64_t SequenceNumber);

    void Flush();
    void FlushAndSync9On12();

    void BeginFrame(Rc<DxvkLatencyTracker> LatencyTracker, uint64_t FrameId);
    void EndFrame(Rc<DxvkLatencyTracker> LatencyTracker);

    void UpdateActiveRTs(uint32_t index);

    template <uint32_t Index>
    void UpdateAnyColorWrites();

    void UpdateTextureBitmasks(uint32_t index, DWORD combinedUsage);

    void UpdateActiveHazardsRT(uint32_t rtMask);

    void UpdateActiveHazardsDS(uint32_t texMask);

    void MarkRenderHazards();

    void UpdateActiveFetch4(uint32_t stateSampler);

    /**
     * @brief Sets the mismatching texture type bits for all samplers if necessary.
     *
     * This function will check all samplers the shader uses and set the  set the mismatching texture type bit for the given sampler if it does not
     * match the texture type expected by the respective shader.
     *
     * It will *not* unset the bit if the texture type does match.
     *
     * @param stateSampler Sampler index (according to our internal way of storing samplers)
     */

     /**
      * @brief Sets the mismatching texture type bits for all samplers if necessary.
      *
      * This function will check all samplers the shader uses and set the  set the mismatching texture type bit for the given sampler if it does not
      * match the texture type expected by the shader.
      *
      * @param shader The shader
      * @param shaderSamplerMask Mask of all samplers that the shader uses (according to our internal way of storing samplers)
      * @param shaderSamplerOffset First index of the shader's samplers according to our internal way of storing samplers.
      *                            Used to transform the sampler indices that are relative to the entire pipeline to ones relative to the shader.
      */
    void UpdateTextureTypeMismatchesForShader(const D3D9CommonShader* shader, uint32_t shaderSamplerMask, uint32_t shaderSamplerOffset);

    /**
     * @brief Sets the mismatching texture type bit for the given sampler.
     *
     * This function will set the mismatching texture type bit for the given sampler if it does not
     * match the texture type expected by the respective shader.
     *
     * It will *not* unset the bit if the texture type does match.
     *
     * @param stateSampler Sampler index (according to our internal way of storing samplers)
     */
    void UpdateTextureTypeMismatchesForTexture(uint32_t stateSampler);

    void UploadManagedTexture(D3D9CommonTexture* pResource);

    void UploadManagedTextures(uint32_t mask);

    void GenerateTextureMips(uint32_t mask);

    void MarkTextureMipsDirty(D3D9CommonTexture* pResource);

    void MarkTextureMipsUnDirty(D3D9CommonTexture* pResource);

    void MarkTextureUploaded(D3D9CommonTexture* pResource);

    void UpdatePointMode(bool pointList);

    void UpdateFog();

    void BindFramebuffer();

    void BindViewportAndScissor();

    inline bool IsAlphaToCoverageEnabled() {
      const bool alphaTest = m_state.renderStates[D3DRS_ALPHATESTENABLE] != 0;

      return (m_amdATOC || (m_nvATOC && alphaTest)) && m_flags.test(D3D9DeviceFlag::ValidSampleMask);
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

    inline void UploadSoftwareConstantSet(const D3D9ShaderConstantsVSSoftware& Src, const D3D9ConstantLayout& Layout);

    inline void* CopySoftwareConstants(D3D9ConstantBuffer& dstBuffer, const void* src, uint32_t size);

    template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
    inline void UploadConstantSet(const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader);
    
    template <DxsoProgramType ShaderStage>
    void UploadConstants();
    
    void UpdateClipPlanes();

    /**
     * \brief Updates the push constant data at the given offset with data from the specified pointer.
     *
     * \param Offset Offset at which the push constant data gets written.
     * \param Length Length of the push constant data to write.
     * \param pData Push constant data
     */
    template <uint32_t Offset, uint32_t Length>
    void UpdatePushConstant(const void* pData);

    /**
     * \brief Updates the specified push constant based on the device state.
     *
     * \param Item Render state push constant to update
     */
    template <D3D9RenderStateItem Item>
    void UpdatePushConstant();

    void BindSampler(DWORD Sampler);

    void BindTexture(DWORD SamplerSampler);

    void UnbindTextures(uint32_t mask);

    void UndirtySamplers(uint32_t mask);

    void UndirtyTextures(uint32_t usedMask);

    void MarkTextureBindingDirty(IDirect3DBaseTexture9* texture);

    HRESULT STDMETHODCALLTYPE SetRenderTargetInternal(
            DWORD              RenderTargetIndex,
            IDirect3DSurface9* pRenderTarget);

    D3D9DrawInfo GenerateDrawInfo(
      D3DPRIMITIVETYPE PrimitiveType,
      UINT             PrimitiveCount,
      UINT             InstanceCount);
    
    uint32_t GetInstanceCount() const;

    void PrepareDraw(D3DPRIMITIVETYPE PrimitiveType, bool UploadVBOs, bool UploadIBOs);

    void EnsureSamplerLimit();

    template <DxsoProgramType ShaderStage>
    void BindShader(
      const D3D9CommonShader*                 pShaderModule);

    void BindInputLayout();

    void BindVertexBuffer(
            UINT                              Slot,
            D3D9VertexBuffer*                 pBuffer,
            UINT                              Offset,
            UINT                              Stride);

    void BindIndices();

    D3D9DeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    const D3D9Options* GetOptions() const {
      return &m_d3d9Options;
    }

    Direct3DState9* GetRawState() {
      return &m_state;
    }

    void Begin(D3D9Query* pQuery);
    void End(D3D9Query* pQuery);

    void SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits);
    void SetPixelBoolBitfield (uint32_t idx, uint32_t mask, uint32_t bits);

    void ConsiderFlush(GpuFlushType FlushType);

    bool ChangeReportedMemory(int64_t delta) {
      if (IsExtended())
        return true;

      int64_t availableMemory = m_availableMemory.fetch_add(delta);

      return !m_d3d9Options.memoryTrackTest || availableMemory >= delta;
    }

    void ResolveZ();

    void TransitionImage(D3D9CommonTexture* pResource, VkImageLayout NewLayout);

    void TransformImage(
            D3D9CommonTexture*       pResource,
      const VkImageSubresourceRange* pSubresources,
            VkImageLayout            OldLayout,
            VkImageLayout            NewLayout);

    const D3D9ConstantLayout& GetVertexConstantLayout() { return m_consts[DxsoProgramType::VertexShader].layout; }
    const D3D9ConstantLayout& GetPixelConstantLayout()  { return m_consts[DxsoProgramType::PixelShader].layout; }

    void ResetState(D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT ResetSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    HRESULT InitialReset(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    /**
     * \brief Returns the allocator used for unmappable system memory texture data
     */
    D3D9MemoryAllocator* GetAllocator() {
      return &m_memoryAllocator;
    }

    /**
     * \brief Gets the pointer of the system memory copy of the texture
     *
     * Also tracks the texture if it is unmappable.
     */
    void* MapTexture(D3D9CommonTexture* pTexture, UINT Subresource);

    /**
     * \brief Moves the texture to the front of the LRU list of mapped textures
     */
    void TouchMappedTexture(D3D9CommonTexture* pTexture);

    /**
     * \brief Removes the texture from the LRU list of mapped textures
     */
    void RemoveMappedTexture(D3D9CommonTexture* pTexture);

    /**
     * \brief Returns whether the device is currently recording a StateBlock
     */
    bool ShouldRecord() const {
      return m_recorder != nullptr;
    }

    bool IsD3D8Compatible() const {
      return m_isD3D8Compatible;
    }

    // Device Lost
    bool IsDeviceLost() const {
      return m_deviceLostState != D3D9DeviceLostState::Ok;
    }

    void NotifyFullscreen(HWND window, bool fullscreen);
    void NotifyWindowActivated(HWND window, bool activated);

    /**
     * \brief Increases the amount of D3DPOOL_DEFAULT resources that block a device reset
     */
    void IncrementLosableCounter() {
      m_losableResourceCounter++;
    }

    /**
     * \brief Decreases the amount of D3DPOOL_DEFAULT resources that block a device reset
     */
    void DecrementLosableCounter() {
      m_losableResourceCounter--;
    }

    /**
     * \brief Returns whether the device is configured to only support vertex processing.
     */
    bool CanOnlySWVP() const {
      return m_behaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    /**
     * \brief Returns whether the device can be set to do software vertex processing.
     * It may also be set up to only support software vertex processing.
     */
    bool CanSWVP() const {
      return m_behaviorFlags & (D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_SOFTWARE_VERTEXPROCESSING);
    }

    /**
     * \brief Returns whether or not the device is currently set to do software vertex processing.
     */
    bool IsSWVP() const {
      return m_isSWVP;
    }

    /**
     * \brief Returns the number of vertex shader modules generated for fixed function state.
     */
    UINT GetFixedFunctionVSCount() const {
      return m_ffModules.GetVSCount();
    }

    /**
     * \brief Returns the number of fragment shader modules generated for fixed function state.
     */
    UINT GetFixedFunctionFSCount() const {
      return m_ffModules.GetFSCount();
    }

    /**
     * \brief Returns the number of shader modules generated for ProcessVertices.
     */
    UINT GetSWVPShaderCount() const {
      return m_swvpEmulator.GetShaderCount();
    }

    void InjectCsChunk(
            DxvkCsChunkRef&&            Chunk,
            bool                        Synchronize);

    template<typename Fn>
    void InjectCs(
            Fn&&                        Command) {
      auto chunk = AllocCsChunk();
      chunk->push(std::move(Command));

      InjectCsChunk(std::move(chunk), false);
    }

    DxvkCsChunkRef AllocCsChunk() {
      DxvkCsChunk* chunk = m_csChunkPool.allocChunk(DxvkCsChunkFlag::SingleUse);
      return DxvkCsChunkRef(chunk, &m_csChunkPool);
    }

  private:

    template<bool AllowFlush = true, typename Cmd>
    void EmitCs(Cmd&& command) {
      if (unlikely(!m_csChunk->push(command))) {
        EmitCsChunk(std::move(m_csChunk));
        m_csChunk = AllocCsChunk();

        if constexpr (AllowFlush)
          ConsiderFlush(GpuFlushType::ImplicitWeakHint);

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

    /**
     * \brief Queries current reset counter
     * Used for the deferred surface creation workaround.
     * (Device Reset detection for D3D9SwapChainEx::Present)
     */
    uint32_t GetResetCounter() {
      return m_resetCtr;
    }

    template <bool Synchronize9On12>
    void ExecuteFlush();

    void DetermineConstantLayouts(bool canSWVP);

    /**
     * \brief Allocates buffer memory for DrawPrimitiveUp draws
     */
    D3D9BufferSlice AllocUPBuffer(VkDeviceSize size);

    /**
     * \brief Allocates buffer memory for resource uploads
     */
    D3D9BufferSlice AllocStagingBuffer(VkDeviceSize size);

    /**
     * \brief Waits until the amount of used staging memory is below a certain threshold.
     */
    void WaitStagingBuffer();

    HRESULT               CreateShaderModule(
            D3D9CommonShader*     pShaderModule,
            uint32_t*             pLength,
            VkShaderStageFlagBits ShaderStage,
      const DWORD*                pShaderBytecode,
      const DxsoModuleInfo*       pModuleInfo);

    inline uint32_t GetUPDataSize(uint32_t vertexCount, uint32_t stride) {
      return vertexCount * stride;
    }

    inline uint32_t GetUPBufferSize(uint32_t vertexCount, uint32_t stride) {
      return (vertexCount - 1) * stride + std::max(m_state.vertexDecl->GetSize(0), stride);
    }

    /**
     * \brief Writes data to the given pointer and zeroes any access buffer space
     */
    inline void FillUPVertexBuffer(void* buffer, const void* userData, uint32_t dataSize, uint32_t bufferSize) {
      uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
      // Don't copy excess data if we don't end up needing it.
      dataSize = std::min(dataSize, bufferSize);
      std::memcpy(data, userData, dataSize);
      // Pad out with 0 to make buffer range checks happy
      // Some games have components out of range in the vertex decl
      // that they don't read from the shader.
      // My tests show that these are read back as 0 always if out of range of
      // the dataSize.
      //
      // So... make the actual buffer the range that satisfies the range of the vertex
      // declaration and pad with 0s outside of it.
      if (dataSize < bufferSize)
        std::memset(data + dataSize, 0, bufferSize - dataSize);
    }

    // So we don't do OOB.
    template <DxsoProgramType  ProgramType,
              D3D9ConstantType ConstantType>
    inline static constexpr uint32_t DetermineSoftwareRegCount() {
      constexpr bool isVS = ProgramType == DxsoProgramType::VertexShader;

      switch (ConstantType) {
        default:
        case D3D9ConstantType::Float:  return isVS ? caps::MaxFloatConstantsSoftware : caps::MaxFloatConstantsPS;
        case D3D9ConstantType::Int:    return isVS ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
        case D3D9ConstantType::Bool:   return isVS ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
      }
    }

    // So we don't copy more than we need.
    template <DxsoProgramType  ProgramType,
              D3D9ConstantType ConstantType>
    inline uint32_t DetermineHardwareRegCount() const {
      const auto& layout = m_consts[ProgramType].layout;

      switch (ConstantType) {
        default:
        case D3D9ConstantType::Float:  return layout.floatCount;
        case D3D9ConstantType::Int:    return layout.intCount;
        case D3D9ConstantType::Bool:   return layout.boolCount;
      }
    }

    inline uint32_t GetFrameLatency() {
      return m_frameLatency;
    }

    template <
      DxsoProgramType  ProgramType,
      D3D9ConstantType ConstantType,
      typename         T>
      HRESULT SetShaderConstants(
              UINT  StartRegister,
        const T*    pConstantData,
              UINT  Count);

    template <
      DxsoProgramType  ProgramType,
      D3D9ConstantType ConstantType,
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

        if constexpr (ConstantType == D3D9ConstantType::Float) {
          const float* source = set->fConsts[StartRegister].data;
          const size_t size   = Count * sizeof(Vector4);

          std::memcpy(pConstantData, source, size);
        }
        else if constexpr (ConstantType == D3D9ConstantType::Int) {
          const int*  source = set->iConsts[StartRegister].data;
          const size_t size  = Count * sizeof(Vector4i);

          std::memcpy(pConstantData, source, size);
        }
        else {
          for (uint32_t i = 0; i < Count; i++) {
            const uint32_t constantIdx = StartRegister + i;
            const uint32_t arrayIdx    = constantIdx / 32;
            const uint32_t bitIdx      = constantIdx % 32;

            const uint32_t bit         = (1u << bitIdx);

            bool constValue = set->bConsts[arrayIdx] & bit;
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

    uint32_t GetAlphaTestPrecision();

    void BindAlphaTestState();

    void UpdateAlphaTestSpec(VkCompareOp alphaOp, uint32_t precision);
    void UpdateVertexBoolSpec(uint32_t value);
    void UpdatePixelBoolSpec(uint32_t value);
    void UpdatePixelShaderSamplerSpec(uint32_t types, uint32_t projections, uint32_t fetch4);
    void UpdateCommonSamplerSpec(uint32_t boundMask, uint32_t depthMask, uint32_t drefMask);
    void UpdatePointModeSpec(uint32_t mode);
    void UpdateFogModeSpec(bool fogEnabled, D3DFOGMODE vertexFogMode, D3DFOGMODE pixelFogMode);

    void BindSpecConstants();

    void TrackBufferMappingBufferSequenceNumber(
      D3D9CommonBuffer* pResource);

    void TrackTextureMappingBufferSequenceNumber(
      D3D9CommonTexture* pResource,
      UINT Subresource);

    uint64_t GetCurrentSequenceNumber();

    /**
     * \brief Will unmap the least recently used textures if the amount of mapped texture memory exceeds a threshold.
     */
    void UnmapTextures();

    /**
     * \brief Get the swapchain that was used the most recently for presenting
     * Has to be externally synchronized.
     */
    D3D9SwapChainEx* GetMostRecentlyUsedSwapchain() {
      return m_mostRecentlyUsedSwapchain;
    }

    /**
     * \brief Set the swapchain that was used the most recently for presenting
     * Has to be externally synchronized.
     */
    void SetMostRecentlyUsedSwapchain(D3D9SwapChainEx* swapchain) {
      m_mostRecentlyUsedSwapchain = swapchain;
    }

    /**
     * @brief Reset the most recently swapchain back to the implicit one
     * Has to be externally synchronized.
     */
    void ResetMostRecentlyUsedSwapchain() {
      m_mostRecentlyUsedSwapchain = m_implicitSwapchain.ptr();
    }

    bool IsTextureBoundAsAttachment(const D3D9CommonTexture* pTexture) const {
      if (unlikely(pTexture->IsRenderTarget())) {
        for (uint32_t i = 0u; i < m_state.renderTargets.size(); i++) {
          if (m_state.renderTargets[i] == nullptr)
            continue;

          auto texInfo = m_state.renderTargets[i]->GetCommonTexture();
          if (unlikely(texInfo == pTexture)) {
            return true;
          }
        }
      } else if (unlikely(pTexture->IsDepthStencil() && m_state.depthStencil != nullptr)) {
        auto texInfo = m_state.depthStencil->GetCommonTexture();
        if (unlikely(texInfo == pTexture)) {
          return true;
        }
      }
      return false;
    }

    inline bool HasRenderTargetBound(uint32_t Index) const {
      return m_state.renderTargets[Index] != nullptr
        && !m_state.renderTargets[Index]->IsNull();
    }

    GpuFlushType GetMaxFlushType() const;

    Com<D3D9InterfaceEx>            m_parent;
    D3DDEVTYPE                      m_deviceType;
    HWND                            m_window;
    WORD                            m_behaviorFlags;

    D3D9Adapter*                    m_adapter;
    Rc<DxvkDevice>                  m_dxvkDevice;

    D3D9MemoryAllocator             m_memoryAllocator;

    // Second memory allocator used for D3D9 shader bytecode.
    // Most games never access the stored bytecode, so putting that
    // into the same chunks as texture memory would waste address space.
    D3D9MemoryAllocator             m_shaderAllocator;

    uint32_t                        m_frameLatency = DefaultFrameLatency;

    D3D9Initializer*                m_initializer = nullptr;
    D3D9FormatHelper*               m_converter   = nullptr;

    D3D9FFShaderModuleSet           m_ffModules;
    D3D9SWVPEmulator                m_swvpEmulator;

    Com<D3D9StateBlock, false>      m_recorder;

    Rc<D3D9ShaderModuleSet>         m_shaderModules;

    D3D9ConstantBuffer              m_vsClipPlanes;

    D3D9ConstantBuffer              m_vsFixedFunction;
    D3D9ConstantBuffer              m_vsVertexBlend;
    D3D9ConstantBuffer              m_psFixedFunction;
    D3D9ConstantBuffer              m_psShared;
    D3D9ConstantBuffer              m_specBuffer;

    Rc<DxvkBuffer>                  m_upBuffer;
    VkDeviceSize                    m_upBufferOffset  = 0ull;
    void*                           m_upBufferMapPtr  = nullptr;

    DxvkStagingBuffer               m_stagingBuffer;
    Rc<sync::Fence>                 m_stagingBufferFence;
    VkDeviceSize                    m_stagingMemorySignaled = 0ull;

    D3D9Cursor                      m_cursor;

    Com<D3D9Surface, false>         m_autoDepthStencil;

    Com<D3D9SwapChainEx, false>     m_implicitSwapchain;

    const D3D9Options               m_d3d9Options;
    DxsoOptions                     m_dxsoOptions;

    std::unordered_map<
      DWORD,
      Com<D3D9VertexDecl,
      false>>                       m_fvfTable;

    D3D9Multithread                 m_multithread;
    D3D9InputAssemblyState          m_iaState;

    D3D9DeviceFlags                 m_flags;
    // Last state of depth textures. Doesn't update when NULL is bound.
    // & with m_activeTextures to normalize.
    uint32_t                        m_instancedData = 0;

    uint32_t                        m_depthTextures = 0;
    uint32_t                        m_drefClamp = 0;
    uint32_t                        m_cubeTextures = 0;
    uint32_t                        m_textureTypes = 0;
    uint32_t                        m_mismatchingTextureTypes = 0;
    uint32_t                        m_projectionBitfield  = 0;

    uint32_t                        m_dirtySamplerStates = 0;
    uint32_t                        m_dirtyTextures      = 0;

    uint32_t                        m_activeRTsWhichAreTextures : 4;
    uint32_t                        m_alphaSwizzleRTs : 4;
    uint32_t                        m_lastHazardsRT   : 4;

    uint32_t                        m_activeTextureRTs       = 0;
    uint32_t                        m_activeTextureDSs       = 0;
    uint32_t                        m_activeHazardsRT        = 0;
    uint32_t                        m_activeHazardsDS        = 0;
    uint32_t                        m_activeTextures         = 0;
    uint32_t                        m_activeTexturesToUpload = 0;
    uint32_t                        m_activeTexturesToGen    = 0;

    uint32_t                        m_activeVertexBuffers                = 0;
    uint32_t                        m_activeVertexBuffersToUpload        = 0;
    uint32_t                        m_activeVertexBuffersToUploadPerDraw = 0;

    // m_fetch4Enabled is whether fetch4 is currently enabled
    // from the application.
    //
    // m_fetch4 is whether it should be enabled in the shader
    // ie. are we in a correct state to use it
    // (enabled + texture supports it + point sampled)
    uint32_t                        m_fetch4Enabled = 0;
    uint32_t                        m_fetch4        = 0;

    uint32_t                        m_lastHazardsDS = 0;
    uint32_t                        m_lastSamplerTypesFF = 0;

    D3D9SpecializationInfo          m_specInfo = D3D9SpecializationInfo();

    D3D9ShaderMasks                 m_vsShaderMasks = D3D9ShaderMasks();
    D3D9ShaderMasks                 m_psShaderMasks = FixedFunctionMask;

    bool                            m_isSWVP;
    bool                            m_isD3D8Compatible;
    bool                            m_amdATOC          = false;
    bool                            m_nvATOC           = false;
    bool                            m_ffZTest          = false;
    
    VkImageLayout                   m_hazardLayout = VK_IMAGE_LAYOUT_GENERAL;

    bool                            m_usingGraphicsPipelines = false;
    uint32_t                        m_resetCtr = 0u;

    DxvkDepthBiasRepresentation     m_depthBiasRepresentation = { VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, false };
    float                           m_depthBiasScale  = 0.0f;

    uint32_t                        m_robustSSBOAlignment     = 1;
    uint32_t                        m_robustUBOAlignment      = 1;

    D3D9ConstantSets                m_consts[DxsoProgramTypes::Count];
	
	D3D9UserDefinedAnnotation*      m_annotation = nullptr;

    D3D9ViewportInfo                m_viewportInfo;

    DxvkCsChunkPool                 m_csChunkPool;
    DxvkCsThread                    m_csThread;
    DxvkCsChunkRef                  m_csChunk;
    uint64_t                        m_csSeqNum = 0ull;

    Rc<sync::Fence>                 m_submissionFence;
    uint64_t                        m_submissionId = 0ull;
    DxvkSubmitStatus                m_submitStatus;

    uint64_t                        m_flushSeqNum = 0ull;
    GpuFlushTracker                 m_flushTracker;

    std::atomic<int64_t>            m_availableMemory = { 0 };

    D3D9DeviceLostState             m_deviceLostState          = D3D9DeviceLostState::Ok;
    HWND                            m_fullscreenWindow         = NULL;
    std::atomic<uint32_t>           m_losableResourceCounter   = { 0 };

    D3D9SwapChainEx*                m_mostRecentlyUsedSwapchain = nullptr;

#ifdef D3D9_ALLOW_UNMAPPING
    lru_list<D3D9CommonTexture*>    m_mappedTextures;
#endif

    // m_state should be declared last (i.e. freed first), because it
    // references objects that can call back into the device when freed.
    Direct3DState9                  m_state;

    D3D9VkInteropDevice             m_d3d9Interop;
    D3D9On12                        m_d3d9On12;
    DxvkD3D8Bridge                  m_d3d8Bridge;

    // Sampler statistics
    constexpr static uint32_t       SamplerCountBits = 12u;
    constexpr static uint64_t       SamplerCountMask = (1u << SamplerCountBits) - 1u;

    uint64_t                        m_samplerBindCount = 0u;

    uint64_t                        m_lastSamplerLiveCount = 0u;
    uint64_t                        m_lastSamplerBindCount = 0u;

    // Written by CS thread
    alignas(CACHE_LINE_SIZE)
    std::atomic<uint64_t>           m_lastSamplerStats = { 0u };
  };

}
