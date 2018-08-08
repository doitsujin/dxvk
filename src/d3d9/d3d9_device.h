#pragma once

#include "d3d9_include.h"
#include "d3d9_adapter.h"

namespace dxvk {
  /// Class representing a logical graphics device.
  ///
  /// To simplify implementation of this huge class and help with modularity,
  /// it's implementation is broken up in multiple source files.
  class D3D9Device final: public ComObject<IDirect3DDevice9> {
    void UpdateOMViews();

    IDirect3D9* m_parent;
    D3DDEVICE_CREATION_PARAMETERS m_creationParams;

    D3D9Adapter& m_adapter;
    Com<ID3D11Device> m_device;
    Com<ID3D11DeviceContext> m_ctx;
    Com<IDXGISwapChain> m_swapChain;

    // TODO: multiple render target support.
    Com<IDirect3DSurface9> m_renderTarget;
    Com<IDirect3DSurface9> m_depthStencil;

  public:
    // What follows are all of Direct3D9Device's functions.
    // They are implemented in their related files.

    D3D9Device(IDirect3D9* parent, D3D9Adapter& adapter,
      const D3DDEVICE_CREATION_PARAMETERS& cp, D3DPRESENT_PARAMETERS& pp);
    virtual ~D3D9Device();

    /// IUnknown functions.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final override;

