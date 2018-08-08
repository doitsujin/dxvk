#pragma once

#include "d3d9_cursor.h"
#include "d3d9_device.h"
#include "d3d9_device_params.h"
#include "d3d9_query.h"
#include "d3d9_rt.h"
#include "d3d9_shader.h"
#include "d3d9_viewport.h"

namespace dxvk {
  /// This final Device implementation mixes in all of the partial implementations of the interface.
  class D3D9DeviceImpl final: public ComObject<>,
    public D3D9DeviceCursor,
    public D3D9DeviceParams,
    public D3D9DevicePixelShader,
    public D3D9DeviceQuery,
    public D3D9DeviceRenderTarget,
    public D3D9DeviceVertexShader,
    public D3D9DeviceViewport {
  public:
    D3D9DeviceImpl(IDirect3D9* parent, D3D9Adapter& adapter,
      const D3DDEVICE_CREATION_PARAMETERS& cp, D3DPRESENT_PARAMETERS& pp);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final override;

    // We delegate these methods to ComObject, but we need to override them here.
    ULONG STDMETHODCALLTYPE AddRef() final override;
    ULONG STDMETHODCALLTYPE Release() final override;

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

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height,
      D3DFORMAT Format,
      D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Discard,
      IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length,
      DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) final override {
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
      IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) final override {
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
      const void *pIndexData, D3DFORMAT IndexDataFormat,
      const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,
      UINT StartVertex, UINT PrimitiveCount) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(
  D3DPRIMITIVETYPE PrimitiveType,
  UINT             PrimitiveCount,
  CONST void       *pVertexStreamZeroData,
  UINT             VertexStreamZeroStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    HRESULT STDMETHODCALLTYPE DrawRectPatch(
  UINT                    Handle,
  CONST float             *pNumSegs,
  CONST D3DRECTPATCH_INFO *pRectPatchInfo) final override {
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
  IDirect3DSurface9  **ppBackBuffer) final override {
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

    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(
  IDirect3DSurface9 **ppZStencilSurface) final override {
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

    void STDMETHODCALLTYPE GetGammaRamp(
  UINT         iSwapChain,
  D3DGAMMARAMP *pRamp) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetIndices(
  IDirect3DIndexBuffer9 **ppIndexData) final override {
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

    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT                   StreamNumber,
  IDirect3DVertexBuffer9 **ppStreamData,
  UINT                   *OffsetInBytes,
  UINT                   *pStride) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(
  UINT StreamNumber,
  UINT *Divider) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetSwapChain(
  UINT                iSwapChain,
  IDirect3DSwapChain9 **pSwapChain) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTexture(
  DWORD                 Stage,
  IDirect3DBaseTexture9 **ppTexture) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTextureStageState(
  DWORD                    Stage,
  D3DTEXTURESTAGESTATETYPE Type,
  DWORD                    *pValue) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetTransform(
  D3DTRANSFORMSTATETYPE State,
  D3DMATRIX             *pMatrix
) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(
  IDirect3DVertexDeclaration9 **ppDecl) final override {
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

    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    void STDMETHODCALLTYPE SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) final override {
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
      IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags) final override {
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


    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
      IDirect3DSurface9* pDestSurface, const RECT* pDestRect,
      D3DTEXTUREFILTERTYPE Filter) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT* pSourceRect,
      IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint) final override {
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
