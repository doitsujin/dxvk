#include "d3d9_device.h"

#include "d3d9_interface.h"
#include "d3d9_swapchain.h"
#include "d3d9_caps.h"
#include "d3d9_util.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"
#include "d3d9_vertex_declaration.h"
#include "d3d9_shader.h"
#include "d3d9_query.h"
#include "d3d9_stateblock.h"
#include "d3d9_monitor.h"
#include "d3d9_spec_constants.h"
#include "d3d9_names.h"
#include "d3d9_format_helpers.h"

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

  D3D9DeviceEx::D3D9DeviceEx(
          D3D9InterfaceEx*       pParent,
          D3D9Adapter*           pAdapter,
          D3DDEVTYPE             DeviceType,
          HWND                   hFocusWindow,
          DWORD                  BehaviorFlags,
          Rc<DxvkDevice>         dxvkDevice)
    : m_adapter        ( pAdapter )
    , m_dxvkDevice     ( dxvkDevice )
    , m_csThread       ( dxvkDevice->createContext() )
    , m_csChunk        ( AllocCsChunk() )
    , m_parent         ( pParent )
    , m_deviceType     ( DeviceType )
    , m_window         ( hFocusWindow )
    , m_behaviorFlags  ( BehaviorFlags )
    , m_multithread    ( BehaviorFlags & D3DCREATE_MULTITHREADED )
    , m_shaderModules  ( new D3D9ShaderModuleSet )
    , m_d3d9Options    ( dxvkDevice, pParent->GetInstance()->config() )
    , m_isSWVP         ( (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) ? TRUE : FALSE ) {
    // If we can SWVP, then we use an extended constant set
    // as SWVP has many more slots available than HWVP.
    bool canSWVP = CanSWVP();
    DetermineConstantLayouts(canSWVP);

    if (canSWVP)
      Logger::info("D3D9DeviceEx: Using extended constant set for software vertex processing.");

    m_initializer      = new D3D9Initializer(m_dxvkDevice);
    m_converter        = new D3D9FormatHelper(m_dxvkDevice);

    EmitCs([
      cDevice = m_dxvkDevice
    ] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());

      DxvkLogicOpState loState;
      loState.enableLogicOp = VK_FALSE;
      loState.logicOp       = VK_LOGIC_OP_CLEAR;
      ctx->setLogicOpState(loState);
    });

    if (!(BehaviorFlags & D3DCREATE_FPU_PRESERVE))
      SetupFPU();

    m_dxsoOptions = DxsoOptions(this, m_d3d9Options);

    CreateConstantBuffers();

    m_availableMemory = DetermineInitialTextureMemory();
  }


  D3D9DeviceEx::~D3D9DeviceEx() {
    Flush();
    SynchronizeCsThread();

    delete m_initializer;
    delete m_converter;

    m_dxvkDevice->waitForIdle(); // Sync Device
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    bool extended = m_parent->IsExtended()
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

    Logger::warn("D3D9DeviceEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::TestCooperativeLevel() {
    // Equivelant of D3D11/DXGI present tests. We can always present.
    return D3D_OK;
  }


  UINT    STDMETHODCALLTYPE D3D9DeviceEx::GetAvailableTextureMem() {
    // This is not meant to be accurate.
    // The values are also wildly incorrect in d3d9... But some games rely
    // on this inaccurate value...

    // Clamp to megabyte range, as per spec.
    constexpr UINT range = 0xfff00000;

    // Can't have negative memory!
    int64_t memory = std::max<int64_t>(m_availableMemory.load(), 0);

    return UINT(memory) & range;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::EvictManagedResources() {
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetDirect3D(IDirect3D9** ppD3D9) {
    if (ppD3D9 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D9 = m_parent.ref();
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetDeviceCaps(D3DCAPS9* pCaps) {
    return m_adapter->GetDeviceCaps(m_deviceType, pCaps);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->GetDisplayMode(pMode);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    if (pParameters == nullptr)
      return D3DERR_INVALIDCALL;

    pParameters->AdapterOrdinal = m_adapter->GetOrdinal();
    pParameters->BehaviorFlags  = m_behaviorFlags;
    pParameters->DeviceType     = m_deviceType;
    pParameters->hFocusWindow   = m_window;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetCursorProperties(
          UINT               XHotSpot,
          UINT               YHotSpot,
          IDirect3DSurface9* pCursorBitmap) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pCursorBitmap == nullptr))
      return D3DERR_INVALIDCALL;

    auto* cursorTex = GetCommonTexture(pCursorBitmap);
    if (unlikely(cursorTex->Desc()->Format != D3D9Format::A8R8G8B8))
      return D3DERR_INVALIDCALL;

    uint32_t inputWidth  = cursorTex->Desc()->Width;
    uint32_t inputHeight = cursorTex->Desc()->Height;

    // Always use a hardware cursor when windowed.
    bool hwCursor  = m_presentParams.Windowed;

    // Always use a hardware cursor w/h <= 32 px
    hwCursor |= inputWidth  <= HardwareCursorWidth
             || inputHeight <= HardwareCursorHeight;

    if (hwCursor) {
      D3DLOCKED_BOX lockedBox;
      HRESULT hr = LockImage(cursorTex, 0, 0, &lockedBox, nullptr, D3DLOCK_READONLY);
      if (FAILED(hr))
        return hr;

      const uint8_t* data  = reinterpret_cast<const uint8_t*>(lockedBox.pBits);

      // Windows works with a stride of 128, lets respect that.
      // Copy data to the bitmap...
      CursorBitmap bitmap = { 0 };
      size_t copyPitch = std::min<size_t>(
        HardwareCursorPitch,
        inputWidth * inputHeight * HardwareCursorFormatSize);

      for (uint32_t h = 0; h < HardwareCursorHeight; h++)
        std::memcpy(&bitmap[h * HardwareCursorPitch], &data[h * lockedBox.RowPitch], copyPitch);

      UnlockImage(cursorTex, 0, 0);

      // Set this as our cursor.
      return m_cursor.SetHardwareCursor(XHotSpot, YHotSpot, bitmap);
    }

    // Software Cursor...
    Logger::warn("D3D9DeviceEx::SetCursorProperties: Software cursor not implemented.");
    return D3D_OK;
  }


  void    STDMETHODCALLTYPE D3D9DeviceEx::SetCursorPosition(int X, int Y, DWORD Flags) {
    D3D9DeviceLock lock = LockDevice();

    // I was not able to find an instance
    // where the cursor update was not immediate.

    // Fullscreen + Windowed seem to have the same
    // behaviour here.

    // Hence we ignore the flag D3DCURSOR_IMMEDIATE_UPDATE.

    m_cursor.UpdateCursor(X, Y);
  }


  BOOL    STDMETHODCALLTYPE D3D9DeviceEx::ShowCursor(BOOL bShow) {
    D3D9DeviceLock lock = LockDevice();

    return m_cursor.ShowCursor(bShow);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateAdditionalSwapChain(
          D3DPRESENT_PARAMETERS* pPresentationParameters,
          IDirect3DSwapChain9**  ppSwapChain) {
    return CreateAdditionalSwapChainEx(pPresentationParameters, nullptr, ppSwapChain);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(pSwapChain);

    if (unlikely(pSwapChain == nullptr))
      return D3DERR_INVALIDCALL;

    // This only returns the implicit swapchain...

    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    *pSwapChain = static_cast<IDirect3DSwapChain9*>(m_implicitSwapchain.ref());

    return D3D_OK;
  }


  UINT    STDMETHODCALLTYPE D3D9DeviceEx::GetNumberOfSwapChains() {
    // This only counts the implicit swapchain...

    return 1;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    D3D9DeviceLock lock = LockDevice();

    HRESULT hr = ResetSwapChain(pPresentationParameters, nullptr);
    if (FAILED(hr))
      return hr;

    hr = ResetState(pPresentationParameters);
    if (FAILED(hr))
      return hr;

    Flush();
    SynchronizeCsThread();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::Present(
    const RECT*    pSourceRect,
    const RECT*    pDestRect,
          HWND     hDestWindowOverride,
    const RGNDATA* pDirtyRegion) {
    return PresentEx(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      0);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetBackBuffer(
          UINT                iSwapChain,
          UINT                iBackBuffer,
          D3DBACKBUFFER_TYPE  Type,
          IDirect3DSurface9** ppBackBuffer) {
    InitReturnPtr(ppBackBuffer);

    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->GetRasterStatus(pRasterStatus);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetDialogBoxMode(BOOL bEnableDialogs) {
    return m_implicitSwapchain->SetDialogBoxMode(bEnableDialogs);
  }


  void    STDMETHODCALLTYPE D3D9DeviceEx::SetGammaRamp(
          UINT          iSwapChain,
          DWORD         Flags,
    const D3DGAMMARAMP* pRamp) {
    if (unlikely(iSwapChain != 0))
      return;

    m_implicitSwapchain->SetGammaRamp(Flags, pRamp);
  }


  void    STDMETHODCALLTYPE D3D9DeviceEx::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    if (unlikely(iSwapChain != 0))
      return;

    m_implicitSwapchain->GetGammaRamp(pRamp);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateTexture(
          UINT                Width,
          UINT                Height,
          UINT                Levels,
          DWORD               Usage,
          D3DFORMAT           Format,
          D3DPOOL             Pool,
          IDirect3DTexture9** ppTexture,
          HANDLE*             pSharedHandle) {
    InitReturnPtr(ppTexture);

    if (unlikely(ppTexture == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.ArraySize          = 1;
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = FALSE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Texture2D> texture = new D3D9Texture2D(this, &desc);

      void* initialData = nullptr;

      if (Pool == D3DPOOL_SYSTEMMEM && Levels == 1 && pSharedHandle != nullptr)
        initialData = *(reinterpret_cast<void**>(pSharedHandle));

      m_initializer->InitTexture(texture->GetCommonTexture(), initialData);
      *ppTexture = texture.ref();

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateVolumeTexture(
          UINT                      Width,
          UINT                      Height,
          UINT                      Depth,
          UINT                      Levels,
          DWORD                     Usage,
          D3DFORMAT                 Format,
          D3DPOOL                   Pool,
          IDirect3DVolumeTexture9** ppVolumeTexture,
          HANDLE*                   pSharedHandle) {
    InitReturnPtr(ppVolumeTexture);

    if (unlikely(ppVolumeTexture == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = Depth;
    desc.ArraySize          = 1;
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = FALSE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Texture3D> texture = new D3D9Texture3D(this, &desc);
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppVolumeTexture = texture.ref();

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateCubeTexture(
          UINT                    EdgeLength,
          UINT                    Levels,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DCubeTexture9** ppCubeTexture,
          HANDLE*                 pSharedHandle) {
    InitReturnPtr(ppCubeTexture);

    if (unlikely(ppCubeTexture == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = EdgeLength;
    desc.Height             = EdgeLength;
    desc.Depth              = 1;
    desc.ArraySize          = 6; // A cube has 6 faces, wowwie!
    desc.MipLevels          = Levels;
    desc.Usage              = Usage;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = FALSE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9TextureCube> texture = new D3D9TextureCube(this, &desc);
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppCubeTexture = texture.ref();

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateVertexBuffer(
          UINT                     Length,
          DWORD                    Usage,
          DWORD                    FVF,
          D3DPOOL                  Pool,
          IDirect3DVertexBuffer9** ppVertexBuffer,
          HANDLE*                  pSharedHandle) {
    InitReturnPtr(ppVertexBuffer);

    if (unlikely(ppVertexBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = D3D9Format::VERTEXDATA;
    desc.FVF    = FVF;
    desc.Pool   = Pool;
    desc.Size   = Length;
    desc.Type   = D3DRTYPE_VERTEXBUFFER;
    desc.Usage  = Usage;

    if (FAILED(D3D9CommonBuffer::ValidateBufferProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9VertexBuffer> buffer = new D3D9VertexBuffer(this, &desc);
      m_initializer->InitBuffer(buffer->GetCommonBuffer());
      *ppVertexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateIndexBuffer(
          UINT                    Length,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DIndexBuffer9** ppIndexBuffer,
          HANDLE*                 pSharedHandle) {
    InitReturnPtr(ppIndexBuffer);

    if (unlikely(ppIndexBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = EnumerateFormat(Format);
    desc.Pool   = Pool;
    desc.Size   = Length;
    desc.Type   = D3DRTYPE_INDEXBUFFER;
    desc.Usage  = Usage;

    if (FAILED(D3D9CommonBuffer::ValidateBufferProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9IndexBuffer> buffer = new D3D9IndexBuffer(this, &desc);
      m_initializer->InitBuffer(buffer->GetCommonBuffer());
      *ppIndexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateRenderTarget(
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateDepthStencilSurface(
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::UpdateSurface(
          IDirect3DSurface9* pSourceSurface,
    const RECT*              pSourceRect,
          IDirect3DSurface9* pDestinationSurface,
    const POINT*             pDestPoint) {
    D3D9DeviceLock lock = LockDevice();

    D3D9Surface* src = static_cast<D3D9Surface*>(pSourceSurface);
    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestinationSurface);

    if (unlikely(src == nullptr || dst == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* srcTextureInfo = src->GetCommonTexture();
    D3D9CommonTexture* dstTextureInfo = dst->GetCommonTexture();

    if (unlikely(srcTextureInfo->Desc()->Pool != D3DPOOL_SYSTEMMEM || dstTextureInfo->Desc()->Pool != D3DPOOL_DEFAULT))
      return D3DERR_INVALIDCALL;

    if (unlikely(srcTextureInfo->Desc()->Format != dstTextureInfo->Desc()->Format))
      return D3DERR_INVALIDCALL;

    const DxvkFormatInfo* formatInfo = imageFormatInfo(dstTextureInfo->GetFormatMapping().FormatColor);

    VkOffset3D srcBlockOffset = { 0u, 0u, 0u };
    VkOffset3D dstOffset = { 0u, 0u, 0u };
    VkExtent3D texLevelExtent = srcTextureInfo->GetExtentMip(src->GetSubresource());
    VkExtent3D texLevelBlockCount = util::computeBlockCount(texLevelExtent, formatInfo->blockSize);

    VkExtent3D copyExtent = texLevelExtent;

    if (pSourceRect != nullptr) {
      const VkExtent3D extent = { uint32_t(pSourceRect->right - pSourceRect->left), uint32_t(pSourceRect->bottom - pSourceRect->top), 1 };

      const bool extentAligned = extent.width % formatInfo->blockSize.width == 0
        && extent.height % formatInfo->blockSize.height == 0;

      if (pSourceRect->left < 0
        || pSourceRect->top < 0
        || pSourceRect->right <= pSourceRect->left
        || pSourceRect->bottom <= pSourceRect->top
        || pSourceRect->left % formatInfo->blockSize.width != 0
        || pSourceRect->top % formatInfo->blockSize.height != 0
        || (extent != texLevelExtent && !extentAligned))
        return D3DERR_INVALIDCALL;

      srcBlockOffset = { pSourceRect->left / int32_t(formatInfo->blockSize.width),
                         pSourceRect->top  / int32_t(formatInfo->blockSize.height),
                         0u };

      copyExtent = { extent.width,
                     extent.height,
                     1u };
    }

    if (pDestPoint != nullptr) {
      if (pDestPoint->x % formatInfo->blockSize.width != 0
        || pDestPoint->y % formatInfo->blockSize.height != 0
        || pDestPoint->x < 0
        || pDestPoint->y < 0)
        return D3DERR_INVALIDCALL;

      dstOffset = { pDestPoint->x,
                    pDestPoint->y,
                    0u };
    }

    VkExtent3D copyBlockCount = util::computeBlockCount(copyExtent, formatInfo->blockSize);

    const auto dstSubresource = vk::makeSubresourceLayers(
      dstTextureInfo->GetSubresourceFromIndex(VK_IMAGE_ASPECT_COLOR_BIT, dst->GetSubresource()));

    DxvkBufferSliceHandle srcSlice = srcTextureInfo->GetMappedSlice(src->GetSubresource());
    VkDeviceSize dirtySize = copyBlockCount.width * copyBlockCount.height * formatInfo->elementSize;
    D3D9BufferSlice slice = AllocTempBuffer<false>(dirtySize);
    VkDeviceSize copySrcOffset = (srcBlockOffset.z * texLevelBlockCount.height * texLevelBlockCount.width
        + srcBlockOffset.y * texLevelBlockCount.width
        + srcBlockOffset.x)
        * formatInfo->elementSize;

    VkDeviceSize pitch = align(texLevelBlockCount.width * formatInfo->elementSize, 4);
    void* srcData = reinterpret_cast<uint8_t*>(srcSlice.mapPtr) + copySrcOffset;
    util::packImageData(
      slice.mapPtr, srcData, copyBlockCount, formatInfo->elementSize,
      pitch, pitch * texLevelBlockCount.height);

    Rc<DxvkImage>  dstImage  = dstTextureInfo->GetImage();

    EmitCs([
      cDstImage   = std::move(dstImage),
      cSrcSlice   = slice.slice,
      cDstLayers  = dstSubresource,
      cDstOffset  = dstOffset,
      cCopyExtent = copyExtent
    ] (DxvkContext* ctx) {
      ctx->copyBufferToImage(
        cDstImage, cDstLayers, cDstOffset, cCopyExtent,
        cSrcSlice.buffer(), cSrcSlice.offset(), 0, 0);
    });

    dstTextureInfo->SetWrittenByGPU(dst->GetSubresource(), true);

    if (dstTextureInfo->IsAutomaticMip())
      MarkTextureMipsDirty(dstTextureInfo);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::UpdateTexture(
          IDirect3DBaseTexture9* pSourceTexture,
          IDirect3DBaseTexture9* pDestinationTexture) {
    D3D9DeviceLock lock = LockDevice();

    if (!pDestinationTexture || !pSourceTexture)
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* dstTexInfo = GetCommonTexture(pDestinationTexture);
    D3D9CommonTexture* srcTexInfo = GetCommonTexture(pSourceTexture);

    if (unlikely(srcTexInfo->Desc()->Pool != D3DPOOL_SYSTEMMEM || dstTexInfo->Desc()->Pool != D3DPOOL_DEFAULT))
      return D3DERR_INVALIDCALL;

    const Rc<DxvkImage> dstImage  = dstTexInfo->GetImage();
    const DxvkFormatInfo* formatInfo = imageFormatInfo(dstTexInfo->GetFormatMapping().FormatColor);
    uint32_t mipLevels   = std::min(srcTexInfo->Desc()->MipLevels, dstTexInfo->Desc()->MipLevels);
    uint32_t arraySlices = std::min(srcTexInfo->Desc()->ArraySize, dstTexInfo->Desc()->ArraySize);

    if (unlikely(srcTexInfo->IsAutomaticMip() && !dstTexInfo->IsAutomaticMip()))
      return D3DERR_INVALIDCALL;

    if (dstTexInfo->IsAutomaticMip())
      mipLevels = 1;

    for (uint32_t a = 0; a < arraySlices; a++) {
      const D3DBOX& box = srcTexInfo->GetDirtyBox(a);
      if (box.Left >= box.Right || box.Top >= box.Bottom || box.Front >= box.Back)
        continue;

      for (uint32_t m = 0; m < mipLevels; m++) {
        VkImageSubresourceLayers dstLayers = { VK_IMAGE_ASPECT_COLOR_BIT, m, a, 1 };

        VkOffset3D scaledBoxOffset = {
          int32_t(alignDown(box.Left  >> m, formatInfo->blockSize.width)),
          int32_t(alignDown(box.Top   >> m, formatInfo->blockSize.height)),
          int32_t(alignDown(box.Front >> m, formatInfo->blockSize.depth))
        };
        VkExtent3D scaledBoxExtent = util::computeMipLevelExtent({
          uint32_t(box.Right - scaledBoxOffset.x),
          uint32_t(box.Bottom - scaledBoxOffset.y),
          uint32_t(box.Back - scaledBoxOffset.z)
        }, m);
        VkExtent3D scaledBoxExtentBlockCount = util::computeBlockCount(scaledBoxExtent, formatInfo->blockSize);
        VkExtent3D scaledAlignedBoxExtent = util::computeBlockExtent(scaledBoxExtentBlockCount, formatInfo->blockSize);

        VkExtent3D texLevelExtent = dstImage->mipLevelExtent(m);
        VkExtent3D texLevelExtentBlockCount = util::computeBlockCount(texLevelExtent, formatInfo->blockSize);

        scaledAlignedBoxExtent.width = std::min<uint32_t>(texLevelExtent.width, scaledAlignedBoxExtent.width);
        scaledAlignedBoxExtent.height = std::min<uint32_t>(texLevelExtent.height, scaledAlignedBoxExtent.height);
        scaledAlignedBoxExtent.depth = std::min<uint32_t>(texLevelExtent.depth, scaledAlignedBoxExtent.depth);

        VkDeviceSize dirtySize = scaledBoxExtentBlockCount.width * scaledBoxExtentBlockCount.height * scaledBoxExtentBlockCount.depth * formatInfo->elementSize;
        D3D9BufferSlice slice = AllocTempBuffer<false>(dirtySize);
        VkOffset3D boxOffsetBlockCount = util::computeBlockOffset(scaledBoxOffset, formatInfo->blockSize);
        VkDeviceSize copySrcOffset = (boxOffsetBlockCount.z * texLevelExtentBlockCount.height * texLevelExtentBlockCount.width
            + boxOffsetBlockCount.y * texLevelExtentBlockCount.width
            + boxOffsetBlockCount.x)
            * formatInfo->elementSize;

        VkDeviceSize pitch = align(texLevelExtentBlockCount.width * formatInfo->elementSize, 4);
        void* srcData = reinterpret_cast<uint8_t*>(srcTexInfo->GetMappedSlice(srcTexInfo->CalcSubresource(a, m)).mapPtr) + copySrcOffset;
        util::packImageData(
          slice.mapPtr, srcData, scaledBoxExtentBlockCount, formatInfo->elementSize,
          pitch, pitch * texLevelExtentBlockCount.height);

        scaledAlignedBoxExtent.width  = std::min<uint32_t>(texLevelExtent.width, scaledAlignedBoxExtent.width);
        scaledAlignedBoxExtent.height = std::min<uint32_t>(texLevelExtent.height, scaledAlignedBoxExtent.height);
        scaledAlignedBoxExtent.depth  = std::min<uint32_t>(texLevelExtent.depth, scaledAlignedBoxExtent.depth);

        EmitCs([
          cDstImage  = dstImage,
          cSrcSlice  = slice.slice,
          cDstLayers = dstLayers,
          cExtent    = scaledAlignedBoxExtent,
          cOffset    = scaledBoxOffset
        ] (DxvkContext* ctx) {
          ctx->copyBufferToImage(
            cDstImage,  cDstLayers,
            cOffset, cExtent,
            cSrcSlice.buffer(), cSrcSlice.offset(), 0, 0);
        });

        dstTexInfo->SetWrittenByGPU(dstTexInfo->CalcSubresource(a, m), true);
      }
    }

    srcTexInfo->ClearDirtyBoxes();
    if (dstTexInfo->IsAutomaticMip() && mipLevels != dstTexInfo->Desc()->MipLevels)
      MarkTextureMipsDirty(dstTexInfo);

    FlushImplicit(false);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetRenderTargetData(
          IDirect3DSurface9* pRenderTarget,
          IDirect3DSurface9* pDestSurface) {
    D3D9DeviceLock lock = LockDevice();

    D3D9Surface* src = static_cast<D3D9Surface*>(pRenderTarget);
    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestSurface);

    if (unlikely(src == nullptr || dst == nullptr))
      return D3DERR_INVALIDCALL;

    if (pRenderTarget == pDestSurface)
      return D3D_OK;

    D3D9CommonTexture* dstTexInfo = GetCommonTexture(dst);
    D3D9CommonTexture* srcTexInfo = GetCommonTexture(src);

    if (srcTexInfo->Desc()->Format != dstTexInfo->Desc()->Format)
      return D3DERR_INVALIDCALL;

    if (dstTexInfo->Desc()->Pool == D3DPOOL_DEFAULT)
      return this->StretchRect(pRenderTarget, nullptr, pDestSurface, nullptr, D3DTEXF_NONE);

    Rc<DxvkBuffer> dstBuffer = dstTexInfo->GetBuffer(dst->GetSubresource());

    Rc<DxvkImage>  srcImage                 = srcTexInfo->GetImage();
    const DxvkFormatInfo* srcFormatInfo     = imageFormatInfo(srcImage->info().format);

    const VkImageSubresource srcSubresource = srcTexInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());
    VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    VkExtent3D srcExtent = srcTexInfo->GetExtentMip(src->GetMipLevel());

    VkExtent3D texLevelExtentBlockCount = util::computeBlockCount(srcExtent, srcFormatInfo->blockSize);
    VkDeviceSize pitch = align(texLevelExtentBlockCount.width * uint32_t(srcFormatInfo->elementSize), 4);
    uint32_t pitchBlocks = uint32_t(pitch / srcFormatInfo->elementSize);
    VkExtent2D dstExtent = VkExtent2D{ pitchBlocks,
                                       texLevelExtentBlockCount.height * pitchBlocks };

    EmitCs([
      cBuffer       = dstBuffer,
      cImage        = srcImage,
      cSubresources = srcSubresourceLayers,
      cLevelExtent  = srcExtent,
      cDstExtent    = dstExtent
    ] (DxvkContext* ctx) {
      ctx->copyImageToBuffer(cBuffer, 0, 4, 0,
        cImage, cSubresources, VkOffset3D { 0, 0, 0 },
        cLevelExtent);
    });

    dstTexInfo->SetWrittenByGPU(dst->GetSubresource(), true);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->GetFrontBufferData(pDestSurface);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::StretchRect(
          IDirect3DSurface9*   pSourceSurface,
    const RECT*                pSourceRect,
          IDirect3DSurface9*   pDestSurface,
    const RECT*                pDestRect,
          D3DTEXTUREFILTERTYPE Filter) {
    D3D9DeviceLock lock = LockDevice();

    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestSurface);
    D3D9Surface* src = static_cast<D3D9Surface*>(pSourceSurface);

    if (unlikely(src == nullptr || dst == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(src == dst))
      return D3DERR_INVALIDCALL;

    bool fastPath = true;

    D3D9CommonTexture* dstTextureInfo = dst->GetCommonTexture();
    D3D9CommonTexture* srcTextureInfo = src->GetCommonTexture();

    if (unlikely(dstTextureInfo->Desc()->Pool != D3DPOOL_DEFAULT ||
                 srcTextureInfo->Desc()->Pool != D3DPOOL_DEFAULT))
      return D3DERR_INVALIDCALL;

    Rc<DxvkImage> dstImage = dstTextureInfo->GetImage();
    Rc<DxvkImage> srcImage = srcTextureInfo->GetImage();

    const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
    const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);

    const VkImageSubresource dstSubresource = dstTextureInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, dst->GetSubresource());
    const VkImageSubresource srcSubresource = srcTextureInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());

    VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);
    VkExtent3D dstExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);

    D3D9Format srcFormat = srcTextureInfo->Desc()->Format;
    D3D9Format dstFormat = dstTextureInfo->Desc()->Format;

    // We may only fast path copy non identicals one way!
    // We don't know what garbage could be in the X8 data.
    bool similar = AreFormatsSimilar(srcFormat, dstFormat);

    // Copies are only supported on similar formats.
    fastPath &= similar;

    // Copies are only supported if the sample count matches,
    // otherwise we need to resolve.
    bool needsResolve = srcImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;
    bool fbBlit       = dstImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;
    fastPath &= !fbBlit;

    // Copies would only work if we are block aligned.
    if (pSourceRect != nullptr) {
      fastPath       &=  (pSourceRect->left   % srcFormatInfo->blockSize.width  == 0);
      fastPath       &=  (pSourceRect->right  % srcFormatInfo->blockSize.width  == 0);
      fastPath       &=  (pSourceRect->top    % srcFormatInfo->blockSize.height == 0);
      fastPath       &=  (pSourceRect->bottom % srcFormatInfo->blockSize.height == 0);
    }

    if (pDestRect != nullptr) {
      fastPath       &=  (pDestRect->left     % dstFormatInfo->blockSize.width  == 0);
      fastPath       &=  (pDestRect->top      % dstFormatInfo->blockSize.height == 0);
    }

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

    if (unlikely(IsBlitRegionInvalid(blitInfo.srcOffsets, srcExtent)))
      return D3DERR_INVALIDCALL;

    if (unlikely(IsBlitRegionInvalid(blitInfo.dstOffsets, dstExtent)))
      return D3DERR_INVALIDCALL;

    VkExtent3D srcCopyExtent =
    { uint32_t(blitInfo.srcOffsets[1].x - blitInfo.srcOffsets[0].x),
      uint32_t(blitInfo.srcOffsets[1].y - blitInfo.srcOffsets[0].y),
      uint32_t(blitInfo.srcOffsets[1].z - blitInfo.srcOffsets[0].z) };

    VkExtent3D dstCopyExtent =
    { uint32_t(blitInfo.dstOffsets[1].x - blitInfo.dstOffsets[0].x),
      uint32_t(blitInfo.dstOffsets[1].y - blitInfo.dstOffsets[0].y),
      uint32_t(blitInfo.dstOffsets[1].z - blitInfo.dstOffsets[0].z) };

    // Copies would only work if the extents match. (ie. no stretching)
    bool stretch = srcCopyExtent != dstCopyExtent;
    fastPath &= !stretch;

    if (!fastPath || needsResolve) {
      // Compressed destination formats are forbidden for blits.
      if (dstFormatInfo->flags.test(DxvkFormatFlag::BlockCompressed))
        return D3DERR_INVALIDCALL;
    }

    auto EmitResolveCS = [&](const Rc<DxvkImage>& resolveDst, bool intermediate) {
      VkImageResolve region;
      region.srcSubresource = blitInfo.srcSubresource;
      region.srcOffset      = blitInfo.srcOffsets[0];
      region.dstSubresource = intermediate ? blitInfo.srcSubresource : blitInfo.dstSubresource;
      region.dstOffset      = intermediate ? blitInfo.srcOffsets[0]  : blitInfo.dstOffsets[0];
      region.extent         = srcCopyExtent;

      EmitCs([
        cDstImage = resolveDst,
        cSrcImage = srcImage,
        cRegion   = region
      ] (DxvkContext* ctx) {
        if (cRegion.srcSubresource.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
          ctx->resolveImage(
            cDstImage, cSrcImage, cRegion,
            VK_FORMAT_UNDEFINED);
        }
        else {
          ctx->resolveDepthStencilImage(
            cDstImage, cSrcImage, cRegion,
            VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
            VK_RESOLVE_MODE_AVERAGE_BIT_KHR);
        }
      });
    };

    if (fastPath) {
      if (needsResolve) {
        EmitResolveCS(dstImage, false);
      } else {
        EmitCs([
          cDstImage  = dstImage,
          cSrcImage  = srcImage,
          cDstLayers = blitInfo.dstSubresource,
          cSrcLayers = blitInfo.srcSubresource,
          cDstOffset = blitInfo.dstOffsets[0],
          cSrcOffset = blitInfo.srcOffsets[0],
          cExtent    = srcCopyExtent
        ] (DxvkContext* ctx) {
          ctx->copyImage(
            cDstImage, cDstLayers, cDstOffset,
            cSrcImage, cSrcLayers, cSrcOffset,
            cExtent);
        });
      }
    }
    else {
      if (needsResolve) {
        auto resolveSrc = srcTextureInfo->GetResolveImage();

        EmitResolveCS(resolveSrc, true);
        srcImage = resolveSrc;
      }

      EmitCs([
        cDstImage = dstImage,
        cDstMap   = dstTextureInfo->GetMapping().Swizzle,
        cSrcImage = srcImage,
        cSrcMap   = srcTextureInfo->GetMapping().Swizzle,
        cBlitInfo = blitInfo,
        cFilter   = stretch ? DecodeFilter(Filter) : VK_FILTER_NEAREST
      ] (DxvkContext* ctx) {
        ctx->blitImage(
          cDstImage,
          cDstMap,
          cSrcImage,
          cSrcMap,
          cBlitInfo,
          cFilter);
      });
    }

    dstTextureInfo->SetWrittenByGPU(dst->GetSubresource(), true);

    if (dstTextureInfo->IsAutomaticMip())
      MarkTextureMipsDirty(dstTextureInfo);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ColorFill(
          IDirect3DSurface9* pSurface,
    const RECT*              pRect,
          D3DCOLOR           Color) {
    D3D9DeviceLock lock = LockDevice();

    D3D9Surface* dst = static_cast<D3D9Surface*>(pSurface);

    if (unlikely(dst == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* dstTextureInfo = dst->GetCommonTexture();

    if (unlikely(dstTextureInfo->Desc()->Pool != D3DPOOL_DEFAULT))
      return D3DERR_INVALIDCALL;

    VkExtent3D mipExtent = dstTextureInfo->GetExtentMip(dst->GetSubresource());

    VkOffset3D offset = VkOffset3D{ 0u, 0u, 0u };
    VkExtent3D extent = mipExtent;

    bool isFullExtent = true;
    if (pRect != nullptr) {
      ConvertRect(*pRect, offset, extent);

      isFullExtent = offset == VkOffset3D{ 0u, 0u, 0u }
                  && extent == mipExtent;
    }

    Rc<DxvkImageView> rtView = dst->GetRenderTargetView(false);

    VkClearValue clearValue;
    DecodeD3DCOLOR(Color, clearValue.color.float32);

    // Fast path for games that may use this as an
    // alternative to Clear on render targets.
    if (isFullExtent && rtView != nullptr) {
      EmitCs([
        cImageView  = rtView,
        cClearValue = clearValue
      ] (DxvkContext* ctx) {
        ctx->clearRenderTarget(
          cImageView,
          VK_IMAGE_ASPECT_COLOR_BIT,
          cClearValue);
      });
    } else {
      if (unlikely(rtView == nullptr)) {
        const D3D9Format format = dstTextureInfo->Desc()->Format;
        if (format != D3D9Format::NULL_FORMAT)
          Logger::err(str::format("D3D9DeviceEx::ColorFill: Unsupported format ", format));

        return D3D_OK;
      }

      EmitCs([
        cImageView  = rtView,
        cOffset     = offset,
        cExtent     = extent,
        cClearValue = clearValue
      ] (DxvkContext* ctx) {
        ctx->clearImageView(
          cImageView,
          cOffset, cExtent,
          VK_IMAGE_ASPECT_COLOR_BIT,
          cClearValue);
      });
    }

    dstTextureInfo->SetWrittenByGPU(dst->GetSubresource(), true);

    if (dstTextureInfo->IsAutomaticMip())
      MarkTextureMipsDirty(dstTextureInfo);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateOffscreenPlainSurface(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    return CreateOffscreenPlainSurfaceEx(
      Width,     Height,
      Format,    Pool,
      ppSurface, pSharedHandle,
      0);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetRenderTarget(
          DWORD              RenderTargetIndex,
          IDirect3DSurface9* pRenderTarget) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(RenderTargetIndex >= caps::MaxSimultaneousRenderTargets
     || (pRenderTarget == nullptr && RenderTargetIndex == 0)))
      return D3DERR_INVALIDCALL;

    D3D9Surface* rt = static_cast<D3D9Surface*>(pRenderTarget);
    D3D9CommonTexture* texInfo = rt != nullptr
      ? rt->GetCommonTexture()
      : nullptr;

    if (unlikely(rt != nullptr && !(texInfo->Desc()->Usage & D3DUSAGE_RENDERTARGET)))
      return D3DERR_INVALIDCALL;

    if (RenderTargetIndex == 0) {
      auto rtSize = rt->GetSurfaceExtent();

      D3DVIEWPORT9 viewport;
      viewport.X       = 0;
      viewport.Y       = 0;
      viewport.Width   = rtSize.width;
      viewport.Height  = rtSize.height;
      viewport.MinZ    = 0.0f;
      viewport.MaxZ    = 1.0f;

      RECT scissorRect;
      scissorRect.left    = 0;
      scissorRect.top     = 0;
      scissorRect.right   = rtSize.width;
      scissorRect.bottom  = rtSize.height;

      if (m_state.viewport != viewport) {
        m_flags.set(D3D9DeviceFlag::DirtyFFViewport);
        m_flags.set(D3D9DeviceFlag::DirtyPointScale);
        m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);
        m_state.viewport = viewport;
      }

      if (m_state.scissorRect != scissorRect) {
        m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);
        m_state.scissorRect = scissorRect;
      }
    }

    if (m_state.renderTargets[RenderTargetIndex] == rt)
      return D3D_OK;

    // Do a strong flush if the first render target is changed.
    FlushImplicit(RenderTargetIndex == 0 ? TRUE : FALSE);
    m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

    m_state.renderTargets[RenderTargetIndex] = rt;

    UpdateActiveRTs(RenderTargetIndex);

    uint32_t originalAlphaSwizzleRTs = m_alphaSwizzleRTs;

    m_alphaSwizzleRTs &= ~(1 << RenderTargetIndex);

    if (rt != nullptr) {
      if (texInfo->GetMapping().Swizzle.a == VK_COMPONENT_SWIZZLE_ONE)
        m_alphaSwizzleRTs |= 1 << RenderTargetIndex;

      if (texInfo->IsAutomaticMip())
        texInfo->SetNeedsMipGen(true);

      texInfo->SetWrittenByGPU(rt->GetSubresource(), true);
    }

    if (originalAlphaSwizzleRTs != m_alphaSwizzleRTs)
      m_flags.set(D3D9DeviceFlag::DirtyBlendState);

    if (RenderTargetIndex == 0) {
      bool validSampleMask = texInfo->Desc()->MultiSample > D3DMULTISAMPLE_NONMASKABLE;

      if (validSampleMask != m_flags.test(D3D9DeviceFlag::ValidSampleMask)) {
        m_flags.clr(D3D9DeviceFlag::ValidSampleMask);
        if (validSampleMask)
          m_flags.set(D3D9DeviceFlag::ValidSampleMask);

        m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetRenderTarget(
          DWORD               RenderTargetIndex,
          IDirect3DSurface9** ppRenderTarget) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppRenderTarget);

    if (unlikely(ppRenderTarget == nullptr || RenderTargetIndex > caps::MaxSimultaneousRenderTargets))
      return D3DERR_INVALIDCALL;

    if (m_state.renderTargets[RenderTargetIndex] == nullptr)
      return D3DERR_NOTFOUND;

    *ppRenderTarget = m_state.renderTargets[RenderTargetIndex].ref();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    D3D9DeviceLock lock = LockDevice();

    D3D9Surface* ds = static_cast<D3D9Surface*>(pNewZStencil);

    if (unlikely(ds && !(ds->GetCommonTexture()->Desc()->Usage & D3DUSAGE_DEPTHSTENCIL)))
      return D3DERR_INVALIDCALL;

    if (m_state.depthStencil == ds)
      return D3D_OK;

    FlushImplicit(FALSE);
    m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

    if (ds != nullptr) {
      float rValue = GetDepthBufferRValue(ds->GetCommonTexture()->GetFormatMapping().FormatColor);
      if (m_depthBiasScale != rValue) {
        m_depthBiasScale = rValue;
        m_flags.set(D3D9DeviceFlag::DirtyDepthBias);
      }
    }

    m_state.depthStencil = ds;

    UpdateActiveHazardsDS(UINT32_MAX);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppZStencilSurface);

    if (unlikely(ppZStencilSurface == nullptr))
      return D3DERR_INVALIDCALL;

    if (m_state.depthStencil == nullptr)
      return D3DERR_NOTFOUND;

    *ppZStencilSurface = m_state.depthStencil.ref();

    return D3D_OK;
  }

  // The Begin/EndScene functions actually do nothing.
  // Some games don't even call them.

  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::BeginScene() {
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::EndScene() {
    FlushImplicit(true);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::Clear(
          DWORD    Count,
    const D3DRECT* pRects,
          DWORD    Flags,
          D3DCOLOR Color,
          float    Z,
          DWORD    Stencil) {
    D3D9DeviceLock lock = LockDevice();

    const auto& vp = m_state.viewport;
    const auto& sc = m_state.scissorRect;

    bool srgb      = m_state.renderStates[D3DRS_SRGBWRITEENABLE];
    bool scissor   = m_state.renderStates[D3DRS_SCISSORTESTENABLE];

    VkOffset3D offset = { int32_t(vp.X),    int32_t(vp.Y),      0  };
    VkExtent3D extent = {         vp.Width,         vp.Height,  1u };

    if (scissor) {
      offset.x = std::max<int32_t> (offset.x, sc.left);
      offset.y = std::max<int32_t> (offset.y, sc.top);

      extent.width  = std::min<uint32_t>(extent.width,  sc.right  - offset.x);
      extent.height = std::min<uint32_t>(extent.height, sc.bottom - offset.y);
    }

    // This becomes pretty unreadable in one singular if statement...
    if (Count) {
      // If pRects is null, or our first rect encompasses the viewport:
      if (!pRects)
        Count = 0;
      else if (pRects[0].x1 <= offset.x                         && pRects[0].y1 <= offset.y
            && pRects[0].x2 >= offset.x + int32_t(extent.width) && pRects[0].y2 >= offset.y + int32_t(extent.height))
        Count = 0;
    }

    // Here, Count of 0 will denote whether or not to care about user rects.
    VkClearValue clearValueDepth;
    clearValueDepth.depthStencil.depth   = Z;
    clearValueDepth.depthStencil.stencil = Stencil;

    VkClearValue clearValueColor;
    DecodeD3DCOLOR(Color, clearValueColor.color.float32);

    auto dsv = m_state.depthStencil != nullptr ? m_state.depthStencil->GetDepthStencilView() : nullptr;
    VkImageAspectFlags depthAspectMask = 0;
    if (dsv != nullptr) {
      if (Flags & D3DCLEAR_ZBUFFER)
        depthAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

      if (Flags & D3DCLEAR_STENCIL)
        depthAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

      depthAspectMask &= imageFormatInfo(dsv->info().format)->aspectMask;
    }

    auto ClearImageView = [this](
      bool               fullClear,
      VkOffset3D         offset,
      VkExtent3D         extent,
      Rc<DxvkImageView>  imageView,
      VkImageAspectFlags aspectMask,
      VkClearValue       clearValue) {
      if (fullClear) {
        EmitCs([
          cClearValue = clearValue,
          cAspectMask = aspectMask,
          cImageView  = imageView
        ] (DxvkContext* ctx) {
          ctx->clearRenderTarget(
            cImageView,
            cAspectMask,
            cClearValue);
        });
      }
      else {
        EmitCs([
          cClearValue = clearValue,
          cAspectMask = aspectMask,
          cImageView  = imageView,
          cOffset     = offset,
          cExtent     = extent
        ] (DxvkContext* ctx) {
          ctx->clearImageView(
            cImageView,
            cOffset, cExtent,
            cAspectMask,
            cClearValue);
        });
      }
    };

    auto ClearViewRect = [&](
      bool               fullClear,
      VkOffset3D         offset,
      VkExtent3D         extent) {
      // Clear depth if we need to.
      if (depthAspectMask != 0)
        ClearImageView(fullClear, offset, extent, dsv, depthAspectMask, clearValueDepth);

      // Clear render targets if we need to.
      if (Flags & D3DCLEAR_TARGET) {
        for (auto rt : m_state.renderTargets) {
          auto rtv = rt != nullptr ? rt->GetRenderTargetView(srgb) : nullptr;

          if (unlikely(rtv != nullptr))
            ClearImageView(fullClear, offset, extent, rtv, VK_IMAGE_ASPECT_COLOR_BIT, clearValueColor);
        }
      }
    };

    // A Hat in Time and other UE3 games only gets partial clears here
    // because of an oversized rt height due to their weird alignment...
    // This works around that.
    uint32_t alignment = m_d3d9Options.lenientClear ? 8 : 1;

    auto rtSize = m_state.renderTargets[0]->GetSurfaceExtent();

    extent.width = std::min(rtSize.width - offset.x, extent.width);
    extent.height = std::min(rtSize.height - offset.y, extent.height);

    bool extentMatches = align(extent.width,  alignment) == align(rtSize.width,  alignment)
                      && align(extent.height, alignment) == align(rtSize.height, alignment);

    bool rtSizeMatchesClearSize = offset.x == 0 && offset.y == 0 && extentMatches;

    if (likely(!Count && rtSizeMatchesClearSize)) {
      // Fast path w/ ClearRenderTarget for when
      // our viewport and stencils match the RT size
      ClearViewRect(true, offset, extent);
    }
    else if (!Count) {
      // Clear our viewport & scissor minified region in this rendertarget.
      ClearViewRect(false, offset, extent);
    }
    else {
      // Clear the application provided rects.
      for (uint32_t i = 0; i < Count; i++) {
        VkOffset3D rectOffset = {
          std::max<int32_t>(pRects[i].x1, offset.x),
          std::max<int32_t>(pRects[i].y1, offset.y),
          0
        };

        VkExtent3D rectExtent = {
          std::min<uint32_t>(pRects[i].x2, offset.x + extent.width)  - rectOffset.x,
          std::min<uint32_t>(pRects[i].y2, offset.y + extent.height) - rectOffset.y,
          1u
        };

        ClearViewRect(false, rectOffset, rectExtent);
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    return SetStateTransform(GetTransformIndex(State), pMatrix);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pMatrix == nullptr))
      return D3DERR_INVALIDCALL;

    *pMatrix = bit::cast<D3DMATRIX>(m_state.transforms[GetTransformIndex(State)]);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->MultiplyStateTransform(TransformState, pMatrix);

    uint32_t idx = GetTransformIndex(TransformState);

    m_state.transforms[idx] = m_state.transforms[idx] * ConvertMatrix(pMatrix);

    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    if (idx == GetTransformIndex(D3DTS_VIEW) || idx >= GetTransformIndex(D3DTS_WORLD))
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetViewport(const D3DVIEWPORT9* pViewport) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pViewport == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetViewport(pViewport);

    if (m_state.viewport == *pViewport)
      return D3D_OK;

    m_state.viewport = *pViewport;

    m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);
    m_flags.set(D3D9DeviceFlag::DirtyFFViewport);
    m_flags.set(D3D9DeviceFlag::DirtyPointScale);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetViewport(D3DVIEWPORT9* pViewport) {
    D3D9DeviceLock lock = LockDevice();

    if (pViewport == nullptr)
      return D3DERR_INVALIDCALL;

    *pViewport = m_state.viewport;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetMaterial(const D3DMATERIAL9* pMaterial) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pMaterial == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetMaterial(pMaterial);

    m_state.material = *pMaterial;
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetMaterial(D3DMATERIAL9* pMaterial) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pMaterial == nullptr))
      return D3DERR_INVALIDCALL;

    *pMaterial = m_state.material;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetLight(DWORD Index, const D3DLIGHT9* pLight) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pLight == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord())) {
      Logger::warn("D3D9DeviceEx::SetLight: State block not implemented.");
      return D3D_OK;
    }

    if (Index >= m_state.lights.size())
      m_state.lights.resize(Index + 1);

    m_state.lights[Index] = *pLight;

    if (m_state.IsLightEnabled(Index))
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pLight == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(Index >= m_state.lights.size() || !m_state.lights[Index]))
      return D3DERR_INVALIDCALL;

    *pLight = m_state.lights[Index].value();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::LightEnable(DWORD Index, BOOL Enable) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(Index >= m_state.lights.size()))
      m_state.lights.resize(Index + 1);

    if (unlikely(!m_state.lights[Index]))
      m_state.lights[Index] = DefaultLight;

    if (m_state.IsLightEnabled(Index) == !!Enable)
      return D3D_OK;

    uint32_t searchIndex = UINT32_MAX;
    uint32_t setIndex    = Index;

    if (!Enable)
      std::swap(searchIndex, setIndex);

    for (auto& idx : m_state.enabledLightIndices) {
      if (idx == searchIndex) {
        idx = setIndex;
        m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);
        m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
        break;
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetLightEnable(DWORD Index, BOOL* pEnable) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pEnable == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(Index >= m_state.lights.size() || !m_state.lights[Index]))
      return D3DERR_INVALIDCALL;

    *pEnable = m_state.IsLightEnabled(Index) ? 128 : 0; // Weird quirk but OK.

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetClipPlane(DWORD Index, const float* pPlane) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(Index >= caps::MaxClipPlanes || !pPlane))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetClipPlane(Index, pPlane);

    bool dirty = false;

    for (uint32_t i = 0; i < 4; i++) {
      dirty |= m_state.clipPlanes[Index].coeff[i] != pPlane[i];
      m_state.clipPlanes[Index].coeff[i] = pPlane[i];
    }

    bool enabled = m_state.renderStates[D3DRS_CLIPPLANEENABLE] & (1u << Index);
    dirty &= enabled;

    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetClipPlane(DWORD Index, float* pPlane) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(Index >= caps::MaxClipPlanes || !pPlane))
      return D3DERR_INVALIDCALL;

    for (uint32_t i = 0; i < 4; i++)
      pPlane[i] = m_state.clipPlanes[Index].coeff[i];

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    D3D9DeviceLock lock = LockDevice();

    // D3D9 only allows reading for values 0 and 7-255 so we don't need to do anything but return OK
    if (unlikely(State > 255 || (State < D3DRS_ZENABLE && State != 0))) {
      return D3D_OK;
    }

    if (unlikely(ShouldRecord()))
      return m_recorder->SetRenderState(State, Value);

    auto& states = m_state.renderStates;

    bool changed = states[State] != Value;

    if (likely(changed)) {
      const bool oldClipPlaneEnabled = IsClipPlaneEnabled();

      const bool oldDepthBiasEnabled = IsDepthBiasEnabled();

      const bool oldATOC = IsAlphaToCoverageEnabled();
      const bool oldNVDB = states[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB);
      const bool oldAlphaTest = IsAlphaTestEnabled();

      states[State] = Value;

      // AMD's driver hack for ATOC and RESZ
      if (unlikely(State == D3DRS_POINTSIZE)) {
        // ATOC
        constexpr uint32_t AlphaToCoverageEnable  = uint32_t(D3D9Format::A2M1);
        constexpr uint32_t AlphaToCoverageDisable = uint32_t(D3D9Format::A2M0);

        if (Value == AlphaToCoverageEnable
         || Value == AlphaToCoverageDisable) {
          m_amdATOC = Value == AlphaToCoverageEnable;

          bool newATOC = IsAlphaToCoverageEnabled();
          bool newAlphaTest = IsAlphaTestEnabled();

          if (oldATOC != newATOC)
            m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);

          if (oldAlphaTest != newAlphaTest)
            m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);

          return D3D_OK;
        }

        // RESZ
        constexpr uint32_t RESZ = 0x7fa05000;
        if (Value == RESZ) {
          ResolveZ();
          return D3D_OK;
        }
      }

      // NV's driver hack for ATOC.
      if (unlikely(State == D3DRS_ADAPTIVETESS_Y)) {
        constexpr uint32_t AlphaToCoverageEnable  = uint32_t(D3D9Format::ATOC);
        constexpr uint32_t AlphaToCoverageDisable = 0;

        if (Value == AlphaToCoverageEnable
         || Value == AlphaToCoverageDisable) {
          m_nvATOC = Value == AlphaToCoverageEnable;

          bool newATOC = IsAlphaToCoverageEnabled();
          bool newAlphaTest = IsAlphaTestEnabled();

          if (oldATOC != newATOC)
            m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);

          if (oldAlphaTest != newAlphaTest)
            m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);

          return D3D_OK;
        }

        if (unlikely(Value == uint32_t(D3D9Format::COPM))) {
          // UE3 calls this MinimalNVIDIADriverShaderOptimization
          Logger::info("D3D9DeviceEx::SetRenderState: MinimalNVIDIADriverShaderOptimization is unsupported");
          return D3D_OK;
        }
      }

      switch (State) {
        case D3DRS_SEPARATEALPHABLENDENABLE:
        case D3DRS_ALPHABLENDENABLE:
        case D3DRS_BLENDOP:
        case D3DRS_BLENDOPALPHA:
        case D3DRS_DESTBLEND:
        case D3DRS_DESTBLENDALPHA:
        case D3DRS_SRCBLEND:
        case D3DRS_SRCBLENDALPHA:
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;

        case D3DRS_COLORWRITEENABLE:
          UpdateActiveRTs(0);
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE1:
          UpdateActiveRTs(1);
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE2:
          UpdateActiveRTs(2);
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE3:
          UpdateActiveRTs(3);
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;

        case D3DRS_ALPHATESTENABLE: {
          bool newATOC = IsAlphaToCoverageEnabled();
          bool newAlphaTest = IsAlphaTestEnabled();

          if (oldATOC != newATOC)
            m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);

          if (oldAlphaTest != newAlphaTest)
            m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);

          break;
        }

        case D3DRS_ALPHAFUNC:
          m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);
          break;

        case D3DRS_BLENDFACTOR:
          BindBlendFactor();
          break;

        case D3DRS_MULTISAMPLEMASK:
          if (m_flags.test(D3D9DeviceFlag::ValidSampleMask))
            m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);
          break;

        case D3DRS_ZWRITEENABLE:
          if (m_activeHazardsDS != 0)
            m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

          m_flags.set(D3D9DeviceFlag::DirtyDepthStencilState);
          break;

        case D3DRS_ZENABLE:
        case D3DRS_ZFUNC:
        case D3DRS_TWOSIDEDSTENCILMODE:
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
          m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
          break;

        case D3DRS_DEPTHBIAS:
        case D3DRS_SLOPESCALEDEPTHBIAS: {
          const bool depthBiasEnabled = IsDepthBiasEnabled();

          if (depthBiasEnabled != oldDepthBiasEnabled)
            m_flags.set(D3D9DeviceFlag::DirtyRasterizerState);

          if (depthBiasEnabled)
            m_flags.set(D3D9DeviceFlag::DirtyDepthBias);

          break;
        }
        case D3DRS_CULLMODE:
        case D3DRS_FILLMODE:
          m_flags.set(D3D9DeviceFlag::DirtyRasterizerState);
          break;

        case D3DRS_CLIPPLANEENABLE: {
          const bool clipPlaneEnabled = IsClipPlaneEnabled();

          if (clipPlaneEnabled != oldClipPlaneEnabled)
            m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);

          m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);
          break;
        }

        case D3DRS_ALPHAREF:
          UpdatePushConstant<D3D9RenderStateItem::AlphaRef>();
          break;

        case D3DRS_TEXTUREFACTOR:
          m_flags.set(D3D9DeviceFlag::DirtyFFPixelData);
          break;

        case D3DRS_DIFFUSEMATERIALSOURCE:
        case D3DRS_AMBIENTMATERIALSOURCE:
        case D3DRS_SPECULARMATERIALSOURCE:
        case D3DRS_EMISSIVEMATERIALSOURCE:
        case D3DRS_COLORVERTEX:
        case D3DRS_LIGHTING:
        case D3DRS_NORMALIZENORMALS:
        case D3DRS_LOCALVIEWER:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          break;

        case D3DRS_AMBIENT:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);
          break;

        case D3DRS_SPECULARENABLE:
          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
          break;

        case D3DRS_FOGENABLE:
        case D3DRS_FOGVERTEXMODE:
        case D3DRS_FOGTABLEMODE:
          m_flags.set(D3D9DeviceFlag::DirtyFogState);
          break;

        case D3DRS_RANGEFOGENABLE:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          break;

        case D3DRS_FOGCOLOR:
          m_flags.set(D3D9DeviceFlag::DirtyFogColor);
          break;

        case D3DRS_FOGSTART:
          m_flags.set(D3D9DeviceFlag::DirtyFogScale);
          break;

        case D3DRS_FOGEND:
          m_flags.set(D3D9DeviceFlag::DirtyFogScale);
          m_flags.set(D3D9DeviceFlag::DirtyFogEnd);
          break;

        case D3DRS_FOGDENSITY:
          m_flags.set(D3D9DeviceFlag::DirtyFogDensity);
          break;

        case D3DRS_POINTSIZE:
          UpdatePushConstant<D3D9RenderStateItem::PointSize>();
          break;

        case D3DRS_POINTSIZE_MIN:
          UpdatePushConstant<D3D9RenderStateItem::PointSizeMin>();
          break;

        case D3DRS_POINTSIZE_MAX:
          UpdatePushConstant<D3D9RenderStateItem::PointSizeMax>();
          break;

        case D3DRS_POINTSCALE_A:
        case D3DRS_POINTSCALE_B:
        case D3DRS_POINTSCALE_C:
          m_flags.set(D3D9DeviceFlag::DirtyPointScale);
          break;

        case D3DRS_POINTSCALEENABLE:
        case D3DRS_POINTSPRITEENABLE:
          // Nothing to do here!
          // This is handled in UpdatePointMode.
          break;

        case D3DRS_SHADEMODE:
          if (m_state.pixelShader != nullptr) {
            BindShader<DxsoProgramType::PixelShader>(
              GetCommonShader(m_state.pixelShader),
              GetPixelShaderPermutation());
          }

          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
          break;

        case D3DRS_TWEENFACTOR:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);
          break;

        case D3DRS_VERTEXBLEND:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          break;

        case D3DRS_INDEXEDVERTEXBLENDENABLE:
          if (CanSWVP() && Value)
            m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);

          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          break;

        case D3DRS_ADAPTIVETESS_X:
        case D3DRS_ADAPTIVETESS_Z:
        case D3DRS_ADAPTIVETESS_W:
          if (states[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB) || oldNVDB) {
            m_flags.set(D3D9DeviceFlag::DirtyDepthBounds);
            break;
          }

        default:
          static bool s_errorShown[256];

          if (!std::exchange(s_errorShown[State], true))
            Logger::warn(str::format("D3D9DeviceEx::SetRenderState: Unhandled render state ", State));
          break;
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pValue == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(State > 255 || (State < D3DRS_ZENABLE && State != 0))) {
      return D3DERR_INVALIDCALL;
    }

    if (State < D3DRS_ZENABLE || State > D3DRS_BLENDOPALPHA)
      *pValue = 0;
    else
      *pValue = m_state.renderStates[State];

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateStateBlock(
          D3DSTATEBLOCKTYPE      Type,
          IDirect3DStateBlock9** ppSB) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppSB);

    if (unlikely(ppSB == nullptr))
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::BeginStateBlock() {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_recorder != nullptr))
      return D3DERR_INVALIDCALL;

    m_recorder = new D3D9StateBlock(this, D3D9StateBlockType::None);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppSB);

    if (unlikely(ppSB == nullptr || m_recorder == nullptr))
      return D3DERR_INVALIDCALL;

    *ppSB = m_recorder.ref();
    m_recorder = nullptr;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("D3D9DeviceEx::SetClipStatus: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("D3D9DeviceEx::GetClipStatus: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    D3D9DeviceLock lock = LockDevice();

    if (ppTexture == nullptr)
      return D3DERR_INVALIDCALL;

    *ppTexture = nullptr;

    if (unlikely(InvalidSampler(Stage)))
      return D3D_OK;

    DWORD stateSampler = RemapSamplerState(Stage);

    *ppTexture = ref(m_state.textures[stateSampler]);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(InvalidSampler(Stage)))
      return D3D_OK;

    DWORD stateSampler = RemapSamplerState(Stage);

    return SetStateTexture(stateSampler, pTexture);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD*                   pValue) {
    auto dxvkType = RemapTextureStageStateType(Type);

    if (unlikely(pValue == nullptr))
      return D3DERR_INVALIDCALL;

    *pValue = 0;

    if (unlikely(Stage >= caps::TextureStageCount))
      return D3DERR_INVALIDCALL;

    if (unlikely(dxvkType >= TextureStageStateCount))
      return D3DERR_INVALIDCALL;

    *pValue = m_state.textureStages[Stage][dxvkType];

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD                    Value) {
    return SetStateTextureStageState(Stage, RemapTextureStageStateType(Type), Value);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetSamplerState(
          DWORD               Sampler,
          D3DSAMPLERSTATETYPE Type,
          DWORD*              pValue) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pValue == nullptr))
      return D3DERR_INVALIDCALL;

    *pValue = 0;

    if (unlikely(InvalidSampler(Sampler)))
      return D3D_OK;

    Sampler = RemapSamplerState(Sampler);

    *pValue = m_state.samplerStates[Sampler][Type];

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetSamplerState(
          DWORD               Sampler,
          D3DSAMPLERSTATETYPE Type,
          DWORD               Value) {
    D3D9DeviceLock lock = LockDevice();
    if (unlikely(InvalidSampler(Sampler)))
      return D3D_OK;

    uint32_t stateSampler = RemapSamplerState(Sampler);

    return SetStateSamplerState(stateSampler, Type, Value);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ValidateDevice(DWORD* pNumPasses) {
    if (pNumPasses != nullptr)
      *pNumPasses = 1;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    // This succeeds even though we don't advertise support.
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    // Don't advertise support for this...
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetCurrentTexturePalette(UINT PaletteNumber) {
    // This succeeds even though we don't advertise support.
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetCurrentTexturePalette(UINT *PaletteNumber) {
    // Don't advertise support for this...
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetScissorRect(const RECT* pRect) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pRect == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetScissorRect(pRect);

    if (m_state.scissorRect == *pRect)
      return D3D_OK;

    m_state.scissorRect = *pRect;

    m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetScissorRect(RECT* pRect) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pRect == nullptr))
      return D3DERR_INVALIDCALL;

    *pRect = m_state.scissorRect;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetSoftwareVertexProcessing(BOOL bSoftware) {
    auto lock = LockDevice();

    if (bSoftware && !CanSWVP())
      return D3DERR_INVALIDCALL;

    m_isSWVP = bSoftware;

    return D3D_OK;
  }


  BOOL    STDMETHODCALLTYPE D3D9DeviceEx::GetSoftwareVertexProcessing() {
    auto lock = LockDevice();

    return m_isSWVP;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetNPatchMode(float nSegments) {
    return D3D_OK;
  }


  float   STDMETHODCALLTYPE D3D9DeviceEx::GetNPatchMode() {
    return 0.0f;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawPrimitive(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             StartVertex,
          UINT             PrimitiveCount) {
	if (unlikely(!PrimitiveCount))
	  return S_OK;

    D3D9DeviceLock lock = LockDevice();

    PrepareDraw(PrimitiveType);

    EmitCs([this,
      cPrimType    = PrimitiveType,
      cPrimCount   = PrimitiveCount,
      cStartVertex = StartVertex,
      cInstanceCount = GetInstanceCount()
    ](DxvkContext* ctx) {
      auto drawInfo = GenerateDrawInfo(cPrimType, cPrimCount, cInstanceCount);

      ApplyPrimitiveType(ctx, cPrimType);

      ctx->draw(
        drawInfo.vertexCount, drawInfo.instanceCount,
        cStartVertex, 0);
    });

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawIndexedPrimitive(
          D3DPRIMITIVETYPE PrimitiveType,
          INT              BaseVertexIndex,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             StartIndex,
          UINT             PrimitiveCount) {
	if (unlikely(!PrimitiveCount))
	  return S_OK;

    D3D9DeviceLock lock = LockDevice();

    PrepareDraw(PrimitiveType);

    EmitCs([this,
      cPrimType        = PrimitiveType,
      cPrimCount       = PrimitiveCount,
      cStartIndex      = StartIndex,
      cBaseVertexIndex = BaseVertexIndex,
      cInstanceCount   = GetInstanceCount()
    ](DxvkContext* ctx) {
      auto drawInfo = GenerateDrawInfo(cPrimType, cPrimCount, cInstanceCount);

      ApplyPrimitiveType(ctx, cPrimType);

      ctx->drawIndexed(
        drawInfo.vertexCount, drawInfo.instanceCount,
        cStartIndex,
        cBaseVertexIndex, 0);
    });

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
	if (unlikely(!PrimitiveCount))
	  return S_OK;

    D3D9DeviceLock lock = LockDevice();

    PrepareDraw(PrimitiveType);

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount, 0);

    const uint32_t dataSize = GetUPDataSize(drawInfo.vertexCount, VertexStreamZeroStride);
    const uint32_t bufferSize = GetUPBufferSize(drawInfo.vertexCount, VertexStreamZeroStride);

    auto upSlice = AllocTempBuffer<true>(bufferSize);
    FillUPVertexBuffer(upSlice.mapPtr, pVertexStreamZeroData, dataSize, bufferSize);

    EmitCs([this,
      cBufferSlice  = std::move(upSlice.slice),
      cPrimType     = PrimitiveType,
      cPrimCount    = PrimitiveCount,
      cInstanceCount = GetInstanceCount(),
      cStride       = VertexStreamZeroStride
    ](DxvkContext* ctx) {
      auto drawInfo = GenerateDrawInfo(cPrimType, cPrimCount, cInstanceCount);

      ApplyPrimitiveType(ctx, cPrimType);

      ctx->bindVertexBuffer(0, cBufferSlice, cStride);
      ctx->draw(
        drawInfo.vertexCount, drawInfo.instanceCount,
        0, 0);
      ctx->bindVertexBuffer(0, DxvkBufferSlice(), 0);
    });

    m_state.vertexBuffers[0].vertexBuffer = nullptr;
    m_state.vertexBuffers[0].offset       = 0;
    m_state.vertexBuffers[0].stride       = 0;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawIndexedPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             PrimitiveCount,
    const void*            pIndexData,
          D3DFORMAT        IndexDataFormat,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
	if (unlikely(!PrimitiveCount))
	  return S_OK;

    D3D9DeviceLock lock = LockDevice();

    PrepareDraw(PrimitiveType);

    auto drawInfo = GenerateDrawInfo(PrimitiveType, PrimitiveCount, 0);

    const uint32_t vertexDataSize = GetUPDataSize(MinVertexIndex + NumVertices, VertexStreamZeroStride);
    const uint32_t vertexBufferSize = GetUPBufferSize(MinVertexIndex + NumVertices, VertexStreamZeroStride);

    const uint32_t indexSize = IndexDataFormat == D3DFMT_INDEX16 ? 2 : 4;
    const uint32_t indicesSize = drawInfo.vertexCount * indexSize;

    const uint32_t upSize = vertexBufferSize + indicesSize;

    auto upSlice = AllocTempBuffer<true>(upSize);
    uint8_t* data = reinterpret_cast<uint8_t*>(upSlice.mapPtr);
    FillUPVertexBuffer(data, pVertexStreamZeroData, vertexDataSize, vertexBufferSize);
    std::memcpy(data + vertexBufferSize, pIndexData, indicesSize);

    EmitCs([this,
      cVertexSize   = vertexBufferSize,
      cBufferSlice  = std::move(upSlice.slice),
      cPrimType     = PrimitiveType,
      cPrimCount    = PrimitiveCount,
      cStride       = VertexStreamZeroStride,
      cInstanceCount = GetInstanceCount(),
      cIndexType    = DecodeIndexType(
                        static_cast<D3D9Format>(IndexDataFormat))
    ](DxvkContext* ctx) {
      auto drawInfo = GenerateDrawInfo(cPrimType, cPrimCount, cInstanceCount);

      ApplyPrimitiveType(ctx, cPrimType);

      ctx->bindVertexBuffer(0, cBufferSlice.subSlice(0, cVertexSize), cStride);
      ctx->bindIndexBuffer(cBufferSlice.subSlice(cVertexSize, cBufferSlice.length() - cVertexSize), cIndexType);
      ctx->drawIndexed(
        drawInfo.vertexCount, drawInfo.instanceCount,
        0,
        0, 0);
      ctx->bindVertexBuffer(0, DxvkBufferSlice(), 0);
      ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);
    });

    m_state.vertexBuffers[0].vertexBuffer = nullptr;
    m_state.vertexBuffers[0].offset       = 0;
    m_state.vertexBuffers[0].stride       = 0;

    m_state.indices = nullptr;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ProcessVertices(
          UINT                         SrcStartIndex,
          UINT                         DestIndex,
          UINT                         VertexCount,
          IDirect3DVertexBuffer9*      pDestBuffer,
          IDirect3DVertexDeclaration9* pVertexDecl,
          DWORD                        Flags) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pDestBuffer == nullptr || pVertexDecl == nullptr))
      return D3DERR_INVALIDCALL;

    if (!SupportsSWVP()) {
      static bool s_errorShown = false;

      if (!std::exchange(s_errorShown, true))
        Logger::err("D3D9DeviceEx::ProcessVertices: SWVP emu unsupported (vertexPipelineStoresAndAtomics)");

      return D3D_OK;
    }

    D3D9CommonBuffer* dst  = static_cast<D3D9VertexBuffer*>(pDestBuffer)->GetCommonBuffer();
    D3D9VertexDecl*   decl = static_cast<D3D9VertexDecl*>  (pVertexDecl);

    PrepareDraw(D3DPT_FORCE_DWORD);

    if (decl == nullptr) {
      DWORD FVF = dst->Desc()->FVF;

      auto iter = m_fvfTable.find(FVF);

      if (iter == m_fvfTable.end()) {
        decl = new D3D9VertexDecl(this, FVF);
        m_fvfTable.insert(std::make_pair(FVF, decl));
      }
      else
        decl = iter->second.ptr();
    }

    uint32_t offset = DestIndex * decl->GetSize();

    auto slice = dst->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>();
         slice = slice.subSlice(offset, slice.length() - offset);

    EmitCs([this,
      cDecl          = ref(decl),
      cVertexCount   = VertexCount,
      cStartIndex    = SrcStartIndex,
      cInstanceCount = GetInstanceCount(),
      cBufferSlice   = slice,
      cIndexed       = m_state.indices != nullptr
    ](DxvkContext* ctx) {
      Rc<DxvkShader> shader = m_swvpEmulator.GetShaderModule(this, cDecl);

      auto drawInfo = GenerateDrawInfo(D3DPT_POINTLIST, cVertexCount, cInstanceCount);

      if (drawInfo.instanceCount != 1) {
        drawInfo.instanceCount = 1;

        Logger::warn("D3D9DeviceEx::ProcessVertices: instancing unsupported");
      }

      ApplyPrimitiveType(ctx, D3DPT_POINTLIST);

      // Unbind the pixel shader, we aren't drawing
      // to avoid val errors / UB.
      ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

      ctx->bindShader(VK_SHADER_STAGE_GEOMETRY_BIT, shader);
      ctx->bindResourceBuffer(getSWVPBufferSlot(), cBufferSlice);
      ctx->draw(
        drawInfo.vertexCount, drawInfo.instanceCount,
        cStartIndex, 0);
      ctx->bindResourceBuffer(getSWVPBufferSlot(), DxvkBufferSlice());
      ctx->bindShader(VK_SHADER_STAGE_GEOMETRY_BIT, nullptr);
    });

    // We unbound the pixel shader before,
    // let's make sure that gets rebound.
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

    if (m_state.pixelShader != nullptr) {
      BindShader<DxsoProgramTypes::PixelShader>(
        GetCommonShader(m_state.pixelShader),
        GetPixelShaderPermutation());
    }

    if (dst->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER) {
      uint32_t copySize = VertexCount * decl->GetSize();

      EmitCs([
        cSrcBuffer = dst->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>(),
        cDstBuffer = dst->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>(),
        cOffset    = offset,
        cCopySize  = copySize
      ](DxvkContext* ctx) {
        ctx->copyBuffer(cDstBuffer, cOffset, cSrcBuffer, cOffset, cCopySize);
      });
    }

    dst->SetWrittenByGPU(true);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateVertexDeclaration(
    const D3DVERTEXELEMENT9*            pVertexElements,
          IDirect3DVertexDeclaration9** ppDecl) {
    InitReturnPtr(ppDecl);

    if (unlikely(ppDecl == nullptr || pVertexElements == nullptr))
      return D3DERR_INVALIDCALL;

    const D3DVERTEXELEMENT9* counter = pVertexElements;
    while (counter->Stream != 0xFF)
      counter++;

    const uint32_t declCount = uint32_t(counter - pVertexElements);

    try {
      const Com<D3D9VertexDecl> decl = new D3D9VertexDecl(this, pVertexElements, declCount);
      *ppDecl = decl.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    D3D9DeviceLock lock = LockDevice();

    D3D9VertexDecl* decl = static_cast<D3D9VertexDecl*>(pDecl);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetVertexDeclaration(decl);

    if (decl == m_state.vertexDecl.ptr())
      return D3D_OK;

    bool dirtyFFShader = decl == nullptr || m_state.vertexDecl == nullptr;
    if (!dirtyFFShader)
      dirtyFFShader |= decl->TestFlag(D3D9VertexDeclFlag::HasPositionT)  != m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasPositionT)
                    || decl->TestFlag(D3D9VertexDeclFlag::HasColor0)     != m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasColor0)
                    || decl->TestFlag(D3D9VertexDeclFlag::HasColor1)     != m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasColor1)
                    || decl->GetTexcoordMask()                           != m_state.vertexDecl->GetTexcoordMask();

    if (dirtyFFShader)
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);

    m_state.vertexDecl = decl;

    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppDecl);

    if (ppDecl == nullptr)
      return D3D_OK;

    if (m_state.vertexDecl == nullptr)
      return D3D_OK;

    *ppDecl = m_state.vertexDecl.ref();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetFVF(DWORD FVF) {
    D3D9DeviceLock lock = LockDevice();

    if (FVF == 0)
      return D3D_OK;

    D3D9VertexDecl* decl = nullptr;

    auto iter = m_fvfTable.find(FVF);

    if (iter == m_fvfTable.end()) {
      decl = new D3D9VertexDecl(this, FVF);
      m_fvfTable.insert(std::make_pair(FVF, decl));
    }
    else
      decl = iter->second.ptr();

    return this->SetVertexDeclaration(decl);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetFVF(DWORD* pFVF) {
    D3D9DeviceLock lock = LockDevice();

    if (pFVF == nullptr)
      return D3DERR_INVALIDCALL;

    *pFVF = m_state.vertexDecl != nullptr
      ? m_state.vertexDecl->GetFVF()
      : 0;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateVertexShader(
    const DWORD*                   pFunction,
          IDirect3DVertexShader9** ppShader) {
    // CreateVertexShader does not init the
    // return ptr unlike CreatePixelShader

    if (unlikely(ppShader == nullptr))
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetVertexShader(IDirect3DVertexShader9* pShader) {
    D3D9DeviceLock lock = LockDevice();

    D3D9VertexShader* shader = static_cast<D3D9VertexShader*>(pShader);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetVertexShader(shader);

    if (shader == m_state.vertexShader.ptr())
      return D3D_OK;

    auto* oldShader = GetCommonShader(m_state.vertexShader);
    auto* newShader = GetCommonShader(shader);

    bool oldCopies = oldShader && oldShader->GetMeta().needsConstantCopies;
    bool newCopies = newShader && newShader->GetMeta().needsConstantCopies;

    m_consts[DxsoProgramTypes::VertexShader].dirty |= oldCopies || newCopies || !oldShader;
    m_consts[DxsoProgramTypes::VertexShader].meta  = newShader ? newShader->GetMeta() : DxsoShaderMetaInfo();

    if (newShader && oldShader) {
      m_consts[DxsoProgramTypes::VertexShader].dirty
        |= newShader->GetMeta().maxConstIndexF > oldShader->GetMeta().maxConstIndexF
        || newShader->GetMeta().maxConstIndexI > oldShader->GetMeta().maxConstIndexI
        || newShader->GetMeta().maxConstIndexB > oldShader->GetMeta().maxConstIndexB;
    }

    m_state.vertexShader = shader;

    if (shader != nullptr) {
      m_flags.clr(D3D9DeviceFlag::DirtyProgVertexShader);
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);

      BindShader<DxsoProgramTypes::VertexShader>(
        GetCommonShader(shader),
        GetVertexShaderPermutation());

      m_vsShaderMasks = newShader->GetShaderMask();
    }
    else
      m_vsShaderMasks = D3D9ShaderMasks();

    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppShader);

    if (unlikely(ppShader == nullptr))
      return D3DERR_INVALIDCALL;

    *ppShader = m_state.vertexShader.ref();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetVertexShaderConstantF(
          UINT   StartRegister,
    const float* pConstantData,
          UINT   Vector4fCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetVertexShaderConstantF(
          UINT   StartRegister,
          float* pConstantData,
          UINT   Vector4fCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetVertexShaderConstantI(
          UINT StartRegister,
    const int* pConstantData,
          UINT Vector4iCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetVertexShaderConstantI(
          UINT StartRegister,
          int* pConstantData,
          UINT Vector4iCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetVertexShaderConstantB(
          UINT  StartRegister,
    const BOOL* pConstantData,
          UINT  BoolCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetVertexShaderConstantB(
          UINT  StartRegister,
          BOOL* pConstantData,
          UINT  BoolCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::VertexShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetStreamSource(
          UINT                    StreamNumber,
          IDirect3DVertexBuffer9* pStreamData,
          UINT                    OffsetInBytes,
          UINT                    Stride) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(StreamNumber >= caps::MaxStreams))
      return D3DERR_INVALIDCALL;

    D3D9VertexBuffer* buffer = static_cast<D3D9VertexBuffer*>(pStreamData);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStreamSource(
        StreamNumber,
        buffer,
        OffsetInBytes,
        Stride);

    auto& vbo = m_state.vertexBuffers[StreamNumber];
    bool needsUpdate = vbo.vertexBuffer != buffer;

    if (needsUpdate)
      vbo.vertexBuffer = buffer;

    if (buffer != nullptr) {
      needsUpdate |= vbo.offset != OffsetInBytes
                  || vbo.stride != Stride;

      vbo.offset = OffsetInBytes;
      vbo.stride = Stride;
    }

    if (needsUpdate)
      BindVertexBuffer(StreamNumber, buffer, OffsetInBytes, Stride);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetStreamSource(
          UINT                     StreamNumber,
          IDirect3DVertexBuffer9** ppStreamData,
          UINT*                    pOffsetInBytes,
          UINT*                    pStride) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppStreamData);

    if (likely(pOffsetInBytes != nullptr))
      *pOffsetInBytes = 0;

    if (likely(pStride != nullptr))
      *pStride = 0;

    if (unlikely(ppStreamData == nullptr || pOffsetInBytes == nullptr || pStride == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(StreamNumber >= caps::MaxStreams))
      return D3DERR_INVALIDCALL;

    const auto& vbo = m_state.vertexBuffers[StreamNumber];

    *ppStreamData   = vbo.vertexBuffer.ref();
    *pOffsetInBytes = vbo.offset;
    *pStride        = vbo.stride;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(StreamNumber >= caps::MaxStreams))
      return D3DERR_INVALIDCALL;

    const bool indexed  = Setting & D3DSTREAMSOURCE_INDEXEDDATA;
    const bool instanced = Setting & D3DSTREAMSOURCE_INSTANCEDATA;

    if (unlikely(StreamNumber == 0 && instanced))
      return D3DERR_INVALIDCALL;

    if (unlikely(instanced && indexed))
      return D3DERR_INVALIDCALL;

    if (unlikely(Setting == 0))
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(StreamNumber >= caps::MaxStreams))
      return D3DERR_INVALIDCALL;

    if (unlikely(pSetting == nullptr))
      return D3DERR_INVALIDCALL;

    *pSetting = m_state.streamFreq[StreamNumber];

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    D3D9DeviceLock lock = LockDevice();

    D3D9IndexBuffer* buffer = static_cast<D3D9IndexBuffer*>(pIndexData);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetIndices(buffer);

    if (buffer == m_state.indices.ptr())
      return D3D_OK;

    m_state.indices = buffer;

    BindIndices();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    D3D9DeviceLock lock = LockDevice();
    InitReturnPtr(ppIndexData);

    if (unlikely(ppIndexData == nullptr))
      return D3DERR_INVALIDCALL;

    *ppIndexData = m_state.indices.ref();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreatePixelShader(
    const DWORD*                  pFunction,
          IDirect3DPixelShader9** ppShader) {
    InitReturnPtr(ppShader);

    if (unlikely(ppShader == nullptr))
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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetPixelShader(IDirect3DPixelShader9* pShader) {
    D3D9DeviceLock lock = LockDevice();

    D3D9PixelShader* shader = static_cast<D3D9PixelShader*>(pShader);

    if (unlikely(ShouldRecord()))
      return m_recorder->SetPixelShader(shader);

    if (shader == m_state.pixelShader.ptr())
      return D3D_OK;

    auto* oldShader = GetCommonShader(m_state.pixelShader);
    auto* newShader = GetCommonShader(shader);

    bool oldCopies = oldShader && oldShader->GetMeta().needsConstantCopies;
    bool newCopies = newShader && newShader->GetMeta().needsConstantCopies;

    m_consts[DxsoProgramTypes::PixelShader].dirty |= oldCopies || newCopies || !oldShader;
    m_consts[DxsoProgramTypes::PixelShader].meta  = newShader ? newShader->GetMeta() : DxsoShaderMetaInfo();

    if (newShader && oldShader) {
      m_consts[DxsoProgramTypes::PixelShader].dirty
        |= newShader->GetMeta().maxConstIndexF > oldShader->GetMeta().maxConstIndexF
        || newShader->GetMeta().maxConstIndexI > oldShader->GetMeta().maxConstIndexI
        || newShader->GetMeta().maxConstIndexB > oldShader->GetMeta().maxConstIndexB;
    }

    m_state.pixelShader = shader;

    if (shader != nullptr) {
      m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

      BindShader<DxsoProgramTypes::PixelShader>(
        GetCommonShader(shader),
        GetPixelShaderPermutation());

      m_psShaderMasks = newShader->GetShaderMask();
    }
    else {
      // TODO: What fixed function textures are in use?
      // Currently we are making all 8 of them as in use here.

      // The RT output is always 0 for fixed function.
      m_psShaderMasks = FixedFunctionMask;
    }

    UpdateActiveHazardsRT(UINT32_MAX);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppShader);

    if (unlikely(ppShader == nullptr))
      return D3DERR_INVALIDCALL;

    *ppShader = m_state.pixelShader.ref();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetPixelShaderConstantF(
    UINT   StartRegister,
    const float* pConstantData,
    UINT   Vector4fCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants <
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetPixelShaderConstantF(
    UINT   StartRegister,
    float* pConstantData,
    UINT   Vector4fCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Float>(
        StartRegister,
        pConstantData,
        Vector4fCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetPixelShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetPixelShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Int>(
        StartRegister,
        pConstantData,
        Vector4iCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetPixelShaderConstantB(
    UINT  StartRegister,
    const BOOL* pConstantData,
    UINT  BoolCount) {
    D3D9DeviceLock lock = LockDevice();

    return SetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetPixelShaderConstantB(
    UINT  StartRegister,
    BOOL* pConstantData,
    UINT  BoolCount) {
    D3D9DeviceLock lock = LockDevice();

    return GetShaderConstants<
      DxsoProgramTypes::PixelShader,
      D3D9ConstantType::Bool>(
        StartRegister,
        pConstantData,
        BoolCount);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawRectPatch(
          UINT               Handle,
    const float*             pNumSegs,
    const D3DRECTPATCH_INFO* pRectPatchInfo) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D9DeviceEx::DrawRectPatch: Stub");
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawTriPatch(
          UINT              Handle,
    const float*            pNumSegs,
    const D3DTRIPATCH_INFO* pTriPatchInfo) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D9DeviceEx::DrawTriPatch: Stub");
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DeletePatch(UINT Handle) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D9DeviceEx::DeletePatch: Stub");
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    HRESULT hr = D3D9Query::QuerySupported(this, Type);

    if (ppQuery == nullptr || hr != D3D_OK)
      return hr;

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


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetConvolutionMonoKernel(
          UINT   width,
          UINT   height,
          float* rows,
          float* columns) {
    // We don't advertise support for this.
    return D3DERR_INVALIDCALL;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ComposeRects(
          IDirect3DSurface9*      pSrc,
          IDirect3DSurface9*      pDst,
          IDirect3DVertexBuffer9* pSrcRectDescs,
          UINT                    NumRects,
          IDirect3DVertexBuffer9* pDstRectDescs,
          D3DCOMPOSERECTSOP       Operation,
          int                     Xoffset,
          int                     Yoffset) {
    Logger::warn("D3D9DeviceEx::ComposeRects: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetGPUThreadPriority(INT* pPriority) {
    Logger::warn("D3D9DeviceEx::GetGPUThreadPriority: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetGPUThreadPriority(INT Priority) {
    Logger::warn("D3D9DeviceEx::SetGPUThreadPriority: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::WaitForVBlank(UINT iSwapChain) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->WaitForVBlank();
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) {
    Logger::warn("D3D9DeviceEx::CheckResourceResidency: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetMaximumFrameLatency(UINT MaxLatency) {
    D3D9DeviceLock lock = LockDevice();

    if (MaxLatency == 0)
      MaxLatency = DefaultFrameLatency;

    if (MaxLatency > MaxFrameLatency)
      MaxLatency = MaxFrameLatency;

    m_frameLatency = MaxLatency;

    m_implicitSwapchain->SyncFrameLatency();

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetMaximumFrameLatency(UINT* pMaxLatency) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(pMaxLatency == nullptr))
      return D3DERR_INVALIDCALL;

    *pMaxLatency = m_frameLatency;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CheckDeviceState(HWND hDestinationWindow) {
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::PresentEx(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
          DWORD dwFlags) {
    return m_implicitSwapchain->Present(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      dwFlags);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateRenderTargetEx(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          DWORD               MultisampleQuality,
          BOOL                Lockable,
          IDirect3DSurface9** ppSurface,
          HANDLE*             pSharedHandle,
          DWORD               Usage) {
    InitReturnPtr(ppSurface);

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.ArraySize          = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage | D3DUSAGE_RENDERTARGET;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = D3DPOOL_DEFAULT;
    desc.Discard            = FALSE;
    desc.MultiSample        = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = TRUE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, nullptr);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateOffscreenPlainSurfaceEx(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DPOOL             Pool,
          IDirect3DSurface9** ppSurface,
          HANDLE*             pSharedHandle,
          DWORD               Usage) {
    InitReturnPtr(ppSurface);

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.ArraySize          = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = Pool;
    desc.Discard            = FALSE;
    desc.MultiSample        = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = Pool == D3DPOOL_DEFAULT;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, nullptr);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateDepthStencilSurfaceEx(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          DWORD               MultisampleQuality,
          BOOL                Discard,
          IDirect3DSurface9** ppSurface,
          HANDLE*             pSharedHandle,
          DWORD               Usage) {
    InitReturnPtr(ppSurface);

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = Width;
    desc.Height             = Height;
    desc.Depth              = 1;
    desc.ArraySize          = 1;
    desc.MipLevels          = 1;
    desc.Usage              = Usage | D3DUSAGE_DEPTHSTENCIL;
    desc.Format             = EnumerateFormat(Format);
    desc.Pool               = D3DPOOL_DEFAULT;
    desc.Discard            = Discard;
    desc.MultiSample        = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = TRUE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, nullptr);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ResetEx(
          D3DPRESENT_PARAMETERS* pPresentationParameters,
          D3DDISPLAYMODEEX*      pFullscreenDisplayMode) {
    D3D9DeviceLock lock = LockDevice();

    HRESULT hr = ResetSwapChain(pPresentationParameters, pFullscreenDisplayMode);
    if (FAILED(hr))
      return hr;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetDisplayModeEx(
          UINT                iSwapChain,
          D3DDISPLAYMODEEX*   pMode,
          D3DDISPLAYROTATION* pRotation) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    return m_implicitSwapchain->GetDisplayModeEx(pMode, pRotation);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::CreateAdditionalSwapChainEx(
          D3DPRESENT_PARAMETERS* pPresentationParameters,
    const D3DDISPLAYMODEEX*      pFullscreenDisplayMode,
          IDirect3DSwapChain9**  ppSwapChain) {
    D3D9DeviceLock lock = LockDevice();

    InitReturnPtr(ppSwapChain);

    if (ppSwapChain == nullptr || pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    // Additional fullscreen swapchains are forbidden.
    if (!pPresentationParameters->Windowed)
      return D3DERR_INVALIDCALL;

    // We can't make another swapchain if we are fullscreen.
    if (!m_implicitSwapchain->GetPresentParams()->Windowed)
      return D3DERR_INVALIDCALL;

    m_implicitSwapchain->Invalidate(pPresentationParameters->hDeviceWindow);

    try {
      auto* swapchain = new D3D9SwapChainEx(this, pPresentationParameters, pFullscreenDisplayMode);
      *ppSwapChain = ref(swapchain);
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::SetStateSamplerState(
    DWORD               StateSampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD               Value) {
    D3D9DeviceLock lock = LockDevice();

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
      else if (Type == D3DSAMP_SRGBTEXTURE && m_state.textures[StateSampler] != nullptr)
        m_dirtyTextures |= 1u << StateSampler;

      constexpr DWORD Fetch4Enabled  = MAKEFOURCC('G', 'E', 'T', '4');
      constexpr DWORD Fetch4Disabled = MAKEFOURCC('G', 'E', 'T', '1');

      if (Type == D3DSAMP_MIPMAPLODBIAS) {
        if (Value == Fetch4Enabled) {
          m_fetch4Enabled |= 1u << StateSampler;
          if (state[StateSampler][D3DSAMP_MAGFILTER] == D3DTEXF_POINT)
            m_fetch4 |= 1u << StateSampler;
        }
        else if (Value == Fetch4Disabled) {
          m_fetch4Enabled &= ~(1u << StateSampler);
          m_fetch4        &= ~(1u << StateSampler);
        }
      }

      if (Type == D3DSAMP_MAGFILTER && m_fetch4Enabled & (1u << StateSampler)) {
        if (Value == D3DTEXF_POINT)
          m_fetch4 |=   1u << StateSampler;
        else
          m_fetch4 &= ~(1u << StateSampler);
      }
    }

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateTexture(StateSampler, pTexture);

    if (m_state.textures[StateSampler] == pTexture)
      return D3D_OK;

    // We need to check our ops and disable respective stages.
    // Given we have transition from a null resource to
    // a valid resource or vice versa.
    if (StateSampler < 16 && (pTexture == nullptr || m_state.textures[StateSampler] == nullptr))
      m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

    auto oldTexture = GetCommonTexture(m_state.textures[StateSampler]);
    auto newTexture = GetCommonTexture(pTexture);

    DWORD oldUsage = oldTexture != nullptr ? oldTexture->Desc()->Usage : 0;
    DWORD newUsage = newTexture != nullptr ? newTexture->Desc()->Usage : 0;

    DWORD combinedUsage = oldUsage | newUsage;

    TextureChangePrivate(m_state.textures[StateSampler], pTexture);

    m_dirtyTextures |= 1u << StateSampler;

    UpdateActiveTextures(StateSampler, combinedUsage);

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::SetStateTransform(uint32_t idx, const D3DMATRIX* pMatrix) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateTransform(idx, pMatrix);

    m_state.transforms[idx] = ConvertMatrix(pMatrix);

    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    if (idx == GetTransformIndex(D3DTS_VIEW) || idx >= GetTransformIndex(D3DTS_WORLD))
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::SetStateTextureStageState(
          DWORD                      Stage,
          D3D9TextureStageStateTypes Type,
          DWORD                      Value) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(Stage >= caps::TextureStageCount))
      return D3DERR_INVALIDCALL;

    if (unlikely(Type >= TextureStageStateCount))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateTextureStageState(Stage, Type, Value);

    if (likely(m_state.textureStages[Stage][Type] != Value)) {
      m_state.textureStages[Stage][Type] = Value;

      switch (Type) {
        case DXVK_TSS_COLOROP:
        case DXVK_TSS_COLORARG0:
        case DXVK_TSS_COLORARG1:
        case DXVK_TSS_COLORARG2:
        case DXVK_TSS_ALPHAOP:
        case DXVK_TSS_ALPHAARG0:
        case DXVK_TSS_ALPHAARG1:
        case DXVK_TSS_ALPHAARG2:
        case DXVK_TSS_RESULTARG:
          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
          break;

        case DXVK_TSS_TEXCOORDINDEX:
          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          break;

        case DXVK_TSS_TEXTURETRANSFORMFLAGS:
          m_projectionBitfield &= ~(1 << Stage);
          if (Value & D3DTTFF_PROJECTED)
            m_projectionBitfield |= 1 << Stage;

          m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
          m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
          break;

        case DXVK_TSS_BUMPENVMAT00:
        case DXVK_TSS_BUMPENVMAT01:
        case DXVK_TSS_BUMPENVMAT10:
        case DXVK_TSS_BUMPENVMAT11:
        case DXVK_TSS_BUMPENVLSCALE:
        case DXVK_TSS_BUMPENVLOFFSET:
        case DXVK_TSS_CONSTANT:
          m_flags.set(D3D9DeviceFlag::DirtySharedPixelShaderData);
          break;

        default: break;
      }
    }

    return D3D_OK;
  }


  bool D3D9DeviceEx::IsExtended() {
    return m_parent->IsExtended();
  }


  bool D3D9DeviceEx::SupportsSWVP() {
    return m_dxvkDevice->features().core.features.vertexPipelineStoresAndAtomics;
  }


  HWND D3D9DeviceEx::GetWindow() {
    return m_window;
  }


  DxvkDeviceFeatures D3D9DeviceEx::GetDeviceFeatures(const Rc<DxvkAdapter>& adapter) {
    DxvkDeviceFeatures supported = adapter->features();
    DxvkDeviceFeatures enabled = {};

    // Geometry shaders are used for some meta ops
    enabled.core.features.geometryShader = VK_TRUE;
    enabled.core.features.robustBufferAccess = VK_TRUE;
    enabled.extRobustness2.robustBufferAccess2 = supported.extRobustness2.robustBufferAccess2;

    enabled.extMemoryPriority.memoryPriority = supported.extMemoryPriority.memoryPriority;

    enabled.extShaderDemoteToHelperInvocation.shaderDemoteToHelperInvocation = supported.extShaderDemoteToHelperInvocation.shaderDemoteToHelperInvocation;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;

    // Null Descriptors
    enabled.extRobustness2.nullDescriptor = supported.extRobustness2.nullDescriptor;

    // ProcessVertices
    enabled.core.features.vertexPipelineStoresAndAtomics = supported.core.features.vertexPipelineStoresAndAtomics;

    // DXVK Meta
    enabled.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    enabled.core.features.imageCubeArray = VK_TRUE;

    // SM1 level hardware
    enabled.core.features.depthClamp = VK_TRUE;
    enabled.core.features.depthBiasClamp = VK_TRUE;
    enabled.core.features.fillModeNonSolid = VK_TRUE;
    enabled.core.features.pipelineStatisticsQuery = supported.core.features.pipelineStatisticsQuery;
    enabled.core.features.sampleRateShading = VK_TRUE;
    enabled.core.features.samplerAnisotropy = supported.core.features.samplerAnisotropy;
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

    // Enable depth bounds test if we support it.
    enabled.core.features.depthBounds = supported.core.features.depthBounds;

    if (supported.extCustomBorderColor.customBorderColorWithoutFormat) {
      enabled.extCustomBorderColor.customBorderColors             = VK_TRUE;
      enabled.extCustomBorderColor.customBorderColorWithoutFormat = VK_TRUE;
    }

    return enabled;
  }


  void D3D9DeviceEx::DetermineConstantLayouts(bool canSWVP) {
    m_vsLayout.floatCount    = canSWVP ? uint32_t(m_d3d9Options.swvpFloatCount) : caps::MaxFloatConstantsVS;
    m_vsLayout.intCount      = canSWVP ? uint32_t(m_d3d9Options.swvpIntCount)   : caps::MaxOtherConstants;
    m_vsLayout.boolCount     = canSWVP ? uint32_t(m_d3d9Options.swvpBoolCount)  : caps::MaxOtherConstants;
    m_vsLayout.bitmaskCount  = align(m_vsLayout.boolCount, 32) / 32;

    m_psLayout.floatCount    = caps::MaxFloatConstantsPS;
    m_psLayout.intCount      = caps::MaxOtherConstants;
    m_psLayout.boolCount     = caps::MaxOtherConstants;
    m_psLayout.bitmaskCount = align(m_psLayout.boolCount, 32) / 32;
  }


  template<bool UpBuffer>
  D3D9BufferSlice D3D9DeviceEx::AllocTempBuffer(VkDeviceSize size) {
    constexpr VkDeviceSize DefaultSize = 1 << 20;

    VkMemoryPropertyFlags memoryFlags
      = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if constexpr (UpBuffer) {
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    D3D9BufferSlice& currentSlice = UpBuffer ? m_upBuffer : m_managedUploadBuffer;

    if (size <= DefaultSize) {
      if (unlikely(!currentSlice.slice.defined())) {
        DxvkBufferCreateInfo info;
        info.size   = DefaultSize;
        if constexpr (UpBuffer) {
          info.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                      | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
          info.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                      | VK_ACCESS_INDEX_READ_BIT;
          info.stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        } else {
          info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
          info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
          info.access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        }

        currentSlice.slice  = DxvkBufferSlice(m_dxvkDevice->createBuffer(info, memoryFlags));
        currentSlice.mapPtr = currentSlice.slice.mapPtr(0);
      } else if (unlikely(currentSlice.slice.length() < size)) {
        auto physSlice = currentSlice.slice.buffer()->allocSlice();

        currentSlice.slice  = DxvkBufferSlice(currentSlice.slice.buffer());
        currentSlice.mapPtr = physSlice.mapPtr;

        EmitCs([
          cBuffer = currentSlice.slice.buffer(),
          cSlice  = physSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cBuffer, cSlice);
        });
      }

      D3D9BufferSlice result;
      result.slice  = currentSlice.slice.subSlice(0, size);
      result.mapPtr = reinterpret_cast<char*>(currentSlice.mapPtr) + currentSlice.slice.offset();

      VkDeviceSize adjust = align(size, CACHE_LINE_SIZE);
      currentSlice.slice = currentSlice.slice.subSlice(adjust, currentSlice.slice.length() - adjust);
      return result;
    } else {
      // Create a temporary buffer for very large allocations
      DxvkBufferCreateInfo info;
      info.size   = size;
      if constexpr (UpBuffer) {
        info.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        info.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                    | VK_ACCESS_INDEX_READ_BIT;
        info.stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      } else {
        info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        info.access = VK_ACCESS_TRANSFER_READ_BIT;
      }

      D3D9BufferSlice result;
      result.slice  = DxvkBufferSlice(m_dxvkDevice->createBuffer(info, memoryFlags));
      result.mapPtr = result.slice.mapPtr(0);
      return result;
    }
  }

  bool D3D9DeviceEx::ShouldRecord() {
    return m_recorder != nullptr && !m_recorder->IsApplying();
  }


  D3D9_VK_FORMAT_MAPPING D3D9DeviceEx::LookupFormat(
    D3D9Format            Format) const {
    return m_adapter->GetFormatMapping(Format);
  }

  DxvkFormatInfo D3D9DeviceEx::UnsupportedFormatInfo(
    D3D9Format            Format) const {
    return m_adapter->GetUnsupportedFormatInfo(Format);
  }

  bool D3D9DeviceEx::WaitForResource(
  const Rc<DxvkResource>&                 Resource,
        DWORD                             MapFlags) {
    // Wait for the any pending D3D9 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.

    // Determine access type to wait for based on map mode
    DxvkAccess access = (MapFlags & D3DLOCK_READONLY)
      ? DxvkAccess::Write
      : DxvkAccess::Read;

    if (!Resource->isInUse(access))
      SynchronizeCsThread();

    if (Resource->isInUse(access)) {
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

        Resource->waitIdle(access);
      }
    }

    return true;
  }


  uint32_t D3D9DeviceEx::CalcImageLockOffset(
            uint32_t                SlicePitch,
            uint32_t                RowPitch,
      const DxvkFormatInfo*         FormatInfo,
      const D3DBOX*                 pBox) {
    if (pBox == nullptr)
      return 0;

    std::array<uint32_t, 3> offsets = { pBox->Front, pBox->Top, pBox->Left };

    uint32_t elementSize = 1;

    if (FormatInfo != nullptr) {
      elementSize = FormatInfo->elementSize;

      offsets[0] = offsets[0] / FormatInfo->blockSize.depth;
      offsets[1] = offsets[1] / FormatInfo->blockSize.height;
      offsets[2] = offsets[2] / FormatInfo->blockSize.width;
    }

    return offsets[0] * SlicePitch +
           offsets[1] * RowPitch   +
           offsets[2] * elementSize;
  }


  HRESULT D3D9DeviceEx::LockImage(
            D3D9CommonTexture*      pResource,
            UINT                    Face,
            UINT                    MipLevel,
            D3DLOCKED_BOX*          pLockedBox,
      const D3DBOX*                 pBox,
            DWORD                   Flags) {
    D3D9DeviceLock lock = LockDevice();

    UINT Subresource = pResource->CalcSubresource(Face, MipLevel);

    // Don't allow multiple lockings.
    if (unlikely(pResource->GetLocked(Subresource)))
      return D3DERR_INVALIDCALL;

    if (unlikely((Flags & (D3DLOCK_DISCARD | D3DLOCK_READONLY)) == (D3DLOCK_DISCARD | D3DLOCK_READONLY)))
      return D3DERR_INVALIDCALL;

    if (unlikely(!m_d3d9Options.allowDoNotWait))
      Flags &= ~D3DLOCK_DONOTWAIT;

    if (unlikely((Flags & (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)) == (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)))
      Flags &= ~D3DLOCK_DISCARD;

    auto& desc = *(pResource->Desc());

    bool alloced = pResource->CreateBufferSubresource(Subresource);

    const Rc<DxvkBuffer> mappedBuffer = pResource->GetBuffer(Subresource);

    auto formatInfo = imageFormatInfo(pResource->GetFormatMapping().FormatColor);
    auto subresource = pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);

    VkExtent3D levelExtent = pResource->GetExtentMip(MipLevel);
    VkExtent3D blockCount  = util::computeBlockCount(levelExtent, formatInfo->blockSize);

    const bool systemmem = desc.Pool == D3DPOOL_SYSTEMMEM;
    const bool managed   = IsPoolManaged(desc.Pool);
    const bool scratch   = desc.Pool == D3DPOOL_SCRATCH;

    bool fullResource = pBox == nullptr;
    if (unlikely(!fullResource)) {
      VkOffset3D lockOffset;
      VkExtent3D lockExtent;

      ConvertBox(*pBox, lockOffset, lockExtent);

      fullResource = lockOffset == VkOffset3D{ 0, 0, 0 }
                  && lockExtent.width  >= levelExtent.width
                  && lockExtent.height >= levelExtent.height
                  && lockExtent.depth  >= levelExtent.depth;
    }

    // If we are not locking the entire image
    // a partial discard is meant to occur.
    // We can't really implement that, so just ignore discard
    // if we are not locking the full resource

    // DISCARD is also ignored for MANAGED and SYSTEMEM.
    // DISCARD is not ignored for non-DYNAMIC unlike what the docs say.

    if (!fullResource || desc.Pool != D3DPOOL_DEFAULT)
      Flags &= ~D3DLOCK_DISCARD;

    if (desc.Usage & D3DUSAGE_WRITEONLY)
      Flags &= ~D3DLOCK_READONLY;

    const bool readOnly = Flags & D3DLOCK_READONLY;
    pResource->SetReadOnlyLocked(Subresource, readOnly);

    bool renderable = desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL);

    // If we recently wrote to the texture on the gpu,
    // then we need to copy -> buffer
    // We are also always dirty if we are a render target,
    // a depth stencil, or auto generate mipmaps.
    bool wasWrittenByGPU = pResource->WasWrittenByGPU(Subresource) || renderable;
    pResource->SetWrittenByGPU(Subresource, false);

    DxvkBufferSliceHandle physSlice;

    if (Flags & D3DLOCK_DISCARD) {
      // We do not have to preserve the contents of the
      // buffer if the entire image gets discarded.
      physSlice = pResource->DiscardMapSlice(Subresource);

      EmitCs([
        cImageBuffer = std::move(mappedBuffer),
        cBufferSlice = physSlice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
      });
    }
    else if ((managed && !m_d3d9Options.evictManagedOnUnlock) || scratch || systemmem) {
      // Managed and scratch resources
      // are meant to be able to provide readback without waiting.
      // We always keep a copy of them in system memory for this reason.
      // No need to wait as its not in use.
      physSlice = pResource->GetMappedSlice(Subresource);

      // We do not need to wait for the resource in the event the
      // calling app promises not to overwrite data that is in use
      // or is reading. Remember! This will only trigger for MANAGED resources
      // that cannot get affected by GPU, therefore readonly is A-OK for NOT waiting.
      const bool usesStagingBuffer = pResource->DoesStagingBufferUploads(Subresource);
      const bool skipWait = (scratch || managed || (systemmem && !wasWrittenByGPU))
        && (usesStagingBuffer || readOnly);

      if (alloced) {
        std::memset(physSlice.mapPtr, 0, physSlice.length);
      }
      else if (!skipWait) {
        if (!(Flags & D3DLOCK_DONOTWAIT) && !WaitForResource(mappedBuffer, D3DLOCK_DONOTWAIT))
          pResource->EnableStagingBufferUploads(Subresource);

        if (!WaitForResource(mappedBuffer, Flags))
          return D3DERR_WASSTILLDRAWING;
      }
    }
    else {
      physSlice = mappedBuffer->getSliceHandle();

      if (unlikely(wasWrittenByGPU)) {
        Rc<DxvkImage> resourceImage = pResource->GetImage();

        Rc<DxvkImage> mappedImage = resourceImage->info().sampleCount != 1
          ? pResource->GetResolveImage()
          : std::move(resourceImage);

        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        auto subresourceLayers = vk::makeSubresourceLayers(subresource);

        // We need to resolve this, some games
        // lock MSAA render targets even though
        // that's entirely illegal and they explicitly
        // tell us that they do NOT want to lock them...
        if (resourceImage != nullptr) {
          EmitCs([
            cMainImage    = resourceImage,
            cResolveImage = mappedImage,
            cSubresource  = subresourceLayers
          ] (DxvkContext* ctx) {
            VkImageResolve region;
            region.srcSubresource = cSubresource;
            region.srcOffset      = VkOffset3D { 0, 0, 0 };
            region.dstSubresource = cSubresource;
            region.dstOffset      = VkOffset3D { 0, 0, 0 };
            region.extent         = cMainImage->mipLevelExtent(cSubresource.mipLevel);

            if (cSubresource.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
              ctx->resolveImage(
                cResolveImage, cMainImage, region,
                cMainImage->info().format);
            }
            else {
              ctx->resolveDepthStencilImage(
                cResolveImage, cMainImage, region,
                VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR,
                VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR);
            }
          });
        }

        VkFormat packedFormat = GetPackedDepthStencilFormat(desc.Format);

        EmitCs([
          cImageBuffer  = mappedBuffer,
          cImage        = std::move(mappedImage),
          cSubresources = subresourceLayers,
          cLevelExtent  = levelExtent,
          cPackedFormat = packedFormat
        ] (DxvkContext* ctx) {
          if (cSubresources.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
            ctx->copyImageToBuffer(cImageBuffer, 0, 4, 0,
              cImage, cSubresources, VkOffset3D { 0, 0, 0 },
              cLevelExtent);
          } else {
            // Copying DS to a packed buffer is only supported for D24S8 and D32S8
            // right now so the 4 byte row alignment is guaranteed by the format size
            ctx->copyDepthStencilImageToPackedBuffer(
              cImageBuffer, 0,
              VkOffset2D { 0, 0 },
              VkExtent2D { cLevelExtent.width, cLevelExtent.height },
              cImage, cSubresources,
              VkOffset2D { 0, 0 },
              VkExtent2D { cLevelExtent.width, cLevelExtent.height },
              cPackedFormat);
          }
        });

        if (!WaitForResource(mappedBuffer, Flags))
          return D3DERR_WASSTILLDRAWING;
      } else if (alloced) {
        // If we are a new alloc, and we weren't written by the GPU
        // that means that we are a newly initialized
        // texture, and hence can just memset -> 0 and
        // avoid a wait here.
        std::memset(physSlice.mapPtr, 0, physSlice.length);
      }
    }

    const bool atiHack = desc.Format == D3D9Format::ATI1 || desc.Format == D3D9Format::ATI2;
    // Set up map pointer.
    if (atiHack) {
      // We need to lie here. The game is expected to use this info and do a workaround.
      // It's stupid. I know.
      pLockedBox->RowPitch   = align(std::max(desc.Width >> MipLevel, 1u), 4);
      pLockedBox->SlicePitch = pLockedBox->RowPitch * std::max(desc.Height >> MipLevel, 1u);
    }
    else {
      // Data is tightly packed within the mapped buffer.
      pLockedBox->RowPitch   = align(formatInfo->elementSize * blockCount.width, 4);
      pLockedBox->SlicePitch = pLockedBox->RowPitch * blockCount.height;
    }

    pResource->SetLocked(Subresource, true);

    if (!(Flags & D3DLOCK_NO_DIRTY_UPDATE) && !(Flags & D3DLOCK_READONLY)) {
      if (pBox && MipLevel != 0) {
        D3DBOX scaledBox = *pBox;
        scaledBox.Left   <<= MipLevel;
        scaledBox.Right    = std::min(scaledBox.Right << MipLevel, pResource->Desc()->Width);
        scaledBox.Top    <<= MipLevel;
        scaledBox.Bottom   = std::min(scaledBox.Bottom << MipLevel, pResource->Desc()->Height);
        scaledBox.Back   <<= MipLevel;
        scaledBox.Front    = std::min(scaledBox.Front << MipLevel, pResource->Desc()->Depth);
        pResource->AddDirtyBox(&scaledBox, Face);
      } else {
        pResource->AddDirtyBox(pBox, Face);
      }
    }

    if (managed && !m_d3d9Options.evictManagedOnUnlock && !readOnly) {
      pResource->SetNeedsUpload(Subresource, true);

      for (uint32_t tex = m_activeTextures; tex; tex &= tex - 1) {
        // Guaranteed to not be nullptr...
        const uint32_t i = bit::tzcnt(tex);
        auto texInfo = GetCommonTexture(m_state.textures[i]);

        if (texInfo == pResource) {
          m_activeTexturesToUpload |= 1 << i;
          // We can early out here, no need to add another index for this.
          break;
        }
      }
    }

    const uint32_t offset = CalcImageLockOffset(
      pLockedBox->SlicePitch,
      pLockedBox->RowPitch,
      (!atiHack) ? formatInfo : nullptr,
      pBox);


    uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
    data += offset;
    pLockedBox->pBits = data;
    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::UnlockImage(
        D3D9CommonTexture*      pResource,
        UINT                    Face,
        UINT                    MipLevel) {
    D3D9DeviceLock lock = LockDevice();

    UINT Subresource = pResource->CalcSubresource(Face, MipLevel);

    // We weren't locked anyway!
    if (unlikely(!pResource->GetLocked(Subresource)))
      return D3D_OK;

    pResource->SetLocked(Subresource, false);

    // Flush image contents from staging if we aren't read only
    // and we aren't deferring for managed.
    bool shouldFlush  = pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
         shouldFlush &= !pResource->GetReadOnlyLocked(Subresource);
         shouldFlush &= !pResource->IsManaged() || m_d3d9Options.evictManagedOnUnlock;

    if (shouldFlush) {
        this->FlushImage(pResource, Subresource);
        if (!pResource->IsAnySubresourceLocked())
          pResource->ClearDirtyBoxes();
    }

    // Toss our staging buffer if we're not dynamic
    // and we aren't managed (for sysmem copy.)
    bool shouldToss  = pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
         shouldToss &= !pResource->IsDynamic();
         shouldToss &= !pResource->IsManaged() || m_d3d9Options.evictManagedOnUnlock;

    if (shouldToss) {
      pResource->DestroyBufferSubresource(Subresource);
      pResource->SetWrittenByGPU(Subresource, true);
    }

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::FlushImage(
        D3D9CommonTexture*      pResource,
        UINT                    Subresource) {
    const Rc<DxvkImage> image = pResource->GetImage();

    // Now that data has been written into the buffer,
    // we need to copy its contents into the image
    const DxvkBufferSliceHandle srcSlice = pResource->GetMappedSlice(Subresource);

    auto formatInfo  = imageFormatInfo(image->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
      formatInfo->aspectMask, Subresource);

    VkImageSubresourceLayers subresourceLayers = {
      subresource.aspectMask,
      subresource.mipLevel,
      subresource.arrayLayer, 1 };

    auto convertFormat = pResource->GetFormatMapping().ConversionFormatInfo;

    if (likely(convertFormat.FormatType == D3D9ConversionFormat_None)) {
      VkImageSubresourceLayers dstLayers = { VK_IMAGE_ASPECT_COLOR_BIT, subresource.mipLevel, subresource.arrayLayer, 1 };

      const D3DBOX& box = pResource->GetDirtyBox(subresource.arrayLayer);
      VkOffset3D scaledBoxOffset = {
        int32_t(alignDown(box.Left  >> subresource.mipLevel, formatInfo->blockSize.width)),
        int32_t(alignDown(box.Top   >> subresource.mipLevel, formatInfo->blockSize.height)),
        int32_t(alignDown(box.Front >> subresource.mipLevel, formatInfo->blockSize.depth))
      };
      VkExtent3D scaledBoxExtent = util::computeMipLevelExtent({
        uint32_t(box.Right  - int32_t(alignDown(box.Left, formatInfo->blockSize.width))),
        uint32_t(box.Bottom - int32_t(alignDown(box.Top, formatInfo->blockSize.height))),
        uint32_t(box.Back   - int32_t(alignDown(box.Front, formatInfo->blockSize.depth)))
      }, subresource.mipLevel);
      VkExtent3D scaledBoxExtentBlockCount = util::computeBlockCount(scaledBoxExtent, formatInfo->blockSize);
      VkExtent3D scaledAlignedBoxExtent = util::computeBlockExtent(scaledBoxExtentBlockCount, formatInfo->blockSize);

      VkExtent3D texLevelExtent = image->mipLevelExtent(subresource.mipLevel);
      VkExtent3D texLevelExtentBlockCount = util::computeBlockCount(texLevelExtent, formatInfo->blockSize);

      scaledAlignedBoxExtent.width = std::min<uint32_t>(texLevelExtent.width - scaledBoxOffset.x, scaledAlignedBoxExtent.width);
      scaledAlignedBoxExtent.height = std::min<uint32_t>(texLevelExtent.height - scaledBoxOffset.y, scaledAlignedBoxExtent.height);
      scaledAlignedBoxExtent.depth = std::min<uint32_t>(texLevelExtent.depth - scaledBoxOffset.z, scaledAlignedBoxExtent.depth);

      VkOffset3D boxOffsetBlockCount = util::computeBlockOffset(scaledBoxOffset, formatInfo->blockSize);
      VkDeviceSize copySrcOffset = (boxOffsetBlockCount.z * texLevelExtentBlockCount.height * texLevelExtentBlockCount.width
          + boxOffsetBlockCount.y * texLevelExtentBlockCount.width
          + boxOffsetBlockCount.x)
          * formatInfo->elementSize;

      VkDeviceSize rowAlignment = 0;
      DxvkBufferSlice copySrcSlice;
      if (pResource->DoesStagingBufferUploads(Subresource)) {
        VkDeviceSize dirtySize = scaledBoxExtentBlockCount.width * scaledBoxExtentBlockCount.height * scaledBoxExtentBlockCount.depth * formatInfo->elementSize;
        VkDeviceSize pitch = align(texLevelExtentBlockCount.width * formatInfo->elementSize, 4);
        D3D9BufferSlice slice = AllocTempBuffer<false>(dirtySize);
        copySrcSlice = slice.slice;
        void* srcData = reinterpret_cast<uint8_t*>(srcSlice.mapPtr) + copySrcOffset;
        util::packImageData(
          slice.mapPtr, srcData, scaledBoxExtentBlockCount, formatInfo->elementSize,
          pitch, pitch * texLevelExtentBlockCount.height);
      } else {
        copySrcSlice = DxvkBufferSlice(pResource->GetBuffer(Subresource), copySrcOffset, srcSlice.length);
        rowAlignment = 4;
      }

      EmitCs([
        cSrcSlice       = std::move(copySrcSlice),
        cDstImage       = image,
        cDstLayers      = dstLayers,
        cDstLevelExtent = scaledAlignedBoxExtent,
        cOffset         = scaledBoxOffset,
        cRowAlignment   = rowAlignment
      ] (DxvkContext* ctx) {
        ctx->copyBufferToImage(
          cDstImage,  cDstLayers,
          cOffset, cDstLevelExtent,
          cSrcSlice.buffer(), cSrcSlice.offset(),
          cRowAlignment, 0);
      });
    }
    else {
      const DxvkFormatInfo* formatInfo = imageFormatInfo(pResource->GetFormatMapping().FormatColor);
      VkExtent3D texLevelExtent = image->mipLevelExtent(subresource.mipLevel);
      VkExtent3D texLevelExtentBlockCount = util::computeBlockCount(texLevelExtent, formatInfo->blockSize);
      // Add more blocks for the other planes that we might have.
      // TODO: PLEASE CLEAN ME
      texLevelExtentBlockCount.height *= std::min(convertFormat.PlaneCount, 2u);

      // the converter can not handle the 4 aligned pitch so we always repack into a staging buffer
      D3D9BufferSlice slice = AllocTempBuffer<false>(srcSlice.length);
      VkDeviceSize pitch = align(texLevelExtentBlockCount.width * formatInfo->elementSize, 4);

      util::packImageData(
        slice.mapPtr, srcSlice.mapPtr, texLevelExtentBlockCount, formatInfo->elementSize,
        pitch, std::min(convertFormat.PlaneCount, 2u) * pitch * texLevelExtentBlockCount.height);

      Flush();
      SynchronizeCsThread();

      m_converter->ConvertFormat(
        convertFormat,
        image, subresourceLayers,
        slice.slice);
    }

    if (pResource->IsAutomaticMip())
      MarkTextureMipsDirty(pResource);

    return D3D_OK;
  }


  void D3D9DeviceEx::EmitGenerateMips(
    D3D9CommonTexture* pResource) {
    if (pResource->IsManaged())
      UploadManagedTexture(pResource);

    EmitCs([
      cImageView = pResource->GetSampleView(false),
      cFilter    = pResource->GetMipFilter()
    ] (DxvkContext* ctx) {
      ctx->generateMipmaps(cImageView, DecodeFilter(cFilter));
    });
  }


  HRESULT D3D9DeviceEx::LockBuffer(
          D3D9CommonBuffer*       pResource,
          UINT                    OffsetToLock,
          UINT                    SizeToLock,
          void**                  ppbData,
          DWORD                   Flags) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(ppbData == nullptr))
      return D3DERR_INVALIDCALL;

    if (!m_d3d9Options.allowDiscard)
      Flags &= ~D3DLOCK_DISCARD;

    auto& desc = *pResource->Desc();

    // Ignore DISCARD if NOOVERWRITE is set
    if (unlikely((Flags & (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)) == (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)))
      Flags &= ~D3DLOCK_DISCARD;

    // Ignore DISCARD and NOOVERWRITE if the buffer is not DEFAULT pool (tests + Halo 2)
    // The docs say DISCARD and NOOVERWRITE are ignored if the buffer is not DYNAMIC
    // but tests say otherwise!
    if (desc.Pool != D3DPOOL_DEFAULT)
      Flags &= ~(D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE);

    // Ignore DONOTWAIT if we are DYNAMIC
    // Yes... D3D9 is a good API.
    if (desc.Usage & D3DUSAGE_DYNAMIC)
      Flags &= ~D3DLOCK_DONOTWAIT;

    // We only bounds check for MANAGED.
    // (TODO: Apparently this is meant to happen for DYNAMIC too but I am not sure
    //  how that works given it is meant to be a DIRECT access..?)
    const bool respectUserBounds = !(Flags & D3DLOCK_DISCARD) &&
                                    SizeToLock != 0;

    // If we don't respect the bounds, encompass it all in our tests/checks
    // These values may be out of range and don't get clamped.
    uint32_t offset = respectUserBounds ? OffsetToLock : 0;
    uint32_t size   = respectUserBounds ? std::min(SizeToLock, desc.Size - offset) : desc.Size;
    D3D9Range lockRange = D3D9Range(offset, offset + size);

    if (!(Flags & D3DLOCK_READONLY))
      pResource->DirtyRange().Conjoin(lockRange);

    Rc<DxvkBuffer> mappingBuffer = pResource->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>();

    DxvkBufferSliceHandle physSlice;

    if (Flags & D3DLOCK_DISCARD) {
      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.
      physSlice = pResource->DiscardMapSlice();

      EmitCs([
        cBuffer      = std::move(mappingBuffer),
        cBufferSlice = physSlice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cBufferSlice);
      });

      pResource->SetWrittenByGPU(false);
      pResource->GPUReadingRange().Clear();
    }
    else {
      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3DLOCK_NOOVERWRITE.
      physSlice = pResource->GetMappedSlice();

      // NOOVERWRITE promises that they will not write in a currently used area.
      // Therefore we can skip waiting for these two cases.
      // We can also skip waiting if there is not dirty range overlap, if we are one of those resources.

      // If we are respecting the bounds ie. (MANAGED) we can test overlap
      // of our bounds, otherwise we just ignore this and go for it all the time.
      const bool wasWrittenByGPU = pResource->WasWrittenByGPU();
      const bool readOnly = Flags & D3DLOCK_READONLY;
      const bool noOverlap = !pResource->GPUReadingRange().Overlaps(lockRange);
      const bool noOverwrite = Flags & D3DLOCK_NOOVERWRITE;
      const bool usesStagingBuffer = pResource->DoesStagingBufferUploads();
      const bool skipWait = (!wasWrittenByGPU && (usesStagingBuffer || readOnly || noOverlap)) || noOverwrite;
      if (!skipWait) {
        if (!(Flags & D3DLOCK_DONOTWAIT) && !WaitForResource(mappingBuffer, D3DLOCK_DONOTWAIT))
          pResource->EnableStagingBufferUploads();

        if (!WaitForResource(mappingBuffer, Flags))
          return D3DERR_WASSTILLDRAWING;

        pResource->SetWrittenByGPU(false);
        pResource->GPUReadingRange().Clear();
      }
    }

    uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
    // The offset/size is not clamped to or affected by the desc size.
    data += OffsetToLock;

    *ppbData = reinterpret_cast<void*>(data);

    DWORD oldFlags = pResource->GetMapFlags();

    // We need to remove the READONLY flags from the map flags
    // if there was ever a non-readonly upload.
    if (!(Flags & D3DLOCK_READONLY))
      oldFlags &= ~D3DLOCK_READONLY;

    pResource->SetMapFlags(Flags | oldFlags);
    pResource->IncrementLockCount();

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::FlushBuffer(
        D3D9CommonBuffer*       pResource) {
    auto dstBuffer = pResource->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>();
    auto srcSlice = pResource->GetMappedSlice();

    D3D9Range& range = pResource->DirtyRange();

    DxvkBufferSlice copySrcSlice;
    if (pResource->DoesStagingBufferUploads()) {
      D3D9BufferSlice slice = AllocTempBuffer<false>(range.max - range.min);
      copySrcSlice = slice.slice;
      void* srcData = reinterpret_cast<uint8_t*>(srcSlice.mapPtr) + range.min;
      memcpy(slice.mapPtr, srcData, range.max - range.min);
    } else {
      copySrcSlice = DxvkBufferSlice(pResource->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>(), range.min, range.max - range.min);
    }

    EmitCs([
      cDstSlice  = dstBuffer,
      cSrcSlice  = copySrcSlice,
      cDstOffset = range.min,
      cLength    = range.max - range.min
    ] (DxvkContext* ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset() + cDstOffset,
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        cLength);
    });

    pResource->GPUReadingRange().Conjoin(pResource->DirtyRange());
    pResource->DirtyRange().Clear();

	  return D3D_OK;
  }


  HRESULT D3D9DeviceEx::UnlockBuffer(
        D3D9CommonBuffer*       pResource) {
    D3D9DeviceLock lock = LockDevice();

    if (pResource->DecrementLockCount() != 0)
      return D3D_OK;

    if (pResource->GetMapMode() != D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      return D3D_OK;

    if (pResource->GetMapFlags() & D3DLOCK_READONLY)
      return D3D_OK;

    pResource->SetMapFlags(0);

    if (pResource->Desc()->Pool != D3DPOOL_DEFAULT)
      return D3D_OK;

    FlushImplicit(FALSE);

    FlushBuffer(pResource);

    return D3D_OK;
  }


  void D3D9DeviceEx::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
    m_csIsBusy = true;
  }


  void D3D9DeviceEx::FlushImplicit(BOOL StrongHint) {
    // Flush only if the GPU is about to go idle, in
    // order to keep the number of submissions low.
    uint32_t pending = m_dxvkDevice->pendingSubmissions();

    if (StrongHint || pending <= MaxPendingSubmits) {
      auto now = dxvk::high_resolution_clock::now();

      uint32_t delay = MinFlushIntervalUs
                     + IncFlushIntervalUs * pending;

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(delay))
        Flush();
    }
  }


  void D3D9DeviceEx::SynchronizeCsThread() {
    D3D9DeviceLock lock = LockDevice();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();

    if (m_csThread.isBusy())
      m_csThread.synchronize();
  }


  void D3D9DeviceEx::SetupFPU() {
    // Should match d3d9 float behaviour.

#if defined(_MSC_VER)
    // For MSVC we can use these cross arch and platform funcs to set the FPU.
    // This will work on any platform, x86, x64, ARM, etc.

    // Clear exceptions.
    _clearfp();

    // Disable exceptions
    _controlfp(_MCW_EM, _MCW_EM);

#ifndef _WIN64
    // Use 24 bit precision
    _controlfp(_PC_24, _MCW_PC);
#endif

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
    Logger::warn("D3D9DeviceEx::SetupFPU: not supported on this arch.");
#endif
  }


  int64_t D3D9DeviceEx::DetermineInitialTextureMemory() {
    auto memoryProp = m_adapter->GetDXVKAdapter()->memoryProperties();

    VkDeviceSize availableTextureMemory = 0;

    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++)
      availableTextureMemory += memoryProp.memoryHeaps[i].size;

    constexpr VkDeviceSize Megabytes = 1024 * 1024;

    // The value returned is a 32-bit value, so we need to clamp it.
    VkDeviceSize maxMemory = (VkDeviceSize(m_d3d9Options.maxAvailableMemory) * Megabytes) - 1;
    availableTextureMemory = std::min(availableTextureMemory, maxMemory);

    return int64_t(availableTextureMemory);
  }


  Rc<DxvkBuffer> D3D9DeviceEx::CreateConstantBuffer(
          bool                SSBO,
          VkDeviceSize        Size,
          DxsoProgramType     ShaderStage,
          DxsoConstantBuffers BufferType) {
    DxvkBufferCreateInfo info = { };
    info.usage  = SSBO ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.access = SSBO ? VK_ACCESS_SHADER_READ_BIT          : VK_ACCESS_UNIFORM_READ_BIT;
    info.size   = Size;
    info.stages = ShaderStage == DxsoProgramType::VertexShader
      ? VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
      : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (m_d3d9Options.deviceLocalConstantBuffers)
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    Rc<DxvkBuffer> buffer = m_dxvkDevice->createBuffer(info, memoryFlags);

    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxsoBindingType::ConstantBuffer,
      BufferType);

    EmitCs([
      cSlotId = slotId,
      cBuffer = buffer
    ] (DxvkContext* ctx) {
      ctx->bindResourceBuffer(cSlotId,
        DxvkBufferSlice(cBuffer, 0, cBuffer->info().size));
    });

    return buffer;
  }


  void D3D9DeviceEx::CreateConstantBuffers() {
    m_consts[DxsoProgramTypes::VertexShader].buffer =
      CreateConstantBuffer(m_dxsoOptions.vertexConstantBufferAsSSBO,
                           m_vsLayout.totalSize(),
                           DxsoProgramType::VertexShader,
                           DxsoConstantBuffers::VSConstantBuffer);

    m_consts[DxsoProgramTypes::PixelShader].buffer =
      CreateConstantBuffer(false,
                           m_psLayout.totalSize(),
                           DxsoProgramType::PixelShader,
                           DxsoConstantBuffers::PSConstantBuffer);

    m_vsClipPlanes =
      CreateConstantBuffer(false,
                           caps::MaxClipPlanes * sizeof(D3D9ClipPlane),
                           DxsoProgramType::VertexShader,
                           DxsoConstantBuffers::VSClipPlanes);

    m_vsFixedFunction =
      CreateConstantBuffer(false,
                           sizeof(D3D9FixedFunctionVS),
                           DxsoProgramType::VertexShader,
                           DxsoConstantBuffers::VSFixedFunction);

    m_psFixedFunction =
      CreateConstantBuffer(false,
                           sizeof(D3D9FixedFunctionPS),
                           DxsoProgramType::PixelShader,
                           DxsoConstantBuffers::PSFixedFunction);

    m_psShared =
      CreateConstantBuffer(false,
                           sizeof(D3D9SharedPS),
                           DxsoProgramType::PixelShader,
                           DxsoConstantBuffers::PSShared);

    m_vsVertexBlend =
      CreateConstantBuffer(true,
                           CanSWVP()
                            ? sizeof(D3D9FixedFunctionVertexBlendDataSW)
                            : sizeof(D3D9FixedFunctionVertexBlendDataHW),
                           DxsoProgramType::VertexShader,
                           DxsoConstantBuffers::VSVertexBlendData);
  }


  template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
  inline void D3D9DeviceEx::UploadHardwareConstantSet(void* pData, const SoftwareLayoutType& Src, const ShaderType& Shader) {
    const D3D9ConstantSets& constSet = m_consts[ShaderStage];

    auto* dst = reinterpret_cast<HardwareLayoutType*>(pData);

    if (constSet.meta.maxConstIndexF)
      std::memcpy(dst->fConsts, Src.fConsts, constSet.meta.maxConstIndexF * sizeof(Vector4));
    if (constSet.meta.maxConstIndexI)
      std::memcpy(dst->iConsts, Src.iConsts, constSet.meta.maxConstIndexI * sizeof(Vector4i));
  }


  template <typename SoftwareLayoutType, typename ShaderType>
  inline void D3D9DeviceEx::UploadSoftwareConstantSet(void* pData, const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader) {
    const D3D9ConstantSets& constSet = m_consts[DxsoProgramType::VertexShader];

    auto dst = reinterpret_cast<uint8_t*>(pData);

    if (constSet.meta.maxConstIndexF)
      std::memcpy(dst + Layout.floatOffset(),   Src.fConsts, constSet.meta.maxConstIndexF * sizeof(Vector4));
    if (constSet.meta.maxConstIndexI)
      std::memcpy(dst + Layout.intOffset(),     Src.iConsts, constSet.meta.maxConstIndexI * sizeof(Vector4i));
    if (constSet.meta.maxConstIndexB)
      std::memcpy(dst + Layout.bitmaskOffset(), Src.bConsts, Layout.bitmaskSize());
  }


  template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
  inline void D3D9DeviceEx::UploadConstantSet(const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader) {
    D3D9ConstantSets& constSet = m_consts[ShaderStage];

    if (!constSet.dirty)
      return;

    constSet.dirty = false;

    DxvkBufferSliceHandle slice = constSet.buffer->allocSlice();

    EmitCs([
      cBuffer = constSet.buffer,
      cSlice  = slice
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cBuffer, cSlice);
    });

    if constexpr (ShaderStage == DxsoProgramType::PixelShader)
      UploadHardwareConstantSet<ShaderStage, HardwareLayoutType>(slice.mapPtr, Src, Shader);
    else if (likely(!CanSWVP()))
      UploadHardwareConstantSet<ShaderStage, HardwareLayoutType>(slice.mapPtr, Src, Shader);
    else
      UploadSoftwareConstantSet(slice.mapPtr, Src, Layout, Shader);

    if (constSet.meta.needsConstantCopies) {
      Vector4* data = reinterpret_cast<Vector4*>(slice.mapPtr);

      auto& shaderConsts = GetCommonShader(Shader)->GetConstants();

      for (const auto& constant : shaderConsts)
        data[constant.uboIdx] = *reinterpret_cast<const Vector4*>(constant.float32);
    }
  }


  template <DxsoProgramType ShaderStage>
  void D3D9DeviceEx::UploadConstants() {
    if constexpr (ShaderStage == DxsoProgramTypes::VertexShader)
      return UploadConstantSet<ShaderStage, D3D9ShaderConstantsVSHardware>(m_state.vsConsts, m_vsLayout, m_state.vertexShader);
    else
      return UploadConstantSet<ShaderStage, D3D9ShaderConstantsPS>        (m_state.psConsts, m_psLayout, m_state.pixelShader);
  }


  void D3D9DeviceEx::UpdateClipPlanes() {
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


  template <uint32_t Offset, uint32_t Length>
  void D3D9DeviceEx::UpdatePushConstant(const void* pData) {
    struct ConstantData { uint8_t Data[Length]; };

    auto* constData = reinterpret_cast<const ConstantData*>(pData);

    EmitCs([
      cData = *constData
    ](DxvkContext* ctx) {
      ctx->pushConstants(Offset, Length, &cData);
    });
  }


  template <D3D9RenderStateItem Item>
  void D3D9DeviceEx::UpdatePushConstant() {
    auto& rs = m_state.renderStates;

    if constexpr (Item == D3D9RenderStateItem::AlphaRef) {
      float alpha = float(rs[D3DRS_ALPHAREF] & 0xFF) / 255.0f;
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, alphaRef), sizeof(float)>(&alpha);
    }
    else if constexpr (Item == D3D9RenderStateItem::FogColor) {
      Vector4 color;
      DecodeD3DCOLOR(D3DCOLOR(rs[D3DRS_FOGCOLOR]), color.data);
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, fogColor), sizeof(D3D9RenderStateInfo::fogColor)>(&color);
    }
    else if constexpr (Item == D3D9RenderStateItem::FogDensity) {
      float density = bit::cast<float>(rs[D3DRS_FOGDENSITY]);
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, fogDensity), sizeof(float)>(&density);
    }
    else if constexpr (Item == D3D9RenderStateItem::FogEnd) {
      float end = bit::cast<float>(rs[D3DRS_FOGEND]);
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, fogEnd), sizeof(float)>(&end);
    }
    else if constexpr (Item == D3D9RenderStateItem::FogScale) {
      float end = bit::cast<float>(rs[D3DRS_FOGEND]);
      float start = bit::cast<float>(rs[D3DRS_FOGSTART]);

      float scale = 1.0f / (end - start);
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, fogScale), sizeof(float)>(&scale);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointSize) {
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointSize), sizeof(float)>(&rs[D3DRS_POINTSIZE]);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointSizeMin) {
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointSizeMin), sizeof(float)>(&rs[D3DRS_POINTSIZE_MIN]);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointSizeMax) {
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointSizeMax), sizeof(float)>(&rs[D3DRS_POINTSIZE_MAX]);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointScaleA) {
      float scale = bit::cast<float>(rs[D3DRS_POINTSCALE_A]);
      scale /= float(m_state.viewport.Height * m_state.viewport.Height);

      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointScaleA), sizeof(float)>(&scale);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointScaleB) {
      float scale = bit::cast<float>(rs[D3DRS_POINTSCALE_B]);
      scale /= float(m_state.viewport.Height * m_state.viewport.Height);

      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointScaleB), sizeof(float)>(&scale);
    }
    else if constexpr (Item == D3D9RenderStateItem::PointScaleC) {
      float scale = bit::cast<float>(rs[D3DRS_POINTSCALE_C]);
      scale /= float(m_state.viewport.Height * m_state.viewport.Height);

      UpdatePushConstant<offsetof(D3D9RenderStateInfo, pointScaleC), sizeof(float)>(&scale);
    }
    else
      Logger::warn("D3D9: Invalid push constant set to update.");
  }



  void D3D9DeviceEx::Flush() {
    D3D9DeviceLock lock = LockDevice();

    m_initializer->Flush();
    m_converter->Flush();

    if (m_csIsBusy || !m_csChunk->empty()) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([](DxvkContext* ctx) {
        ctx->flushCommandList();
      });

      FlushCsChunk();

      // Reset flush timer used for implicit flushes
      m_lastFlush = dxvk::high_resolution_clock::now();
      m_csIsBusy = false;
    }
  }


  inline void D3D9DeviceEx::UpdateActiveRTs(uint32_t index) {
    const uint32_t bit = 1 << index;

    m_activeRTs &= ~bit;

    if (m_state.renderTargets[index] != nullptr &&
        m_state.renderTargets[index]->GetBaseTexture() != nullptr &&
        m_state.renderStates[ColorWriteIndex(index)])
      m_activeRTs |= bit;

    UpdateActiveHazardsRT(bit);
  }


  inline void D3D9DeviceEx::UpdateActiveTextures(uint32_t index, DWORD combinedUsage) {
    const uint32_t bit = 1 << index;

    m_activeRTTextures       &= ~bit;
    m_activeDSTextures       &= ~bit;
    m_activeTextures         &= ~bit;
    m_activeTexturesToUpload &= ~bit;
    m_activeTexturesToGen    &= ~bit;

    auto tex = GetCommonTexture(m_state.textures[index]);
    if (tex != nullptr) {
      m_activeTextures |= bit;

      if (unlikely(tex->IsRenderTarget()))
        m_activeRTTextures |= bit;

      if (unlikely(tex->IsDepthStencil()))
        m_activeDSTextures |= bit;

      if (unlikely(tex->NeedsAnyUpload()))
        m_activeTexturesToUpload |= bit;

      if (unlikely(tex->NeedsMipGen()))
        m_activeTexturesToGen |= bit;
    }

    if (unlikely(combinedUsage & D3DUSAGE_RENDERTARGET))
      UpdateActiveHazardsRT(UINT32_MAX);

    if (unlikely(combinedUsage & D3DUSAGE_DEPTHSTENCIL))
      UpdateActiveHazardsDS(bit);
  }


  inline void D3D9DeviceEx::UpdateActiveHazardsRT(uint32_t rtMask) {
    auto masks = m_psShaderMasks;
    masks.rtMask      &= m_activeRTs & rtMask;
    masks.samplerMask &= m_activeRTTextures;

    m_activeHazardsRT = m_activeHazardsRT & (~rtMask);
    for (uint32_t rt = masks.rtMask; rt; rt &= rt - 1) {
      for (uint32_t sampler = masks.samplerMask; sampler; sampler &= sampler - 1) {
        const uint32_t rtIdx = bit::tzcnt(rt);
        D3D9Surface* rtSurf = m_state.renderTargets[rtIdx].ptr();

        IDirect3DBaseTexture9* rtBase  = rtSurf->GetBaseTexture();
        IDirect3DBaseTexture9* texBase = m_state.textures[bit::tzcnt(sampler)];

        // HACK: Don't mark for hazards if we aren't rendering to mip 0!
        // Some games use screenspace passes like this for blurring
        // Sampling from mip 0 (texture) -> mip 1 (rt)
        // and we'd trigger the hazard path otherwise which is unnecessary,
        // and would shove us into GENERAL and emitting readback barriers.
        if (likely(rtSurf->GetMipLevel() != 0 || rtBase != texBase))
          continue;

        m_activeHazardsRT |= 1 << rtIdx;
      }
    }
  }


  inline void D3D9DeviceEx::UpdateActiveHazardsDS(uint32_t texMask) {
    m_activeHazardsDS = m_activeHazardsDS & (~texMask);
    if (m_state.depthStencil != nullptr &&
        m_state.depthStencil->GetBaseTexture() != nullptr) {
      uint32_t samplerMask = m_activeDSTextures & texMask;
      for (uint32_t sampler = samplerMask; sampler; sampler &= sampler - 1) {
        const uint32_t samplerIdx = bit::tzcnt(sampler);

        IDirect3DBaseTexture9* dsBase  = m_state.depthStencil->GetBaseTexture();
        IDirect3DBaseTexture9* texBase = m_state.textures[samplerIdx];

        if (likely(dsBase != texBase))
          continue;

        m_activeHazardsDS |= 1 << samplerIdx;
      }
    }
  }


  void D3D9DeviceEx::MarkRenderHazards() {
    for (uint32_t rt = m_activeHazardsRT; rt; rt &= rt - 1) {
      // Guaranteed to not be nullptr...
      auto tex = m_state.renderTargets[bit::tzcnt(rt)]->GetCommonTexture();
      if (unlikely(!tex->MarkHazardous())) {
        TransitionImage(tex, VK_IMAGE_LAYOUT_GENERAL);
        m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      }
    }
  }


  void D3D9DeviceEx::UploadManagedTexture(D3D9CommonTexture* pResource) {
    for (uint32_t subresource = 0; subresource < pResource->CountSubresources(); subresource++) {
      if (!pResource->NeedsUpload(subresource) || pResource->GetBuffer(subresource) == nullptr)
        continue;

      this->FlushImage(pResource, subresource);
    }

    pResource->ClearDirtyBoxes();
    pResource->ClearNeedsUpload();
  }


  void D3D9DeviceEx::UploadManagedTextures(uint32_t mask) {
    // Guaranteed to not be nullptr...
    for (uint32_t tex = mask; tex; tex &= tex - 1)
      UploadManagedTexture(GetCommonTexture(m_state.textures[bit::tzcnt(tex)]));

    m_activeTexturesToUpload &= ~mask;
  }


  void D3D9DeviceEx::GenerateTextureMips(uint32_t mask) {
    for (uint32_t tex = mask; tex; tex &= tex - 1) {
      // Guaranteed to not be nullptr...
      auto texInfo = GetCommonTexture(m_state.textures[bit::tzcnt(tex)]);

      if (texInfo->NeedsMipGen()) {
        this->EmitGenerateMips(texInfo);
        texInfo->SetNeedsMipGen(false);
      }
    }

    m_activeTexturesToGen &= ~mask;
  }


  void D3D9DeviceEx::MarkTextureMipsDirty(D3D9CommonTexture* pResource) {
    pResource->SetNeedsMipGen(true);
    pResource->MarkAllWrittenByGPU();

    for (uint32_t tex = m_activeTextures; tex; tex &= tex - 1) {
      // Guaranteed to not be nullptr...
      const uint32_t i = bit::tzcnt(tex);
      auto texInfo = GetCommonTexture(m_state.textures[i]);

      if (texInfo == pResource) {
        m_activeTexturesToGen |= 1 << i;
        // We can early out here, no need to add another index for this.
        break;
      }
    }
  }


  void D3D9DeviceEx::MarkTextureMipsUnDirty(D3D9CommonTexture* pResource) {
    pResource->SetNeedsMipGen(false);

    for (uint32_t tex = m_activeTextures; tex; tex &= tex - 1) {
      // Guaranteed to not be nullptr...
      const uint32_t i = bit::tzcnt(tex);
      auto texInfo = GetCommonTexture(m_state.textures[i]);

      if (texInfo == pResource)
        m_activeTexturesToGen &= ~(1 << i);
    }
  }


  void D3D9DeviceEx::MarkTextureUploaded(D3D9CommonTexture* pResource) {
    for (uint32_t tex = m_activeTextures; tex; tex &= tex - 1) {
      // Guaranteed to not be nullptr...
      const uint32_t i = bit::tzcnt(tex);
      auto texInfo = GetCommonTexture(m_state.textures[i]);

      if (texInfo == pResource)
        m_activeTexturesToUpload &= ~(1 << i);
    }
  }


  template <bool Points>
  void D3D9DeviceEx::UpdatePointMode() {
    if constexpr (!Points) {
      m_lastPointMode = 0;

      EmitCs([](DxvkContext* ctx) {
        ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PointMode, 0);
      });
    }
    else {
      auto& rs = m_state.renderStates;

      const bool scale  = rs[D3DRS_POINTSCALEENABLE] && !UseProgrammableVS();
      const bool sprite = rs[D3DRS_POINTSPRITEENABLE];

      const uint32_t scaleBit  = scale  ? 1u : 0u;
      const uint32_t spriteBit = sprite ? 2u : 0u;

      uint32_t mode = scaleBit | spriteBit;

      if (rs[D3DRS_POINTSCALEENABLE] && m_flags.test(D3D9DeviceFlag::DirtyPointScale)) {
        m_flags.clr(D3D9DeviceFlag::DirtyPointScale);

        UpdatePushConstant<D3D9RenderStateItem::PointScaleA>();
        UpdatePushConstant<D3D9RenderStateItem::PointScaleB>();
        UpdatePushConstant<D3D9RenderStateItem::PointScaleC>();
      }

      if (unlikely(mode != m_lastPointMode)) {
        EmitCs([cMode = mode] (DxvkContext* ctx) {
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PointMode, cMode);
        });

        m_lastPointMode = mode;
      }
    }
  }


  void D3D9DeviceEx::UpdateFog() {
    auto& rs = m_state.renderStates;

    bool fogEnabled = rs[D3DRS_FOGENABLE];

    bool pixelFog   = rs[D3DRS_FOGTABLEMODE]  != D3DFOG_NONE && fogEnabled;
    bool vertexFog  = rs[D3DRS_FOGVERTEXMODE] != D3DFOG_NONE && fogEnabled && !pixelFog;

    auto UpdateFogConstants = [&](D3DFOGMODE FogMode) {
      if (m_flags.test(D3D9DeviceFlag::DirtyFogColor)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogColor);
        UpdatePushConstant<D3D9RenderStateItem::FogColor>();
      }

      if (FogMode == D3DFOG_LINEAR) {
        if (m_flags.test(D3D9DeviceFlag::DirtyFogScale)) {
          m_flags.clr(D3D9DeviceFlag::DirtyFogScale);
          UpdatePushConstant<D3D9RenderStateItem::FogScale>();
        }

        if (m_flags.test(D3D9DeviceFlag::DirtyFogEnd)) {
          m_flags.clr(D3D9DeviceFlag::DirtyFogEnd);
          UpdatePushConstant<D3D9RenderStateItem::FogEnd>();
        }
      }
      else if (FogMode == D3DFOG_EXP || FogMode == D3DFOG_EXP2) {
        if (m_flags.test(D3D9DeviceFlag::DirtyFogDensity)) {
          m_flags.clr(D3D9DeviceFlag::DirtyFogDensity);
          UpdatePushConstant<D3D9RenderStateItem::FogDensity>();
        }
      }
    };

    if (vertexFog) {
      D3DFOGMODE mode = D3DFOGMODE(rs[D3DRS_FOGVERTEXMODE]);

      UpdateFogConstants(mode);

      if (m_flags.test(D3D9DeviceFlag::DirtyFogState)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogState);

        EmitCs([cMode = mode] (DxvkContext* ctx) {
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::FogEnabled,    true);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::VertexFogMode, cMode);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PixelFogMode,  D3DFOG_NONE);
        });
      }
    }
    else if (pixelFog) {
      D3DFOGMODE mode = D3DFOGMODE(rs[D3DRS_FOGTABLEMODE]);

      UpdateFogConstants(mode);

      if (m_flags.test(D3D9DeviceFlag::DirtyFogState)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogState);

        EmitCs([cMode = mode] (DxvkContext* ctx) {
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::FogEnabled,    true);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::VertexFogMode, D3DFOG_NONE);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PixelFogMode,  cMode);
        });
      }
    }
    else {
      if (fogEnabled)
        UpdateFogConstants(D3DFOG_NONE);

      if (m_flags.test(D3D9DeviceFlag::DirtyFogState)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogState);

        EmitCs([cEnabled = fogEnabled] (DxvkContext* ctx) {
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::FogEnabled,    cEnabled);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::VertexFogMode, D3DFOG_NONE);
          ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PixelFogMode,  D3DFOG_NONE);
        });
      }
    }
  }


  void D3D9DeviceEx::BindFramebuffer() {
    m_flags.clr(D3D9DeviceFlag::DirtyFramebuffer);

    DxvkRenderTargets attachments;

    bool srgb = m_state.renderStates[D3DRS_SRGBWRITEENABLE];

    // D3D9 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

    for (UINT i = 0; i < m_state.renderTargets.size(); i++) {
      if (m_state.renderTargets[i] != nullptr && !m_state.renderTargets[i]->IsNull()) {
        const DxvkImageCreateInfo& rtImageInfo = m_state.renderTargets[i]->GetCommonTexture()->GetImage()->info();

        if (likely(sampleCount == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM))
          sampleCount = rtImageInfo.sampleCount;
        else if (unlikely(sampleCount != rtImageInfo.sampleCount))
          continue;

        attachments.color[i] = {
          m_state.renderTargets[i]->GetRenderTargetView(srgb),
          m_state.renderTargets[i]->GetRenderTargetLayout() };
      }
    }

    if (m_state.depthStencil != nullptr) {
      const DxvkImageCreateInfo& dsImageInfo = m_state.depthStencil->GetCommonTexture()->GetImage()->info();
      const bool depthWrite = m_state.renderStates[D3DRS_ZWRITEENABLE];

      if (likely(sampleCount == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM || sampleCount == dsImageInfo.sampleCount)) {
        attachments.depth = {
          m_state.depthStencil->GetDepthStencilView(),
          m_state.depthStencil->GetDepthStencilLayout(depthWrite, m_activeHazardsDS != 0) };
      }
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments)
    ] (DxvkContext* ctx) {
      ctx->bindRenderTargets(cAttachments);
    });
  }


  void D3D9DeviceEx::BindViewportAndScissor() {
    m_flags.clr(D3D9DeviceFlag::DirtyViewportScissor);

    VkViewport viewport;
    VkRect2D scissor;

    // D3D9's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    const D3DVIEWPORT9& vp = m_state.viewport;

    // Correctness Factor for 1/2 texel offset
    // We need to bias this slightly to make
    // imprecision in games happy.
    // Originally we did this only for powers of two
    // resolutions but since NEAREST filtering fixed to
    // truncate, we need to do this all the time now.
    float cf = 0.5f - (1.0f / 128.0f);

    viewport = VkViewport{
      float(vp.X)     + cf,    float(vp.Height + vp.Y) + cf,
      float(vp.Width),        -float(vp.Height),
      vp.MinZ,                 vp.MaxZ,
    };

    // Scissor rectangles. Vulkan does not provide an easy way
    // to disable the scissor test, so we'll have to set scissor
    // rects that are at least as large as the framebuffer.
    bool enableScissorTest = m_state.renderStates[D3DRS_SCISSORTESTENABLE];

    if (enableScissorTest) {
      RECT sr = m_state.scissorRect;

      VkOffset2D srPosA;
      srPosA.x = std::max<int32_t>(0, sr.left);
      srPosA.x = std::max<int32_t>(vp.X, srPosA.x);
      srPosA.y = std::max<int32_t>(0, sr.top);
      srPosA.y = std::max<int32_t>(vp.Y, srPosA.y);

      VkOffset2D srPosB;
      srPosB.x = std::max<int32_t>(srPosA.x, sr.right);
      srPosB.x = std::min<int32_t>(vp.X + vp.Width, srPosB.x);
      srPosB.y = std::max<int32_t>(srPosA.y, sr.bottom);
      srPosB.y = std::min<int32_t>(vp.Y + vp.Height, srPosB.y);

      VkExtent2D srSize;
      srSize.width  = uint32_t(srPosB.x - srPosA.x);
      srSize.height = uint32_t(srPosB.y - srPosA.y);

      scissor = VkRect2D{ srPosA, srSize };
    }
    else {
      scissor = VkRect2D{
        VkOffset2D { int32_t(vp.X), int32_t(vp.Y) },
        VkExtent2D { vp.Width,      vp.Height     }};
    }

    EmitCs([
      cViewport = viewport,
      cScissor = scissor
    ] (DxvkContext* ctx) {
      ctx->setViewports(
        1,
        &cViewport,
        &cScissor);
    });
  }


  void D3D9DeviceEx::BindMultiSampleState() {
    m_flags.clr(D3D9DeviceFlag::DirtyMultiSampleState);

    DxvkMultisampleState msState;
    msState.sampleMask = m_flags.test(D3D9DeviceFlag::ValidSampleMask)
      ? m_state.renderStates[D3DRS_MULTISAMPLEMASK]
      : 0xffffffff;
    msState.enableAlphaToCoverage = IsAlphaToCoverageEnabled();

    EmitCs([
      cState = msState
    ] (DxvkContext* ctx) {
      ctx->setMultisampleState(cState);
    });
  }


  void D3D9DeviceEx::BindBlendState() {
    m_flags.clr(D3D9DeviceFlag::DirtyBlendState);

    auto& state = m_state.renderStates;

    bool separateAlpha = state[D3DRS_SEPARATEALPHABLENDENABLE];

    DxvkBlendMode mode;
    mode.enableBlending = state[D3DRS_ALPHABLENDENABLE] != FALSE;

    D3D9BlendState color, alpha;

    color.Src = D3DBLEND(state[D3DRS_SRCBLEND]);
    color.Dst = D3DBLEND(state[D3DRS_DESTBLEND]);
    color.Op  = D3DBLENDOP(state[D3DRS_BLENDOP]);
    FixupBlendState(color);

    if (separateAlpha) {
      alpha.Src = D3DBLEND(state[D3DRS_SRCBLENDALPHA]);
      alpha.Dst = D3DBLEND(state[D3DRS_DESTBLENDALPHA]);
      alpha.Op  = D3DBLENDOP(state[D3DRS_BLENDOPALPHA]);
      FixupBlendState(alpha);
    }
    else
      alpha = color;

    mode.colorSrcFactor = DecodeBlendFactor(color.Src, false);
    mode.colorDstFactor = DecodeBlendFactor(color.Dst, false);
    mode.colorBlendOp   = DecodeBlendOp    (color.Op);

    mode.alphaSrcFactor = DecodeBlendFactor(alpha.Src, true);
    mode.alphaDstFactor = DecodeBlendFactor(alpha.Dst, true);
    mode.alphaBlendOp   = DecodeBlendOp    (alpha.Op);

    mode.writeMask = state[ColorWriteIndex(0)];

    std::array<VkColorComponentFlags, 3> extraWriteMasks;
    for (uint32_t i = 0; i < 3; i++)
      extraWriteMasks[i] = state[ColorWriteIndex(i + 1)];

    EmitCs([
      cMode       = mode,
      cWriteMasks = extraWriteMasks,
      cAlphaMasks = m_alphaSwizzleRTs
    ](DxvkContext* ctx) {
      for (uint32_t i = 0; i < 4; i++) {
        DxvkBlendMode mode = cMode;
        if (i != 0)
          mode.writeMask = cWriteMasks[i - 1];

        const bool alphaSwizzle = cAlphaMasks & (1 << i);

        auto NormalizeFactor = [alphaSwizzle](VkBlendFactor Factor) {
          if (alphaSwizzle) {
            if (Factor == VK_BLEND_FACTOR_DST_ALPHA)
              return VK_BLEND_FACTOR_ONE;
            else if (Factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA)
              return VK_BLEND_FACTOR_ZERO;
          }

          return Factor;
        };

        mode.colorSrcFactor = NormalizeFactor(mode.colorSrcFactor);
        mode.colorDstFactor = NormalizeFactor(mode.colorDstFactor);
        mode.alphaSrcFactor = NormalizeFactor(mode.alphaSrcFactor);
        mode.alphaDstFactor = NormalizeFactor(mode.alphaDstFactor);

        ctx->setBlendMode(i, mode);
      }
    });
  }


  void D3D9DeviceEx::BindBlendFactor() {
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


  void D3D9DeviceEx::BindDepthStencilState() {
    m_flags.clr(D3D9DeviceFlag::DirtyDepthStencilState);

    auto& rs = m_state.renderStates;

    bool stencil            = rs[D3DRS_STENCILENABLE];
    bool twoSidedStencil    = stencil && rs[D3DRS_TWOSIDEDSTENCILMODE];

    DxvkDepthStencilState state;
    state.enableDepthTest   = rs[D3DRS_ZENABLE]       != FALSE;
    state.enableDepthWrite  = rs[D3DRS_ZWRITEENABLE]  != FALSE;
    state.enableStencilTest = stencil;
    state.depthCompareOp    = DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ZFUNC]));

    if (stencil) {
      state.stencilOpFront.failOp      = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILFAIL]));
      state.stencilOpFront.passOp      = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILPASS]));
      state.stencilOpFront.depthFailOp = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILZFAIL]));
      state.stencilOpFront.compareOp   = DecodeCompareOp(D3DCMPFUNC  (rs[D3DRS_STENCILFUNC]));
      state.stencilOpFront.compareMask = uint32_t(rs[D3DRS_STENCILMASK]);
      state.stencilOpFront.writeMask   = uint32_t(rs[D3DRS_STENCILWRITEMASK]);
      state.stencilOpFront.reference   = 0;
    }
    else
      state.stencilOpFront = VkStencilOpState();

    if (twoSidedStencil) {
      state.stencilOpBack.failOp      = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILFAIL]));
      state.stencilOpBack.passOp      = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILPASS]));
      state.stencilOpBack.depthFailOp = DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILZFAIL]));
      state.stencilOpBack.compareOp   = DecodeCompareOp(D3DCMPFUNC  (rs[D3DRS_CCW_STENCILFUNC]));
      state.stencilOpBack.compareMask = state.stencilOpFront.compareMask;
      state.stencilOpBack.writeMask   = state.stencilOpFront.writeMask;
      state.stencilOpBack.reference   = 0;
    }
    else
      state.stencilOpBack = state.stencilOpFront;

    EmitCs([
      cState = state
    ](DxvkContext* ctx) {
      ctx->setDepthStencilState(cState);
    });
  }


  void D3D9DeviceEx::BindRasterizerState() {
    m_flags.clr(D3D9DeviceFlag::DirtyRasterizerState);

    auto& rs = m_state.renderStates;

    DxvkRasterizerState state;
    state.cullMode        = DecodeCullMode(D3DCULL(rs[D3DRS_CULLMODE]));
    state.depthBiasEnable = IsDepthBiasEnabled();
    state.depthClipEnable = true;
    state.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));
    state.conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    state.sampleCount     = 0;

    EmitCs([
      cState  = state
    ](DxvkContext* ctx) {
      ctx->setRasterizerState(cState);
    });
  }


  void D3D9DeviceEx::BindDepthBias() {
    m_flags.clr(D3D9DeviceFlag::DirtyDepthBias);

    auto& rs = m_state.renderStates;

    float depthBias            = bit::cast<float>(rs[D3DRS_DEPTHBIAS]) * m_depthBiasScale;
    float slopeScaledDepthBias = bit::cast<float>(rs[D3DRS_SLOPESCALEDEPTHBIAS]);

    DxvkDepthBias biases;
    biases.depthBiasConstant = depthBias;
    biases.depthBiasSlope    = slopeScaledDepthBias;
    biases.depthBiasClamp    = 0.0f;

    EmitCs([
      cBiases = biases
    ](DxvkContext* ctx) {
      ctx->setDepthBias(cBiases);
    });
  }


  void D3D9DeviceEx::BindAlphaTestState() {
    m_flags.clr(D3D9DeviceFlag::DirtyAlphaTestState);

    auto& rs = m_state.renderStates;

    VkCompareOp alphaOp = IsAlphaTestEnabled()
      ? DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ALPHAFUNC]))
      : VK_COMPARE_OP_ALWAYS;

    EmitCs([cAlphaOp = alphaOp] (DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::AlphaTestEnable, cAlphaOp != VK_COMPARE_OP_ALWAYS);
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::AlphaCompareOp,  cAlphaOp);
    });
  }


  void D3D9DeviceEx::BindDepthStencilRefrence() {
    auto& rs = m_state.renderStates;

    uint32_t ref = uint32_t(rs[D3DRS_STENCILREF]);

    EmitCs([cRef = ref] (DxvkContext* ctx) {
      ctx->setStencilReference(cRef);
    });
  }


  void D3D9DeviceEx::BindSampler(DWORD Sampler) {
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

    if (m_d3d9Options.samplerAnisotropy != -1) {
      if (key.MagFilter == D3DTEXF_LINEAR)
        key.MagFilter = D3DTEXF_ANISOTROPIC;

      if (key.MinFilter == D3DTEXF_LINEAR)
        key.MinFilter = D3DTEXF_ANISOTROPIC;

      key.MaxAnisotropy = m_d3d9Options.samplerAnisotropy;
    }

    NormalizeSamplerKey(key);

    auto samplerInfo = RemapStateSamplerShader(Sampler);

    const uint32_t colorSlot = computeResourceSlotId(
      samplerInfo.first, DxsoBindingType::ColorImage,
      samplerInfo.second);

    const uint32_t depthSlot = computeResourceSlotId(
      samplerInfo.first, DxsoBindingType::DepthImage,
      samplerInfo.second);

    EmitCs([this,
      cColorSlot = colorSlot,
      cDepthSlot = depthSlot,
      cKey       = key
    ] (DxvkContext* ctx) {
      auto pair = m_samplers.find(cKey);
      if (pair != m_samplers.end()) {
        ctx->bindResourceSampler(cColorSlot, pair->second.color);
        ctx->bindResourceSampler(cDepthSlot, pair->second.depth);
        return;
      }

      auto mipFilter = DecodeMipFilter(cKey.MipFilter);

      DxvkSamplerCreateInfo colorInfo;
      colorInfo.addressModeU   = DecodeAddressMode(cKey.AddressU);
      colorInfo.addressModeV   = DecodeAddressMode(cKey.AddressV);
      colorInfo.addressModeW   = DecodeAddressMode(cKey.AddressW);
      colorInfo.compareToDepth = VK_FALSE;
      colorInfo.compareOp      = VK_COMPARE_OP_NEVER;
      colorInfo.magFilter      = DecodeFilter(cKey.MagFilter);
      colorInfo.minFilter      = DecodeFilter(cKey.MinFilter);
      colorInfo.mipmapMode     = mipFilter.MipFilter;
      colorInfo.maxAnisotropy  = float(cKey.MaxAnisotropy);
      colorInfo.useAnisotropy  = cKey.MaxAnisotropy > 1;
      colorInfo.mipmapLodBias  = cKey.MipmapLodBias;
      colorInfo.mipmapLodMin   = mipFilter.MipsEnabled ? float(cKey.MaxMipLevel) : 0;
      colorInfo.mipmapLodMax   = mipFilter.MipsEnabled ? FLT_MAX                 : 0;
      colorInfo.usePixelCoord  = VK_FALSE;

      DecodeD3DCOLOR(cKey.BorderColor, colorInfo.borderColor.float32);

      if (!m_dxvkDevice->features().extCustomBorderColor.customBorderColorWithoutFormat) {
        // HACK: Let's get OPAQUE_WHITE border color over
        // TRANSPARENT_BLACK if the border RGB is white.
        if (colorInfo.borderColor.float32[0] == 1.0f
        && colorInfo.borderColor.float32[1] == 1.0f
        && colorInfo.borderColor.float32[2] == 1.0f
        && !m_dxvkDevice->features().extCustomBorderColor.customBorderColors) {
          // Then set the alpha to 1.
          colorInfo.borderColor.float32[3] = 1.0f;
        }
      }

      DxvkSamplerCreateInfo depthInfo = colorInfo;
      depthInfo.compareToDepth = VK_TRUE;
      depthInfo.compareOp      = VK_COMPARE_OP_LESS_OR_EQUAL;
      depthInfo.magFilter      = VK_FILTER_LINEAR;
      depthInfo.minFilter      = VK_FILTER_LINEAR;

      try {
        D3D9SamplerPair pair;

        pair.color = m_dxvkDevice->createSampler(colorInfo);
        pair.depth = m_dxvkDevice->createSampler(depthInfo);

        m_samplerCount++;

        m_samplers.insert(std::make_pair(cKey, pair));
        ctx->bindResourceSampler(cColorSlot, pair.color);
        ctx->bindResourceSampler(cDepthSlot, pair.depth);
      }
      catch (const DxvkError& e) {
        Logger::err(e.message());
      }
    });
  }


  void D3D9DeviceEx::BindTexture(DWORD StateSampler) {
    auto shaderSampler = RemapStateSamplerShader(StateSampler);

    uint32_t colorSlot = computeResourceSlotId(shaderSampler.first,
      DxsoBindingType::ColorImage, uint32_t(shaderSampler.second));

    uint32_t depthSlot = computeResourceSlotId(shaderSampler.first,
      DxsoBindingType::DepthImage, uint32_t(shaderSampler.second));

    const bool srgb =
      m_state.samplerStates[StateSampler][D3DSAMP_SRGBTEXTURE];

    D3D9CommonTexture* commonTex =
      GetCommonTexture(m_state.textures[StateSampler]);

    // For all our pixel shader textures
    if (likely(StateSampler < 16)) {
      const uint32_t offset = StateSampler * 2;
      const uint32_t textureType = commonTex != nullptr
        ? uint32_t(commonTex->GetType() - D3DRTYPE_TEXTURE)
        : 0;
      const uint32_t textureBitMask = 0b11u << offset;
      const uint32_t textureBits = textureType << offset;

      if ((m_samplerTypeBitfield & textureBitMask) != textureBits) {
        m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

        m_samplerTypeBitfield &= ~textureBitMask;
        m_samplerTypeBitfield |= textureBits;
      }
    }

    if (commonTex != nullptr) {
      EmitCs([
        cColorSlot = colorSlot,
        cDepthSlot = depthSlot,
        cDepth     = commonTex->IsShadow(),
        cImageView = commonTex->GetSampleView(srgb)
      ](DxvkContext* ctx) {
        ctx->bindResourceView(cColorSlot, !cDepth ? cImageView : nullptr, nullptr);
        ctx->bindResourceView(cDepthSlot,  cDepth ? cImageView : nullptr, nullptr);
      });
    } else {
      EmitCs([
        cColorSlot = colorSlot,
        cDepthSlot = depthSlot
      ](DxvkContext* ctx) {
        ctx->bindResourceView(cColorSlot, nullptr, nullptr);
        ctx->bindResourceView(cDepthSlot, nullptr, nullptr);
      });
    }
  }


  void D3D9DeviceEx::UndirtySamplers() {
    for (uint32_t dirty = m_dirtySamplerStates; dirty; dirty &= dirty - 1)
      BindSampler(bit::tzcnt(dirty));

    m_dirtySamplerStates = 0;
  }


  void D3D9DeviceEx::UndirtyTextures() {
    for (uint32_t tex = m_dirtyTextures; tex; tex &= tex - 1)
      BindTexture(bit::tzcnt(tex));
  
    m_dirtyTextures = 0;
  }

  void D3D9DeviceEx::MarkSamplersDirty() {
    m_dirtySamplerStates = 0x001fffff; // 21 bits.
  }


  D3D9DrawInfo D3D9DeviceEx::GenerateDrawInfo(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
          UINT             InstanceCount) {
    D3D9DrawInfo drawInfo;
    drawInfo.vertexCount = GetVertexCount(PrimitiveType, PrimitiveCount);
    drawInfo.instanceCount = m_iaState.streamsInstanced & m_iaState.streamsUsed
      ? InstanceCount
      : 1u;
    return drawInfo;
  }


  uint32_t D3D9DeviceEx::GetInstanceCount() const {
    return std::max(m_state.streamFreq[0] & 0x7FFFFFu, 1u);
  }


  void D3D9DeviceEx::PrepareDraw(D3DPRIMITIVETYPE PrimitiveType) {
    if (unlikely(m_activeHazardsRT != 0)) {
      EmitCs([](DxvkContext* ctx) {
        ctx->emitRenderTargetReadbackBarrier();
      });

      if (m_d3d9Options.generalHazards)
        MarkRenderHazards();
    }

    if (unlikely((m_lastHazardsDS == 0) != (m_activeHazardsDS == 0))) {
      m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      m_lastHazardsDS = m_activeHazardsDS;
    }

    for (uint32_t i = 0; i < caps::MaxStreams; i++) {
      auto* vbo = GetCommonBuffer(m_state.vertexBuffers[i].vertexBuffer);
      if (vbo != nullptr && vbo->NeedsUpload())
        FlushBuffer(vbo);
    }

    uint32_t texturesToUpload = m_activeTexturesToUpload;
    texturesToUpload &= m_psShaderMasks.samplerMask | m_vsShaderMasks.samplerMask;

    if (unlikely(texturesToUpload != 0))
      UploadManagedTextures(texturesToUpload);

    uint32_t texturesToGen = m_activeTexturesToGen;
    texturesToGen &= m_psShaderMasks.samplerMask | m_vsShaderMasks.samplerMask;

    if (unlikely(texturesToGen != 0))
      GenerateTextureMips(texturesToGen);

    auto* ibo = GetCommonBuffer(m_state.indices);
    if (ibo != nullptr && ibo->NeedsUpload())
      FlushBuffer(ibo);

    UpdateFog();

    if (m_flags.test(D3D9DeviceFlag::DirtyFramebuffer))
      BindFramebuffer();

    if (m_flags.test(D3D9DeviceFlag::DirtyViewportScissor))
      BindViewportAndScissor();

    if (m_dirtySamplerStates)
      UndirtySamplers();

    if (m_dirtyTextures)
      UndirtyTextures();

    if (m_flags.test(D3D9DeviceFlag::DirtyBlendState))
      BindBlendState();

    if (m_flags.test(D3D9DeviceFlag::DirtyDepthStencilState))
      BindDepthStencilState();

    if (m_flags.test(D3D9DeviceFlag::DirtyRasterizerState))
      BindRasterizerState();

    if (m_flags.test(D3D9DeviceFlag::DirtyDepthBias))
      BindDepthBias();

    if (m_flags.test(D3D9DeviceFlag::DirtyMultiSampleState))
      BindMultiSampleState();

    if (m_flags.test(D3D9DeviceFlag::DirtyAlphaTestState))
      BindAlphaTestState();

    if (m_flags.test(D3D9DeviceFlag::DirtyClipPlanes))
      UpdateClipPlanes();

    if (PrimitiveType == D3DPT_POINTLIST)
      UpdatePointMode<true>();
    else if (m_lastPointMode != 0)
      UpdatePointMode<false>();

    if (likely(UseProgrammableVS())) {
      if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyProgVertexShader))) {
        m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

        BindShader<DxsoProgramType::VertexShader>(
          GetCommonShader(m_state.vertexShader),
          GetVertexShaderPermutation());
      }
      UploadConstants<DxsoProgramTypes::VertexShader>();

      if (likely(!CanSWVP())) {
        UpdateBoolSpecConstantVertex(
          m_state.vsConsts.bConsts[0] &
          m_consts[DxsoProgramType::VertexShader].meta.boolConstantMask);
      } else
        UpdateBoolSpecConstantVertex(0);
    }
    else {
      UpdateBoolSpecConstantVertex(0);
      UpdateFixedFunctionVS();
    }

    if (m_flags.test(D3D9DeviceFlag::DirtyInputLayout))
      BindInputLayout();

    auto UpdateSamplerTypes = [&](uint32_t types, uint32_t projections, uint32_t fetch4) {
      if (m_lastSamplerTypeBitfield != types)
        UpdateSamplerSpecConsant(types);

      if (m_lastProjectionBitfield != projections)
        UpdateProjectionSpecConstant(projections);

      if (m_lastFetch4 != fetch4)
        UpdateFetch4SpecConstant(fetch4);
    };

    if (likely(UseProgrammablePS())) {
      UploadConstants<DxsoProgramTypes::PixelShader>();

      const uint32_t psTextureMask = m_activeTextures & m_psShaderMasks.samplerMask;

      uint32_t fetch4    = m_fetch4             & psTextureMask;
      uint32_t projected = m_projectionBitfield & psTextureMask;

      if (GetCommonShader(m_state.pixelShader)->GetInfo().majorVersion() >= 2)
        UpdateSamplerTypes(m_d3d9Options.forceSamplerTypeSpecConstants ? m_samplerTypeBitfield : 0u, 0u, fetch4);
      else
        UpdateSamplerTypes(m_samplerTypeBitfield, projected, fetch4); // For implicit samplers...

      UpdateBoolSpecConstantPixel(
        m_state.psConsts.bConsts[0] &
        m_consts[DxsoProgramType::PixelShader].meta.boolConstantMask);
    }
    else {
      UpdateBoolSpecConstantPixel(0);
      UpdateSamplerTypes(0u, 0u, 0u);

      UpdateFixedFunctionPS();
    }

    if (m_flags.test(D3D9DeviceFlag::DirtySharedPixelShaderData)) {
      m_flags.clr(D3D9DeviceFlag::DirtySharedPixelShaderData);

      DxvkBufferSliceHandle slice = m_psShared->allocSlice();

      EmitCs([
        cBuffer = m_psShared,
        cSlice  = slice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cSlice);
      });

      D3D9SharedPS* data = reinterpret_cast<D3D9SharedPS*>(slice.mapPtr);

      for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
        DecodeD3DCOLOR(D3DCOLOR(m_state.textureStages[i][DXVK_TSS_CONSTANT]), data->Stages[i].Constant);

        // Flip major-ness so we can get away with a nice easy
        // dot in the shader without complex access
        data->Stages[i].BumpEnvMat[0][0] = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVMAT00]);
        data->Stages[i].BumpEnvMat[1][0] = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVMAT01]);
        data->Stages[i].BumpEnvMat[0][1] = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVMAT10]);
        data->Stages[i].BumpEnvMat[1][1] = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVMAT11]);

        data->Stages[i].BumpEnvLScale    = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVLSCALE]);
        data->Stages[i].BumpEnvLOffset   = bit::cast<float>(m_state.textureStages[i][DXVK_TSS_BUMPENVLOFFSET]);
      }
    }

    if (m_flags.test(D3D9DeviceFlag::DirtyDepthBounds)) {
      m_flags.clr(D3D9DeviceFlag::DirtyDepthBounds);

      DxvkDepthBounds db;
      db.enableDepthBounds  = (m_state.renderStates[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB));
      db.minDepthBounds     = bit::cast<float>(m_state.renderStates[D3DRS_ADAPTIVETESS_Z]);
      db.maxDepthBounds     = bit::cast<float>(m_state.renderStates[D3DRS_ADAPTIVETESS_W]);

      EmitCs([
        cDepthBounds = db
      ] (DxvkContext* ctx) {
        ctx->setDepthBounds(cDepthBounds);
      });
    }
  }


  template <DxsoProgramType ShaderStage>
  void D3D9DeviceEx::BindShader(
  const D3D9CommonShader*                 pShaderModule,
        D3D9ShaderPermutation             Permutation) {
    EmitCs([
      cShader = pShaderModule->GetShader(Permutation)
    ] (DxvkContext* ctx) {
      ctx->bindShader(GetShaderStage(ShaderStage), cShader);
    });
  }


  void D3D9DeviceEx::BindInputLayout() {
    m_flags.clr(D3D9DeviceFlag::DirtyInputLayout);

    if (m_state.vertexDecl == nullptr) {
      EmitCs([&cIaState = m_iaState] (DxvkContext* ctx) {
        cIaState.streamsUsed = 0;
        ctx->setInputLayout(0, nullptr, 0, nullptr);
      });
    }
    else {
      std::array<uint32_t, caps::MaxStreams> streamFreq;

      for (uint32_t i = 0; i < caps::MaxStreams; i++)
        streamFreq[i] = m_state.streamFreq[i];

      Com<D3D9VertexDecl,   false> vertexDecl = m_state.vertexDecl;
      Com<D3D9VertexShader, false> vertexShader;

      if (UseProgrammableVS())
        vertexShader = m_state.vertexShader;

      EmitCs([
        &cIaState         = m_iaState,
        cVertexDecl       = std::move(vertexDecl),
        cVertexShader     = std::move(vertexShader),
        cStreamsInstanced = m_instancedData,
        cStreamFreq       = streamFreq
      ] (DxvkContext* ctx) {
        cIaState.streamsInstanced = cStreamsInstanced;
        cIaState.streamsUsed      = 0;

        const auto& elements = cVertexDecl->GetElements();

        std::array<DxvkVertexAttribute, 2 * caps::InputRegisterCount> attrList;
        std::array<DxvkVertexBinding,   2 * caps::InputRegisterCount> bindList;

        uint32_t attrMask = 0;
        uint32_t bindMask = 0;

        const auto& isgn = cVertexShader != nullptr
          ? GetCommonShader(cVertexShader)->GetIsgn()
          : GetFixedFunctionIsgn();

        for (uint32_t i = 0; i < isgn.elemCount; i++) {
          const auto& decl = isgn.elems[i];

          DxvkVertexAttribute attrib;
          attrib.location = i;
          attrib.binding  = NullStreamIdx;
          attrib.format   = VK_FORMAT_R32G32B32A32_SFLOAT;
          attrib.offset   = 0;

          for (const auto& element : elements) {
            DxsoSemantic elementSemantic = { static_cast<DxsoUsage>(element.Usage), element.UsageIndex };
            if (elementSemantic.usage == DxsoUsage::PositionT)
              elementSemantic.usage = DxsoUsage::Position;

            if (elementSemantic == decl.semantic) {
              attrib.binding = uint32_t(element.Stream);
              attrib.format  = DecodeDecltype(D3DDECLTYPE(element.Type));
              attrib.offset  = element.Offset;

              cIaState.streamsUsed |= 1u << attrib.binding;
              break;
            }
          }

          attrList[i] = attrib;

          DxvkVertexBinding binding;
          binding.binding = attrib.binding;

          uint32_t instanceData = cStreamFreq[binding.binding % caps::MaxStreams];
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
        uint32_t attrCount = CompactSparseList(attrList.data(), attrMask);
        uint32_t bindCount = CompactSparseList(bindList.data(), bindMask);

        ctx->setInputLayout(
          attrCount, attrList.data(),
          bindCount, bindList.data());
      });
    }
  }


  void D3D9DeviceEx::BindVertexBuffer(
        UINT                              Slot,
        D3D9VertexBuffer*                 pBuffer,
        UINT                              Offset,
        UINT                              Stride) {
    EmitCs([
      cSlotId       = Slot,
      cBufferSlice  = pBuffer != nullptr ?
          pBuffer->GetCommonBuffer()->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>(Offset)
        : DxvkBufferSlice(),
      cStride       = pBuffer != nullptr ? Stride : 0
    ] (DxvkContext* ctx) {
      ctx->bindVertexBuffer(cSlotId, cBufferSlice, cStride);
    });
  }

  void D3D9DeviceEx::BindIndices() {
    D3D9CommonBuffer* buffer = GetCommonBuffer(m_state.indices);

    D3D9Format format = buffer != nullptr
                      ? buffer->Desc()->Format
                      : D3D9Format::INDEX32;

    const VkIndexType indexType = DecodeIndexType(format);

    EmitCs([
      cBufferSlice = buffer != nullptr ? buffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>() : DxvkBufferSlice(),
      cIndexType   = indexType
    ](DxvkContext* ctx) {
      ctx->bindIndexBuffer(cBufferSlice, cIndexType);
    });
  }


  void D3D9DeviceEx::Begin(D3D9Query* pQuery) {
    D3D9DeviceLock lock = LockDevice();

    EmitCs([cQuery = Com<D3D9Query, false>(pQuery)](DxvkContext* ctx) {
      cQuery->Begin(ctx);
    });
  }


  void D3D9DeviceEx::End(D3D9Query* pQuery) {
    D3D9DeviceLock lock = LockDevice();

    EmitCs([cQuery = Com<D3D9Query, false>(pQuery)](DxvkContext* ctx) {
      cQuery->End(ctx);
    });

    pQuery->NotifyEnd();
    if (unlikely(pQuery->IsEvent())) {
      pQuery->IsStalling()
        ? Flush()
        : FlushImplicit(TRUE);
    } else if (pQuery->IsStalling()) {
      FlushImplicit(FALSE);
    }
  }


  void D3D9DeviceEx::SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.vsConsts.bConsts[idx] &= ~mask;
    m_state.vsConsts.bConsts[idx] |= bits & mask;

    m_consts[DxsoProgramTypes::VertexShader].dirty = true;
  }


  void D3D9DeviceEx::SetPixelBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.psConsts.bConsts[idx] &= ~mask;
    m_state.psConsts.bConsts[idx] |= bits & mask;

    m_consts[DxsoProgramTypes::PixelShader].dirty = true;
  }


  HRESULT D3D9DeviceEx::CreateShaderModule(
        D3D9CommonShader*     pShaderModule,
        VkShaderStageFlagBits ShaderStage,
  const DWORD*                pShaderBytecode,
  const DxsoModuleInfo*       pModuleInfo) {
    try {
      m_shaderModules->GetShaderModule(this, pShaderModule,
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
    HRESULT D3D9DeviceEx::SetShaderConstants(
            UINT  StartRegister,
      const T*    pConstantData,
            UINT  Count) {
    const     uint32_t regCountHardware = DetermineHardwareRegCount<ProgramType, ConstantType>();
    constexpr uint32_t regCountSoftware = DetermineSoftwareRegCount<ProgramType, ConstantType>();

    if (unlikely(StartRegister + Count > regCountSoftware))
      return D3DERR_INVALIDCALL;

    Count = UINT(
      std::max<INT>(
        std::clamp<INT>(Count + StartRegister, 0, regCountHardware) - INT(StartRegister),
        0));

    if (unlikely(Count == 0))
      return D3D_OK;

    if (unlikely(pConstantData == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetShaderConstants<ProgramType, ConstantType, T>(
        StartRegister,
        pConstantData,
        Count);

    if constexpr (ConstantType != D3D9ConstantType::Bool) {
      uint32_t maxCount = ConstantType == D3D9ConstantType::Float
        ? m_consts[ProgramType].meta.maxConstIndexF
        : m_consts[ProgramType].meta.maxConstIndexI;

      m_consts[ProgramType].dirty |= StartRegister < maxCount;
    }

    UpdateStateConstants<ProgramType, ConstantType, T>(
      &m_state,
      StartRegister,
      pConstantData,
      Count,
      m_d3d9Options.d3d9FloatEmulation);

    return D3D_OK;
  }


  void D3D9DeviceEx::UpdateFixedFunctionVS() {
    // Shader...
    bool hasPositionT = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasPositionT) : false;
    bool hasBlendWeight    = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasBlendWeight)  : false;
    bool hasBlendIndices   = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasBlendIndices) : false;

    bool indexedVertexBlend = hasBlendIndices && m_state.renderStates[D3DRS_INDEXEDVERTEXBLENDENABLE];

    D3D9FF_VertexBlendMode vertexBlendMode = D3D9FF_VertexBlendMode_Disabled;

    if (m_state.renderStates[D3DRS_VERTEXBLEND] != D3DVBF_DISABLE && !hasPositionT) {
      vertexBlendMode = m_state.renderStates[D3DRS_VERTEXBLEND] == D3DVBF_TWEENING
        ? D3D9FF_VertexBlendMode_Tween
        : D3D9FF_VertexBlendMode_Normal;

      if (m_state.renderStates[D3DRS_VERTEXBLEND] != D3DVBF_0WEIGHTS) {
        if (!hasBlendWeight)
          vertexBlendMode = D3D9FF_VertexBlendMode_Disabled;
      }
      else if (!indexedVertexBlend)
        vertexBlendMode = D3D9FF_VertexBlendMode_Disabled;
    }

    if (unlikely(hasPositionT && m_state.vertexShader != nullptr && !m_flags.test(D3D9DeviceFlag::DirtyProgVertexShader))) {
      m_flags.set(D3D9DeviceFlag::DirtyInputLayout);
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
      m_flags.set(D3D9DeviceFlag::DirtyProgVertexShader);
    }

    if (m_flags.test(D3D9DeviceFlag::DirtyFFVertexShader)) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFVertexShader);

      D3D9FFShaderKeyVS key;
      key.Data.Contents.HasPositionT = hasPositionT;
      key.Data.Contents.HasColor0    = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasColor0)    : false;
      key.Data.Contents.HasColor1    = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasColor1)    : false;
      key.Data.Contents.HasPointSize = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasPointSize) : false;
      key.Data.Contents.HasFog       = m_state.vertexDecl != nullptr ? m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasFog)       : false;

      bool lighting    = m_state.renderStates[D3DRS_LIGHTING] != 0 && !key.Data.Contents.HasPositionT;
      bool colorVertex = m_state.renderStates[D3DRS_COLORVERTEX] != 0;
      uint32_t mask    = (lighting && colorVertex)
                       ? (key.Data.Contents.HasColor0 ? D3DMCS_COLOR1 : D3DMCS_MATERIAL)
                       | (key.Data.Contents.HasColor1 ? D3DMCS_COLOR2 : D3DMCS_MATERIAL)
                       : 0;

      key.Data.Contents.UseLighting      = lighting;
      key.Data.Contents.NormalizeNormals = m_state.renderStates[D3DRS_NORMALIZENORMALS];
      key.Data.Contents.LocalViewer      = m_state.renderStates[D3DRS_LOCALVIEWER] && lighting;

      key.Data.Contents.RangeFog         = m_state.renderStates[D3DRS_RANGEFOGENABLE];

      key.Data.Contents.DiffuseSource    = m_state.renderStates[D3DRS_DIFFUSEMATERIALSOURCE]  & mask;
      key.Data.Contents.AmbientSource    = m_state.renderStates[D3DRS_AMBIENTMATERIALSOURCE]  & mask;
      key.Data.Contents.SpecularSource   = m_state.renderStates[D3DRS_SPECULARMATERIALSOURCE] & mask;
      key.Data.Contents.EmissiveSource   = m_state.renderStates[D3DRS_EMISSIVEMATERIALSOURCE] & mask;

      uint32_t lightCount = 0;

      if (key.Data.Contents.UseLighting) {
        for (uint32_t i = 0; i < caps::MaxEnabledLights; i++) {
          if (m_state.enabledLightIndices[i] != UINT32_MAX)
            lightCount++;
        }
      }

      key.Data.Contents.LightCount = lightCount;

      for (uint32_t i = 0; i < caps::MaxTextureBlendStages; i++) {
        uint32_t transformFlags = m_state.textureStages[i][DXVK_TSS_TEXTURETRANSFORMFLAGS] & ~(D3DTTFF_PROJECTED);
        uint32_t index          = m_state.textureStages[i][DXVK_TSS_TEXCOORDINDEX];
        uint32_t indexFlags     = (index & TCIMask) >> TCIOffset;

        transformFlags &= 0b111;
        index          &= 0b111;

        key.Data.Contents.TransformFlags  |= transformFlags << (i * 3);
        key.Data.Contents.TexcoordFlags   |= indexFlags     << (i * 3);
        key.Data.Contents.TexcoordIndices |= index          << (i * 3);
      }

      key.Data.Contents.TexcoordDeclMask = m_state.vertexDecl != nullptr ? m_state.vertexDecl->GetTexcoordMask() : 0;

      key.Data.Contents.VertexBlendMode    = uint32_t(vertexBlendMode);

      if (vertexBlendMode == D3D9FF_VertexBlendMode_Normal) {
        key.Data.Contents.VertexBlendIndexed = indexedVertexBlend;
        key.Data.Contents.VertexBlendCount   = m_state.renderStates[D3DRS_VERTEXBLEND] & 0xff;
      }

      key.Data.Contents.VertexClipping = IsClipPlaneEnabled();

      EmitCs([
        this,
        cKey     = key,
       &cShaders = m_ffModules
      ](DxvkContext* ctx) {
        auto shader = cShaders.GetShaderModule(this, cKey);
        ctx->bindShader(VK_SHADER_STAGE_VERTEX_BIT, shader.GetShader());
      });
    }

    if (hasPositionT && (m_flags.test(D3D9DeviceFlag::DirtyFFViewport) || m_ffZTest != IsZTestEnabled())) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFViewport);
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

      const auto& vp = m_state.viewport;
      // For us to account for the Vulkan viewport rules
      // when translating Window Coords -> Real Coords:
      // We need to negate the inverse extent we multiply by,
      // this follows through to the offset when that gets
      // timesed by it.
      // The 1.0f additional offset however does not,
      // so we account for that there manually.

      m_ffZTest = IsZTestEnabled();

      m_viewportInfo.inverseExtent = Vector4(
         2.0f / float(vp.Width),
        -2.0f / float(vp.Height),
        m_ffZTest ? 1.0f : 0.0f,
        1.0f);

      m_viewportInfo.inverseOffset = Vector4(
        -float(vp.X), -float(vp.Y),
         0.0f,         0.0f);

      m_viewportInfo.inverseOffset = m_viewportInfo.inverseOffset * m_viewportInfo.inverseExtent;

      m_viewportInfo.inverseOffset = m_viewportInfo.inverseOffset + Vector4(-1.0f, 1.0f, 0.0f, 0.0f);
    }

    // Constants...
    if (m_flags.test(D3D9DeviceFlag::DirtyFFVertexData)) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFVertexData);

      DxvkBufferSliceHandle slice = m_vsFixedFunction->allocSlice();

      EmitCs([
        cBuffer = m_vsFixedFunction,
        cSlice  = slice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cSlice);
      });

      auto WorldView    = m_state.transforms[GetTransformIndex(D3DTS_VIEW)] * m_state.transforms[GetTransformIndex(D3DTS_WORLD)];
      auto NormalMatrix = inverse(WorldView);

      D3D9FixedFunctionVS* data = reinterpret_cast<D3D9FixedFunctionVS*>(slice.mapPtr);
      data->WorldView    = WorldView;
      data->NormalMatrix = NormalMatrix;
      data->InverseView  = transpose(inverse(m_state.transforms[GetTransformIndex(D3DTS_VIEW)]));
      data->Projection   = m_state.transforms[GetTransformIndex(D3DTS_PROJECTION)];

      for (uint32_t i = 0; i < data->TexcoordMatrices.size(); i++)
        data->TexcoordMatrices[i] = m_state.transforms[GetTransformIndex(D3DTS_TEXTURE0) + i];

      data->ViewportInfo = m_viewportInfo;

      DecodeD3DCOLOR(m_state.renderStates[D3DRS_AMBIENT], data->GlobalAmbient.data);

      uint32_t lightIdx = 0;
      for (uint32_t i = 0; i < caps::MaxEnabledLights; i++) {
        auto idx = m_state.enabledLightIndices[i];
        if (idx == UINT32_MAX)
          continue;

        data->Lights[lightIdx++] = D3D9Light(m_state.lights[idx].value(), m_state.transforms[GetTransformIndex(D3DTS_VIEW)]);
      }

      data->Material = m_state.material;
      data->TweenFactor = bit::cast<float>(m_state.renderStates[D3DRS_TWEENFACTOR]);
    }

    if (m_flags.test(D3D9DeviceFlag::DirtyFFVertexBlend) && vertexBlendMode == D3D9FF_VertexBlendMode_Normal) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFVertexBlend);

      DxvkBufferSliceHandle slice = m_vsVertexBlend->allocSlice();

      EmitCs([
        cBuffer = m_vsVertexBlend,
        cSlice  = slice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cSlice);
      });

      auto UploadVertexBlendData = [&](auto data) {
        for (uint32_t i = 0; i < countof(data->WorldView); i++)
          data->WorldView[i] = m_state.transforms[GetTransformIndex(D3DTS_VIEW)] * m_state.transforms[GetTransformIndex(D3DTS_WORLDMATRIX(i))];
      };

      (m_isSWVP && indexedVertexBlend)
        ? UploadVertexBlendData(reinterpret_cast<D3D9FixedFunctionVertexBlendDataSW*>(slice.mapPtr))
        : UploadVertexBlendData(reinterpret_cast<D3D9FixedFunctionVertexBlendDataHW*>(slice.mapPtr));
    }
  }


  void D3D9DeviceEx::UpdateFixedFunctionPS() {
    // Shader...
    if (m_flags.test(D3D9DeviceFlag::DirtyFFPixelShader)) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFPixelShader);

      // Used args for a given operation.
      auto ArgsMask = [](DWORD Op) {
        switch (Op) {
          case D3DTOP_DISABLE:
            return 0b000u; // No Args
          case D3DTOP_SELECTARG1:
          case D3DTOP_PREMODULATE:
            return 0b010u; // Arg 1
          case D3DTOP_SELECTARG2:
            return 0b100u; // Arg 2
          case D3DTOP_MULTIPLYADD:
          case D3DTOP_LERP:
            return 0b111u; // Arg 0, 1, 2
          default:
            return 0b110u; // Arg 1, 2
        }
      };

      D3D9FFShaderKeyFS key;

      uint32_t idx;
      for (idx = 0; idx < caps::TextureStageCount; idx++) {
        auto& stage = key.Stages[idx].Contents;
        auto& data  = m_state.textureStages[idx];

        // Subsequent stages do not occur if this is true.
        if (data[DXVK_TSS_COLOROP] == D3DTOP_DISABLE)
          break;

        // If the stage is invalid (ie. no texture bound),
        // this and all subsequent stages get disabled.
        if (m_state.textures[idx] == nullptr) {
          if (((data[DXVK_TSS_COLORARG0] & D3DTA_SELECTMASK) == D3DTA_TEXTURE && (ArgsMask(data[DXVK_TSS_COLOROP]) & (1 << 0u)))
           || ((data[DXVK_TSS_COLORARG1] & D3DTA_SELECTMASK) == D3DTA_TEXTURE && (ArgsMask(data[DXVK_TSS_COLOROP]) & (1 << 1u)))
           || ((data[DXVK_TSS_COLORARG2] & D3DTA_SELECTMASK) == D3DTA_TEXTURE && (ArgsMask(data[DXVK_TSS_COLOROP]) & (1 << 2u))))
            break;
        }

        stage.ColorOp = data[DXVK_TSS_COLOROP];
        stage.AlphaOp = data[DXVK_TSS_ALPHAOP];

        stage.ColorArg0 = data[DXVK_TSS_COLORARG0];
        stage.ColorArg1 = data[DXVK_TSS_COLORARG1];
        stage.ColorArg2 = data[DXVK_TSS_COLORARG2];

        stage.AlphaArg0 = data[DXVK_TSS_ALPHAARG0];
        stage.AlphaArg1 = data[DXVK_TSS_ALPHAARG1];
        stage.AlphaArg2 = data[DXVK_TSS_ALPHAARG2];

        const uint32_t samplerOffset = idx * 2;
        stage.Type         = (m_samplerTypeBitfield >> samplerOffset) & 0xffu;
        stage.ResultIsTemp = data[DXVK_TSS_RESULTARG] == D3DTA_TEMP;

        uint32_t ttff  = data[DXVK_TSS_TEXTURETRANSFORMFLAGS];
        uint32_t count = ttff & ~D3DTTFF_PROJECTED;

        stage.Projected      = (ttff & D3DTTFF_PROJECTED) ? 1      : 0;
        stage.ProjectedCount = (ttff & D3DTTFF_PROJECTED) ? count  : 0;
      }

      auto& stage0 = key.Stages[0].Contents;

      if (stage0.ResultIsTemp &&
          stage0.ColorOp != D3DTOP_DISABLE &&
          stage0.AlphaOp == D3DTOP_DISABLE) {
        stage0.AlphaOp   = D3DTOP_SELECTARG1;
        stage0.AlphaArg1 = D3DTA_DIFFUSE;
      }

      stage0.GlobalSpecularEnable = m_state.renderStates[D3DRS_SPECULARENABLE];
      stage0.GlobalFlatShade      = m_state.renderStates[D3DRS_SHADEMODE] == D3DSHADE_FLAT;

      // The last stage *always* writes to current.
      if (idx >= 1)
        key.Stages[idx - 1].Contents.ResultIsTemp = false;

      EmitCs([
        this,
        cKey     = key,
       &cShaders = m_ffModules
      ](DxvkContext* ctx) {
        auto shader = cShaders.GetShaderModule(this, cKey);
        ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, shader.GetShader());
      });
    }

    // Constants

    if (m_flags.test(D3D9DeviceFlag::DirtyFFPixelData)) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFPixelData);

      DxvkBufferSliceHandle slice = m_psFixedFunction->allocSlice();

      EmitCs([
        cBuffer = m_psFixedFunction,
        cSlice  = slice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cSlice);
      });

      auto& rs = m_state.renderStates;

      D3D9FixedFunctionPS* data = reinterpret_cast<D3D9FixedFunctionPS*>(slice.mapPtr);
      DecodeD3DCOLOR((D3DCOLOR)rs[D3DRS_TEXTUREFACTOR], data->textureFactor.data);
    }
  }


  bool D3D9DeviceEx::UseProgrammableVS() {
    return m_state.vertexShader != nullptr
      && m_state.vertexDecl != nullptr
      && !m_state.vertexDecl->TestFlag(D3D9VertexDeclFlag::HasPositionT);
  }


  bool D3D9DeviceEx::UseProgrammablePS() {
    return m_state.pixelShader != nullptr;
  }


  void D3D9DeviceEx::UpdateBoolSpecConstantVertex(uint32_t value) {
    if (value == m_lastBoolSpecConstantVertex)
      return;

    EmitCs([cBitfield = value](DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::VertexShaderBools, cBitfield);
      });

    m_lastBoolSpecConstantVertex = value;
  }


  void D3D9DeviceEx::UpdateBoolSpecConstantPixel(uint32_t value) {
    if (value == m_lastBoolSpecConstantPixel)
      return;

    EmitCs([cBitfield = value](DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::PixelShaderBools, cBitfield);
      });

    m_lastBoolSpecConstantPixel = value;
  }


  void D3D9DeviceEx::UpdateSamplerSpecConsant(uint32_t value) {
    EmitCs([cBitfield = value](DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::SamplerType, cBitfield);
    });

    m_lastSamplerTypeBitfield = value;
  }


  void D3D9DeviceEx::UpdateProjectionSpecConstant(uint32_t value) {
    EmitCs([cBitfield = value](DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::ProjectionType, cBitfield);
    });

    m_lastProjectionBitfield = value;
  }


  void D3D9DeviceEx::UpdateFetch4SpecConstant(uint32_t value) {
    EmitCs([cBitfield = value](DxvkContext* ctx) {
      ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, D3D9SpecConstantId::Fetch4, cBitfield);
      });

    m_lastFetch4 = value;
  }


  void D3D9DeviceEx::ApplyPrimitiveType(
    DxvkContext*      pContext,
    D3DPRIMITIVETYPE  PrimType) {
    if (m_iaState.primitiveType != PrimType) {
      m_iaState.primitiveType = PrimType;

      auto iaState = DecodeInputAssemblyState(PrimType);
      pContext->setInputAssemblyState(iaState);
    }
  }


  void D3D9DeviceEx::ResolveZ() {
    D3D9Surface*           src = m_state.depthStencil.ptr();
    IDirect3DBaseTexture9* dst = m_state.textures[0];

    if (unlikely(!src || !dst))
      return;

    D3D9CommonTexture* srcTextureInfo = GetCommonTexture(src);
    D3D9CommonTexture* dstTextureInfo = GetCommonTexture(dst);

    const D3D9_COMMON_TEXTURE_DESC* srcDesc = srcTextureInfo->Desc();
    const D3D9_COMMON_TEXTURE_DESC* dstDesc = dstTextureInfo->Desc();

    VkSampleCountFlagBits dstSampleCount;
    DecodeMultiSampleType(dstDesc->MultiSample, dstDesc->MultisampleQuality, &dstSampleCount);

    if (unlikely(dstSampleCount != VK_SAMPLE_COUNT_1_BIT)) {
      Logger::warn("D3D9DeviceEx::ResolveZ: dstSampleCount != 1. Discarding.");
      return;
    }

    const D3D9_VK_FORMAT_MAPPING srcFormatInfo = LookupFormat(srcDesc->Format);
    const D3D9_VK_FORMAT_MAPPING dstFormatInfo = LookupFormat(dstDesc->Format);

    auto srcVulkanFormatInfo = imageFormatInfo(srcFormatInfo.FormatColor);
    auto dstVulkanFormatInfo = imageFormatInfo(dstFormatInfo.FormatColor);

    const VkImageSubresource dstSubresource =
      dstTextureInfo->GetSubresourceFromIndex(
        dstVulkanFormatInfo->aspectMask, 0);

    const VkImageSubresource srcSubresource =
      srcTextureInfo->GetSubresourceFromIndex(
        srcVulkanFormatInfo->aspectMask, src->GetSubresource());

    const VkImageSubresourceLayers dstSubresourceLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };

    const VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    VkSampleCountFlagBits srcSampleCount;
    DecodeMultiSampleType(srcDesc->MultiSample, srcDesc->MultisampleQuality, &srcSampleCount);

    if (srcSampleCount == VK_SAMPLE_COUNT_1_BIT) {
      EmitCs([
        cDstImage  = dstTextureInfo->GetImage(),
        cSrcImage  = srcTextureInfo->GetImage(),
        cDstLayers = dstSubresourceLayers,
        cSrcLayers = srcSubresourceLayers
      ] (DxvkContext* ctx) {
        ctx->copyImage(
          cDstImage, cDstLayers, VkOffset3D { 0, 0, 0 },
          cSrcImage, cSrcLayers, VkOffset3D { 0, 0, 0 },
          cDstImage->mipLevelExtent(cDstLayers.mipLevel));
      });
    } else {
      EmitCs([
        cDstImage  = dstTextureInfo->GetImage(),
        cSrcImage  = srcTextureInfo->GetImage(),
        cDstSubres = dstSubresourceLayers,
        cSrcSubres = srcSubresourceLayers
      ] (DxvkContext* ctx) {
        // We should resolve using the first sample according to
        // http://amd-dev.wpengine.netdna-cdn.com/wordpress/media/2012/10/Advanced-DX9-Capabilities-for-ATI-Radeon-Cards_v2.pdf
        // "The resolve operation copies the depth value from the *first sample only* into the resolved depth stencil texture."
        constexpr auto resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR;

        VkImageResolve region;
        region.srcSubresource = cSrcSubres;
        region.srcOffset      = VkOffset3D { 0, 0, 0 };
        region.dstSubresource = cDstSubres;
        region.dstOffset      = VkOffset3D { 0, 0, 0 };
        region.extent         = cDstImage->mipLevelExtent(cDstSubres.mipLevel);

        ctx->resolveDepthStencilImage(cDstImage, cSrcImage, region, resolveMode, resolveMode);
      });
    }

    dstTextureInfo->MarkAllWrittenByGPU();
  }


  void D3D9DeviceEx::TransitionImage(D3D9CommonTexture* pResource, VkImageLayout NewLayout) {
    EmitCs([
      cImage        = pResource->GetImage(),
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->changeImageLayout(
        cImage, cNewLayout);
    });
  }


  void D3D9DeviceEx::TransformImage(
          D3D9CommonTexture*       pResource,
    const VkImageSubresourceRange* pSubresources,
          VkImageLayout            OldLayout,
          VkImageLayout            NewLayout) {
    EmitCs([
      cImage        = pResource->GetImage(),
      cSubresources = *pSubresources,
      cOldLayout    = OldLayout,
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->transformImage(
        cImage, cSubresources,
        cOldLayout, cNewLayout);
    });
  }


  HRESULT D3D9DeviceEx::ResetState(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (!pPresentationParameters->EnableAutoDepthStencil)
      SetDepthStencilSurface(nullptr);

    for (uint32_t i = 1; i < caps::MaxSimultaneousRenderTargets; i++)
      SetRenderTarget(0, nullptr);

    auto& rs = m_state.renderStates;

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

    rs[D3DRS_ZENABLE]                  = pPresentationParameters->EnableAutoDepthStencil
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
    BindDepthBias();

    rs[D3DRS_SCISSORTESTENABLE]   = FALSE;

    rs[D3DRS_ALPHATESTENABLE]     = FALSE;
    rs[D3DRS_ALPHAFUNC]           = D3DCMP_ALWAYS;
    BindAlphaTestState();
    rs[D3DRS_ALPHAREF]            = 0;
    UpdatePushConstant<D3D9RenderStateItem::AlphaRef>();

    rs[D3DRS_MULTISAMPLEMASK]     = 0xffffffff;
    BindMultiSampleState();

    rs[D3DRS_TEXTUREFACTOR]       = 0xffffffff;
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelData);

    rs[D3DRS_DIFFUSEMATERIALSOURCE]  = D3DMCS_COLOR1;
    rs[D3DRS_SPECULARMATERIALSOURCE] = D3DMCS_COLOR2;
    rs[D3DRS_AMBIENTMATERIALSOURCE]  = D3DMCS_MATERIAL;
    rs[D3DRS_EMISSIVEMATERIALSOURCE] = D3DMCS_MATERIAL;
    rs[D3DRS_LIGHTING]               = TRUE;
    rs[D3DRS_COLORVERTEX]            = TRUE;
    rs[D3DRS_LOCALVIEWER]            = TRUE;
    rs[D3DRS_RANGEFOGENABLE]         = FALSE;
    rs[D3DRS_NORMALIZENORMALS]       = FALSE;
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);

    // PS
    rs[D3DRS_SPECULARENABLE] = FALSE;

    rs[D3DRS_AMBIENT]                = 0;
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    rs[D3DRS_FOGENABLE]                  = FALSE;
    rs[D3DRS_FOGCOLOR]                   = 0;
    rs[D3DRS_FOGTABLEMODE]               = D3DFOG_NONE;
    rs[D3DRS_FOGSTART]                   = bit::cast<DWORD>(0.0f);
    rs[D3DRS_FOGEND]                     = bit::cast<DWORD>(1.0f);
    rs[D3DRS_FOGDENSITY]                 = bit::cast<DWORD>(1.0f);
    rs[D3DRS_FOGVERTEXMODE]              = D3DFOG_NONE;
    m_flags.set(D3D9DeviceFlag::DirtyFogColor);
    m_flags.set(D3D9DeviceFlag::DirtyFogDensity);
    m_flags.set(D3D9DeviceFlag::DirtyFogEnd);
    m_flags.set(D3D9DeviceFlag::DirtyFogScale);
    m_flags.set(D3D9DeviceFlag::DirtyFogState);

    rs[D3DRS_CLIPPLANEENABLE] = 0;
    m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);

    rs[D3DRS_POINTSPRITEENABLE]          = FALSE;
    rs[D3DRS_POINTSCALEENABLE]           = FALSE;
    rs[D3DRS_POINTSCALE_A]               = bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSCALE_B]               = bit::cast<DWORD>(0.0f);
    rs[D3DRS_POINTSCALE_C]               = bit::cast<DWORD>(0.0f);
    rs[D3DRS_POINTSIZE]                  = bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSIZE_MIN]              = bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSIZE_MAX]              = bit::cast<DWORD>(64.0f);
    UpdatePushConstant<D3D9RenderStateItem::PointSize>();
    UpdatePushConstant<D3D9RenderStateItem::PointSizeMin>();
    UpdatePushConstant<D3D9RenderStateItem::PointSizeMax>();
    m_flags.set(D3D9DeviceFlag::DirtyPointScale);
    UpdatePointMode<false>();

    rs[D3DRS_SRGBWRITEENABLE]            = 0;

    rs[D3DRS_SHADEMODE]                  = D3DSHADE_GOURAUD;

    rs[D3DRS_VERTEXBLEND]                = D3DVBF_DISABLE;
    rs[D3DRS_INDEXEDVERTEXBLENDENABLE]   = FALSE;
    rs[D3DRS_TWEENFACTOR]                = bit::cast<DWORD>(0.0f);
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);

    // Render States not implemented beyond this point.
    rs[D3DRS_LASTPIXEL]                  = TRUE;
    rs[D3DRS_DITHERENABLE]               = FALSE;
    rs[D3DRS_WRAP0]                      = 0;
    rs[D3DRS_WRAP1]                      = 0;
    rs[D3DRS_WRAP2]                      = 0;
    rs[D3DRS_WRAP3]                      = 0;
    rs[D3DRS_WRAP4]                      = 0;
    rs[D3DRS_WRAP5]                      = 0;
    rs[D3DRS_WRAP6]                      = 0;
    rs[D3DRS_WRAP7]                      = 0;
    rs[D3DRS_CLIPPING]                   = TRUE;
    rs[D3DRS_MULTISAMPLEANTIALIAS]       = TRUE;
    rs[D3DRS_PATCHEDGESTYLE]             = D3DPATCHEDGE_DISCRETE;
    rs[D3DRS_DEBUGMONITORTOKEN]          = D3DDMT_ENABLE;
    rs[D3DRS_POSITIONDEGREE]             = D3DDEGREE_CUBIC;
    rs[D3DRS_NORMALDEGREE]               = D3DDEGREE_LINEAR;
    rs[D3DRS_ANTIALIASEDLINEENABLE]      = FALSE;
    rs[D3DRS_MINTESSELLATIONLEVEL]       = bit::cast<DWORD>(1.0f);
    rs[D3DRS_MAXTESSELLATIONLEVEL]       = bit::cast<DWORD>(1.0f);
    rs[D3DRS_ADAPTIVETESS_X]             = bit::cast<DWORD>(0.0f);
    rs[D3DRS_ADAPTIVETESS_Y]             = bit::cast<DWORD>(0.0f);
    rs[D3DRS_ADAPTIVETESS_Z]             = bit::cast<DWORD>(1.0f);
    rs[D3DRS_ADAPTIVETESS_W]             = bit::cast<DWORD>(0.0f);
    rs[D3DRS_ENABLEADAPTIVETESSELLATION] = FALSE;
    rs[D3DRS_WRAP8]                      = 0;
    rs[D3DRS_WRAP9]                      = 0;
    rs[D3DRS_WRAP10]                     = 0;
    rs[D3DRS_WRAP11]                     = 0;
    rs[D3DRS_WRAP12]                     = 0;
    rs[D3DRS_WRAP13]                     = 0;
    rs[D3DRS_WRAP14]                     = 0;
    rs[D3DRS_WRAP15]                     = 0;
    // End Unimplemented Render States

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      auto& stage = m_state.textureStages[i];

      stage[DXVK_TSS_COLOROP]               = i == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE;
      stage[DXVK_TSS_COLORARG1]             = D3DTA_TEXTURE;
      stage[DXVK_TSS_COLORARG2]             = D3DTA_CURRENT;
      stage[DXVK_TSS_ALPHAOP]               = i == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
      stage[DXVK_TSS_ALPHAARG1]             = D3DTA_TEXTURE;
      stage[DXVK_TSS_ALPHAARG2]             = D3DTA_CURRENT;
      stage[DXVK_TSS_BUMPENVMAT00]          = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_BUMPENVMAT01]          = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_BUMPENVMAT10]          = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_BUMPENVMAT11]          = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_TEXCOORDINDEX]         = i;
      stage[DXVK_TSS_BUMPENVLSCALE]         = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_BUMPENVLOFFSET]        = bit::cast<DWORD>(0.0f);
      stage[DXVK_TSS_TEXTURETRANSFORMFLAGS] = D3DTTFF_DISABLE;
      stage[DXVK_TSS_COLORARG0]             = D3DTA_CURRENT;
      stage[DXVK_TSS_ALPHAARG0]             = D3DTA_CURRENT;
      stage[DXVK_TSS_RESULTARG]             = D3DTA_CURRENT;
      stage[DXVK_TSS_CONSTANT]              = 0x00000000;
    }
    m_flags.set(D3D9DeviceFlag::DirtySharedPixelShaderData);
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

    for (uint32_t i = 0; i < caps::MaxStreams; i++)
      m_state.streamFreq[i] = 1;

    for (uint32_t i = 0; i < m_state.textures.size(); i++) {
      TextureChangePrivate(m_state.textures[i], nullptr);

      DWORD sampler = i;
      auto samplerInfo = RemapStateSamplerShader(sampler);
      uint32_t colorSlot = computeResourceSlotId(samplerInfo.first, DxsoBindingType::ColorImage, uint32_t(samplerInfo.second));
      uint32_t depthSlot = computeResourceSlotId(samplerInfo.first, DxsoBindingType::DepthImage, uint32_t(samplerInfo.second));

      EmitCs([
        cColorSlot = colorSlot,
        cDepthSlot = depthSlot
      ](DxvkContext* ctx) {
        ctx->bindResourceView(cColorSlot, nullptr, nullptr);
        ctx->bindResourceView(cDepthSlot, nullptr, nullptr);
      });
    }

    m_dirtyTextures = 0;

    auto& ss = m_state.samplerStates;
    for (uint32_t i = 0; i < ss.size(); i++) {
      auto& state = ss[i];
      state[D3DSAMP_ADDRESSU]      = D3DTADDRESS_WRAP;
      state[D3DSAMP_ADDRESSV]      = D3DTADDRESS_WRAP;
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

    // We should do this...
    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

    UpdateSamplerSpecConsant(0u);
    UpdateBoolSpecConstantVertex(0u);
    UpdateBoolSpecConstantPixel(0u);

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::ResetSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    D3D9Format backBufferFmt = EnumerateFormat(pPresentationParameters->BackBufferFormat);

    Logger::info(str::format(
      "D3D9DeviceEx::ResetSwapChain:\n",
      "  Requested Presentation Parameters\n",
      "    - Width:              ", pPresentationParameters->BackBufferWidth, "\n",
      "    - Height:             ", pPresentationParameters->BackBufferHeight, "\n",
      "    - Format:             ", backBufferFmt, "\n"
      "    - Auto Depth Stencil: ", pPresentationParameters->EnableAutoDepthStencil ? "true" : "false", "\n",
      "                ^ Format: ", EnumerateFormat(pPresentationParameters->AutoDepthStencilFormat), "\n",
      "    - Windowed:           ", pPresentationParameters->Windowed ? "true" : "false", "\n"));

    if (backBufferFmt != D3D9Format::Unknown) {
      if (!IsSupportedBackBufferFormat(backBufferFmt)) {
        Logger::err(str::format("D3D9DeviceEx::ResetSwapChain: Unsupported backbuffer format: ",
          EnumerateFormat(pPresentationParameters->BackBufferFormat)));
        return D3DERR_INVALIDCALL;
      }
    }

    if (m_implicitSwapchain != nullptr)
      m_implicitSwapchain->Reset(pPresentationParameters, pFullscreenDisplayMode);
    else
      m_implicitSwapchain = new D3D9SwapChainEx(this, pPresentationParameters, pFullscreenDisplayMode);

    if (pPresentationParameters->EnableAutoDepthStencil) {
      D3D9_COMMON_TEXTURE_DESC desc;
      desc.Width              = pPresentationParameters->BackBufferWidth;
      desc.Height             = pPresentationParameters->BackBufferHeight;
      desc.Depth              = 1;
      desc.ArraySize          = 1;
      desc.MipLevels          = 1;
      desc.Usage              = D3DUSAGE_DEPTHSTENCIL;
      desc.Format             = EnumerateFormat(pPresentationParameters->AutoDepthStencilFormat);
      desc.Pool               = D3DPOOL_DEFAULT;
      desc.Discard            = FALSE;
      desc.MultiSample        = pPresentationParameters->MultiSampleType;
      desc.MultisampleQuality = pPresentationParameters->MultiSampleQuality;
      desc.IsBackBuffer       = FALSE;
      desc.IsAttachmentOnly   = TRUE;

      if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, &desc)))
        return D3DERR_NOTAVAILABLE;

      m_autoDepthStencil = new D3D9Surface(this, &desc, nullptr);
      m_initializer->InitTexture(m_autoDepthStencil->GetCommonTexture());
      SetDepthStencilSurface(m_autoDepthStencil.ptr());
    }

    SetRenderTarget(0, m_implicitSwapchain->GetBackBuffer(0));

    // Force this if we end up binding the same RT to make scissor change go into effect.
    BindViewportAndScissor();

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::InitialReset(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    HRESULT hr = ResetSwapChain(pPresentationParameters, pFullscreenDisplayMode);
    if (FAILED(hr))
      return hr;

    hr = ResetState(pPresentationParameters);
    if (FAILED(hr))
      return hr;

    Flush();
    SynchronizeCsThread();

    return D3D_OK;
  }

}