    /// Getters for the initial parameters.
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** ppD3D9) final override;
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* pCaps) final override;
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) final override;

    /// Device state functions.
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() final override;
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) final override;

    /// Resource management functions.
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() final override;
    HRESULT STDMETHODCALLTYPE EvictManagedResources() final override;

    /// Viewport functions.
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* pViewport) final override;
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* pViewport) final override;

    /// Render target functions.
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height,
      D3DFORMAT Format,  D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) final override;

    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex,
      IDirect3DSurface9 *pRenderTarget) final override;
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex,
      IDirect3DSurface9** ppRenderTarget) final override;
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
      IDirect3DSurface9* pDestSurface) final override;

    /// Depth stencil functions.
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height,
      D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);

    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) final override;
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) final override;

    /// Gamma control functions.
    void STDMETHODCALLTYPE SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) final override;
    void STDMETHODCALLTYPE GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) final override;

    /// Hardware cursor functions.
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow) final override;
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) final override;
    void STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags) final override;

    /// Vertex and pixel shader functions.
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* pFunction,
      IDirect3DVertexShader9** ppShader) final override;

    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* pShader) final override;
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** ppShader) final override;

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT StartRegister,
      const BOOL* pConstantData, UINT BoolCount) final override;

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT StartRegister,
      const float* pConstantData, UINT Vector4fCount) final override;

    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT StartRegister,
      const int* pConstantData, UINT Vector4iCount) final override;

    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT StartRegister,
      BOOL* pConstantData, UINT BoolCount) final override;
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT StartRegister,
      float* pConstantData, UINT Vector4fCount) final override;
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT StartRegister,
      int* pConstantData, UINT Vector4iCount) final override;


    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* pFunction,
      IDirect3DPixelShader9** ppShader) final override;

    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* pShader) final override;
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** ppShader) final override;

    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT StartRegister,
      BOOL* pConstantData, UINT BoolCount) final override;
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT StartRegister,
      float* pConstantData, UINT Vector4fCount) final override;
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT StartRegister,
      int* pConstantData, UINT Vector4iCount) final override;

    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT StartRegister,
      const BOOL* pConstantData, UINT BoolCount) final override;
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT StartRegister,
      const float* pConstantData, UINT Vector4fCount) final override;
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT StartRegister,
      const int* pConstantData, UINT Vector4iCount) final override;

    /// Surface-related functions.
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
      IDirect3DSurface9* pDestSurface, const RECT* pDestRect,
      D3DTEXTUREFILTERTYPE Filter) final override;
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
      IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint) final override;

    /// Query creation function.
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type,
      IDirect3DQuery9** ppQuery) final override;


    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE BeginScene() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE EndScene() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type,
      IDirect3DStateBlock9** ppSB) final override {
      Logger::err("CreateStateBlock stub");
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE BeginStateBlock() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** ppSB) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags,
      D3DCOLOR Color, float Z, DWORD Stencil) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* pSurface,
      const RECT* pRect, D3DCOLOR color) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,
      IDirect3DSwapChain9** pSwapChain) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength, UINT Levels,
      DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length,
      DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT Width, UINT Height,
      D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements,
      IDirect3DVertexDeclaration9** ppDecl) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length,
      DWORD Usage, DWORD FVF, D3DPOOL Pool,
      IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels,
      DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width, UINT Height,
      UINT Depth, UINT Levels, DWORD Usage,
      D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,
      INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices,
      UINT startIndex, UINT primCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
      UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount,
      const void* pIndexData, D3DFORMAT IndexDataFormat,
      const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,
      UINT StartVertex, UINT PrimitiveCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
      UINT PrimitiveCount, const void* pVertexStreamZeroData,
      UINT VertexStreamZeroStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE DrawRectPatch(
  UINT                    Handle,
  CONST float            * pNumSegs,
  CONST D3DRECTPATCH_INFO* pRectPatchInfo) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawTriPatch(
  UINT                   Handle,
  CONST float            *pNumSegs,
  CONST D3DTRIPATCH_INFO *pTriPatchInfo) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
  UINT               iSwapChain,
  UINT               iBackBuffer,
  D3DBACKBUFFER_TYPE Type,
  IDirect3DSurface9**  ppBackBuffer) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float *pPlane) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* pClipStatus) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* PaletteNumber) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetFrontBufferData(
  UINT              iSwapChain,
  IDirect3DSurface9 *pDestSurface) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetFVF(
  DWORD *pFVF) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetIndices(
  IDirect3DIndexBuffer9** ppIndexData) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetLight(
  DWORD     Index,
  D3DLIGHT9 *) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetLightEnable(
  DWORD Index,
  BOOL  *pEnable) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetMaterial(
  D3DMATERIAL9 *pMaterial) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    float STDMETHODCALLTYPE GetNPatchMode() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    UINT STDMETHODCALLTYPE GetNumberOfSwapChains() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber,
      PALETTEENTRY* pEntries) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT iSwapChain,
      D3DRASTER_STATUS* pRasterStatus) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State,
      DWORD* pValue) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetSamplerState(
  DWORD               Sampler,
  D3DSAMPLERSTATETYPE Type,
  DWORD               *pValue) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetScissorRect(
  RECT *pRect) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber,
      IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT StreamNumber, UINT* Divider) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT iSwapChain,
      IDirect3DSwapChain9** pSwapChain) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage,
      IDirect3DBaseTexture9** ppTexture) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage,
      D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State,
      D3DMATRIX* pMatrix) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE TransformStateType,
      const D3DMATRIX* Matrix) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE Present(const RECT* pSourceRect, const RECT* pDestRect,
      HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* pIndexData) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT9* Light) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* pMaterial) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetNPatchMode(float nSegments) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex,
      UINT DestIndex, UINT VertexCount,
      IDirect3DVertexBuffer9* pDestBuffer,
      IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, const float* pPlane) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber,
  CONST PALETTEENTRY *pEntries) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State,
  DWORD              Value) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD               Sampler,
  D3DSAMPLERSTATETYPE Type,
  DWORD               Value
) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* pRect) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber,
      IDirect3DVertexBuffer9* pStreamData,
      UINT OffsetInBytes, UINT Stride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT StreamNumber,
  UINT Divider) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage,
      IDirect3DBaseTexture9* pTexture) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage,
      D3DTEXTURESTAGESTATETYPE Type, DWORD Value) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State,
      const D3DMATRIX *pMatrix) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,
      IDirect3DBaseTexture9* pDestinationTexture) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL bEnableDialogs) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }
  };
}
