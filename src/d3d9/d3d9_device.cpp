#include "d3d9_device.h"

#include "d3d9_swapchain.h"
#include "d3d9_caps.h"

#include <algorithm>

namespace dxvk {

  Direct3DDevice9Ex::Direct3DDevice9Ex(
    bool extended,
    IDirect3D9Ex* parent,
    UINT adapter,
    Rc<DxvkAdapter> dxvkAdapter,
    Rc<DxvkDevice> dxvkDevice,
    D3DDEVTYPE deviceType,
    HWND window,
    DWORD flags,
    D3DPRESENT_PARAMETERS* presentParams,
    D3DDISPLAYMODEEX* displayMode)
    : m_extended{ extended }
    , m_parent{ parent }
    , m_adapter{ adapter }
    , m_dxvkAdapter{ dxvkAdapter }
    , m_device{ dxvkDevice }
    , m_deviceType{ deviceType }
    , m_window{ window } {
    HRESULT hr = this->Reset(presentParams);

    if (FAILED(hr))
      throw DxvkError("Direct3DDevice9Ex: device initial reset failed.");
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DDevice9)
     || (m_extended && riid == __uuidof(IDirect3DDevice9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DDevice9Ex::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::TestCooperativeLevel() {
    // Equivelant of D3D11/DXGI present tests. We can always present.
    return D3D_OK;
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetAvailableTextureMem() {
    auto memoryProp = m_dxvkAdapter->memoryProperties();

    VkDeviceSize availableTextureMemory = 0;

    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++)
      availableTextureMemory += memoryProp.memoryHeaps[i].size;

    // The value returned is a 32-bit value, so we need to clamp it.
    VkDeviceSize maxMemory = UINT32_MAX;
    availableTextureMemory = std::min(availableTextureMemory, maxMemory);

    return UINT(availableTextureMemory);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EvictManagedResources() {
    Logger::warn("Direct3DDevice9Ex::EvictManagedResources: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDirect3D(IDirect3D9** ppD3D9) {
    if (ppD3D9 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D9 = m_parent.ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDeviceCaps(D3DCAPS9* pCaps) {
    return caps::getDeviceCaps(m_adapter, m_deviceType, pCaps);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetDisplayMode(pMode);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    if (pParameters == nullptr)
      return D3DERR_INVALIDCALL;

    pParameters->AdapterOrdinal = m_adapter;
    pParameters->BehaviorFlags = m_flags;
    pParameters->DeviceType = m_deviceType;
    pParameters->hFocusWindow = m_window;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorProperties(
    UINT XHotSpot,
    UINT YHotSpot,
    IDirect3DSurface9* pCursorBitmap) {
    Logger::warn("Direct3DDevice9Ex::SetCursorProperties: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorPosition(int X, int Y, DWORD Flags) {
    m_cursor.updateCursor(X, Y, Flags & D3DCURSOR_IMMEDIATE_UPDATE);
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::ShowCursor(BOOL bShow) {
    // Ok so if they call FALSE here it means they want to use the regular windows cursor.
    // if they call TRUE here it means they want to use some weird bitmap cursor that I currently dont care about.
    // Therefore we always want to show the regular cursor no matter what!
    ::ShowCursor(true);

    return TRUE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) {
    Logger::warn("Direct3DDevice9Ex::CreateAdditionalSwapChain: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    InitReturnPtr(pSwapChain);

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr || pSwapChain == nullptr)
      return D3DERR_INVALIDCALL;

    *pSwapChain = static_cast<IDirect3DSwapChain9*>(ref(swapchain));

    return D3D_OK;
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetNumberOfSwapChains() {
    return UINT(m_swapchains.size());
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    Logger::warn("Direct3DDevice9Ex::Reset: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect, HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion) {
    Logger::warn("Direct3DDevice9Ex::Present: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetBackBuffer(
    UINT iSwapChain,
    UINT iBackBuffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppBackBuffer) {
    Logger::warn("Direct3DDevice9Ex::GetBackBuffer: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    Logger::warn("Direct3DDevice9Ex::GetRasterStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDialogBoxMode(BOOL bEnableDialogs) {
    Logger::warn("Direct3DDevice9Ex::SetDialogBoxMode: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetGammaRamp(
    UINT iSwapChain,
    DWORD Flags,
    const D3DGAMMARAMP* pRamp) {
    Logger::warn("Direct3DDevice9Ex::SetGammaRamp: Stub");
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    Logger::warn("Direct3DDevice9Ex::GetGammaRamp: Stub");
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateTexture(UINT Width,
    UINT Height,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVolumeTexture(
    UINT Width,
    UINT Height,
    UINT Depth,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DVolumeTexture9** ppVolumeTexture,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateVolumeTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateCubeTexture(
    UINT EdgeLength,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DCubeTexture9** ppCubeTexture,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateCubeTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexBuffer(
    UINT Length,
    DWORD Usage,
    DWORD FVF,
    D3DPOOL Pool,
    IDirect3DVertexBuffer9** ppVertexBuffer,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateVertexBuffer: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateIndexBuffer(
    UINT Length,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DIndexBuffer9** ppIndexBuffer,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateIndexBuffer: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTarget(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Lockable,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateRenderTarget: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurface(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateDepthStencilSurface: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateSurface(
    IDirect3DSurface9* pSourceSurface,
    const RECT* pSourceRect,
    IDirect3DSurface9* pDestinationSurface,
    const POINT* pDestPoint) {
    Logger::warn("Direct3DDevice9Ex::UpdateSurface: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    Logger::warn("Direct3DDevice9Ex::UpdateTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    Logger::warn("Direct3DDevice9Ex::GetRenderTargetData: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    Logger::warn("Direct3DDevice9Ex::GetFrontBufferData: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::StretchRect(
    IDirect3DSurface9* pSourceSurface,
    const RECT* pSourceRect,
    IDirect3DSurface9* pDestSurface,
    const RECT* pDestRect,
    D3DTEXTUREFILTERTYPE Filter) {
    Logger::warn("Direct3DDevice9Ex::StretchRect: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ColorFill(
    IDirect3DSurface9* pSurface,
    const RECT* pRect,
    D3DCOLOR color) {
    Logger::warn("Direct3DDevice9Ex::ColorFill: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurface(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateOffscreenPlainSurface: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    Logger::warn("Direct3DDevice9Ex::SetRenderTarget: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    Logger::warn("Direct3DDevice9Ex::GetRenderTarget: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    Logger::warn("Direct3DDevice9Ex::SetDepthStencilSurface: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    Logger::warn("Direct3DDevice9Ex::GetDepthStencilSurface: Stub");
    return D3D_OK;
  }

  // The Begin/EndScene functions actually do nothing.
  // Some games don't even call them.

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginScene() {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndScene() {
    // We may want to flush cmds here.

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Clear(
    DWORD Count,
    const D3DRECT* pRects,
    DWORD Flags,
    D3DCOLOR Color,
    float Z,
    DWORD Stencil) {
    Logger::warn("Direct3DDevice9Ex::Clear: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::SetTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::GetTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::MultiplyTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetViewport(const D3DVIEWPORT9* pViewport) {
    Logger::warn("Direct3DDevice9Ex::SetViewport: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetViewport(D3DVIEWPORT9* pViewport) {
    Logger::warn("Direct3DDevice9Ex::GetViewport: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaterial(const D3DMATERIAL9* pMaterial) {
    Logger::warn("Direct3DDevice9Ex::SetMaterial: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaterial(D3DMATERIAL9* pMaterial) {
    Logger::warn("Direct3DDevice9Ex::GetMaterial: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetLight(DWORD Index, const D3DLIGHT9* pLight) {
    Logger::warn("Direct3DDevice9Ex::SetLight: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    Logger::warn("Direct3DDevice9Ex::GetLight: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::LightEnable(DWORD Index, BOOL Enable) {
    Logger::warn("Direct3DDevice9Ex::LightEnable: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLightEnable(DWORD Index, BOOL* pEnable) {
    Logger::warn("Direct3DDevice9Ex::GetLightEnable: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipPlane(DWORD Index, const float* pPlane) {
    Logger::warn("Direct3DDevice9Ex::SetClipPlane: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipPlane(DWORD Index, float* pPlane) {
    Logger::warn("Direct3DDevice9Ex::GetClipPlane: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetRenderState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetRenderState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    Logger::warn("Direct3DDevice9Ex::CreateStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginStateBlock() {
    Logger::warn("Direct3DDevice9Ex::BeginStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    Logger::warn("Direct3DDevice9Ex::EndStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("Direct3DDevice9Ex::SetClipStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("Direct3DDevice9Ex::GetClipStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    Logger::warn("Direct3DDevice9Ex::GetTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    Logger::warn("Direct3DDevice9Ex::SetTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTextureStageState(
    DWORD Stage,
    D3DTEXTURESTAGESTATETYPE Type,
    DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTextureStageState(
    DWORD Stage,
    D3DTEXTURESTAGESTATETYPE Type,
    DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetSamplerState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetSamplerState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ValidateDevice(DWORD* pNumPasses) {
    if (pNumPasses != nullptr)
      *pNumPasses = 1;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    Logger::warn("Direct3DDevice9Ex::SetPaletteEntries: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    Logger::warn("Direct3DDevice9Ex::GetPaletteEntries: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCurrentTexturePalette(UINT PaletteNumber) {
    Logger::warn("Direct3DDevice9Ex::SetCurrentTexturePalette: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCurrentTexturePalette(UINT *PaletteNumber) {
    Logger::warn("Direct3DDevice9Ex::GetCurrentTexturePalette: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetScissorRect(const RECT* pRect) {
    Logger::warn("Direct3DDevice9Ex::SetScissorRect: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetScissorRect(RECT* pRect) {
    Logger::warn("Direct3DDevice9Ex::GetScissorRect: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSoftwareVertexProcessing(BOOL bSoftware) {
    Logger::warn("Direct3DDevice9Ex::SetSoftwareVertexProcessing: Stub");
    return D3D_OK;
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::GetSoftwareVertexProcessing() {
    Logger::warn("Direct3DDevice9Ex::GetSoftwareVertexProcessing: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetNPatchMode(float nSegments) {
    Logger::warn("Direct3DDevice9Ex::SetNPatchMode: Stub");
    return D3D_OK;
  }

  float   STDMETHODCALLTYPE Direct3DDevice9Ex::GetNPatchMode() {
    Logger::warn("Direct3DDevice9Ex::GetNPatchMode: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT StartVertex,
    UINT PrimitiveCount) {
    Logger::warn("Direct3DDevice9Ex::DrawPrimitive: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    INT BaseVertexIndex,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT startIndex,
    UINT primCount) {
    Logger::warn("Direct3DDevice9Ex::DrawIndexedPrimitive: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT PrimitiveCount,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride) {
    Logger::warn("Direct3DDevice9Ex::DrawPrimitiveUP: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT PrimitiveCount,
    const void* pIndexData,
    D3DFORMAT IndexDataFormat,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride) {
    Logger::warn("Direct3DDevice9Ex::DrawIndexedPrimitiveUP: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ProcessVertices(
    UINT SrcStartIndex,
    UINT DestIndex,
    UINT VertexCount,
    IDirect3DVertexBuffer9* pDestBuffer,
    IDirect3DVertexDeclaration9* pVertexDecl,
    DWORD Flags) {
    Logger::warn("Direct3DDevice9Ex::ProcessVertices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
    Logger::warn("Direct3DDevice9Ex::CreateVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    Logger::warn("Direct3DDevice9Ex::SetVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    Logger::warn("Direct3DDevice9Ex::GetVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetFVF(DWORD FVF) {
    Logger::warn("Direct3DDevice9Ex::SetFVF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFVF(DWORD* pFVF) {
    Logger::warn("Direct3DDevice9Ex::GetFVF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::CreateVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShader(IDirect3DVertexShader9* pShader) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantF(
    UINT StartRegister,
    float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantB(
    UINT StartRegister,
    const BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantB(
    UINT StartRegister,
    BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData,
    UINT OffsetInBytes,
    UINT Stride) {
    Logger::warn("Direct3DDevice9Ex::SetStreamSource: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9** ppStreamData,
    UINT* pOffsetInBytes,
    UINT* pStride) {
    Logger::warn("Direct3DDevice9Ex::GetStreamSource: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    Logger::warn("Direct3DDevice9Ex::SetStreamSourceFreq: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    Logger::warn("Direct3DDevice9Ex::GetStreamSourceFreq: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    Logger::warn("Direct3DDevice9Ex::SetIndices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    Logger::warn("Direct3DDevice9Ex::GetIndices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::CreatePixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShader(IDirect3DPixelShader9* pShader) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantF(
    UINT StartRegister,
    float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantB(
    UINT StartRegister,
    const BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantB(
    UINT StartRegister,
    BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawRectPatch(
    UINT Handle,
    const float* pNumSegs,
    const D3DRECTPATCH_INFO* pRectPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawRectPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawTriPatch(
    UINT Handle,
    const float* pNumSegs,
    const D3DTRIPATCH_INFO* pTriPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawTriPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DeletePatch(UINT Handle) {
    Logger::warn("Direct3DDevice9Ex::DeletePatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    Logger::warn("Direct3DDevice9Ex::CreateQuery: Stub");
    return D3D_OK;
  }

  // Ex Methods

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetConvolutionMonoKernel(
    UINT width,
    UINT height,
    float* rows,
    float* columns) {
    Logger::warn("Direct3DDevice9Ex::SetConvolutionMonoKernel: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ComposeRects(
    IDirect3DSurface9* pSrc,
    IDirect3DSurface9* pDst,
    IDirect3DVertexBuffer9* pSrcRectDescs,
    UINT NumRects,
    IDirect3DVertexBuffer9* pDstRectDescs,
    D3DCOMPOSERECTSOP Operation,
    int Xoffset,
    int Yoffset) {
    Logger::warn("Direct3DDevice9Ex::ComposeRects: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetGPUThreadPriority(INT* pPriority) {
    Logger::warn("Direct3DDevice9Ex::GetGPUThreadPriority: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetGPUThreadPriority(INT Priority) {
    Logger::warn("Direct3DDevice9Ex::SetGPUThreadPriority: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::WaitForVBlank(UINT iSwapChain) {
    Logger::warn("Direct3DDevice9Ex::WaitForVBlank: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) {
    Logger::warn("Direct3DDevice9Ex::CheckResourceResidency: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaximumFrameLatency(UINT MaxLatency) {
    Logger::warn("Direct3DDevice9Ex::SetMaximumFrameLatency: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaximumFrameLatency(UINT* pMaxLatency) {
    Logger::warn("Direct3DDevice9Ex::GetMaximumFrameLatency: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckDeviceState(HWND hDestinationWindow) {
    Logger::warn("Direct3DDevice9Ex::CheckDeviceState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::PresentEx(
    const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
    DWORD dwFlags) {
    Logger::warn("Direct3DDevice9Ex::PresentEx: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTargetEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Lockable,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    Logger::warn("Direct3DDevice9Ex::CreateRenderTargetEx: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    Logger::warn("Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurfaceEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    Logger::warn("Direct3DDevice9Ex::CreateDepthStencilSurfaceEx: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode) {
    Logger::warn("Direct3DDevice9Ex::ResetEx: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayModeEx(
    UINT iSwapChain,
    D3DDISPLAYMODEEX* pMode,
    D3DDISPLAYROTATION* pRotation) {
    Logger::warn("Direct3DDevice9Ex::GetDisplayModeEx: Stub");
    return D3D_OK;
  }

  bool Direct3DDevice9Ex::IsExtended() {
    return m_extended;
  }

  HWND Direct3DDevice9Ex::GetWindow() {
    return m_window;
  }

  DxvkDeviceFeatures Direct3DDevice9Ex::GetDeviceFeatures(const Rc<DxvkAdapter>& adapter) {
    DxvkDeviceFeatures supported = adapter->features();
    DxvkDeviceFeatures enabled = {};

    enabled.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    enabled.core.pNext = nullptr;

    enabled.extMemoryPriority.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
    enabled.extMemoryPriority.pNext = nullptr;
    enabled.extMemoryPriority.memoryPriority = supported.extMemoryPriority.memoryPriority;

    enabled.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
    enabled.extTransformFeedback.pNext = nullptr;

    enabled.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
    enabled.extVertexAttributeDivisor.pNext = nullptr;

    enabled.core.features.depthClamp = VK_TRUE;
    enabled.core.features.depthBiasClamp = VK_TRUE;
    enabled.core.features.fillModeNonSolid = VK_TRUE;
    enabled.core.features.pipelineStatisticsQuery = supported.core.features.pipelineStatisticsQuery;
    enabled.core.features.sampleRateShading = VK_TRUE;
    enabled.core.features.samplerAnisotropy = VK_TRUE;
    enabled.core.features.shaderClipDistance = VK_TRUE;
    enabled.core.features.shaderCullDistance = VK_TRUE;
    enabled.core.features.robustBufferAccess = VK_TRUE;

    enabled.core.features.occlusionQueryPrecise = VK_TRUE;

    enabled.core.features.multiViewport = VK_TRUE;
    enabled.core.features.independentBlend = VK_TRUE;

    // These two are likely not necessary... TODO: investigate this further.
    enabled.core.features.fullDrawIndexUint32 = VK_TRUE;
    enabled.core.features.textureCompressionBC = VK_TRUE;

    if (supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor
      && supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor) {
      enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = VK_TRUE;
      enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = VK_TRUE;
    }

    return enabled;
  }

  Direct3DSwapChain9Ex* Direct3DDevice9Ex::getInternalSwapchain(UINT index) {
    if (index >= m_swapchains.size())
      return nullptr;

    return static_cast<Direct3DSwapChain9Ex*>(m_swapchains[index].ptr());
  }

}
