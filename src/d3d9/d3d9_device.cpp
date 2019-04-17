#include "d3d9_device.h"

#include "d3d9_swapchain.h"
#include "d3d9_caps.h"
#include "d3d9_util.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"
#include "d3d9_vertex_declaration.h"
#include "d3d9_shader.h"
#include "d3d9_query.h"
#include "d3d9_stateblock.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_instance.h"

#include "../util/util_bit.h"
#include "../util/util_math.h"

#include "d3d9_initializer.h"

#include <algorithm>
#include <cfloat>
#ifdef MSC_VER
#pragma fenv_access (on)
#endif

namespace dxvk {

  Direct3DDevice9Ex::Direct3DDevice9Ex(
          bool              extended,
          IDirect3D9Ex*     parent,
          UINT              adapter,
          Rc<DxvkAdapter>   dxvkAdapter,
          Rc<DxvkDevice>    dxvkDevice,
          D3DDEVTYPE        deviceType,
          HWND              window,
          DWORD             flags,
          D3DDISPLAYMODEEX* displayMode)
    : m_dxvkAdapter    ( dxvkAdapter )
    , m_dxvkDevice     ( dxvkDevice )
    , m_csThread       ( dxvkDevice->createContext() )
    , m_frameLatency   ( DefaultFrameLatency )
    , m_csChunk        ( AllocCsChunk() )
    , m_parent         ( parent )
    , m_adapter        ( adapter )
    , m_deviceType     ( deviceType )
    , m_window         ( window )
    , m_behaviourFlags ( flags )
    , m_multithread    ( flags & D3DCREATE_MULTITHREADED )
    , m_shaderModules  ( new D3D9ShaderModuleSet )
    , m_d3d9Formats    ( dxvkAdapter )
    , m_d3d9Options    ( dxvkAdapter->instance()->config() )
    , m_dxsoOptions    ( m_dxvkDevice, m_d3d9Options ) {
    if (extended)
      m_flags.set(D3D9DeviceFlag::ExtendedDevice);

    m_initializer = new D3D9Initializer(m_dxvkDevice);
    m_frameLatencyCap = m_d3d9Options.maxFrameLatency;

    for (uint32_t i = 0; i < m_frameEvents.size(); i++)
      m_frameEvents[i] = new DxvkEvent();

    EmitCs([
      cDevice = m_dxvkDevice
    ] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());

      DxvkMultisampleState msState;
      msState.sampleMask            = 0xffffffff;
      msState.enableAlphaToCoverage = VK_FALSE;
      ctx->setMultisampleState(msState);

      DxvkLogicOpState loState;
      loState.enableLogicOp = VK_FALSE;
      loState.logicOp       = VK_LOGIC_OP_CLEAR;
      ctx->setLogicOpState(loState);
      
      DxvkExtraState xsState;
      xsState.alphaCompareOp = VK_COMPARE_OP_ALWAYS;
      ctx->setExtraState(xsState);
    });

    CreateConstantBuffers();

    if (!(m_behaviourFlags & D3DCREATE_FPU_PRESERVE))
      SetupFPU();
  }

  Direct3DDevice9Ex::~Direct3DDevice9Ex() {
    Flush();
    SynchronizeCsThread();

    delete m_initializer;

    m_dxvkDevice->waitForIdle(); // Sync Device
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    bool extended = m_flags.test(D3D9DeviceFlag::ExtendedDevice)
                 && riid == __uuidof(IDirect3DDevice9Ex);

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DDevice9)
     || extended) {
      *ppvObject = ref(this);
      return S_OK;
    }

    // We want to ignore this if the extended device is queried and we weren't made extended.
    if (riid == __uuidof(IDirect3DDevice9Ex))
      return E_NOINTERFACE;

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
    return caps::getDeviceCaps(m_d3d9Options, m_adapter, m_deviceType, pCaps);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetDisplayMode(pMode);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    if (pParameters == nullptr)
      return D3DERR_INVALIDCALL;

    pParameters->AdapterOrdinal = m_adapter;
    pParameters->BehaviorFlags  = m_behaviourFlags;
    pParameters->DeviceType     = m_deviceType;
    pParameters->hFocusWindow   = m_window;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorProperties(
          UINT               XHotSpot,
          UINT               YHotSpot,
          IDirect3DSurface9* pCursorBitmap) {
    Logger::warn("Direct3DDevice9Ex::SetCursorProperties: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorPosition(int X, int Y, DWORD Flags) {
    auto lock = LockDevice();

    m_cursor.updateCursor(X, Y, Flags & D3DCURSOR_IMMEDIATE_UPDATE);
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::ShowCursor(BOOL bShow) {
    // Ok so if they call FALSE here it means they want to use the regular windows cursor.
    // if they call TRUE here it means they want to use some weird bitmap cursor that I currently dont care about.
    // Therefore we always want to show the regular cursor no matter what!
    ::ShowCursor(true);

    return TRUE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateAdditionalSwapChain(
          D3DPRESENT_PARAMETERS* pPresentationParameters,
          IDirect3DSwapChain9**  ppSwapChain) {
    auto lock = LockDevice();

    InitReturnPtr(ppSwapChain);

    if (ppSwapChain == nullptr || pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    auto* swapchain = new Direct3DSwapChain9Ex(this, pPresentationParameters);
    swapchain->AddRefPrivate();
    m_swapchains.push_back(swapchain);
    *ppSwapChain = ref(swapchain);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    auto lock = LockDevice();

    InitReturnPtr(pSwapChain);

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr || pSwapChain == nullptr)
      return D3DERR_INVALIDCALL;

    *pSwapChain = static_cast<IDirect3DSwapChain9*>(ref(swapchain));

    return D3D_OK;
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetNumberOfSwapChains() {
    auto lock = LockDevice();

    return UINT(m_swapchains.size());
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    return ResetEx(pPresentationParameters, nullptr);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion) {
    return PresentEx(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetBackBuffer(
    UINT iSwapChain,
    UINT iBackBuffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppBackBuffer) {
    auto lock = LockDevice();

    InitReturnPtr(ppBackBuffer);

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetRasterStatus(pRasterStatus);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDialogBoxMode(BOOL bEnableDialogs) {
    Logger::warn("Direct3DDevice9Ex::SetDialogBoxMode: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetGammaRamp(
    UINT iSwapChain,
    DWORD Flags,
    const D3DGAMMARAMP* pRamp) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return;

    swapchain->SetGammaRamp(Flags, pRamp);
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return;

    swapchain->GetGammaRamp(pRamp);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateTexture(UINT Width,
          UINT                Height,
          UINT                Levels,
          DWORD               Usage,
          D3DFORMAT           Format,
          D3DPOOL             Pool,
          IDirect3DTexture9** ppTexture,
          HANDLE*             pSharedHandle) {
    auto lock = LockDevice();

    InitReturnPtr(ppTexture);
    InitReturnPtr(pSharedHandle);

    if (ppTexture == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_TEXTURE;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = fixupFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable           = TRUE;
    desc.Offscreen          = FALSE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DTexture9> texture = new Direct3DTexture9(this, &desc);
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppTexture = texture.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVolumeTexture(
          UINT                      Width,
          UINT                      Height,
          UINT                      Depth,
          UINT                      Levels,
          DWORD                     Usage,
          D3DFORMAT                 Format,
          D3DPOOL                   Pool,
          IDirect3DVolumeTexture9** ppVolumeTexture,
          HANDLE*                   pSharedHandle) {
    auto lock = LockDevice();

    InitReturnPtr(ppVolumeTexture);
    InitReturnPtr(pSharedHandle);

    if (ppVolumeTexture == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_VOLUMETEXTURE;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = Depth;
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = fixupFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable           = TRUE;
    desc.Offscreen          = FALSE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DVolumeTexture9> texture = new Direct3DVolumeTexture9(this, &desc);
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppVolumeTexture = texture.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateCubeTexture(
          UINT                    EdgeLength,
          UINT                    Levels,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DCubeTexture9** ppCubeTexture,
          HANDLE*                 pSharedHandle) {
    auto lock = LockDevice();

    InitReturnPtr(ppCubeTexture);
    InitReturnPtr(pSharedHandle);

    if (ppCubeTexture == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_CUBETEXTURE;
    desc.Width              = EdgeLength;
    desc.Height             = EdgeLength;
    desc.Depth              = 1;
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = fixupFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable           = TRUE;
    desc.Offscreen          = FALSE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DCubeTexture9> texture = new Direct3DCubeTexture9(this, &desc);
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppCubeTexture = texture.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexBuffer(
          UINT                     Length,
          DWORD                    Usage,
          DWORD                    FVF,
          D3DPOOL                  Pool,
          IDirect3DVertexBuffer9** ppVertexBuffer,
          HANDLE*                  pSharedHandle) {
    auto lock = LockDevice();

    if (ppVertexBuffer == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = D3D9Format::VERTEXDATA;
    desc.FVF    = FVF;
    desc.Pool   = Pool;
    desc.Size   = Length;
    desc.Type   = D3DRTYPE_VERTEXBUFFER;
    desc.Usage  = Usage;

    try {
      const Com<Direct3DVertexBuffer9> buffer = new Direct3DVertexBuffer9(this, &desc);
      m_initializer->InitBuffer(buffer->GetCommonBuffer().ptr());
      *ppVertexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateIndexBuffer(
          UINT                    Length,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DIndexBuffer9** ppIndexBuffer,
          HANDLE*                 pSharedHandle) {
    auto lock = LockDevice();

    if (ppIndexBuffer == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = fixupFormat(Format);
    desc.Pool   = Pool;
    desc.Size   = Length;
    desc.Type   = D3DRTYPE_INDEXBUFFER;
    desc.Usage  = Usage;

    try {
      const Com<Direct3DIndexBuffer9> buffer = new Direct3DIndexBuffer9(this, &desc);
      m_initializer->InitBuffer(buffer->GetCommonBuffer().ptr());
      *ppIndexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTarget(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          DWORD               MultisampleQuality,
          BOOL                Lockable,
          IDirect3DSurface9** ppSurface,
          HANDLE*             pSharedHandle) {
    return CreateRenderTargetEx(
      Width,
      Height,
      Format,
      MultiSample,
      MultisampleQuality,
      Lockable,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurface(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          DWORD               MultisampleQuality,
          BOOL                Discard,
          IDirect3DSurface9** ppSurface,
          HANDLE*             pSharedHandle) {
    return CreateDepthStencilSurfaceEx(
      Width,
      Height,
      Format,
      MultiSample,
      MultisampleQuality,
      Discard,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateSurface(
          IDirect3DSurface9* pSourceSurface,
    const RECT*              pSourceRect,
          IDirect3DSurface9* pDestinationSurface,
    const POINT*             pDestPoint) {
    auto lock = LockDevice();

    FlushImplicit(FALSE);

    Direct3DSurface9* src = static_cast<Direct3DSurface9*>(pSourceSurface);
    Direct3DSurface9* dst = static_cast<Direct3DSurface9*>(pDestinationSurface);

    if (src == nullptr || dst == nullptr)
      return D3DERR_INVALIDCALL;

    Direct3DCommonTexture9* srcTextureInfo = src->GetCommonTexture();
    Direct3DCommonTexture9* dstTextureInfo = dst->GetCommonTexture();

    Rc<DxvkImage> srcImage = src->GetCommonTexture()->GetImage();
    Rc<DxvkImage> dstImage = dst->GetCommonTexture()->GetImage();

    const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
    const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);

    const VkImageSubresource dstSubresource = dstTextureInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, dst->GetSubresource());
    const VkImageSubresource srcSubresource = srcTextureInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());

    // Copies are only supported on size-compatible formats
    if (dstFormatInfo->elementSize != srcFormatInfo->elementSize) {
      Logger::err(str::format(
        "D3D9: UpdateSurface: Incompatible texel size"
        "\n  Dst texel size: ", dstFormatInfo->elementSize,
        "\n  Src texel size: ", srcFormatInfo->elementSize));
      return D3D_OK;
    }

    // Copies are only supported if the sample count matches
    if (dstImage->info().sampleCount != srcImage->info().sampleCount) {
      Logger::err(str::format(
        "D3D9: UpdateSurface: Incompatible sample count",
        "\n  Dst sample count: ", dstImage->info().sampleCount,
        "\n  Src sample count: ", srcImage->info().sampleCount));
      return D3D_OK;
    }

    VkOffset3D srcOffset = { 0,0,0 };
    VkOffset3D dstOffset = { 0,0,0 };

    VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);
    VkExtent3D dstExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);

    VkExtent3D regExtent = srcExtent;

    if (pDestPoint != nullptr) {
      dstOffset = { align(pDestPoint->x, dstFormatInfo->blockSize.width),
                    align(pDestPoint->y, dstFormatInfo->blockSize.height),
                    0 };
    }

    if (pSourceRect != nullptr) {
      srcOffset = { 
        align(pSourceRect->left, srcFormatInfo->blockSize.width),
        align(pSourceRect->top,  srcFormatInfo->blockSize.height),
        0 };

      regExtent = { 
        align(uint32_t(pSourceRect->right  - pSourceRect->left), srcFormatInfo->blockSize.width),
        align(uint32_t(pSourceRect->bottom - pSourceRect->top),  srcFormatInfo->blockSize.height),
        0 };
    }

    VkImageSubresourceLayers dstLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };
      
    VkImageSubresourceLayers srcLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    VkExtent3D regBlockCount = util::computeBlockCount(regExtent, srcFormatInfo->blockSize);
    VkExtent3D dstBlockCount = util::computeMaxBlockCount(dstOffset, dstExtent, dstFormatInfo->blockSize);
    VkExtent3D srcBlockCount = util::computeMaxBlockCount(srcOffset, srcExtent, srcFormatInfo->blockSize);

    regBlockCount = util::minExtent3D(regBlockCount, dstBlockCount);
    regBlockCount = util::minExtent3D(regBlockCount, srcBlockCount);

    regExtent = util::minExtent3D(regExtent, util::computeBlockExtent(regBlockCount, srcFormatInfo->blockSize));

    EmitCs([
      cDstImage  = dstImage,
      cSrcImage  = srcImage,
      cDstLayers = dstLayers,
      cSrcLayers = srcLayers,
      cDstOffset = dstOffset,
      cSrcOffset = srcOffset,
      cExtent    = regExtent
    ] (DxvkContext* ctx) {
      ctx->copyImage(
        cDstImage, cDstLayers, cDstOffset,
        cSrcImage, cSrcLayers, cSrcOffset,
        cExtent);
    });

    dst->GetCommonTexture()->GenerateMipSubLevels();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateTexture(
          IDirect3DBaseTexture9* pSourceTexture,
          IDirect3DBaseTexture9* pDestinationTexture) {
    auto lock = LockDevice();

    FlushImplicit(FALSE);

    if (!pDestinationTexture || !pSourceTexture)
      return D3DERR_INVALIDCALL;

    if (pDestinationTexture == pSourceTexture)
      return D3D_OK;

    const Rc<DxvkImage> dstImage = GetCommonTexture(pDestinationTexture)->GetImage();
    const Rc<DxvkImage> srcImage = GetCommonTexture(pSourceTexture)->GetImage();

    const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
    const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);

    // Copies are only supported on size-compatible formats
    if (dstFormatInfo->elementSize != srcFormatInfo->elementSize) {
      Logger::err(str::format(
        "D3D9: UpdateTexture: Incompatible texel size"
        "\n  Dst texel size: ", dstFormatInfo->elementSize,
        "\n  Src texel size: ", srcFormatInfo->elementSize));
      return D3DERR_INVALIDCALL;
    }

    // Layer count, mip level count, and sample count must match
    if ( srcImage->info().numLayers   != dstImage->info().numLayers
      || srcImage->info().mipLevels   != dstImage->info().mipLevels
      || srcImage->info().sampleCount != dstImage->info().sampleCount) {
      Logger::err(str::format(
        "D3D9: UpdateTexture: Incompatible images"
        "\n  Dst: (", dstImage->info().numLayers,
                  ",", dstImage->info().mipLevels,
                  ",", dstImage->info().sampleCount, ")",
        "\n  Src: (", srcImage->info().numLayers,
                  ",", srcImage->info().mipLevels,
                  ",", srcImage->info().sampleCount, ")"));
      return D3DERR_INVALIDCALL;
    }
      
    for (uint32_t i = 0; i < srcImage->info().mipLevels; i++) {
      VkImageSubresourceLayers dstLayers = { dstFormatInfo->aspectMask, i, 0, dstImage->info().numLayers };
      VkImageSubresourceLayers srcLayers = { srcFormatInfo->aspectMask, i, 0, srcImage->info().numLayers };
        
      VkExtent3D extent = srcImage->mipLevelExtent(i);
        
      EmitCs([
        cDstImage  = dstImage,
        cSrcImage  = srcImage,
        cDstLayers = dstLayers,
        cSrcLayers = srcLayers,
        cExtent    = extent
      ] (DxvkContext* ctx) {
        ctx->copyImage(
          cDstImage, cDstLayers, VkOffset3D { 0, 0, 0 },
          cSrcImage, cSrcLayers, VkOffset3D { 0, 0, 0 },
          cExtent);
      });
    }

    pDestinationTexture->GenerateMipSubLevels();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTargetData(
          IDirect3DSurface9* pRenderTarget,
          IDirect3DSurface9* pDestSurface) {
    auto lock = LockDevice();

    return UpdateSurface(pRenderTarget, nullptr, pDestSurface, nullptr);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetFrontBufferData(pDestSurface);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::StretchRect(
          IDirect3DSurface9*   pSourceSurface,
    const RECT*                pSourceRect,
          IDirect3DSurface9*   pDestSurface,
    const RECT*                pDestRect,
          D3DTEXTUREFILTERTYPE Filter) {
    auto lock = LockDevice();

    Direct3DSurface9* dst = static_cast<Direct3DSurface9*>(pDestSurface);
    Direct3DSurface9* src = static_cast<Direct3DSurface9*>(pSourceSurface);

    if (src == nullptr || dst == nullptr)
      return D3DERR_INVALIDCALL;

    bool fastPath     = true;
    bool needsResolve = false;

    Direct3DCommonTexture9* dstTextureInfo = dst->GetCommonTexture();
    Direct3DCommonTexture9* srcTextureInfo = src->GetCommonTexture();

    Rc<DxvkImage> dstImage = dst->GetCommonTexture()->GetImage();
    Rc<DxvkImage> srcImage = src->GetCommonTexture()->GetImage();

    const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
    const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);

    const VkImageSubresource dstSubresource = dstTextureInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, dst->GetSubresource());
    const VkImageSubresource srcSubresource = srcTextureInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());

    VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);
    VkExtent3D dstExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);

    // Copies are only supported on size-compatible formats
    fastPath &= dstFormatInfo->elementSize == srcFormatInfo->elementSize;

    // Copies are only supported if the sample count matches,
    // otherwise we need to resolve.
    needsResolve = dstImage->info().sampleCount != srcImage->info().sampleCount;

    // Copies would only work if the extents match. (ie. no stretching)
    bool niceRect = true;

    if (pSourceRect != nullptr && pDestRect != nullptr) {
      niceRect       &=  (pSourceRect->right  - pSourceRect->left) == (pDestRect->right  - pDestRect->left);
      niceRect       &=  (pSourceRect->bottom - pSourceRect->top)  == (pDestRect->bottom - pDestRect->top);
    }

    // Copies would only work if we are block aligned.
    if (pSourceRect != nullptr) {
      niceRect       &=  (pSourceRect->left   % srcFormatInfo->blockSize.width  == 0);
      niceRect       &=  (pSourceRect->right  % srcFormatInfo->blockSize.width  == 0);
      niceRect       &=  (pSourceRect->top    % srcFormatInfo->blockSize.height == 0);
      niceRect       &=  (pSourceRect->bottom % srcFormatInfo->blockSize.height == 0);
    }

    if (pDestRect != nullptr) {
      niceRect       &=  (pDestRect->left     % dstFormatInfo->blockSize.width  == 0);
      niceRect       &=  (pDestRect->top      % dstFormatInfo->blockSize.height == 0);
    }

    fastPath         &= niceRect;

    VkImageSubresourceLayers dstSubresourceLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };

    VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    VkImageBlit blitInfo;
    blitInfo.dstSubresource = dstSubresourceLayers;
    blitInfo.srcSubresource = srcSubresourceLayers;

    blitInfo.dstOffsets[0] = pDestRect != nullptr
      ? VkOffset3D{ int32_t(pDestRect->left), int32_t(pDestRect->top), 0 }
      : VkOffset3D{ 0,                        0,                       0 };

    blitInfo.dstOffsets[1] = pDestRect != nullptr
      ? VkOffset3D{ int32_t(pDestRect->right), int32_t(pDestRect->bottom), 1 }
      : VkOffset3D{ int32_t(dstExtent.width),  int32_t(dstExtent.height),  1 };

    blitInfo.srcOffsets[0] = pSourceRect != nullptr
      ? VkOffset3D{ int32_t(pSourceRect->left), int32_t(pSourceRect->top), 0 }
      : VkOffset3D{ 0,                          0,                         0 };

    blitInfo.srcOffsets[1] = pSourceRect != nullptr
      ? VkOffset3D{ int32_t(pSourceRect->right), int32_t(pSourceRect->bottom), 1 }
      : VkOffset3D{ int32_t(srcExtent.width),    int32_t(srcExtent.height),    1 };

    if (fastPath && !needsResolve) {
      VkExtent3D regExtent =
      { uint32_t(blitInfo.srcOffsets[1].x - blitInfo.srcOffsets[0].x),
        uint32_t(blitInfo.srcOffsets[1].y - blitInfo.srcOffsets[0].y),
        uint32_t(blitInfo.srcOffsets[1].z - blitInfo.srcOffsets[0].z) };

      EmitCs([
        cDstImage  = dstImage,
        cSrcImage  = srcImage,
        cDstLayers = blitInfo.dstSubresource,
        cSrcLayers = blitInfo.srcSubresource,
        cDstOffset = blitInfo.dstOffsets[0],
        cSrcOffset = blitInfo.srcOffsets[0],
        cExtent    = regExtent
      ] (DxvkContext* ctx) {
        ctx->copyImage(
          cDstImage, cDstLayers, cDstOffset,
          cSrcImage, cSrcLayers, cSrcOffset,
          cExtent);
      });
    }
    /*else if (fastPath && needsResolve) {

    }*/
    else {
      EmitCs([
        cDstImage = dstImage,
        cSrcImage = srcImage,
        cBlitInfo = blitInfo,
        cFilter = DecodeFilter(Filter)
      ] (DxvkContext* ctx) {
        ctx->blitImage(
          cDstImage,
          cSrcImage,
          cBlitInfo,
          cFilter);
      });
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ColorFill(
          IDirect3DSurface9* pSurface,
    const RECT*              pRect,
          D3DCOLOR           Color) {
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
    return CreateOffscreenPlainSurfaceEx(
      Width,
      Height,
      Format,
      Pool,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderTarget(
          DWORD              RenderTargetIndex,
          IDirect3DSurface9* pRenderTarget) {
    auto lock = LockDevice();

    if (RenderTargetIndex >= caps::MaxSimultaneousRenderTargets
     || (pRenderTarget == nullptr && RenderTargetIndex == 0) )
      return D3DERR_INVALIDCALL;

    Direct3DSurface9* rt = static_cast<Direct3DSurface9*>(pRenderTarget);

    if (m_state.renderTargets[RenderTargetIndex] == rt)
      return D3D_OK;

    FlushImplicit(FALSE);

    changePrivate(m_state.renderTargets[RenderTargetIndex], rt);
    
    BindFramebuffer();

    if (RenderTargetIndex == 0) {
      const auto* desc = m_state.renderTargets[0]->GetCommonTexture()->Desc();

      D3DVIEWPORT9 viewport;
      viewport.X = 0;
      viewport.Y = 0;
      viewport.Width = desc->Width;
      viewport.Height = desc->Height;
      viewport.MinZ = 0.0f;
      viewport.MaxZ = 1.0f;
      m_state.viewport = viewport;

      RECT scissorRect;
      scissorRect.left = 0;
      scissorRect.top = 0;
      scissorRect.right = desc->Width;
      scissorRect.bottom = desc->Height;
      m_state.scissorRect = scissorRect;

      BindViewportAndScissor();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTarget(
          DWORD               RenderTargetIndex,
          IDirect3DSurface9** ppRenderTarget) {
    auto lock = LockDevice();

    InitReturnPtr(ppRenderTarget);

    if (ppRenderTarget == nullptr || RenderTargetIndex > caps::MaxSimultaneousRenderTargets)
      return D3DERR_INVALIDCALL;

    if (m_state.renderTargets[RenderTargetIndex] == nullptr)
      return D3DERR_NOTFOUND;

    *ppRenderTarget = ref(m_state.renderTargets[RenderTargetIndex]);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    auto lock = LockDevice();

    Direct3DSurface9* ds = static_cast<Direct3DSurface9*>(pNewZStencil);

    if (m_state.depthStencil == ds)
      return D3D_OK;

    FlushImplicit(FALSE);

    changePrivate(m_state.depthStencil, ds);

    BindFramebuffer();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    auto lock = LockDevice();

    InitReturnPtr(ppZStencilSurface);

    if (ppZStencilSurface == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state.depthStencil == nullptr)
      return D3DERR_NOTFOUND;

    *ppZStencilSurface = ref(m_state.depthStencil);

    return D3D_OK;
  }

  // The Begin/EndScene functions actually do nothing.
  // Some games don't even call them.

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginScene() {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndScene() {
    FlushImplicit(true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Clear(
          DWORD    Count,
    const D3DRECT* pRects,
          DWORD    Flags,
          D3DCOLOR Color,
          float    Z,
          DWORD    Stencil) {
    auto lock = LockDevice();

    if (Flags & D3DCLEAR_STENCIL || Flags & D3DCLEAR_ZBUFFER) {
      auto dsv = m_state.depthStencil != nullptr ? m_state.depthStencil->GetDepthStencilView() : nullptr;

      if (dsv != nullptr) {
        VkImageAspectFlags aspectMask = 0;

        if (Flags & D3DCLEAR_ZBUFFER)
          aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (Flags & D3DCLEAR_STENCIL)
          aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        aspectMask &= imageFormatInfo(dsv->info().format)->aspectMask;

        if (aspectMask != 0) {
          VkClearValue clearValue;
          clearValue.depthStencil.depth = Z;
          clearValue.depthStencil.stencil = Stencil;

          EmitCs([
            cClearValue = clearValue,
            cAspectMask = aspectMask,
            cImageView  = dsv
          ] (DxvkContext * ctx) {
              ctx->clearRenderTarget(
                cImageView,
                cAspectMask,
                cClearValue);
            });
        }
      }
    }

    if (Flags & D3DCLEAR_TARGET) {
      VkClearValue clearValue;
      DecodeD3DCOLOR(Color, clearValue.color.float32);

      for (auto rt : m_state.renderTargets) {
        auto rtv = rt != nullptr ? rt->GetRenderTargetView(false) : nullptr; // TODO: handle srgb-ness

        if (rtv != nullptr) {
          EmitCs([
            cClearValue = clearValue,
            cImageView  = rtv
          ] (DxvkContext* ctx) {
            ctx->clearRenderTarget(
              cImageView,
              VK_IMAGE_ASPECT_COLOR_BIT,
              cClearValue);
          });
        }
      }
    }

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
    auto lock = LockDevice();

    D3DVIEWPORT9 viewport;
    if (pViewport == nullptr) {
      auto rtv = m_state.renderTargets[0]->GetRenderTargetView(false);

      viewport.X      = 0;
      viewport.Y      = 0;
      viewport.Width  = rtv->image()->info().extent.width;
      viewport.Height = rtv->image()->info().extent.height;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;
    }
    else
      viewport = *pViewport;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetViewport(&viewport);

    m_state.viewport = viewport;

    m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetViewport(D3DVIEWPORT9* pViewport) {
    auto lock = LockDevice();

    if (pViewport == nullptr)
      return D3DERR_INVALIDCALL;

    *pViewport = m_state.viewport;

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
    if (Index >= caps::MaxClipPlanes || !pPlane)
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetClipPlane(Index, pPlane);
    
    bool dirty = false;
    
    for (uint32_t i = 0; i < 4; i++) {
      dirty |= m_state.clipPlanes[Index].coeff[i] != pPlane[i];
      m_state.clipPlanes[Index].coeff[i] = pPlane[i];
    }
    
    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);
    
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipPlane(DWORD Index, float* pPlane) {
    if (Index >= caps::MaxClipPlanes || !pPlane)
      return D3DERR_INVALIDCALL;
    
    for (uint32_t i = 0; i < 4; i++)
      pPlane[i] = m_state.clipPlanes[Index].coeff[i];
    
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    auto lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetRenderState(State, Value);

    auto& states = m_state.renderStates;

    bool changed = states[State] != Value;

    if (likely(changed)) {
      states[State] = Value;

      switch (State) {
        case D3DRS_SEPARATEALPHABLENDENABLE:
        case D3DRS_ALPHABLENDENABLE:
        case D3DRS_BLENDOP:
        case D3DRS_BLENDOPALPHA:
        case D3DRS_DESTBLEND:
        case D3DRS_DESTBLENDALPHA:
        case D3DRS_COLORWRITEENABLE:
        case D3DRS_COLORWRITEENABLE1:
        case D3DRS_COLORWRITEENABLE2:
        case D3DRS_COLORWRITEENABLE3:
        case D3DRS_SRCBLEND:
        case D3DRS_SRCBLENDALPHA:
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        
        case D3DRS_ALPHATESTENABLE:
        case D3DRS_ALPHAFUNC:
          m_flags.set(D3D9DeviceFlag::DirtyExtraState);
          break;

        case D3DRS_BLENDFACTOR:
          BindBlendFactor();
          break;

        case D3DRS_ZENABLE:
        case D3DRS_ZFUNC:
        case D3DRS_TWOSIDEDSTENCILMODE:
        case D3DRS_ZWRITEENABLE:
        case D3DRS_STENCILENABLE:
        case D3DRS_STENCILFAIL:
        case D3DRS_STENCILZFAIL:
        case D3DRS_STENCILPASS:
        case D3DRS_STENCILFUNC:
        case D3DRS_CCW_STENCILFAIL:
        case D3DRS_CCW_STENCILZFAIL:
        case D3DRS_CCW_STENCILPASS:
        case D3DRS_CCW_STENCILFUNC:
        case D3DRS_STENCILMASK:
        case D3DRS_STENCILWRITEMASK:
          m_flags.set(D3D9DeviceFlag::DirtyDepthStencilState);
          break;

        case D3DRS_STENCILREF:
          BindDepthStencilRefrence();
          break;

        case D3DRS_SCISSORTESTENABLE:
          m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);
          break;

        case D3DRS_SRGBWRITEENABLE:
          BindFramebuffer();
          break;

        case D3DRS_DEPTHBIAS:
        case D3DRS_SLOPESCALEDEPTHBIAS:
        case D3DRS_CULLMODE:
        case D3DRS_FILLMODE:
          m_flags.set(D3D9DeviceFlag::DirtyRasterizerState);
          break;

        case D3DRS_CLIPPLANEENABLE:
          m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);
          break;

        case D3DRS_ALPHAREF:
          m_flags.set(D3D9DeviceFlag::DirtyRenderStateBuffer);
          break;

        default:
          Logger::warn(str::format("Direct3DDevice9Ex::SetRenderState: Unhandled render state ", State));
          break;
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    auto lock = LockDevice();

    if (pValue == nullptr)
      return D3DERR_INVALIDCALL;

    if (State < D3DRS_ZENABLE || State > D3DRS_BLENDOPALPHA)
      *pValue = 0;
    else
      *pValue = m_state.renderStates[State];

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateStateBlock(
          D3DSTATEBLOCKTYPE      Type,
          IDirect3DStateBlock9** ppSB) {
    auto lock = LockDevice();

    InitReturnPtr(ppSB);

    if (ppSB == nullptr)
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9StateBlock> sb = new D3D9StateBlock(this, ConvertStateBlockType(Type));
      *ppSB = sb.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginStateBlock() {
    auto lock = LockDevice();

    if (m_recorder != nullptr)
      return D3DERR_INVALIDCALL;

    m_recorder = new D3D9StateBlock(this, D3D9StateBlockType::None);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    auto lock = LockDevice();

    InitReturnPtr(ppSB);

    if (ppSB == nullptr || m_recorder == nullptr)
      return D3DERR_INVALIDCALL;

    *ppSB = m_recorder.ref();
    m_recorder = nullptr;

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
    auto lock = LockDevice();

    if (ppTexture == nullptr)
      return D3DERR_INVALIDCALL;

    *ppTexture = nullptr;

    if (InvalidSampler(Stage))
      return D3D_OK;

    DWORD stateSampler = RemapSamplerState(Stage);

    *ppTexture = ref(m_state.textures[stateSampler]);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    auto lock = LockDevice();

    if (InvalidSampler(Stage))
      return D3D_OK;

    DWORD stateSampler = RemapSamplerState(Stage);

    return SetStateTexture(stateSampler, pTexture);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD*                   pValue) {
    Logger::warn("Direct3DDevice9Ex::GetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD                    Value) {
    Logger::warn("Direct3DDevice9Ex::SetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSamplerState(
          DWORD               Sampler,
          D3DSAMPLERSTATETYPE Type,
          DWORD*              pValue) {
    auto lock = LockDevice();

    if (pValue == nullptr)
      return D3DERR_INVALIDCALL;

    *pValue = 0;

    if (InvalidSampler(Sampler))
      return D3D_OK;

    Sampler = RemapSamplerState(Sampler);

    *pValue = m_state.samplerStates[Sampler][Type];

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSamplerState(
          DWORD               Sampler,
          D3DSAMPLERSTATETYPE Type,
          DWORD               Value) {
    auto lock = LockDevice();
    if (InvalidSampler(Sampler))
      return D3D_OK;

    uint32_t stateSampler = RemapSamplerState(Sampler);

    return SetStateSamplerState(stateSampler, Type, Value);
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
    auto lock = LockDevice();

    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetScissorRect(pRect);

    m_state.scissorRect = *pRect;

    m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetScissorRect(RECT* pRect) {
    auto lock = LockDevice();

    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    *pRect = m_state.scissorRect;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSoftwareVertexProcessing(BOOL bSoftware) {
    Logger::warn("Direct3DDevice9Ex::SetSoftwareVertexProcessing: Stub");
    return D3D_OK;
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::GetSoftwareVertexProcessing() {
    Logger::warn("Direct3DDevice9Ex::GetSoftwareVertexProcessing: Stub");
    return FALSE;
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
          UINT             StartVertex,
          UINT             PrimitiveCount) {
    auto lock = LockDevice();

    PrepareDraw();

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount);

    EmitCs([
      cDrawInfo    = drawInfo,
      cStartVertex = StartVertex
    ](DxvkContext* ctx) {
      ctx->setInputAssemblyState(cDrawInfo.iaState);
      ctx->draw(
        cDrawInfo.vertexCount, cDrawInfo.instanceCount,
        cStartVertex, 0);
    });

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitive(
          D3DPRIMITIVETYPE PrimitiveType,
          INT              BaseVertexIndex,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             StartIndex,
          UINT             PrimitiveCount) {
    auto lock = LockDevice();

    PrepareDraw();

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount);
    
    EmitCs([
      cDrawInfo        = drawInfo,
      cStartIndex      = StartIndex,
      cBaseVertexIndex = BaseVertexIndex
    ](DxvkContext* ctx) {
      ctx->setInputAssemblyState(cDrawInfo.iaState);
      ctx->drawIndexed(
        cDrawInfo.vertexCount, cDrawInfo.instanceCount,
        cStartIndex,
        cBaseVertexIndex, 0);
    });

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
    auto lock = LockDevice();

    PrepareDraw(true);

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount);

    const uint32_t upSize = drawInfo.vertexCount * VertexStreamZeroStride;

    AllocUpBuffer(upSize);

    DxvkBufferSliceHandle physSlice = m_upBuffer->allocSlice();

    std::memcpy(physSlice.mapPtr, pVertexStreamZeroData, upSize);

    EmitCs([
      cDrawInfo     = drawInfo,
      cBuffer       = m_upBuffer,
      cBufferSlice  = physSlice,
      cStride       = VertexStreamZeroStride
    ](DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cBufferSlice);
      ctx->bindVertexBuffer(0, DxvkBufferSlice(cBuffer), cStride);
      ctx->setInputAssemblyState(cDrawInfo.iaState);
      ctx->draw(
        cDrawInfo.vertexCount, cDrawInfo.instanceCount,
        0, 0);
    });

    m_flags.set(D3D9DeviceFlag::UpDirtiedVertices);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             PrimitiveCount,
    const void*            pIndexData,
          D3DFORMAT        IndexDataFormat,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
    auto lock = LockDevice();

    PrepareDraw(true);

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount);

    const uint32_t vertexSize  = (MinVertexIndex + NumVertices) * VertexStreamZeroStride;

    const uint32_t indexSize = IndexDataFormat == D3DFMT_INDEX16 ? 2 : 4;
    const uint32_t indicesSize = drawInfo.vertexCount * indexSize;

    const uint32_t upSize = vertexSize + indicesSize;

    AllocUpBuffer(upSize);

    DxvkBufferSliceHandle physSlice = m_upBuffer->allocSlice();
    uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);

    std::memcpy(data, pVertexStreamZeroData, vertexSize);
    std::memcpy(data + vertexSize, pIndexData, indicesSize);

    EmitCs([
      cDrawInfo     = drawInfo,
      cVertexSize   = vertexSize,
      cBuffer       = m_upBuffer,
      cBufferSlice  = physSlice,
      cStride       = VertexStreamZeroStride,
      cIndexType    = DecodeIndexType(
                        static_cast<D3D9Format>(IndexDataFormat))
    ](DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cBufferSlice);
      ctx->bindVertexBuffer(0, DxvkBufferSlice(cBuffer, 0, cVertexSize), cStride);
      ctx->bindIndexBuffer(DxvkBufferSlice(cBuffer, cVertexSize, cBuffer->info().size - cVertexSize), cIndexType);
      ctx->setInputAssemblyState(cDrawInfo.iaState);
      ctx->drawIndexed(
        cDrawInfo.vertexCount, cDrawInfo.instanceCount,
        0,
        0, 0);
    });

    m_flags.set(D3D9DeviceFlag::UpDirtiedVertices);
    m_flags.set(D3D9DeviceFlag::UpDirtiedIndices);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ProcessVertices(
          UINT                         SrcStartIndex,
          UINT                         DestIndex,
          UINT                         VertexCount,
          IDirect3DVertexBuffer9*      pDestBuffer,
          IDirect3DVertexDeclaration9* pVertexDecl,
          DWORD                        Flags) {
    Logger::warn("Direct3DDevice9Ex::ProcessVertices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexDeclaration(
    const D3DVERTEXELEMENT9*            pVertexElements,
          IDirect3DVertexDeclaration9** ppDecl) {
    InitReturnPtr(ppDecl);

    if (ppDecl == nullptr || pVertexElements == nullptr)
      return D3DERR_INVALIDCALL;

    const D3DVERTEXELEMENT9* counter = pVertexElements;
    while (counter->Stream != 0xFF)
      counter++;

    const uint32_t declCount = uint32_t(counter - pVertexElements);

    try {
      const Com<Direct3DVertexDeclaration9> decl = new Direct3DVertexDeclaration9(this, pVertexElements, declCount);
      *ppDecl = decl.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    auto lock = LockDevice();

    Direct3DVertexDeclaration9* decl = static_cast<Direct3DVertexDeclaration9*>(pDecl);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetVertexDeclaration(decl);

    if (decl == m_state.vertexDecl)
      return D3D_OK;

    changePrivate(m_state.vertexDecl, decl);

    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    auto lock = LockDevice();

    InitReturnPtr(ppDecl);

    if (ppDecl == nullptr)
      return D3D_OK;

    if (m_state.vertexDecl == nullptr)
      return D3DERR_NOTFOUND;

    *ppDecl = ref(m_state.vertexDecl);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetFVF(DWORD FVF) {
    auto lock = LockDevice();

    if (FVF == 0)
      return D3D_OK;

    Direct3DVertexDeclaration9* decl = nullptr;

    auto iter = m_fvfTable.find(FVF);

    if (iter == m_fvfTable.end()) {
      decl = new Direct3DVertexDeclaration9(this, FVF);
      m_fvfTable.insert(std::make_pair(FVF, decl));
    }
    else
      decl = iter->second.ptr();

    return this->SetVertexDeclaration(decl);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFVF(DWORD* pFVF) {
    auto lock = LockDevice();

    if (pFVF == nullptr)
      return D3DERR_INVALIDCALL;

    *pFVF = m_state.vertexDecl != nullptr
      ? m_state.vertexDecl->GetFVF()
      : 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexShader(
    const DWORD*                   pFunction,
          IDirect3DVertexShader9** ppShader) {
    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    DxsoModuleInfo moduleInfo;
    moduleInfo.options = m_dxsoOptions;

    D3D9CommonShader module;

    if (FAILED(this->CreateShaderModule(&module,
      VK_SHADER_STAGE_VERTEX_BIT,
      pFunction,
      &moduleInfo)))
      return D3DERR_INVALIDCALL;

    *ppShader = ref(new D3D9VertexShader(this, module));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShader(IDirect3DVertexShader9* pShader) {
    auto lock = LockDevice();

    D3D9VertexShader* shader = static_cast<D3D9VertexShader*>(pShader);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetVertexShader(shader);

    if (shader == m_state.vertexShader)
      return D3D_OK;

    changePrivate(m_state.vertexShader, shader);

    BindShader(
      DxsoProgramType::VertexShader,
      GetCommonShader(shader));

    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    auto lock = LockDevice();

    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    *ppShader = ref(m_state.vertexShader);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantF(
          UINT   StartRegister,
    const float* pConstantData,
          UINT   Vector4fCount) {
    auto lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantF(
          UINT   StartRegister,
          float* pConstantData,
          UINT   Vector4fCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantI(
          UINT StartRegister,
    const int* pConstantData,
          UINT Vector4iCount) {
    auto lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantI(
          UINT StartRegister,
          int* pConstantData,
          UINT Vector4iCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantB(
          UINT  StartRegister,
    const BOOL* pConstantData,
          UINT  BoolCount) {
    auto lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantB(
          UINT  StartRegister,
          BOOL* pConstantData,
          UINT  BoolCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::VertexShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSource(
          UINT                    StreamNumber,
          IDirect3DVertexBuffer9* pStreamData,
          UINT                    OffsetInBytes,
          UINT                    Stride) {
    auto lock = LockDevice();

    if (StreamNumber >= caps::MaxStreams)
      return D3DERR_INVALIDCALL;

    Direct3DVertexBuffer9* buffer = static_cast<Direct3DVertexBuffer9*>(pStreamData);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStreamSource(
        StreamNumber,
        buffer,
        OffsetInBytes,
        Stride);

    auto& vbo = m_state.vertexBuffers[StreamNumber];
    changePrivate(vbo.vertexBuffer, buffer);
    vbo.offset = OffsetInBytes;
    vbo.stride = Stride;

    BindVertexBuffer(StreamNumber, buffer, OffsetInBytes, Stride);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSource(
          UINT                     StreamNumber,
          IDirect3DVertexBuffer9** ppStreamData,
          UINT*                    pOffsetInBytes,
          UINT*                    pStride) {
    auto lock = LockDevice();

    InitReturnPtr(ppStreamData);

    if (pOffsetInBytes != nullptr)
      *pOffsetInBytes = 0;

    if (pStride != nullptr)
      *pStride = 0;
    
    if (ppStreamData == nullptr || pOffsetInBytes == nullptr || pStride == nullptr)
      return D3DERR_INVALIDCALL;

    if (StreamNumber >= caps::MaxStreams)
      return D3DERR_INVALIDCALL;

    const auto& vbo = m_state.vertexBuffers[StreamNumber];

    *ppStreamData   = ref(vbo.vertexBuffer);
    *pOffsetInBytes = vbo.offset;
    *pStride        = vbo.stride;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    auto lock = LockDevice();

    if (StreamNumber >= caps::MaxStreams)
      return D3DERR_INVALIDCALL;

    const bool indexed  = Setting & D3DSTREAMSOURCE_INDEXEDDATA;
    const bool instanced = Setting & D3DSTREAMSOURCE_INSTANCEDATA;

    if (StreamNumber == 0 && instanced)
      return D3DERR_INVALIDCALL;

    if (instanced && indexed)
      return D3DERR_INVALIDCALL;

    if (Setting == 0)
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStreamSourceFreq(StreamNumber, Setting);

    if (m_state.streamFreq[StreamNumber] == Setting)
      return D3D_OK;

    m_state.streamFreq[StreamNumber] = Setting;

    if (instanced)
      m_instancedData |=   1u << StreamNumber;
    else
      m_instancedData &= ~(1u << StreamNumber);

    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    Logger::warn("Direct3DDevice9Ex::GetStreamSourceFreq: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    auto lock = LockDevice();

    Direct3DIndexBuffer9* buffer = static_cast<Direct3DIndexBuffer9*>(pIndexData);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetIndices(buffer);

    if (buffer == m_state.indices)
      return D3D_OK;

    changePrivate(m_state.indices, buffer);

    BindIndices();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    auto lock = LockDevice();
    InitReturnPtr(ppIndexData);

    if (ppIndexData == nullptr)
      return D3DERR_INVALIDCALL;

    *ppIndexData = ref(m_state.indices);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreatePixelShader(
    const DWORD*                  pFunction,
          IDirect3DPixelShader9** ppShader) {
    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    DxsoModuleInfo moduleInfo;
    moduleInfo.options = m_dxsoOptions;

    D3D9CommonShader module;

    if (FAILED(this->CreateShaderModule(&module,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      pFunction,
      &moduleInfo)))
      return D3DERR_INVALIDCALL;

    *ppShader = ref(new D3D9PixelShader(this, module));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShader(IDirect3DPixelShader9* pShader) {
    auto lock = LockDevice();

    D3D9PixelShader* shader = static_cast<D3D9PixelShader*>(pShader);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetPixelShader(shader);

    if (shader == m_state.pixelShader)
      return D3D_OK;

    changePrivate(m_state.pixelShader, shader);

    BindShader(
      DxsoProgramType::PixelShader,
      GetCommonShader(shader));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    auto lock = LockDevice();

    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    *ppShader = ref(m_state.pixelShader);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantF(
    UINT   StartRegister,
    const float* pConstantData,
    UINT   Vector4fCount) {
    auto lock = LockDevice();

    return SetShaderConstants <
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantF(
    UINT   StartRegister,
    float* pConstantData,
    UINT   Vector4fCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    auto lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantB(
    UINT  StartRegister,
    const BOOL* pConstantData,
    UINT  BoolCount) {
    auto lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantB(
    UINT  StartRegister,
    BOOL* pConstantData,
    UINT  BoolCount) {
    auto lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramType::PixelShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawRectPatch(
          UINT               Handle,
    const float*             pNumSegs,
    const D3DRECTPATCH_INFO* pRectPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawRectPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawTriPatch(
          UINT              Handle,
    const float*            pNumSegs,
    const D3DTRIPATCH_INFO* pTriPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawTriPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DeletePatch(UINT Handle) {
    Logger::warn("Direct3DDevice9Ex::DeletePatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    InitReturnPtr(ppQuery);

    if (ppQuery == nullptr) {
      if (D3D9Query::QuerySupported(Type))
        return D3D_OK;
      else
        return D3DERR_NOTAVAILABLE;
    }

    try {
      *ppQuery = ref(new D3D9Query(this, Type));
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  // Ex Methods

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetConvolutionMonoKernel(
          UINT   width,
          UINT   height,
          float* rows,
          float* columns) {
    Logger::warn("Direct3DDevice9Ex::SetConvolutionMonoKernel: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ComposeRects(
          IDirect3DSurface9*      pSrc,
          IDirect3DSurface9*      pDst,
          IDirect3DVertexBuffer9* pSrcRectDescs,
          UINT                    NumRects,
          IDirect3DVertexBuffer9* pDstRectDescs,
          D3DCOMPOSERECTSOP       Operation,
          int                     Xoffset,
          int                     Yoffset) {
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
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->WaitForVBlank();
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) {
    Logger::warn("Direct3DDevice9Ex::CheckResourceResidency: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaximumFrameLatency(UINT MaxLatency) {
    auto lock = LockDevice();

    if (MaxLatency == 0)
      MaxLatency = DefaultFrameLatency;

    if (MaxLatency > m_frameEvents.size())
      MaxLatency = m_frameEvents.size();

    m_frameLatency = MaxLatency;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaximumFrameLatency(UINT* pMaxLatency) {
    auto lock = LockDevice();

    if (pMaxLatency == nullptr)
      return D3DERR_INVALIDCALL;

    *pMaxLatency = m_frameLatency;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckDeviceState(HWND hDestinationWindow) {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::PresentEx(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
          DWORD dwFlags) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(0);
    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;
    
    return swapchain->Present(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      dwFlags);
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
    auto lock = LockDevice();

    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_SURFACE;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage | D3DUSAGE_RENDERTARGET;
    desc.Format             = fixupFormat(Format);
    desc.Pool               = D3DPOOL_DEFAULT;
    desc.Discard            = FALSE;
    desc.MultiSample        = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.Lockable           = Lockable;
    desc.Offscreen          = TRUE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9(this, &desc);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    auto lock = LockDevice();

    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_SURFACE;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage;
    desc.Format             = fixupFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable           = TRUE; // Offscreen surfaces are always lockable.
    desc.Offscreen          = TRUE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9(this, &desc);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
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
    auto lock = LockDevice();

    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    auto format = fixupFormat(Format);
    bool lockable = format == D3D9Format::D32_LOCKABLE
                 || format == D3D9Format::D32F_LOCKABLE
                 || format == D3D9Format::D16_LOCKABLE
                 || format == D3D9Format::S8_LOCKABLE;

    D3D9TextureDesc desc;
    desc.Type               = D3DRTYPE_SURFACE;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage | D3DUSAGE_DEPTHSTENCIL;
    desc.Format             = format;
    desc.Pool               = D3DPOOL_DEFAULT;
    desc.Discard            = Discard;
    desc.MultiSample        = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.Lockable           = lockable;
    desc.Offscreen          = TRUE;

    if (FAILED(Direct3DCommonTexture9::NormalizeTextureProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9(this, &desc);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    auto lock = LockDevice();

    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    SetDepthStencilSurface(nullptr);

    for (uint32_t i = 0; i < caps::MaxSimultaneousRenderTargets; i++)
      SetRenderTarget(0, nullptr);

    auto & rs = m_state.renderStates;

    rs[D3DRS_SEPARATEALPHABLENDENABLE] = FALSE;
    rs[D3DRS_ALPHABLENDENABLE]         = FALSE;
    rs[D3DRS_BLENDOP]                  = D3DBLENDOP_ADD;
    rs[D3DRS_BLENDOPALPHA]             = D3DBLENDOP_ADD;
    rs[D3DRS_DESTBLEND]                = D3DBLEND_ZERO;
    rs[D3DRS_DESTBLENDALPHA]           = D3DBLEND_ZERO;
    rs[D3DRS_COLORWRITEENABLE]         = 0x0000000f;
    rs[D3DRS_COLORWRITEENABLE1]        = 0x0000000f;
    rs[D3DRS_COLORWRITEENABLE2]        = 0x0000000f;
    rs[D3DRS_COLORWRITEENABLE3]        = 0x0000000f;
    rs[D3DRS_SRCBLEND]                 = D3DBLEND_ONE;
    rs[D3DRS_SRCBLENDALPHA]            = D3DBLEND_ONE;
    BindBlendState();

    rs[D3DRS_BLENDFACTOR]              = 0xffffffff;
    BindBlendFactor();

    rs[D3DRS_ZENABLE]                  = pPresentationParameters->EnableAutoDepthStencil != FALSE
                                       ? D3DZB_TRUE
                                       : D3DZB_FALSE;
    rs[D3DRS_ZFUNC]                    = D3DCMP_LESSEQUAL;
    rs[D3DRS_TWOSIDEDSTENCILMODE]      = FALSE;
    rs[D3DRS_ZWRITEENABLE]             = TRUE;
    rs[D3DRS_STENCILENABLE]            = FALSE;
    rs[D3DRS_STENCILFAIL]              = D3DSTENCILOP_KEEP;
    rs[D3DRS_STENCILZFAIL]             = D3DSTENCILOP_KEEP;
    rs[D3DRS_STENCILPASS]              = D3DSTENCILOP_KEEP;
    rs[D3DRS_STENCILFUNC]              = D3DCMP_ALWAYS;
    rs[D3DRS_CCW_STENCILFAIL]          = D3DSTENCILOP_KEEP;
    rs[D3DRS_CCW_STENCILZFAIL]         = D3DSTENCILOP_KEEP;
    rs[D3DRS_CCW_STENCILPASS]          = D3DSTENCILOP_KEEP;
    rs[D3DRS_CCW_STENCILFUNC]          = D3DCMP_ALWAYS;
    rs[D3DRS_STENCILMASK]              = 0xFFFFFFFF;
    rs[D3DRS_STENCILWRITEMASK]         = 0xFFFFFFFF;
    BindDepthStencilState();

    rs[D3DRS_STENCILREF] = 0;
    BindDepthStencilRefrence();

    rs[D3DRS_FILLMODE]            = D3DFILL_SOLID;
    rs[D3DRS_CULLMODE]            = D3DCULL_CCW;
    rs[D3DRS_DEPTHBIAS]           = bit::cast<DWORD>(0.0f);
    rs[D3DRS_SLOPESCALEDEPTHBIAS] = bit::cast<DWORD>(0.0f);
    BindRasterizerState();

    rs[D3DRS_SCISSORTESTENABLE]   = FALSE;

    SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    SetRenderState(D3DRS_LASTPIXEL, TRUE);
    SetRenderState(D3DRS_ALPHAREF, 0);
    SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
    SetRenderState(D3DRS_DITHERENABLE, FALSE);
    SetRenderState(D3DRS_FOGENABLE, FALSE);
    SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    //	SetRenderState(D3DRS_ZVISIBLE, 0);
    SetRenderState(D3DRS_FOGCOLOR, 0);
    SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    SetRenderState(D3DRS_FOGSTART, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_FOGEND, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_FOGDENSITY, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
    SetRenderState(D3DRS_WRAP0, 0);
    SetRenderState(D3DRS_WRAP1, 0);
    SetRenderState(D3DRS_WRAP2, 0);
    SetRenderState(D3DRS_WRAP3, 0);
    SetRenderState(D3DRS_WRAP4, 0);
    SetRenderState(D3DRS_WRAP5, 0);
    SetRenderState(D3DRS_WRAP6, 0);
    SetRenderState(D3DRS_WRAP7, 0);
    SetRenderState(D3DRS_CLIPPING, TRUE);
    SetRenderState(D3DRS_LIGHTING, TRUE);
    SetRenderState(D3DRS_AMBIENT, 0);
    SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    SetRenderState(D3DRS_COLORVERTEX, TRUE);
    SetRenderState(D3DRS_LOCALVIEWER, TRUE);
    SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
    SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    SetRenderState(D3DRS_POINTSIZE, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSIZE_MIN, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
    SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);
    SetRenderState(D3DRS_POINTSCALE_A, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSCALE_B, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_POINTSCALE_C, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
    SetRenderState(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE);
    SetRenderState(D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE);
    SetRenderState(D3DRS_POINTSIZE_MAX, bit::cast<DWORD>(64.0f));
    SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    SetRenderState(D3DRS_TWEENFACTOR, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC);
    SetRenderState(D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR);
    SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
    SetRenderState(D3DRS_MINTESSELLATIONLEVEL, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_MAXTESSELLATIONLEVEL, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_X, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_Y, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_Z, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_W, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
    SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
    SetRenderState(D3DRS_WRAP8, 0);
    SetRenderState(D3DRS_WRAP9, 0);
    SetRenderState(D3DRS_WRAP10, 0);
    SetRenderState(D3DRS_WRAP11, 0);
    SetRenderState(D3DRS_WRAP12, 0);
    SetRenderState(D3DRS_WRAP13, 0);
    SetRenderState(D3DRS_WRAP14, 0);
    SetRenderState(D3DRS_WRAP15, 0);

    for (uint32_t i = 0; i < caps::MaxTextureBlendStages; i++)
    {
      SetTextureStageState(i, D3DTSS_COLOROP, i == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE);
      SetTextureStageState(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      SetTextureStageState(i, D3DTSS_COLORARG2, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_ALPHAOP, i == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE);
      SetTextureStageState(i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      SetTextureStageState(i, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_BUMPENVMAT00, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT01, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT10, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT11, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
      SetTextureStageState(i, D3DTSS_BUMPENVLSCALE, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVLOFFSET, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
      SetTextureStageState(i, D3DTSS_COLORARG0, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_RESULTARG, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_CONSTANT, 0x00000000);
    }

    for (uint32_t i = 0; i < caps::MaxStreams; i++)
      m_state.streamFreq[i] = 1;

    for (uint32_t i = 0; i < m_state.textures.size(); i++) {
      m_state.textures[i] = nullptr;

      DWORD sampler = i;
      auto samplerInfo = RemapStateSamplerShader(sampler);
      uint32_t slot = computeResourceSlotId(samplerInfo.first, DxsoBindingType::Image, uint32_t(samplerInfo.second));

      EmitCs([
        cSlot = slot
      ](DxvkContext* ctx) {
        ctx->bindResourceView(cSlot, nullptr, nullptr);
      });
    }

    auto& ss = m_state.samplerStates;
    for (uint32_t i = 0; i < ss.size(); i++) {
      auto& state = ss[i];
      state[D3DSAMP_ADDRESSU]      = D3DTADDRESS_WRAP;
      state[D3DSAMP_ADDRESSV]      = D3DTADDRESS_WRAP;
      state[D3DSAMP_ADDRESSU]      = D3DTADDRESS_WRAP;
      state[D3DSAMP_ADDRESSW]      = D3DTADDRESS_WRAP;
      state[D3DSAMP_BORDERCOLOR]   = 0x00000000;
      state[D3DSAMP_MAGFILTER]     = D3DTEXF_POINT;
      state[D3DSAMP_MINFILTER]     = D3DTEXF_POINT;
      state[D3DSAMP_MIPFILTER]     = D3DTEXF_NONE;
      state[D3DSAMP_MIPMAPLODBIAS] = bit::cast<DWORD>(0.0f);
      state[D3DSAMP_MAXMIPLEVEL]   = 0;
      state[D3DSAMP_MAXANISOTROPY] = 1;
      state[D3DSAMP_SRGBTEXTURE]   = 0;
      state[D3DSAMP_ELEMENTINDEX]  = 0;
      state[D3DSAMP_DMAPOFFSET]    = 0;

      BindSampler(i);
    }

    m_dirtySamplerStates = 0;

    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      float plane[4] = { 0, 0, 0, 0 };
      SetClipPlane(i, plane);
    }

    Flush();
    SynchronizeCsThread();

    HRESULT hr;
    auto* implicitSwapchain = GetInternalSwapchain(0);
    if (implicitSwapchain == nullptr) {
      Com<IDirect3DSwapChain9> swapchain;
      hr = CreateAdditionalSwapChain(pPresentationParameters, &swapchain);
      if (FAILED(hr))
        throw DxvkError("Reset: failed to create implicit swapchain");
    }
    else {
      hr = implicitSwapchain->Reset(pPresentationParameters);
      if (FAILED(hr))
        throw DxvkError("Reset: failed to reset swapchain");
    }

    Com<IDirect3DSurface9> backbuffer;
    hr = m_swapchains[0]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    if (FAILED(hr))
      throw DxvkError("Reset: failed to get implicit swapchain backbuffers");

    SetRenderTarget(0, backbuffer.ptr());

    if (pPresentationParameters->EnableAutoDepthStencil) {
      Com<IDirect3DSurface9> autoDepthStencil;
      CreateDepthStencilSurface(
        pPresentationParameters->BackBufferWidth,
        pPresentationParameters->BackBufferHeight,
        pPresentationParameters->AutoDepthStencilFormat,
        pPresentationParameters->MultiSampleType,
        pPresentationParameters->MultiSampleQuality,
        FALSE,
        &autoDepthStencil,
        nullptr);

      SetDepthStencilSurface(autoDepthStencil.ptr());
    }

    ShowCursor(FALSE);

    // Force this if we end up binding the same RT to make scissor change go into effect.
    BindViewportAndScissor();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayModeEx(
    UINT iSwapChain,
    D3DDISPLAYMODEEX* pMode,
    D3DDISPLAYROTATION* pRotation) {
    auto lock = LockDevice();

    auto* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetDisplayModeEx(pMode, pRotation);
  }

  HRESULT Direct3DDevice9Ex::SetStateSamplerState(
    DWORD               StateSampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD               Value) {
    auto lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateSamplerState(StateSampler, Type, Value);

    auto& state = m_state.samplerStates;

    bool changed = state[StateSampler][Type] != Value;

    if (likely(changed)) {
      state[StateSampler][Type] = Value;

      if (Type == D3DSAMP_ADDRESSU
       || Type == D3DSAMP_ADDRESSV
       || Type == D3DSAMP_ADDRESSW
       || Type == D3DSAMP_MAGFILTER
       || Type == D3DSAMP_MINFILTER
       || Type == D3DSAMP_MIPFILTER
       || Type == D3DSAMP_MAXANISOTROPY
       || Type == D3DSAMP_MIPMAPLODBIAS
       || Type == D3DSAMP_MAXMIPLEVEL
       || Type == D3DSAMP_BORDERCOLOR)
        m_dirtySamplerStates |= 1u << StateSampler;
      else if (Type == D3DSAMP_SRGBTEXTURE)
        BindTexture(StateSampler);
    }

    return D3D_OK;
  }

  HRESULT Direct3DDevice9Ex::SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture) {
    auto lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateTexture(StateSampler, pTexture);

    if (m_state.textures[StateSampler] == pTexture)
      return D3D_OK;
    
    TextureChangePrivate(m_state.textures[StateSampler], pTexture);

    BindTexture(StateSampler);

    return D3D_OK;
  }

  bool Direct3DDevice9Ex::IsExtended() {
    return m_flags.test(D3D9DeviceFlag::ExtendedDevice);
  }

  HWND Direct3DDevice9Ex::GetWindow() {
    return m_window;
  }

  Rc<DxvkEvent> Direct3DDevice9Ex::GetFrameSyncEvent() {
    uint32_t frameLatency = m_frameLatency;

    if (m_frameLatencyCap != 0
      && m_frameLatencyCap <= frameLatency)
      frameLatency = m_frameLatencyCap;

    uint32_t frameId = m_frameId++ % frameLatency;
    return m_frameEvents[frameId];
  }

  DxvkDeviceFeatures Direct3DDevice9Ex::GetDeviceFeatures(const Rc<DxvkAdapter>& adapter) {
    DxvkDeviceFeatures supported = adapter->features();
    DxvkDeviceFeatures enabled = {};

    // Geometry shaders are used for some meta ops
    enabled.core.features.geometryShader = VK_TRUE;
    enabled.core.features.robustBufferAccess = VK_TRUE;

    enabled.extMemoryPriority.memoryPriority = supported.extMemoryPriority.memoryPriority;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;

    // DXVK Meta
    enabled.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    enabled.core.features.shaderStorageImageExtendedFormats    = VK_TRUE;

    // SM1 level hardware
    enabled.core.features.depthClamp = VK_TRUE;
    enabled.core.features.depthBiasClamp = VK_TRUE;
    enabled.core.features.fillModeNonSolid = VK_TRUE;
    enabled.core.features.pipelineStatisticsQuery = supported.core.features.pipelineStatisticsQuery;
    enabled.core.features.sampleRateShading = VK_TRUE;
    enabled.core.features.samplerAnisotropy = VK_TRUE;
    enabled.core.features.shaderClipDistance = VK_TRUE;
    enabled.core.features.shaderCullDistance = VK_TRUE;

    // Ensure we support real BC formats and unofficial vendor ones.
    enabled.core.features.textureCompressionBC = VK_TRUE;

    enabled.extDepthClipEnable.depthClipEnable = supported.extDepthClipEnable.depthClipEnable;
    enabled.extHostQueryReset.hostQueryReset = supported.extHostQueryReset.hostQueryReset;

    // SM2 level hardware
    enabled.core.features.occlusionQueryPrecise = VK_TRUE;

    // SM3 level hardware
    enabled.core.features.multiViewport = VK_TRUE;
    enabled.core.features.independentBlend = VK_TRUE;

    // D3D10 level hardware supports this in D3D9 native.
    enabled.core.features.fullDrawIndexUint32 = VK_TRUE;

    return enabled;
  }

  void Direct3DDevice9Ex::AllocUpBuffer(uint32_t size) {
    const uint32_t currentSize = m_upBuffer != nullptr
      ? m_upBuffer->info().size
      : 0;

    if (currentSize >= size)
      return;

    DxvkBufferCreateInfo  info;
    info.size   = size;
    info.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                | VK_ACCESS_INDEX_READ_BIT;
    info.stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    m_upBuffer = m_dxvkDevice->createBuffer(info, memoryFlags);
  }

  Direct3DSwapChain9Ex* Direct3DDevice9Ex::GetInternalSwapchain(UINT index) {
    if (index >= m_swapchains.size())
      return nullptr;

    return static_cast<Direct3DSwapChain9Ex*>(m_swapchains[index]);
  }

  bool Direct3DDevice9Ex::ShouldRecord() {
    return m_recorder != nullptr && !m_recorder->IsApplying();
  }

  D3D9_VK_FORMAT_MAPPING Direct3DDevice9Ex::LookupFormat(
    D3D9Format            Format) const {
    return m_d3d9Formats.GetFormatMapping(Format);
  }

  VkFormat Direct3DDevice9Ex::LookupDecltype(D3DDECLTYPE d3d9DeclType) {
    switch (d3d9DeclType) {
      case D3DDECLTYPE_FLOAT1:    return VK_FORMAT_R32_SFLOAT;
      case D3DDECLTYPE_FLOAT2:    return VK_FORMAT_R32G32_SFLOAT;
      case D3DDECLTYPE_FLOAT3:    return VK_FORMAT_R32G32B32_SFLOAT;
      case D3DDECLTYPE_FLOAT4:    return VK_FORMAT_R32G32B32A32_SFLOAT;
      case D3DDECLTYPE_D3DCOLOR:  return VK_FORMAT_B8G8R8A8_UNORM;
      case D3DDECLTYPE_UBYTE4:    return VK_FORMAT_R8G8B8A8_USCALED;
      case D3DDECLTYPE_SHORT2:    return VK_FORMAT_R16G16_SSCALED;
      case D3DDECLTYPE_SHORT4:    return VK_FORMAT_R16G16B16A16_SSCALED;
      case D3DDECLTYPE_UBYTE4N:   return VK_FORMAT_R8G8B8A8_UNORM;
      case D3DDECLTYPE_SHORT2N:   return VK_FORMAT_R16G16_SNORM;
      case D3DDECLTYPE_SHORT4N:   return VK_FORMAT_R16G16B16A16_SNORM;
      case D3DDECLTYPE_USHORT2N:  return VK_FORMAT_R16G16_UNORM;
      case D3DDECLTYPE_USHORT4N:  return VK_FORMAT_R16G16B16A16_UNORM;
      case D3DDECLTYPE_UDEC3:     return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
      case D3DDECLTYPE_FLOAT16_2: return VK_FORMAT_R16G16_SFLOAT;
      case D3DDECLTYPE_FLOAT16_4: return VK_FORMAT_R16G16B16A16_SFLOAT;
      case D3DDECLTYPE_DEC3N:     return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
      case D3DDECLTYPE_UNUSED:
      default:                    return VK_FORMAT_UNDEFINED;
    }
  }

  bool Direct3DDevice9Ex::WaitForResource(
  const Rc<DxvkResource>&                 Resource,
        DWORD                             MapFlags) {
    // Wait for the any pending D3D9 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.

    SynchronizeCsThread();

    if (Resource->isInUse()) {
      if (MapFlags & D3DLOCK_DONOTWAIT) {
        // We don't have to wait, but misbehaving games may
        // still try to spin on `Map` until the resource is
        // idle, so we should flush pending commands
        FlushImplicit(FALSE);
        return false;
      }
      else {
        // Make sure pending commands using the resource get
        // executed on the the GPU if we have to wait for it
        Flush();
        SynchronizeCsThread();

        while (Resource->isInUse())
          dxvk::this_thread::yield();
      }
    }

    return true;
  }

  uint32_t Direct3DDevice9Ex::CalcImageLockOffset(
            uint32_t                SlicePitch,
            uint32_t                RowPitch,
      const DxvkFormatInfo*         FormatInfo,
      const D3DBOX*                 pBox) {
    if (pBox == nullptr)
      return 0;

    return pBox->Front * SlicePitch +
           pBox->Top   * RowPitch   +
           FormatInfo->elementSize * align(pBox->Left, FormatInfo->blockSize.width);
  }

  HRESULT Direct3DDevice9Ex::LockImage(
            Direct3DCommonTexture9* pResource,
            UINT                    Face,
            UINT                    MipLevel,
            D3DLOCKED_BOX*          pLockedBox,
      const D3DBOX*                 pBox,
            DWORD                   Flags) {
    auto lock = LockDevice();

    if (unlikely(pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_NONE)) {
      Logger::err("D3D9: Cannot map a device-local image");
      return D3DERR_INVALIDCALL;
    }

    // TODO: Some fastpath for D3DLOCK_READONLY.

    UINT Subresource = pResource->CalcSubresource(Face, MipLevel);

    pResource->AllocBuffers(Face, MipLevel);

    const Rc<DxvkImage>  mappedImage  = pResource->GetImage();
    const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer(Subresource);
    
    auto formatInfo = imageFormatInfo(mappedImage->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);
    
    pResource->MarkSubresourceMapped(Face, MipLevel, Flags);
    
    if (pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      const VkImageType imageType = mappedImage->info().type;
      
      // Wait for the resource to become available
      if (!WaitForResource(mappedImage, Flags))
        return D3DERR_WASSTILLDRAWING;
      
      // Query the subresource's memory layout and hope that
      // the application respects the returned pitch values.
      VkSubresourceLayout layout  = mappedImage->querySubresourceLayout(subresource);
      pLockedBox->RowPitch   = imageType >= VK_IMAGE_TYPE_2D ? layout.rowPitch   : layout.size;
      pLockedBox->SlicePitch = imageType >= VK_IMAGE_TYPE_3D ? layout.depthPitch : layout.size;

      const uint32_t offset = CalcImageLockOffset(
        pLockedBox->SlicePitch,
        pLockedBox->RowPitch,
        formatInfo,
        pBox);

      uint8_t* data = reinterpret_cast<uint8_t*>(mappedImage->mapPtr(layout.offset));
      data += offset;
      pLockedBox->pBits = data;
      return D3D_OK;
    } else if (formatInfo->aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);

      // The actual Vulkan image format may differ
      // from the format requested by the application
      VkFormat packFormat = GetPackedDepthStencilFormat(pResource->Desc()->Format);
      auto packFormatInfo = imageFormatInfo(packFormat);

      // This is slow, but we have to dispatch a pack
      // operation and then immediately synchronize.
      EmitCs([
        cImageBuffer = mappedBuffer,
        cImage       = mappedImage,
        cSubresource = subresource,
        cFormat      = packFormat
      ] (DxvkContext* ctx) {
        auto layers = vk::makeSubresourceLayers(cSubresource);
        auto x = cImage->mipLevelExtent(cSubresource.mipLevel);

        VkOffset2D offset = { 0, 0 };
        VkExtent2D extent = { x.width, x.height };

        ctx->copyDepthStencilImageToPackedBuffer(
          cImageBuffer, 0, cImage, layers, offset, extent, cFormat);
      });

      WaitForResource(mappedBuffer, 0);

      DxvkBufferSliceHandle physSlice = mappedBuffer->getSliceHandle();

      pLockedBox->RowPitch   = packFormatInfo->elementSize * levelExtent.width;
      pLockedBox->SlicePitch = packFormatInfo->elementSize * levelExtent.width * levelExtent.height;

      const uint32_t offset = CalcImageLockOffset(
        pLockedBox->SlicePitch,
        pLockedBox->RowPitch,
        packFormatInfo,
        pBox);

      uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
      data += offset;
      pLockedBox->pBits = data;
      return D3D_OK;
    } else {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);
      VkExtent3D blockCount  = util::computeBlockCount(levelExtent, formatInfo->blockSize);
      bool managed = pResource->Desc()->Pool == D3DPOOL_MANAGED;
      
      DxvkBufferSliceHandle physSlice;
      
      if (Flags & D3DLOCK_DISCARD) {
        // We do not have to preserve the contents of the
        // buffer if the entire image gets discarded.
        physSlice = mappedBuffer->allocSlice();
        
        EmitCs([
          cImageBuffer = mappedBuffer,
          cBufferSlice = physSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
        });
      }
      else if (managed) {
        // Managed resources are meant to be able to provide readback without waiting.
        // We always keep a copy of them in system memory for this reason.
        // No need to wait as its not in use.
        physSlice = mappedBuffer->getSliceHandle();
      }
      else {
        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        const bool copyExistingData = !pResource->IsWriteOnly();
        
        if (copyExistingData) {
          auto subresourceLayers = vk::makeSubresourceLayers(subresource);
          
          EmitCs([
            cImageBuffer  = mappedBuffer,
            cImage        = mappedImage,
            cSubresources = subresourceLayers,
            cLevelExtent  = levelExtent
          ] (DxvkContext* ctx) {
            ctx->copyImageToBuffer(
              cImageBuffer, 0, VkExtent2D { 0u, 0u },
              cImage, cSubresources, VkOffset3D { 0, 0, 0 },
              cLevelExtent);
          });
        }
        
        WaitForResource(mappedBuffer, 0);
        physSlice = mappedBuffer->getSliceHandle();
      }
      
      // Set up map pointer. Data is tightly packed within the mapped buffer.
      pLockedBox->RowPitch   = formatInfo->elementSize * blockCount.width;
      pLockedBox->SlicePitch = formatInfo->elementSize * blockCount.width * blockCount.height;

      const uint32_t offset = CalcImageLockOffset(
        pLockedBox->SlicePitch,
        pLockedBox->RowPitch,
        formatInfo,
        pBox);

      uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
      data += offset;
      pLockedBox->pBits = data;
      return D3D_OK;
    }
  }

  HRESULT Direct3DDevice9Ex::UnlockImage(
        Direct3DCommonTexture9* pResource,
        UINT                    Face,
        UINT                    MipLevel) {
    auto lock = LockDevice();

    bool readOnly      = pResource->IsReadOnlyLock(Face, MipLevel);

    bool fullyUnlocked = pResource->MarkSubresourceUnmapped(Face, MipLevel);

    bool readRemaining = pResource->ReadLocksRemaining();

    bool managed = pResource->Desc()->Pool == D3DPOOL_MANAGED;

    // Do we have a pending copy?
    if (pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BUFFER) {
      bool fixup8888 = pResource->Desc()->Format == D3D9Format::R8G8B8;

      // Do we need to do some fixup before copying to image?
      if (fixup8888 && !readOnly)
        FixupFormat(pResource, Face, MipLevel);

      // Are we fully unlocked to flush me to the GPU?
      if (fullyUnlocked) {
        if (!readOnly)
          this->FlushImage(pResource);

        // If we have no remaining read-only locks we can clear our mapping buffers.
        if (!readRemaining && !managed)
          pResource->DeallocMappingBuffers();
      }
    }

    if (fullyUnlocked && !readRemaining)
      pResource->GenerateMipSubLevels();

    return D3D_OK;
  }

  HRESULT Direct3DDevice9Ex::FlushImage(
    Direct3DCommonTexture9* pResource) {
    auto dirtySubresources = pResource->DiscardSubresourceMasking();

    const Rc<DxvkImage>  mappedImage = pResource->GetImage();
    const uint32_t       mipLevels   = mappedImage->info().mipLevels;

    for (uint32_t l = 0; l < pResource->GetLayerCount(); l++) {
      uint16_t mask = dirtySubresources[l];

      for (uint32_t i = 0; i < mipLevels; i++) {
        if (!(mask & 1u << i))
          continue;

        const UINT Subresource = pResource->CalcSubresource(l, i);
        const bool fixup8888   = pResource->Desc()->Format == D3D9Format::R8G8B8;

        // Now that data has been written into the buffer,
        // we need to copy its contents into the image
        const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer(Subresource);
        const Rc<DxvkBuffer> fixupBuffer  = pResource->GetFixupBuffer(Subresource);

        auto formatInfo  = imageFormatInfo(mappedImage->info().format);
        auto subresource = pResource->GetSubresourceFromIndex(
          formatInfo->aspectMask, Subresource);

        VkExtent3D levelExtent = mappedImage
          ->mipLevelExtent(subresource.mipLevel);

        VkImageSubresourceLayers subresourceLayers = {
          subresource.aspectMask,
          subresource.mipLevel,
          subresource.arrayLayer, 1 };

        EmitCs([
          cSrcBuffer = fixup8888 ? fixupBuffer : mappedBuffer,
          cDstImage = mappedImage,
          cDstLayers = subresourceLayers,
          cDstLevelExtent = levelExtent
        ] (DxvkContext* ctx) {
          ctx->copyBufferToImage(cDstImage, cDstLayers,
            VkOffset3D{ 0, 0, 0 }, cDstLevelExtent,
            cSrcBuffer, 0, { 0u, 0u });
        });

        pResource->DeallocFixupBuffer(Subresource);
      }
    }

    return D3D_OK;
  }

  void Direct3DDevice9Ex::GenerateMips(
    Direct3DCommonTexture9* pResource) {
    EmitCs([
      cImageView = pResource->GetRenderTargetView(false)
    ] (DxvkContext* ctx) {
      ctx->generateMipmaps(cImageView);
    });
  }

  void Direct3DDevice9Ex::FixupFormat(
        Direct3DCommonTexture9* pResource,
        UINT                    Face,
        UINT                    MipLevel) {
    UINT Subresource = pResource->CalcSubresource(Face, MipLevel);

    const Rc<DxvkImage>  mappedImage  = pResource->GetImage();
    const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer(Subresource);
    const Rc<DxvkBuffer> fixupBuffer  = pResource->GetFixupBuffer(Subresource);

    auto formatInfo = imageFormatInfo(mappedImage->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
      formatInfo->aspectMask, Subresource);

    DxvkBufferSliceHandle mappingSlice = mappedBuffer->getSliceHandle();
    DxvkBufferSliceHandle fixupSlice   = fixupBuffer->allocSlice();

    EmitCs([
      cImageBuffer = fixupBuffer,
      cBufferSlice = fixupSlice
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
    });

    VkExtent3D levelExtent = mappedImage
      ->mipLevelExtent(subresource.mipLevel);
    VkExtent3D blockCount = util::computeBlockCount(levelExtent, formatInfo->blockSize);

    uint32_t rowPitch   = formatInfo->elementSize * blockCount.width;
    uint32_t slicePitch = formatInfo->elementSize * blockCount.width * blockCount.height;

    uint8_t * dst = reinterpret_cast<uint8_t*>(fixupSlice.mapPtr);
    uint8_t * src = reinterpret_cast<uint8_t*>(mappingSlice.mapPtr);

    for (uint32_t z = 0; z < levelExtent.depth; z++) {
      for (uint32_t y = 0; y < levelExtent.height; y++) {
        for (uint32_t x = 0; x < levelExtent.width; x++) {
          for (uint32_t c = 0; c < 3; c++)
            dst[z * slicePitch + y * rowPitch + x * 4 + c] = src[z * slicePitch + y * rowPitch + x * 3 + c];

          dst[z * slicePitch + y * rowPitch + x * 4 + 3] = 255;
        }
      }
    }
  }

  HRESULT Direct3DDevice9Ex::LockBuffer(
          Direct3DCommonBuffer9*  pResource,
          UINT                    OffsetToLock,
          UINT                    SizeToLock,
          void**                  ppbData,
          DWORD                   Flags) {
    auto lock = LockDevice();

    if (ppbData == nullptr)
      return D3DERR_INVALIDCALL;

    pResource->SetMapFlags(Flags);

    if (Flags & D3DLOCK_DISCARD) {
      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.

      // TODO: Investigate locking regions rather than the whole resource.
      auto physSlice = pResource->DiscardMapSlice();
      uint8_t* data =  reinterpret_cast<uint8_t*>(physSlice.mapPtr);
               data += OffsetToLock;

      *ppbData = reinterpret_cast<void*>(data);

      EmitCs([
        cBuffer      = pResource->GetBuffer(D3D9_COMMON_BUFFER_TYPE_MAPPING),
        cBufferSlice = physSlice
      ] (DxvkContext * ctx) {
          ctx->invalidateBuffer(cBuffer, cBufferSlice);
      });

      return D3D_OK;
    }
    else {
      // Wait until the resource is no longer in use
      if (!(Flags & D3DLOCK_NOOVERWRITE) && pResource->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_DIRECT) {
        if (!WaitForResource(pResource->GetBuffer(D3D9_COMMON_BUFFER_TYPE_REAL), Flags))
          return D3DERR_WASSTILLDRAWING;
      }

      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3DLOCK_NOOVERWRITE.
      DxvkBufferSliceHandle physSlice = pResource->GetMappedSlice();

      uint8_t* data =  reinterpret_cast<uint8_t*>(physSlice.mapPtr);
               data += OffsetToLock;

      *ppbData = reinterpret_cast<void*>(data);

      return D3D_OK;
    }
  }

  HRESULT Direct3DDevice9Ex::UnlockBuffer(
        Direct3DCommonBuffer9* pResource) {
    auto lock = LockDevice();

    if (pResource->GetMapMode() != D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      return D3D_OK;

    if (pResource->SetMapFlags(0) & D3DLOCK_READONLY)
      return D3D_OK;

    FlushImplicit(FALSE);

    auto dstBuffer = pResource->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL);
    auto srcBuffer = pResource->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_STAGING);

    EmitCs([
      cDstSlice = dstBuffer,
      cSrcSlice = srcBuffer
    ] (DxvkContext * ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset(),
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        cSrcSlice.length());
    });

    return D3D_OK;
  }

  void Direct3DDevice9Ex::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
    m_csIsBusy = true;
  }


  void Direct3DDevice9Ex::FlushImplicit(BOOL StrongHint) {
    // Flush only if the GPU is about to go idle, in
    // order to keep the number of submissions low.
    if (StrongHint || m_dxvkDevice->pendingSubmissions() <= MaxPendingSubmits) {
      auto now = std::chrono::high_resolution_clock::now();

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(MinFlushIntervalUs))
        Flush();
    }
  }

  void Direct3DDevice9Ex::SynchronizeCsThread() {
    auto lock = LockDevice();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();

    m_csThread.synchronize();
  }

  void Direct3DDevice9Ex::SetupFPU() {
    // Should match d3d9 float behaviour.

#if defined(_MSC_VER)
    // For MSVC we can use these cross arch and platform funcs to set the FPU.
    // This will work on any platform, x86, x64, ARM, etc.

    // Clear exceptions.
    _clearfp();

    // Disable exceptions
    _controlfp(_MCW_EM, _MCW_EM);

    // Use 24 bit precision
    _controlfp(_PC_24, _MCW_PC);

    // Round to nearest
    _controlfp(_RC_NEAR, _MCW_RC);
#elif (defined(__GNUC__) || defined(__MINGW32__)) && (defined(__i386__) || defined(__x86_64__) || defined(__ia64))
    // For GCC/MinGW we can use inline asm to set it.
    // This only works for x86 and x64 processors however.

    uint16_t control;

    // Get current control word.
    __asm__ __volatile__("fnstcw %0" : "=m" (*&control));

    // Clear existing settings.
    control &= 0xF0C0;

    // Disable exceptions
    // Use 24 bit precision
    // Round to nearest
    control |= 0x003F;

    // Set new control word.
    __asm__ __volatile__("fldcw %0" : : "m" (*&control));
#else
    Logger::warn("Direct3DDevice9Ex::SetupFPU: not supported on this arch.");
#endif
  }

  void Direct3DDevice9Ex::CreateConstantBuffers() {
    DxvkBufferCreateInfo info;
    info.size   = D3D9ConstantSets::SetSize;
    info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.access = VK_ACCESS_UNIFORM_READ_BIT;
    info.stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    m_vsConst.buffer = m_dxvkDevice->createBuffer(info, memoryFlags);
    m_psConst.buffer = m_dxvkDevice->createBuffer(info, memoryFlags);

    info.size = caps::MaxClipPlanes * sizeof(D3D9ClipPlane);
    m_vsClipPlanes = m_dxvkDevice->createBuffer(info, memoryFlags);
    
    info.size = sizeof(D3D9RenderStateInfo);
    m_psRenderStates = m_dxvkDevice->createBuffer(info, memoryFlags);

    auto BindConstantBuffer = [this](
      DxsoProgramType     shaderStage,
      Rc<DxvkBuffer>      buffer,
      DxsoConstantBuffers cbuffer) {
      const uint32_t slotId = computeResourceSlotId(
        shaderStage, DxsoBindingType::ConstantBuffer,
        cbuffer);

      EmitCs([
        cSlotId = slotId,
        cBuffer = buffer
      ] (DxvkContext* ctx) {
        ctx->bindResourceBuffer(cSlotId,
          DxvkBufferSlice(cBuffer, 0, cBuffer->info().size));
      });
    };

    BindConstantBuffer(DxsoProgramType::VertexShader, m_vsConst.buffer, DxsoConstantBuffers::VSConstantBuffer);
    BindConstantBuffer(DxsoProgramType::PixelShader,  m_psConst.buffer, DxsoConstantBuffers::PSConstantBuffer);
    BindConstantBuffer(DxsoProgramType::VertexShader, m_vsClipPlanes,   DxsoConstantBuffers::VSClipPlanes);
    BindConstantBuffer(DxsoProgramType::PixelShader,  m_psRenderStates, DxsoConstantBuffers::PSRenderStates);
    
    m_flags.set(
      D3D9DeviceFlag::DirtyClipPlanes,
      D3D9DeviceFlag::DirtyRenderStateBuffer);
  }

  void Direct3DDevice9Ex::UploadConstants(DxsoProgramType ShaderStage) {
    D3D9ConstantSets& constSet =
      ShaderStage == DxsoProgramType::VertexShader
        ? m_vsConst
        : m_psConst;

    if (!constSet.dirty)
      return;

    constSet.dirty = false;

    const void* constantData =
      ShaderStage == DxsoProgramType::VertexShader
      ? &m_state.vsConsts
      : &m_state.psConsts;

    DxvkBufferSliceHandle slice = constSet.buffer->allocSlice();

    std::memcpy(slice.mapPtr, constantData, D3D9ConstantSets::SetSize);

    EmitCs([
      cBuffer = constSet.buffer,
      cSlice  = slice
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cSlice);
    });
  }
  
  void Direct3DDevice9Ex::UpdateClipPlanes() {
    m_flags.clr(D3D9DeviceFlag::DirtyClipPlanes);
    
    auto slice = m_vsClipPlanes->allocSlice();
    auto dst = reinterpret_cast<D3D9ClipPlane*>(slice.mapPtr);
    
    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      dst[i] = (m_state.renderStates[D3DRS_CLIPPLANEENABLE] & (1 << i))
        ? m_state.clipPlanes[i]
        : D3D9ClipPlane();
    }
    
    EmitCs([
      cBuffer = m_vsClipPlanes,
      cSlice  = slice
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cSlice);
    });
  }
  
  void Direct3DDevice9Ex::UpdateRenderStateBuffer() {
    m_flags.clr(D3D9DeviceFlag::DirtyRenderStateBuffer);
    
    auto& rs = m_state.renderStates;
    
    auto slice = m_psRenderStates->allocSlice();
    auto dst = reinterpret_cast<D3D9RenderStateInfo*>(slice.mapPtr);
    
    dst->alphaRef = float(rs[D3DRS_ALPHAREF]) / 255.0f;
    
    EmitCs([
      cBuffer = m_psRenderStates,
      cSlice  = slice
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cSlice);
    });
  }

  void Direct3DDevice9Ex::Flush() {
    auto lock = LockDevice();

    m_initializer->Flush();

    if (m_csIsBusy || m_csChunk->commandCount() != 0) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([](DxvkContext* ctx) {
        ctx->flushCommandList();
      });

      FlushCsChunk();

      // Reset flush timer used for implicit flushes
      m_lastFlush = std::chrono::high_resolution_clock::now();
      m_csIsBusy = false;
    }
  }

  void Direct3DDevice9Ex::BindFramebuffer() {
    DxvkRenderTargets attachments;

    bool srgb = m_state.renderStates[D3DRS_SRGBWRITEENABLE] != FALSE;

    // D3D9 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.renderTargets.size(); i++) {
      if (m_state.renderTargets.at(i) != nullptr) {
        attachments.color[i] = {
          m_state.renderTargets.at(i)->GetRenderTargetView(srgb),
          m_state.renderTargets.at(i)->GetRenderTargetLayout() };
      }
    }

    if (m_state.depthStencil != nullptr) {
      attachments.depth = {
        m_state.depthStencil->GetDepthStencilView(),
        m_state.depthStencil->GetDepthLayout() };
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments)
    ] (DxvkContext * ctx) {
        ctx->bindRenderTargets(cAttachments, false);
      });
  }

  void Direct3DDevice9Ex::BindViewportAndScissor() {
    m_flags.clr(D3D9DeviceFlag::DirtyViewportScissor);

    VkViewport viewport;
    VkRect2D scissor;

    // D3D9's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    const D3DVIEWPORT9& vp = m_state.viewport;

    // Correctness Factor
    float cf = m_d3d9Options.halfPixelOffset ? 0.5f : 0.0f;

    viewport = VkViewport{
      float(vp.X)     + cf,    float(vp.Height + vp.Y) + cf,
      float(vp.Width) + cf,   -float(vp.Height)        - cf,
      vp.MinZ,           vp.MaxZ,
    };

    // Scissor rectangles. Vulkan does not provide an easy way
    // to disable the scissor test, so we'll have to set scissor
    // rects that are at least as large as the framebuffer.
    bool enableScissorTest = m_state.renderStates[D3DRS_SCISSORTESTENABLE] != FALSE;

    // TODO: Hook up scissor rect enabled when we get state in.

    if (enableScissorTest) {
      RECT sr = m_state.scissorRect;

      VkOffset2D srPosA;
      srPosA.x = std::max<int32_t>(0, sr.left);
      srPosA.y = std::max<int32_t>(0, sr.top);

      VkOffset2D srPosB;
      srPosB.x = std::max<int32_t>(srPosA.x, sr.right);
      srPosB.y = std::max<int32_t>(srPosA.y, sr.bottom);

      VkExtent2D srSize;
      srSize.width = uint32_t(srPosB.x - srPosA.x);
      srSize.height = uint32_t(srPosB.y - srPosA.y);

      scissor = VkRect2D{ srPosA, srSize };
    }
    else {
      scissor = VkRect2D{
        VkOffset2D { 0, 0 },
        VkExtent2D {
          16383,
          16383 } };
    }

    EmitCs([
      cViewport = viewport,
      cScissor = scissor
    ] (DxvkContext * ctx) {
      ctx->setViewports(
        1,
        &cViewport,
        &cScissor);
    });
  }

  void Direct3DDevice9Ex::BindBlendState() {
    m_flags.clr(D3D9DeviceFlag::DirtyBlendState);

    auto& state = m_state.renderStates;

    bool separateAlpha  = state[D3DRS_SEPARATEALPHABLENDENABLE] != FALSE;

    DxvkBlendMode baseMode;
    baseMode.enableBlending = state[D3DRS_ALPHABLENDENABLE] != FALSE;

    baseMode.colorSrcFactor = DecodeBlendFactor(D3DBLEND  ( state[D3DRS_SRCBLEND] ),  false);
    baseMode.colorDstFactor = DecodeBlendFactor(D3DBLEND  ( state[D3DRS_DESTBLEND] ), false);
    baseMode.colorBlendOp   = DecodeBlendOp    (D3DBLENDOP( state[D3DRS_BLENDOP] ));

    baseMode.alphaSrcFactor = DecodeBlendFactor(separateAlpha ? D3DBLEND  ( state[D3DRS_SRCBLENDALPHA] )  : D3DBLEND_ONE, true);
    baseMode.alphaDstFactor = DecodeBlendFactor(separateAlpha ? D3DBLEND  ( state[D3DRS_DESTBLENDALPHA] ) : D3DBLEND_ONE, true);
    baseMode.alphaBlendOp   = DecodeBlendOp    (separateAlpha ? D3DBLENDOP( state[D3DRS_BLENDOPALPHA] )   : D3DBLENDOP_ADD);

    std::array<DxvkBlendMode, 4> modes;
    for (uint32_t i = 0; i < modes.size(); i++) {
      auto& mode = modes[i];
      mode = baseMode;

      // These state indices are non-contiguous... Of course.
      static const std::array<D3DRENDERSTATETYPE, 4> colorWriteIndices = {
        D3DRS_COLORWRITEENABLE,
        D3DRS_COLORWRITEENABLE1,
        D3DRS_COLORWRITEENABLE2,
        D3DRS_COLORWRITEENABLE3
      };

      mode.writeMask = state[colorWriteIndices[i]];
    }

    EmitCs([
      cModes = modes
    ](DxvkContext* ctx) {
      for (uint32_t i = 0; i < cModes.size(); i++)
        ctx->setBlendMode(i, cModes.at(i));
    });
  }

  void Direct3DDevice9Ex::BindBlendFactor() {
    DxvkBlendConstants blendConstants;
    DecodeD3DCOLOR(
      D3DCOLOR(m_state.renderStates[D3DRS_BLENDFACTOR]),
      reinterpret_cast<float*>(&blendConstants));

    EmitCs([
      cBlendConstants = blendConstants
    ](DxvkContext* ctx) {
      ctx->setBlendConstants(cBlendConstants);
    });
  }

  void Direct3DDevice9Ex::BindDepthStencilState() {
    m_flags.clr(D3D9DeviceFlag::DirtyDepthStencilState);

    auto& rs = m_state.renderStates;

    bool stencil            = rs[D3DRS_STENCILENABLE] != FALSE;
    bool twoSidedStencil    = stencil && (rs[D3DRS_TWOSIDEDSTENCILMODE] != FALSE);

    DxvkDepthStencilState state;
    state.enableDepthTest   = rs[D3DRS_ZENABLE]       != FALSE;
    state.enableDepthWrite  = rs[D3DRS_ZWRITEENABLE]  != FALSE;
    state.enableStencilTest = stencil;
    state.depthCompareOp    = DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ZFUNC]));

    state.stencilOpFront.failOp      = stencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILFAIL]))  : VK_STENCIL_OP_KEEP;
    state.stencilOpFront.passOp      = stencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILPASS]))  : VK_STENCIL_OP_KEEP;
    state.stencilOpFront.depthFailOp = stencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILZFAIL])) : VK_STENCIL_OP_KEEP;
    state.stencilOpFront.compareOp   = stencil ? DecodeCompareOp(D3DCMPFUNC  (rs[D3DRS_STENCILFUNC]))  : VK_COMPARE_OP_ALWAYS;
    state.stencilOpFront.compareMask = uint32_t(rs[D3DRS_STENCILMASK]);
    state.stencilOpFront.writeMask   = uint32_t(rs[D3DRS_STENCILWRITEMASK]);
    state.stencilOpFront.reference   = 0;

    state.stencilOpBack.failOp      = twoSidedStencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILFAIL]))  : VK_STENCIL_OP_KEEP;
    state.stencilOpBack.passOp      = twoSidedStencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILPASS]))  : VK_STENCIL_OP_KEEP;
    state.stencilOpBack.depthFailOp = twoSidedStencil ? DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILZFAIL])) : VK_STENCIL_OP_KEEP;
    state.stencilOpBack.compareOp   = twoSidedStencil ? DecodeCompareOp(D3DCMPFUNC  (rs[D3DRS_CCW_STENCILFUNC]))  : VK_COMPARE_OP_ALWAYS;
    state.stencilOpBack.compareMask = state.stencilOpFront.compareMask;
    state.stencilOpBack.writeMask   = state.stencilOpFront.writeMask;
    state.stencilOpBack.reference   = 0;

    EmitCs([
      cState = state
    ](DxvkContext* ctx) {
      ctx->setDepthStencilState(cState);
    });
  }

  void Direct3DDevice9Ex::BindRasterizerState() {
    m_flags.clr(D3D9DeviceFlag::DirtyRasterizerState);

    // TODO: Can we get a specific non-magic number in Vulkan for this based on device/adapter?
    constexpr float DepthBiasFactor = float(1 << 23);

    auto& rs = m_state.renderStates;

    float depthBias            = bit::cast<float>(rs[D3DRS_DEPTHBIAS]) * DepthBiasFactor;
    float slopeScaledDepthBias = bit::cast<float>(rs[D3DRS_SLOPESCALEDEPTHBIAS]);

    DxvkRasterizerState state;
    state.cullMode        = DecodeCullMode(D3DCULL(rs[D3DRS_CULLMODE]));
    state.depthBiasEnable = depthBias != 0.0f || slopeScaledDepthBias != 0.0f;
    state.depthClipEnable = true;
    state.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));
    state.sampleCount     = 0;

    DxvkDepthBias biases;
    biases.depthBiasConstant = depthBias;
    biases.depthBiasSlope    = slopeScaledDepthBias;
    biases.depthBiasClamp    = 0.0f;

    EmitCs([
      cState  = state,
      cBiases = biases
    ](DxvkContext* ctx) {
      ctx->setRasterizerState(cState);
      ctx->setDepthBias(cBiases);
    });
  }

  void Direct3DDevice9Ex::BindExtraState() {
    m_flags.clr(D3D9DeviceFlag::DirtyExtraState);
    
    auto& rs = m_state.renderStates;
    
    VkCompareOp alphaOp = rs[D3DRS_ALPHATESTENABLE]
      ? DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ALPHAFUNC]))
      : VK_COMPARE_OP_ALWAYS;
    
    EmitCs([cAlphaOp = alphaOp] (DxvkContext* ctx) {
      DxvkExtraState xs;
      xs.alphaCompareOp = cAlphaOp;
      ctx->setExtraState(xs);
    });
  }
  
  void Direct3DDevice9Ex::BindDepthStencilRefrence() {
    auto& rs = m_state.renderStates;

    uint32_t ref = uint32_t(rs[D3DRS_STENCILREF]);

    EmitCs([
      cRef = ref
    ](DxvkContext* ctx) {
      ctx->setStencilReference(cRef);
    });
  }

  Rc<DxvkSampler> Direct3DDevice9Ex::CreateSampler(DWORD Sampler) {
    auto& state = m_state.samplerStates[Sampler];

    D3D9SamplerKey key;
    key.AddressU      = D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSU]);
    key.AddressV      = D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSV]);
    key.AddressW      = D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSW]);
    key.MagFilter     = D3DTEXTUREFILTERTYPE(state[D3DSAMP_MAGFILTER]);
    key.MinFilter     = D3DTEXTUREFILTERTYPE(state[D3DSAMP_MINFILTER]);
    key.MipFilter     = D3DTEXTUREFILTERTYPE(state[D3DSAMP_MIPFILTER]);
    key.MaxAnisotropy = state[D3DSAMP_MAXANISOTROPY];
    key.MipmapLodBias = bit::cast<float>(state[D3DSAMP_MIPMAPLODBIAS]);
    key.MaxMipLevel   = state[D3DSAMP_MAXMIPLEVEL];
    key.BorderColor   = D3DCOLOR(state[D3DSAMP_BORDERCOLOR]);

    auto pair = m_samplers.find(key);
    if (pair != m_samplers.end())
      return pair->second;

    auto mipFilter = DecodeMipFilter(key.MipFilter);

    DxvkSamplerCreateInfo info;
    info.addressModeU   = DecodeAddressMode(key.AddressU);
    info.addressModeV   = DecodeAddressMode(key.AddressV);
    info.addressModeW   = DecodeAddressMode(key.AddressW);
    info.compareToDepth = VK_FALSE;
    info.compareOp      = VK_COMPARE_OP_NEVER;
    info.magFilter      = DecodeFilter   (key.MagFilter);
    info.minFilter      = DecodeFilter   (key.MinFilter);
    info.mipmapMode     = mipFilter.MipFilter;
    info.maxAnisotropy  = std::clamp(float(key.MaxAnisotropy), 1.0f, 16.0f);
    info.useAnisotropy  = IsAnisotropic(key.MinFilter)
                       || IsAnisotropic(key.MagFilter);
    info.mipmapLodBias  = key.MipmapLodBias;
    info.mipmapLodMin   = mipFilter.MipsEnabled ? float(key.MaxMipLevel) : 0;
    info.mipmapLodMax   = mipFilter.MipsEnabled ? FLT_MAX                : 0;
    info.usePixelCoord  = VK_FALSE;
    DecodeD3DCOLOR(key.BorderColor, reinterpret_cast<float*>(&info.borderColor));

    Rc<DxvkSampler> sampler = m_dxvkDevice->createSampler(info);

    m_samplers.insert(std::make_pair(key, sampler));
    return sampler;
  }

  void Direct3DDevice9Ex::BindSampler(DWORD Sampler) {
    Rc<DxvkSampler> sampler = CreateSampler(Sampler);

    auto samplerInfo = RemapStateSamplerShader(Sampler);

    const uint32_t slotId = computeResourceSlotId(
      samplerInfo.first, DxsoBindingType::Image,
      samplerInfo.second);

    EmitCs([
      cSlot    = slotId,
      cSampler = sampler
    ] (DxvkContext* ctx) {
      ctx->bindResourceSampler(cSlot, cSampler);
    });
  }

  void Direct3DDevice9Ex::BindTexture(DWORD StateSampler) {
    auto shaderSampler = RemapStateSamplerShader(StateSampler);
    uint32_t slot      = computeResourceSlotId(shaderSampler.first, DxsoBindingType::Image, uint32_t(shaderSampler.second));

    const bool srgb = m_state.samplerStates[StateSampler][D3DSAMP_SRGBTEXTURE] != FALSE;

    Direct3DCommonTexture9* commonTex = GetCommonTexture(m_state.textures[StateSampler]);

    EmitCs([
      cSlot      = slot,
      cImageView = commonTex != nullptr
                 ? commonTex->GetImageView(srgb)
                 : nullptr // TODO: SRGB-ness
    ](DxvkContext* ctx) {
      ctx->bindResourceView(cSlot, cImageView, nullptr);
    });
  }

  void Direct3DDevice9Ex::UndirtySamplers() {
    for (uint32_t i = 0; i < 21; i++) {
      if (m_dirtySamplerStates & (1u << i))
        BindSampler(i);
    }

    m_dirtySamplerStates = 0;
  }

  D3D9DrawInfo Direct3DDevice9Ex::GenerateDrawInfo(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount) {
    D3D9DrawInfo drawInfo;

    drawInfo.iaState     = InputAssemblyState(PrimitiveType);
    drawInfo.vertexCount = VertexCount(PrimitiveType, PrimitiveCount);

    drawInfo.instanceCount = 1;
    if (m_instancedData & m_streamUsageMask)
      drawInfo.instanceCount = std::max<uint32_t>(m_state.streamFreq[0] & 0x7FFFFF, 1);

    return drawInfo;
  }

  void Direct3DDevice9Ex::PrepareDraw(bool up) {
    if (m_flags.test(D3D9DeviceFlag::DirtyViewportScissor))
      BindViewportAndScissor();

    UndirtySamplers();

    if (m_flags.test(D3D9DeviceFlag::DirtyBlendState))
      BindBlendState();
    
    if (m_flags.test(D3D9DeviceFlag::DirtyDepthStencilState))
      BindDepthStencilState();

    if (m_flags.test(D3D9DeviceFlag::DirtyRasterizerState))
      BindRasterizerState();
    
    if (m_flags.test(D3D9DeviceFlag::DirtyExtraState))
      BindExtraState();
    
    if (m_flags.test(D3D9DeviceFlag::DirtyClipPlanes))
      UpdateClipPlanes();

    if (m_flags.test(D3D9DeviceFlag::DirtyRenderStateBuffer))
      UpdateRenderStateBuffer();

    if (m_flags.test(D3D9DeviceFlag::DirtyInputLayout))
      BindInputLayout();

    if (!up && m_flags.test(D3D9DeviceFlag::UpDirtiedVertices)) {
      m_flags.clr(D3D9DeviceFlag::UpDirtiedVertices);
      if (m_state.vertexBuffers[0].vertexBuffer != nullptr)
        BindVertexBuffer(
          0,
          m_state.vertexBuffers[0].vertexBuffer,
          m_state.vertexBuffers[0].offset,
          m_state.vertexBuffers[0].stride);
    }

    if (!up && m_flags.test(D3D9DeviceFlag::UpDirtiedIndices)) {
      m_flags.clr(D3D9DeviceFlag::UpDirtiedIndices);
      BindIndices();
    }

    UpdateConstants();
  }

  void Direct3DDevice9Ex::BindShader(
        DxsoProgramType                   ShaderStage,
  const D3D9CommonShader*                 pShaderModule) {
    EmitCs([
      cStage  = GetShaderStage(ShaderStage),
      cShader = pShaderModule != nullptr
        ? pShaderModule->GetShader()
        : nullptr
    ] (DxvkContext * ctx) {
      ctx->bindShader(cStage, cShader);
    });
  }

  void Direct3DDevice9Ex::BindInputLayout() {
    m_flags.clr(D3D9DeviceFlag::DirtyInputLayout);

    m_streamUsageMask = 0;

    if (m_state.vertexShader == nullptr || m_state.vertexDecl == nullptr) {
      EmitCs([](DxvkContext* ctx) {
        ctx->setInputLayout(0, nullptr, 0, nullptr);
      });
    }
    else {
      std::array<DxvkVertexAttribute, caps::InputRegisterCount> attrList;
      std::array<DxvkVertexBinding,   caps::InputRegisterCount> bindList;

      uint32_t attrMask = 0;
      uint32_t bindMask = 0;

      const auto* commonShader = m_state.vertexShader->GetCommonShader();
      const auto& shaderDecls  = commonShader->GetDeclarations();
      const auto  slots        = commonShader->GetShader()->interfaceSlots();
      const auto& elements     = m_state.vertexDecl->GetElements();

      for (uint32_t i = 0; i < shaderDecls.size(); i++) {
        if (!(slots.inputSlots & (1u << i)))
          continue;

        const auto& decl = shaderDecls[i];

        DxvkVertexAttribute attrib;
        attrib.location = i;
        attrib.binding  = 0;
        attrib.format   = VK_FORMAT_R8_UNORM;
        attrib.offset   = 0;

        for (const auto& element : elements) {
          DxsoSemantic elementSemantic = { static_cast<DxsoUsage>(element.Usage), element.UsageIndex };

          if (elementSemantic == decl.semantic) {
            attrib.binding = uint32_t(element.Stream);
            attrib.format  = LookupDecltype(D3DDECLTYPE(element.Type));
            attrib.offset  = element.Offset;

            m_streamUsageMask |= 1u << attrib.binding;

            break;
          }
        }

        attrList.at(i) = attrib;

        DxvkVertexBinding binding;
        binding.binding     = attrib.binding;

        uint32_t instanceData = m_state.streamFreq[binding.binding];
        if (instanceData & D3DSTREAMSOURCE_INSTANCEDATA) {
          binding.fetchRate = instanceData & 0x7FFFFF; // Remove instance packed-in flags in the data.
          binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        }
        else {
          binding.fetchRate = 0;
          binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        }

        // Check if the binding was already defined.
        bool bindingDefined = false;

        for (uint32_t j = 0; j < i; j++) {
          if (!(slots.inputSlots & (1u << j)))
            continue;

          uint32_t bindingId = attrList.at(j).binding;

          if (binding.binding == bindingId) {
            bindingDefined = true;
          }
        }

        if (!bindingDefined)
          bindList.at(binding.binding) = binding;

        attrMask |= 1u << i;
        bindMask |= 1u << binding.binding;
      }

      // Compact the attribute and binding lists to filter
      // out attributes and bindings not used by the shader
      uint32_t attrCount    = CompactSparseList(attrList.data(), attrMask);
      uint32_t bindCount    = CompactSparseList(bindList.data(), bindMask);
      
      EmitCs([
        cAttrCount  = attrCount,
        cAttributes = attrList,
        cBindCount  = bindCount,
        cBindings   = bindList
      ](DxvkContext * ctx) {
        ctx->setInputLayout(
          cAttrCount,
          cAttributes.data(),
          cBindCount,
          cBindings.data());
      });
    }
  }

  void Direct3DDevice9Ex::BindVertexBuffer(
        UINT                              Slot,
        Direct3DVertexBuffer9*            pBuffer,
        UINT                              Offset,
        UINT                              Stride) {
    EmitCs([
      cSlotId       = Slot,
      cBufferSlice  = pBuffer != nullptr ? 
        pBuffer->GetCommonBuffer()->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL, Offset) 
      : DxvkBufferSlice(),
      cStride       = pBuffer != nullptr ? Stride                          : 0
    ] (DxvkContext* ctx) {
      ctx->bindVertexBuffer(cSlotId, cBufferSlice, cStride);
    });
  }

  void Direct3DDevice9Ex::BindIndices() {
    Direct3DCommonBuffer9* buffer = m_state.indices != nullptr
                                  ? m_state.indices->GetCommonBuffer().ptr()
                                  : nullptr;

    D3D9Format format = buffer != nullptr
                      ? buffer->Desc()->Format
                      : D3D9Format::INDEX32;

    const VkIndexType indexType = DecodeIndexType(format);

    EmitCs([
      cBufferSlice = buffer != nullptr ? buffer->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL) : DxvkBufferSlice(),
      cIndexType   = indexType
    ](DxvkContext* ctx) {
      ctx->bindIndexBuffer(cBufferSlice, cIndexType);
    });
  }

  void Direct3DDevice9Ex::Begin(D3D9Query* pQuery) {
    auto lock = LockDevice();

    Com<D3D9Query> queryPtr = pQuery;

    EmitCs([queryPtr](DxvkContext* ctx) {
      queryPtr->Begin(ctx);
    });
  }

  void Direct3DDevice9Ex::End(D3D9Query* pQuery) {
    auto lock = LockDevice();

    Com<D3D9Query> queryPtr = pQuery;

    EmitCs([queryPtr](DxvkContext* ctx) {
      queryPtr->End(ctx);
    });

    if (queryPtr->GetType() == D3DQUERYTYPE_EVENT)
      FlushImplicit(TRUE);
  }

  void Direct3DDevice9Ex::SetVertexBoolBitfield(uint32_t mask, uint32_t bits) {
    m_state.vsConsts.hardware.boolBitfield &= ~mask;
    m_state.vsConsts.hardware.boolBitfield |= bits & mask;

    m_vsConst.dirty = true;
  }

  void Direct3DDevice9Ex::SetPixelBoolBitfield(uint32_t mask, uint32_t bits) {
    m_state.psConsts.hardware.boolBitfield &= ~mask;
    m_state.psConsts.hardware.boolBitfield |= bits & mask;

    m_psConst.dirty = true;
  }

  HRESULT Direct3DDevice9Ex::CreateShaderModule(
        D3D9CommonShader*     pShaderModule,
        VkShaderStageFlagBits ShaderStage,
  const DWORD*                pShaderBytecode,
  const DxsoModuleInfo*       pModuleInfo) {
    try {
      *pShaderModule = m_shaderModules->GetShaderModule(this,
        ShaderStage, pModuleInfo, pShaderBytecode);

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  template <
    DxsoProgramType  ProgramType,
    D3D9ConstantType ConstantType,
    typename         T>
    HRESULT Direct3DDevice9Ex::SetShaderConstants(
      UINT  StartRegister,
      const T* pConstantData,
      UINT  Count)
    {
      constexpr uint32_t regCountHardware = DetermineRegCount(ConstantType, false);
      constexpr uint32_t regCountSoftware = DetermineRegCount(ConstantType, true);

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

      if (unlikely(ShouldRecord()))
        return m_recorder->SetShaderConstants<
          ProgramType,
          ConstantType,
          T>(
            StartRegister,
            pConstantData,
            Count);

      bool& dirtyFlag = ProgramType == DxsoProgramType::VertexShader
        ? m_vsConst.dirty
        : m_psConst.dirty;

      dirtyFlag = true;

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

}
