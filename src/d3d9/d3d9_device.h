#pragma once

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_cs.h"

#include "d3d9_include.h"
#include "d3d9_cursor.h"
#include "d3d9_format.h"
#include "d3d9_multithread.h"
#include "d3d9_adapter.h"
#include "d3d9_constant_set.h"

#include "d3d9_state.h"

#include "d3d9_options.h"

#include "../dxso/dxso_module.h"
#include "../dxso/dxso_util.h"
#include "../dxso/dxso_options.h"
#include "../dxso/dxso_modinfo.h"

#include "d3d9_sampler.h"
#include "d3d9_fixed_function.h"
#include "d3d9_swvp_emu.h"

#include "d3d9_shader_permutations.h"

#include <vector>
#include <type_traits>
#include <unordered_map>

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

  using D3D9DeviceFlags = Flags<D3D9DeviceFlag>;

  struct D3D9DrawInfo {
    uint32_t vertexCount;
    uint32_t instanceCount;
  };

  struct D3D9SamplerPair {
    Rc<DxvkSampler> color;
    Rc<DxvkSampler> depth;
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

    friend class D3D9SwapChainEx;
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

    HRESULT SetStateSamplerState(
        DWORD               StateSampler,
        D3DSAMPLERSTATETYPE Type,
        DWORD               Value);

    HRESULT SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture);

    HRESULT SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix);

    HRESULT SetStateTextureStageState(
            DWORD                      Stage,
            D3D9TextureStageStateTypes Type,
            DWORD                      Value);

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

    D3D9_VK_FORMAT_MAPPING LookupFormat(
      D3D9Format            Format) const;

    DxvkFormatInfo UnsupportedFormatInfo(
      D3D9Format            Format) const;

    bool WaitForResource(
      const Rc<DxvkResource>&                 Resource,
            DWORD                             MapFlags);

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

    HRESULT FlushImage(
            D3D9CommonTexture*      pResource,
            UINT                    Subresource);

    void EmitGenerateMips(
            D3D9CommonTexture* pResource);

    HRESULT LockBuffer(
            D3D9CommonBuffer*       pResource,
            UINT                    OffsetToLock,
            UINT                    SizeToLock,
            void**                  ppbData,
            DWORD                   Flags);

    HRESULT FlushBuffer(
            D3D9CommonBuffer*       pResource);

    HRESULT UnlockBuffer(
            D3D9CommonBuffer*       pResource);

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

    void UploadManagedTexture(D3D9CommonTexture* pResource);

    void UploadManagedTextures(uint32_t mask);

    void GenerateTextureMips(uint32_t mask);

    void MarkTextureMipsDirty(D3D9CommonTexture* pResource);

    void MarkTextureMipsUnDirty(D3D9CommonTexture* pResource);

    void MarkTextureUploaded(D3D9CommonTexture* pResource);

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
    inline void UploadSoftwareConstantSet(void* pData, const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader);

    template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
    inline void UploadConstantSet(const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader);
    
    template <DxsoProgramType ShaderStage>
    void UploadConstants();
    
    void UpdateClipPlanes();
    
    template <uint32_t Offset, uint32_t Length>
    void UpdatePushConstant(const void* pData);

    template <D3D9RenderStateItem Item>
    void UpdatePushConstant();

    void BindSampler(DWORD Sampler);

    void BindTexture(DWORD SamplerSampler);

    void UndirtySamplers();

    void UndirtyTextures();

    void MarkSamplersDirty();

    D3D9DrawInfo GenerateDrawInfo(
      D3DPRIMITIVETYPE PrimitiveType,
      UINT             PrimitiveCount,
      UINT             InstanceCount);
    
    uint32_t GetInstanceCount() const;

    void PrepareDraw(D3DPRIMITIVETYPE PrimitiveType);

    template <DxsoProgramType ShaderStage>
    void BindShader(
      const D3D9CommonShader*                 pShaderModule,
            D3D9ShaderPermutation             Permutation);

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

    void FlushImplicit(BOOL StrongHint);

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

    const D3D9ConstantLayout& GetVertexConstantLayout() { return m_vsLayout; }
    const D3D9ConstantLayout& GetPixelConstantLayout()  { return m_psLayout; }

    HRESULT ResetState(D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT ResetSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    HRESULT InitialReset(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode);

    UINT GetSamplerCount() const {
      return m_samplerCount.load();
    }

  private:

    D3D9DeviceFlags                 m_flags;
    uint32_t                        m_dirtySamplerStates = 0;
    uint32_t                        m_dirtyTextures = 0;

    D3D9Adapter*                    m_adapter;
    Rc<DxvkDevice>                  m_dxvkDevice;

    Rc<DxvkDataBuffer>              m_updateBuffer;
    DxvkCsChunkPool                 m_csChunkPool;
    dxvk::high_resolution_clock::time_point m_lastFlush
      = dxvk::high_resolution_clock::now();
    DxvkCsThread                    m_csThread;
    bool                            m_csIsBusy = false;

    uint32_t                        m_frameLatency = DefaultFrameLatency;

    D3D9Initializer*                m_initializer = nullptr;
    D3D9FormatHelper*               m_converter   = nullptr;

    DxvkCsChunkRef                  m_csChunk;

    D3D9FFShaderModuleSet           m_ffModules;
    D3D9SWVPEmulator                m_swvpEmulator;

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

    inline constexpr D3D9ShaderPermutation GetVertexShaderPermutation() {
      return D3D9ShaderPermutations::None;
    }

    inline D3D9ShaderPermutation GetPixelShaderPermutation() {
      if (unlikely(m_state.renderStates[D3DRS_SHADEMODE] == D3DSHADE_FLAT))
        return D3D9ShaderPermutations::FlatShade;

      return D3D9ShaderPermutations::None;
    }

    Com<D3D9InterfaceEx>            m_parent;
    D3DDEVTYPE                      m_deviceType;
    HWND                            m_window;

    DWORD                           m_behaviorFlags;
    Direct3DState9                  m_state;
    Com<D3D9StateBlock>             m_recorder;
    D3D9Multithread                 m_multithread;

    Rc<D3D9ShaderModuleSet>         m_shaderModules;

    D3D9ConstantSets                m_consts[DxsoProgramTypes::Count];

    Rc<DxvkBuffer>                  m_vsClipPlanes;

    Rc<DxvkBuffer>                  m_vsFixedFunction;
    Rc<DxvkBuffer>                  m_vsVertexBlend;
    Rc<DxvkBuffer>                  m_psFixedFunction;
    Rc<DxvkBuffer>                  m_psShared;

    D3D9BufferSlice                 m_upBuffer;
    D3D9BufferSlice                 m_managedUploadBuffer;

    const D3D9Options               m_d3d9Options;
    DxsoOptions                     m_dxsoOptions;

    BOOL                            m_isSWVP;

    D3DPRESENT_PARAMETERS           m_presentParams;

    D3D9Cursor                      m_cursor;

    Com<D3D9Surface, false>         m_autoDepthStencil;

    Com<D3D9SwapChainEx, false>     m_implicitSwapchain;

    std::unordered_map<
      D3D9SamplerKey,
      D3D9SamplerPair,
      D3D9SamplerKeyHash,
      D3D9SamplerKeyEq>             m_samplers;

    std::unordered_map<
      DWORD,
      Com<D3D9VertexDecl,
      false>>                       m_fvfTable;

    D3D9InputAssemblyState          m_iaState;

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

    D3D9ShaderMasks                 m_vsShaderMasks = D3D9ShaderMasks();
    D3D9ShaderMasks                 m_psShaderMasks = FixedFunctionMask;

    D3D9ViewportInfo                m_viewportInfo;

    std::atomic<int64_t>            m_availableMemory = 0;
    std::atomic<int32_t>            m_samplerCount = 0;

    bool                            m_amdATOC         = false;
    bool                            m_nvATOC          = false;
    bool                            m_ffZTest         = false;

    float                           m_depthBiasScale  = 0.0f;

    D3D9ConstantLayout              m_vsLayout;
    D3D9ConstantLayout              m_psLayout;

    void DetermineConstantLayouts(bool canSWVP);

    template<bool UpBuffer>
    D3D9BufferSlice AllocTempBuffer(VkDeviceSize size);

    bool ShouldRecord();

    HRESULT               CreateShaderModule(
            D3D9CommonShader*     pShaderModule,
            VkShaderStageFlagBits ShaderStage,
      const DWORD*                pShaderBytecode,
      const DxsoModuleInfo*       pModuleInfo);

    inline uint32_t GetUPDataSize(uint32_t vertexCount, uint32_t stride) {
      return vertexCount * stride;
    }

    inline uint32_t GetUPBufferSize(uint32_t vertexCount, uint32_t stride) {
      return (vertexCount - 1) * stride + std::max(m_state.vertexDecl->GetSize(), stride);
    }

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
      const auto& layout = ProgramType == DxsoProgramType::VertexShader
        ? m_vsLayout : m_psLayout;

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
          const float* source = set.fConsts[StartRegister].data;
          const size_t size   = Count * sizeof(Vector4);

          std::memcpy(pConstantData, source, size);
        }
        else if constexpr (ConstantType == D3D9ConstantType::Int) {
          const int*  source = set.iConsts[StartRegister].data;
          const size_t size  = Count * sizeof(Vector4i);

          std::memcpy(pConstantData, source, size);
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

    void UpdateFetch4SpecConstant(uint32_t value);

  };

}
