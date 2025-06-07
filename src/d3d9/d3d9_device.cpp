#include "d3d9_device.h"

#include "d3d9_annotation.h"
#include "d3d9_common_texture.h"
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
    : m_parent             ( pParent )
    , m_deviceType         ( DeviceType )
    , m_window             ( hFocusWindow )
    , m_behaviorFlags      ( BehaviorFlags )
    , m_adapter            ( pAdapter )
    , m_dxvkDevice         ( dxvkDevice )
    , m_memoryAllocator    ( )
    , m_shaderAllocator    ( )
    , m_shaderModules      ( new D3D9ShaderModuleSet )
    , m_stagingBuffer      ( dxvkDevice, StagingBufferSize )
    , m_stagingBufferFence ( new sync::Fence() )
    , m_d3d9Options        ( dxvkDevice, pParent->GetInstance()->config() )
    , m_multithread        ( BehaviorFlags & D3DCREATE_MULTITHREADED )
    , m_isSWVP             ( (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) ? true : false )
    , m_isD3D8Compatible   ( pParent->IsD3D8Compatible() )
    , m_csThread           ( dxvkDevice, dxvkDevice->createContext() )
    , m_csChunk            ( AllocCsChunk() )
    , m_submissionFence    ( new sync::Fence() )
    , m_flushTracker       ( GetMaxFlushType() )
    , m_d3d9Interop        ( this )
    , m_d3d9On12           ( this )
    , m_d3d8Bridge         ( this ) {
    // If we can SWVP, then we use an extended constant set
    // as SWVP has many more slots available than HWVP.
    bool canSWVP = CanSWVP();
    DetermineConstantLayouts(canSWVP);

    if (canSWVP)
      Logger::info("D3D9DeviceEx: Using extended constant set for software vertex processing.");

    if (m_dxvkDevice->debugFlags().test(DxvkDebugFlag::Markers))
      m_annotation = new D3D9UserDefinedAnnotation(this);

    m_initializer      = new D3D9Initializer(this);
    m_converter        = new D3D9FormatHelper(m_dxvkDevice);

    EmitCs([
      cDevice = m_dxvkDevice
    ] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());

      // Disable logic op once and for all.
      DxvkLogicOpState loState = { };
      ctx->setLogicOpState(loState);
    });

    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);

    if (!(BehaviorFlags & D3DCREATE_FPU_PRESERVE))
      SetupFPU();

    m_dxsoOptions = DxsoOptions(this, m_d3d9Options);

    // Check if VK_EXT_robustness2 is supported, so we can optimize the number of constants we need to copy.
    // Also check the required alignments.
    const bool supportsRobustness2 = m_dxvkDevice->features().extRobustness2.robustBufferAccess2;
    bool useRobustConstantAccess = supportsRobustness2;
    D3D9ConstantSets& vsConstSet = m_consts[DxsoProgramType::VertexShader];
    D3D9ConstantSets& psConstSet = m_consts[DxsoProgramType::PixelShader];
    if (useRobustConstantAccess) {
      m_robustSSBOAlignment = m_dxvkDevice->properties().extRobustness2.robustStorageBufferAccessSizeAlignment;
      m_robustUBOAlignment  = m_dxvkDevice->properties().extRobustness2.robustUniformBufferAccessSizeAlignment;
      if (canSWVP) {
        const uint32_t floatBufferAlignment = m_dxsoOptions.vertexFloatConstantBufferAsSSBO ? m_robustSSBOAlignment : m_robustUBOAlignment;

        useRobustConstantAccess &= vsConstSet.layout.floatSize() % floatBufferAlignment == 0;
        useRobustConstantAccess &= vsConstSet.layout.intSize() % m_robustUBOAlignment == 0;
        useRobustConstantAccess &= vsConstSet.layout.bitmaskSize() % m_robustUBOAlignment == 0;
      } else {
        useRobustConstantAccess &= vsConstSet.layout.totalSize() % m_robustUBOAlignment == 0;
      }
      useRobustConstantAccess &= psConstSet.layout.totalSize() % m_robustUBOAlignment == 0;
    }

    if (!useRobustConstantAccess) {
      // Disable optimized constant copies, we always have to copy all constants.
      vsConstSet.maxChangedConstF = vsConstSet.layout.floatCount;
      vsConstSet.maxChangedConstI = vsConstSet.layout.intCount;
      vsConstSet.maxChangedConstB = vsConstSet.layout.boolCount;
      psConstSet.maxChangedConstF = psConstSet.layout.floatCount;

      if (supportsRobustness2) {
        Logger::warn("Disabling robust constant buffer access because of alignment.");
      }
    }

    // Check for VK_EXT_graphics_pipeline_libraries
    m_usingGraphicsPipelines = dxvkDevice->features().extGraphicsPipelineLibrary.graphicsPipelineLibrary;

    // Check for VK_EXT_depth_bias_control and set up initial state
    m_depthBiasRepresentation = { VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, false };
    if (dxvkDevice->features().extDepthBiasControl.depthBiasControl) {
      if (dxvkDevice->features().extDepthBiasControl.depthBiasExact)
        m_depthBiasRepresentation.depthBiasExact = true;

      if (dxvkDevice->features().extDepthBiasControl.floatRepresentation) {
        m_depthBiasRepresentation.depthBiasRepresentation = VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT;
        m_depthBiasScale = 1.0f;
      }
      else if (dxvkDevice->features().extDepthBiasControl.leastRepresentableValueForceUnormRepresentation)
        m_depthBiasRepresentation.depthBiasRepresentation = VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT;
    }

    EmitCs([
      cRepresentation = m_depthBiasRepresentation
    ] (DxvkContext* ctx) {
      ctx->setDepthBiasRepresentation(cRepresentation);
    });

    CreateConstantBuffers();

    m_availableMemory = DetermineInitialTextureMemory();

    m_hazardLayout = dxvkDevice->features().extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout
      ? VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
      : VK_IMAGE_LAYOUT_GENERAL;

    // Initially set all the dirty flags so we
    // always end up giving the backend *something* to work with.
    m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
    m_flags.set(D3D9DeviceFlag::DirtyClipPlanes);
    m_flags.set(D3D9DeviceFlag::DirtyDepthStencilState);
    m_flags.set(D3D9DeviceFlag::DirtyBlendState);
    m_flags.set(D3D9DeviceFlag::DirtyRasterizerState);
    m_flags.set(D3D9DeviceFlag::DirtyDepthBias);
    m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);
    m_flags.set(D3D9DeviceFlag::DirtyInputLayout);
    m_flags.set(D3D9DeviceFlag::DirtyViewportScissor);
    m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);

    m_flags.set(D3D9DeviceFlag::DirtyFogState);
    m_flags.set(D3D9DeviceFlag::DirtyFogColor);
    m_flags.set(D3D9DeviceFlag::DirtyFogDensity);
    m_flags.set(D3D9DeviceFlag::DirtyFogScale);
    m_flags.set(D3D9DeviceFlag::DirtyFogEnd);

    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);
    m_flags.set(D3D9DeviceFlag::DirtyFFVertexShader);
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
    m_flags.set(D3D9DeviceFlag::DirtyFFViewport);
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelData);
    m_flags.set(D3D9DeviceFlag::DirtyProgVertexShader);
    m_flags.set(D3D9DeviceFlag::DirtySharedPixelShaderData);
    m_flags.set(D3D9DeviceFlag::DirtyDepthBounds);
    m_flags.set(D3D9DeviceFlag::DirtyPointScale);

    m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);

    // Bitfields can't be initialized in header.
    m_activeRTsWhichAreTextures = 0;
    m_alphaSwizzleRTs = 0;
    m_lastHazardsRT = 0;
  }


  D3D9DeviceEx::~D3D9DeviceEx() {
    // Avoids hanging when in this state, see comment
    // in DxvkDevice::~DxvkDevice.
    if (this_thread::isInModuleDetachment())
      return;

    Flush();
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);

    if (m_annotation)
      delete m_annotation;

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

    if (riid == __uuidof(IDxvkD3D8Bridge)) {
      *ppvObject = ref(&m_d3d8Bridge);
      return S_OK;
    }

    if (riid == __uuidof(ID3D9VkInteropDevice)) {
      *ppvObject = ref(&m_d3d9Interop);
      return S_OK;
    }

    if (riid == __uuidof(IDirect3DDevice9On12)) {
      *ppvObject = ref(&m_d3d9On12);
      return S_OK;
    }

    // We want to ignore this if the extended device is queried and we weren't made extended.
    if (riid == __uuidof(IDirect3DDevice9Ex))
      return E_NOINTERFACE;

    if (logQueryInterfaceError(__uuidof(IDirect3DDevice9), riid)) {
      Logger::warn("D3D9DeviceEx::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::TestCooperativeLevel() {
    D3D9DeviceLock lock = LockDevice();

    // Equivelant of D3D11/DXGI present tests. We can always present.
    if (likely(m_deviceLostState == D3D9DeviceLostState::Ok)) {
      return D3D_OK;
    } else if (m_deviceLostState == D3D9DeviceLostState::NotReset) {
      return D3DERR_DEVICENOTRESET;
    } else {
      return D3DERR_DEVICELOST;
    }
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
    if (pCaps == nullptr)
      return D3DERR_INVALIDCALL;

    m_adapter->GetDeviceCaps(m_deviceType, pCaps);

    // When in SWVP mode, 256 matrices can be used for indexed vertex blending
    pCaps->MaxVertexBlendMatrixIndex = m_isSWVP ? 255 : 8;

    return D3D_OK;
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

    // Check if surface dimensions are powers of two.
    if ((inputWidth  && (inputWidth  & (inputWidth  - 1)))
     || (inputHeight && (inputHeight & (inputHeight - 1))))
      return D3DERR_INVALIDCALL;

    // It makes no sense to have a hotspot outside of the bitmap.
    if ((inputWidth  && (XHotSpot > inputWidth  - 1))
     || (inputHeight && (YHotSpot > inputHeight - 1)))
      return D3DERR_INVALIDCALL;

    D3DPRESENT_PARAMETERS params;
    m_implicitSwapchain->GetPresentParameters(&params);

    if (inputWidth  > params.BackBufferWidth
     || inputHeight > params.BackBufferHeight)
      return D3DERR_INVALIDCALL;

    // Always use a hardware cursor when windowed.
    bool hwCursor  = params.Windowed;

    // Always use a hardware cursor w/h <= 32 px
    hwCursor |= inputWidth  <= HardwareCursorWidth
             || inputHeight <= HardwareCursorHeight;

    D3DLOCKED_BOX lockedBox;
    HRESULT hr = LockImage(cursorTex, 0, 0, &lockedBox, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
      return hr;

    const uint8_t* data  = reinterpret_cast<const uint8_t*>(lockedBox.pBits);

    if (hwCursor) {
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
    } else {
      size_t copyPitch = inputWidth * HardwareCursorFormatSize;
      std::vector<uint8_t> bitmap(inputHeight * copyPitch, 0);

      for (uint32_t h = 0; h < inputHeight; h++)
        std::memcpy(&bitmap[h * copyPitch], &data[h * lockedBox.RowPitch], copyPitch);

      UnlockImage(cursorTex, 0, 0);

      m_implicitSwapchain->SetCursorTexture(inputWidth, inputHeight, &bitmap[0]);

      return m_cursor.SetSoftwareCursor(inputWidth, inputHeight, XHotSpot, YHotSpot);
    }

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

    Logger::info("Device reset");
    m_deviceLostState = D3D9DeviceLostState::Ok;

    HRESULT hr;
    // Black Desert creates a D3DDEVTYPE_NULLREF device and
    // expects reset to work despite passing invalid parameters.
    if (likely(m_deviceType != D3DDEVTYPE_NULLREF)) {
      hr = m_parent->ValidatePresentationParameters(pPresentationParameters);

      if (unlikely(FAILED(hr)))
        return hr;
    }

    if (!IsExtended()) {
      // The internal references are always cleared, regardless of whether the Reset call succeeds.
      ResetState(pPresentationParameters);
      m_implicitSwapchain->DestroyBackBuffers();
      m_autoDepthStencil = nullptr;

      // Unbind all buffers that were still bound to the backend to avoid leaks.
      EmitCs([](DxvkContext* ctx) {
        ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);
        for (uint32_t i = 0; i < caps::MaxStreams; i++) {
          ctx->bindVertexBuffer(i, DxvkBufferSlice(), 0);
        }
      });

      // Tests show that regular D3D9 ends the scene in Reset
      // while D3D9Ex doesn't.
      // Observed in Empires: Dawn of the Modern World (D3D8)
      // and the OSU compatibility mode (D3D9Ex).
      m_flags.clr(D3D9DeviceFlag::InScene);
    } else {
      // Extended devices only reset the bound render targets
      for (uint32_t i = 0; i < caps::MaxSimultaneousRenderTargets; i++) {
        SetRenderTargetInternal(i, nullptr);
      }
      SetDepthStencilSurface(nullptr);
    }

    m_cursor.ResetCursor();

    /*
      * Before calling the IDirect3DDevice9::Reset method for a device,
      * an application should release any explicit render targets,
      * depth stencil surfaces, additional swap chains, state blocks,
      * and D3DPOOL_DEFAULT resources associated with the device.
      *
      * We have to check after ResetState clears the references held by SetTexture, etc.
      * This matches what Windows D3D9 does.
    */
    if (unlikely(m_losableResourceCounter.load() != 0 && !IsExtended() && m_d3d9Options.countLosableResources)) {
      Logger::warn(str::format("Device reset failed because device still has alive losable resources: Device not reset. Remaining resources: ", m_losableResourceCounter.load()));
      m_deviceLostState = D3D9DeviceLostState::NotReset;
      // D3D8 returns D3DERR_DEVICELOST here, whereas D3D9 returns D3DERR_INVALIDCALL.
      return m_isD3D8Compatible ? D3DERR_DEVICELOST : D3DERR_INVALIDCALL;
    }

    hr = ResetSwapChain(pPresentationParameters, nullptr);
    if (unlikely(FAILED(hr))) {
      if (!IsExtended()) {
        Logger::warn("Device reset failed: Device not reset");
        m_deviceLostState = D3D9DeviceLostState::NotReset;
      }
      return hr;
    }

    Flush();
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);

    if (m_d3d9Options.deferSurfaceCreation)
      m_resetCtr++;

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
    // Docs:
    // Textures placed in the D3DPOOL_DEFAULT pool cannot be locked
    // unless they are dynamic textures or they are private, FOURCC, driver formats.
    desc.IsLockable         = Pool != D3DPOOL_DEFAULT
                            || (Usage & D3DUSAGE_DYNAMIC)
                            || IsVendorFormat(EnumerateFormat(Format));

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_TEXTURE, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      void* initialData = nullptr;

      // On Windows Vista (so most likely D3D9Ex), pSharedHandle can be used to pass initial data for a texture,
      // but only for a very specific type of texture.
      if (Pool == D3DPOOL_SYSTEMMEM && Levels == 1 && pSharedHandle != nullptr) {
        initialData = *(reinterpret_cast<void**>(pSharedHandle));
        pSharedHandle = nullptr;
      }

      // Shared textures have to be in POOL_DEFAULT
      if (pSharedHandle != nullptr && Pool != D3DPOOL_DEFAULT)
        return D3DERR_INVALIDCALL;

      const Com<D3D9Texture2D> texture = new D3D9Texture2D(this, &desc, IsExtended(), pSharedHandle);

      m_initializer->InitTexture(texture->GetCommonTexture(), initialData);
      *ppTexture = texture.ref();

      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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

    if (pSharedHandle)
        Logger::err("CreateVolumeTexture: Shared volume textures not supported");

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
    // Docs:
    // Textures placed in the D3DPOOL_DEFAULT pool cannot be locked
    // unless they are dynamic textures. Volume textures do not
    // exempt private, FOURCC, driver formats from these checks.
    desc.IsLockable         = Pool != D3DPOOL_DEFAULT
                            || (Usage & D3DUSAGE_DYNAMIC);

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_VOLUMETEXTURE, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Texture3D> texture = new D3D9Texture3D(this, &desc, IsExtended());
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppVolumeTexture = texture.ref();

      // The device cannot be reset if there's any remaining default resources
      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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

    if (pSharedHandle)
        Logger::err("CreateCubeTexture: Shared cube textures not supported");

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
    // Docs:
    // Textures placed in the D3DPOOL_DEFAULT pool cannot be locked
    // unless they are dynamic textures or they are private, FOURCC, driver formats.
    desc.IsLockable         = Pool != D3DPOOL_DEFAULT
                            || (Usage & D3DUSAGE_DYNAMIC)
                            || IsVendorFormat(EnumerateFormat(Format));

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_CUBETEXTURE, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9TextureCube> texture = new D3D9TextureCube(this, &desc, IsExtended());
      m_initializer->InitTexture(texture->GetCommonTexture());
      *ppCubeTexture = texture.ref();

      // The device cannot be reset if there's any remaining default resources
      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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

    if (pSharedHandle)
        Logger::err("CreateVertexBuffer: Shared vertex buffers not supported");

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
      const Com<D3D9VertexBuffer> buffer = new D3D9VertexBuffer(this, &desc, IsExtended());
      m_initializer->InitBuffer(buffer->GetCommonBuffer());
      *ppVertexBuffer = buffer.ref();

      // The device cannot be reset if there's any remaining default resources
      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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

    if (pSharedHandle)
        Logger::err("CreateIndexBuffer: Shared index buffers not supported");

    D3D9_BUFFER_DESC desc;
    desc.Format = EnumerateFormat(Format);
    desc.Pool   = Pool;
    desc.Size   = Length;
    desc.Type   = D3DRTYPE_INDEXBUFFER;
    desc.Usage  = Usage;

    if (FAILED(D3D9CommonBuffer::ValidateBufferProperties(&desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9IndexBuffer> buffer = new D3D9IndexBuffer(this, &desc, IsExtended());
      m_initializer->InitBuffer(buffer->GetCommonBuffer());
      *ppIndexBuffer = buffer.ref();

      // The device cannot be reset if there's any remaining default resources
      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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

    if (unlikely(srcTextureInfo->Desc()->MultiSample != D3DMULTISAMPLE_NONE))
      return D3DERR_INVALIDCALL;

    if (unlikely(dstTextureInfo->Desc()->MultiSample != D3DMULTISAMPLE_NONE))
      return D3DERR_INVALIDCALL;

    const DxvkFormatInfo* formatInfo = lookupFormatInfo(dstTextureInfo->GetFormatMapping().FormatColor);

    VkOffset3D srcOffset = { 0u, 0u, 0u };
    VkOffset3D dstOffset = { 0u, 0u, 0u };
    VkExtent3D texLevelExtent = srcTextureInfo->GetExtentMip(src->GetSubresource());
    VkExtent3D extent = texLevelExtent;

    if (pSourceRect != nullptr) {
      srcOffset = { pSourceRect->left,
                    pSourceRect->top,
                    0u };

      extent = { uint32_t(pSourceRect->right - pSourceRect->left), uint32_t(pSourceRect->bottom - pSourceRect->top), 1 };

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

    // The source surface must be in D3DPOOL_SYSTEMMEM so we just treat it as just another texture upload except with a different source.
    UpdateTextureFromBuffer(dstTextureInfo, srcTextureInfo, dst->GetSubresource(), src->GetSubresource(), srcOffset, extent, dstOffset);

    // The contents of the mapping no longer match the image.
    dstTextureInfo->SetNeedsReadback(dst->GetSubresource(), true);

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

    if (unlikely(srcTexInfo->Desc()->MipLevels < dstTexInfo->Desc()->MipLevels && !dstTexInfo->IsAutomaticMip()))
      return D3DERR_INVALIDCALL;

    if (unlikely(dstTexInfo->Desc()->Format != srcTexInfo->Desc()->Format))
      return D3DERR_INVALIDCALL;

    if (unlikely(srcTexInfo->IsAutomaticMip() && !dstTexInfo->IsAutomaticMip()))
      return D3DERR_INVALIDCALL;

    const Rc<DxvkImage> dstImage  = dstTexInfo->GetImage();
    uint32_t mipLevels = dstTexInfo->IsAutomaticMip() ? 1 : dstTexInfo->Desc()->MipLevels;
    uint32_t arraySlices = std::min(srcTexInfo->Desc()->ArraySize, dstTexInfo->Desc()->ArraySize);

    uint32_t srcMipOffset = 0;
    VkExtent3D srcFirstMipExtent = srcTexInfo->GetExtent();
    VkExtent3D dstFirstMipExtent = dstTexInfo->GetExtent();

    if (srcFirstMipExtent != dstFirstMipExtent) {
      // UpdateTexture can be used with textures that have different mip lengths.
      // It will either match the the top mips or the bottom ones.
      // If the largest mip maps don't match in size, we try to take the smallest ones
      // of the source.

      srcMipOffset = srcTexInfo->Desc()->MipLevels - mipLevels;
      srcFirstMipExtent = util::computeMipLevelExtent(srcTexInfo->GetExtent(), srcMipOffset);
      dstFirstMipExtent = dstTexInfo->GetExtent();
    }

    if (srcFirstMipExtent != dstFirstMipExtent)
      return D3DERR_INVALIDCALL;

    for (uint32_t a = 0; a < arraySlices; a++) {
      // The docs claim that the dirty box is just a performance optimization, however in practice games rely on it.
      const D3DBOX& box = srcTexInfo->GetDirtyBox(a);
      if (box.Left >= box.Right || box.Top >= box.Bottom || box.Front >= box.Back)
        continue;

      // The dirty box is only tracked for mip level 0
      VkExtent3D mip0Extent = {
        uint32_t(box.Right - box.Left),
        uint32_t(box.Bottom - box.Top),
        uint32_t(box.Back - box.Front)
      };
      VkOffset3D mip0Offset = { int32_t(box.Left), int32_t(box.Top), int32_t(box.Front) };

      for (uint32_t dstMip = 0; dstMip < mipLevels; dstMip++) {
        // Scale the dirty box for the respective mip level
        uint32_t srcMip = dstMip + srcMipOffset;
        uint32_t srcSubresource = srcTexInfo->CalcSubresource(a, srcMip);
        uint32_t dstSubresource = dstTexInfo->CalcSubresource(a, dstMip);
        VkExtent3D extent = util::computeMipLevelExtent(mip0Extent, srcMip);
        VkOffset3D offset = util::computeMipLevelOffset(mip0Offset, srcMip);

        // The source surface must be in D3DPOOL_SYSTEMMEM so we just treat it as just another texture upload except with a different source.
        UpdateTextureFromBuffer(dstTexInfo, srcTexInfo, dstSubresource, srcSubresource, offset, extent, offset);

        // The contents of the mapping no longer match the image.
        dstTexInfo->SetNeedsReadback(dstSubresource, true);
      }
    }

    srcTexInfo->ClearDirtyBoxes();
    if (dstTexInfo->IsAutomaticMip() && mipLevels != dstTexInfo->Desc()->MipLevels)
      MarkTextureMipsDirty(dstTexInfo);

    ConsiderFlush(GpuFlushType::ImplicitWeakHint);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetRenderTargetData(
          IDirect3DSurface9* pRenderTarget,
          IDirect3DSurface9* pDestSurface) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(IsDeviceLost())) {
      return D3DERR_DEVICELOST;
    }

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

    if (src->GetSurfaceExtent() != dst->GetSurfaceExtent())
      return D3DERR_INVALIDCALL;

    if (dstTexInfo->Desc()->Pool == D3DPOOL_DEFAULT)
      return this->StretchRect(pRenderTarget, nullptr, pDestSurface, nullptr, D3DTEXF_NONE);

    VkExtent3D dstTexExtent = dstTexInfo->GetExtentMip(dst->GetMipLevel());
    VkExtent3D srcTexExtent = srcTexInfo->GetExtentMip(src->GetMipLevel());

    const bool clearDst = dstTexInfo->Desc()->MipLevels > 1
                       || dstTexExtent.width > srcTexExtent.width
                       || dstTexExtent.height > srcTexExtent.height;

    dstTexInfo->CreateBuffer(clearDst);
    DxvkBufferSlice dstBufferSlice      = dstTexInfo->GetBufferSlice(dst->GetSubresource());
    Rc<DxvkImage> srcImage              = srcTexInfo->GetImage();
    const DxvkFormatInfo* srcFormatInfo = lookupFormatInfo(srcImage->info().format);

    const VkImageSubresource srcSubresource = srcTexInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());
    VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    EmitCs([
      cBufferSlice  = std::move(dstBufferSlice),
      cImage        = srcImage,
      cSubresources = srcSubresourceLayers,
      cLevelExtent  = srcTexExtent
    ] (DxvkContext* ctx) {
      ctx->copyImageToBuffer(cBufferSlice.buffer(), cBufferSlice.offset(),
        4, 0, VK_FORMAT_UNDEFINED, cImage, cSubresources,
        VkOffset3D { 0, 0, 0 }, cLevelExtent);
    });

    dstTexInfo->SetNeedsReadback(dst->GetSubresource(), true);
    TrackTextureMappingBufferSequenceNumber(dstTexInfo, dst->GetSubresource());

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    if (unlikely(iSwapChain != 0))
      return D3DERR_INVALIDCALL;

    D3D9DeviceLock lock = LockDevice();

    // In windowed mode, GetFrontBufferData takes a screenshot of the entire screen.
    // We use the last used swapchain as a workaround.
    // Total War: Medieval 2 relies on this.
    return m_mostRecentlyUsedSwapchain->GetFrontBufferData(pDestSurface);
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

    if (dstImage == nullptr || srcImage == nullptr)
        return D3DERR_INVALIDCALL;

    const DxvkFormatInfo* dstFormatInfo = lookupFormatInfo(dstImage->info().format);
    const DxvkFormatInfo* srcFormatInfo = lookupFormatInfo(srcImage->info().format);

    const VkImageSubresource dstSubresource = dstTextureInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, dst->GetSubresource());
    const VkImageSubresource srcSubresource = srcTextureInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, src->GetSubresource());

    if (unlikely(Filter != D3DTEXF_NONE && Filter != D3DTEXF_LINEAR && Filter != D3DTEXF_POINT))
      return D3DERR_INVALIDCALL;

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
    auto needsResolve = false;
    if (srcImage->info().sampleCount != dstImage->info().sampleCount) {
      needsResolve = srcImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;
      auto fbBlit = dstImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;
      fastPath &= !fbBlit;
    }

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

    bool srcIsDS = IsDepthStencilFormat(srcFormat);
    bool dstIsDS = IsDepthStencilFormat(dstFormat);
    if (unlikely(srcIsDS || dstIsDS)) {
      if (unlikely(!srcIsDS || !dstIsDS))
        return D3DERR_INVALIDCALL;

      if (unlikely(srcTextureInfo->Desc()->Discard || dstTextureInfo->Desc()->Discard))
        return D3DERR_INVALIDCALL;

      if (unlikely(srcCopyExtent.width != srcExtent.width || srcCopyExtent.height != srcExtent.height))
        return D3DERR_INVALIDCALL;

      if (unlikely(m_flags.test(D3D9DeviceFlag::InScene)))
        return D3DERR_INVALIDCALL;
    }

    // Copies would only work if the extents match. (ie. no stretching)
    bool stretch = srcCopyExtent != dstCopyExtent;

    bool dstHasRTUsage = (dstTextureInfo->Desc()->Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0;
    bool dstIsSurface = dstTextureInfo->GetType() == D3DRTYPE_SURFACE;
    if (stretch) {
      if (unlikely(pSourceSurface == pDestSurface))
        return D3DERR_INVALIDCALL;

      if (unlikely(dstIsDS))
        return D3DERR_INVALIDCALL;

      // The docs say that stretching is only allowed if the destination is either a render target surface or a render target texture.
      // However in practice, using an offscreen plain surface in D3DPOOL_DEFAULT as the destination works fine.
      // Using a texture without USAGE_RENDERTARGET as destination however does not.
      if (unlikely(!dstIsSurface && !dstHasRTUsage))
        return D3DERR_INVALIDCALL;
    } else {
      bool srcIsSurface = srcTextureInfo->GetType() == D3DRTYPE_SURFACE;
      bool srcHasRTUsage = (srcTextureInfo->Desc()->Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0;
      // Non-stretching copies are only allowed if:
      // - the destination is either a render target surface or a render target texture
      // - both destination and source are depth stencil surfaces
      // - both destination and source are offscreen plain surfaces.
      // The only way to get a surface with resource type D3DRTYPE_SURFACE without USAGE_RT or USAGE_DS is CreateOffscreenPlainSurface.
      if (unlikely((!dstHasRTUsage && (!dstIsSurface || !srcIsSurface || srcHasRTUsage)) && !m_isD3D8Compatible))
        return D3DERR_INVALIDCALL;
    }

    fastPath &= !stretch;

    if (!fastPath || needsResolve) {
      // Compressed destination formats are forbidden for blits.
      if (dstFormatInfo->flags.test(DxvkFormatFlag::BlockCompressed))
        return D3DERR_INVALIDCALL;
    }

    auto EmitResolveCS = [&](const Rc<DxvkImage>& resolveDst, bool intermediate) {
      VkImageResolve region;
      region.srcSubresource = blitInfo.srcSubresource;
      region.srcOffset      = intermediate ? VkOffset3D { 0, 0, 0 }  : blitInfo.srcOffsets[0];
      region.dstSubresource = intermediate ? blitInfo.srcSubresource : blitInfo.dstSubresource;
      region.dstOffset      = intermediate ? VkOffset3D { 0, 0, 0 }  : blitInfo.dstOffsets[0];
      region.extent         = intermediate ? resolveDst->mipLevelExtent(blitInfo.srcSubresource.mipLevel) : srcCopyExtent;

      EmitCs([
        cDstImage    = resolveDst,
        cSrcImage    = srcImage,
        cRegion      = region
      ] (DxvkContext* ctx) {
        // Deliberately use AVERAGE even for depth resolves here
        ctx->resolveImage(cDstImage, cSrcImage, cRegion, cSrcImage->info().format,
          VK_RESOLVE_MODE_AVERAGE_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
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

      DxvkImageViewKey dstViewInfo;
      dstViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      dstViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      dstViewInfo.format = dstImage->info().format;
      dstViewInfo.aspects = blitInfo.dstSubresource.aspectMask;
      dstViewInfo.mipIndex = blitInfo.dstSubresource.mipLevel;
      dstViewInfo.mipCount = 1;
      dstViewInfo.layerIndex = blitInfo.dstSubresource.baseArrayLayer;
      dstViewInfo.layerCount = blitInfo.dstSubresource.layerCount;
      dstViewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(dstTextureInfo->GetMapping().Swizzle);

      DxvkImageViewKey srcViewInfo;
      srcViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      srcViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      srcViewInfo.format = srcImage->info().format;
      srcViewInfo.aspects = blitInfo.srcSubresource.aspectMask;
      srcViewInfo.mipIndex = blitInfo.srcSubresource.mipLevel;
      srcViewInfo.mipCount = 1;
      srcViewInfo.layerIndex = blitInfo.srcSubresource.baseArrayLayer;
      srcViewInfo.layerCount = blitInfo.srcSubresource.layerCount;
      srcViewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(srcTextureInfo->GetMapping().Swizzle);

      EmitCs([
        cDstView  = dstImage->createView(dstViewInfo),
        cSrcView  = srcImage->createView(srcViewInfo),
        cBlitInfo = blitInfo,
        cFilter   = stretch ? DecodeFilter(Filter) : VK_FILTER_NEAREST
      ] (DxvkContext* ctx) {
        ctx->blitImageView(
          cDstView, cBlitInfo.dstOffsets,
          cSrcView, cBlitInfo.srcOffsets,
          cFilter);
      });
    }

    dstTextureInfo->SetNeedsReadback(dst->GetSubresource(), true);

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

    if (dstTextureInfo->IsNull())
      return D3D_OK;

    if (unlikely(dstTextureInfo->Desc()->Pool != D3DPOOL_DEFAULT))
      return D3DERR_INVALIDCALL;

    VkExtent3D mipExtent = dstTextureInfo->GetExtentMip(dst->GetSubresource());

    VkOffset3D offset = VkOffset3D{ 0u, 0u, 0u };
    VkExtent3D extent = mipExtent;

    if (pRect != nullptr)
      ConvertRect(*pRect, offset, extent);

    VkClearValue clearValue = { };
    DecodeD3DCOLOR(Color, clearValue.color.float32);

    Rc<DxvkImage> image = dstTextureInfo->GetImage();

    if (image->formatInfo()->aspectMask != VK_IMAGE_ASPECT_COLOR_BIT)
      return D3DERR_INVALIDCALL;

    VkImageSubresourceLayers subresource = { };
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = dst->GetMipLevel();

    if (dst->GetFace() == D3D9CommonTexture::AllLayers) {
      subresource.baseArrayLayer = 0u;
      subresource.layerCount = image->info().numLayers;
    } else {
      subresource.baseArrayLayer = dst->GetFace();
      subresource.layerCount = 1u;
    }

    if (image->formatInfo()->flags.test(DxvkFormatFlag::BlockCompressed)) {
      EmitCs([
        cImage      = std::move(image),
        cSubresource = subresource,
        cOffset     = offset,
        cExtent     = extent,
        cClearValue = clearValue
      ] (DxvkContext* ctx) {
        auto formatInfo = cImage->formatInfo();

        VkFormat blockFormat = formatInfo->elementSize == 16u
          ? VK_FORMAT_R32G32B32A32_UINT
          : VK_FORMAT_R32G32_UINT;

        DxvkImageUsageInfo usage = { };
        usage.usage = VK_IMAGE_USAGE_STORAGE_BIT;
        usage.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
                    | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT
                    | VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
        usage.viewFormatCount = 1;
        usage.viewFormats = &blockFormat;
        usage.layout = VK_IMAGE_LAYOUT_GENERAL;

        ctx->ensureImageCompatibility(cImage, usage);

        DxvkImageViewKey viewKey = { };
        viewKey.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewKey.format = blockFormat;
        viewKey.usage = VK_IMAGE_USAGE_STORAGE_BIT;
        viewKey.aspects = cSubresource.aspectMask;
        viewKey.mipIndex = cSubresource.mipLevel;
        viewKey.mipCount = 1u;
        viewKey.layerIndex = cSubresource.baseArrayLayer;
        viewKey.layerCount = cSubresource.layerCount;

        Rc<DxvkImageView> view = cImage->createView(viewKey);

        VkClearValue clearBlock = { };
        clearBlock.color = util::encodeClearBlockValue(cImage->info().format, cClearValue.color);

        VkOffset3D offset = util::computeBlockOffset(cOffset, formatInfo->blockSize);
        VkExtent3D extent = util::computeBlockExtent(cExtent, formatInfo->blockSize);

        ctx->clearImageView(view, offset, extent,
          VK_IMAGE_ASPECT_COLOR_BIT, clearBlock);
      });
    } else {
      EmitCs([
        cImage      = std::move(image),
        cSubresource = subresource,
        cOffset     = offset,
        cExtent     = extent,
        cClearValue = clearValue
      ] (DxvkContext* ctx) {
        DxvkImageUsageInfo usage = { };
        usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        ctx->ensureImageCompatibility(cImage, usage);

        DxvkImageViewKey viewKey = { };
        viewKey.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewKey.format = cImage->info().format;
        viewKey.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        viewKey.aspects = cSubresource.aspectMask;
        viewKey.mipIndex = cSubresource.mipLevel;
        viewKey.mipCount = 1u;
        viewKey.layerIndex = cSubresource.baseArrayLayer;
        viewKey.layerCount = cSubresource.layerCount;

        Rc<DxvkImageView> view = cImage->createView(viewKey);

        if (cOffset == VkOffset3D() && cExtent == cImage->mipLevelExtent(viewKey.mipIndex)) {
          ctx->clearRenderTarget(view, cSubresource.aspectMask, cClearValue, 0u);
        } else {
          ctx->clearImageView(view, cOffset, cExtent,
            cSubresource.aspectMask, cClearValue);
        }
      });
    }

    dstTextureInfo->SetNeedsReadback(dst->GetSubresource(), true);

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

    if (unlikely(pRenderTarget == nullptr && RenderTargetIndex == 0))
      return D3DERR_INVALIDCALL;

    // We need to make sure the render target was created using this device.
    D3D9Surface* rt = static_cast<D3D9Surface*>(pRenderTarget);
    if (unlikely(rt != nullptr && rt->GetDevice() != this))
      return D3DERR_INVALIDCALL;

    return SetRenderTargetInternal(RenderTargetIndex, pRenderTarget);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetRenderTargetInternal(
          DWORD              RenderTargetIndex,
          IDirect3DSurface9* pRenderTarget) {
    if (unlikely(RenderTargetIndex >= caps::MaxSimultaneousRenderTargets))
      return D3DERR_INVALIDCALL;

    D3D9Surface* rt = static_cast<D3D9Surface*>(pRenderTarget);
    D3D9CommonTexture* texInfo = rt != nullptr
      ? rt->GetCommonTexture()
      : nullptr;

    if (unlikely(rt != nullptr && !(texInfo->Desc()->Usage & D3DUSAGE_RENDERTARGET)))
      return D3DERR_INVALIDCALL;

    if (RenderTargetIndex == 0) {
      D3DVIEWPORT9 viewport;
      viewport.X       = 0;
      viewport.Y       = 0;
      viewport.MinZ    = 0.0f;
      viewport.MaxZ    = 1.0f;

      RECT scissorRect;
      scissorRect.left    = 0;
      scissorRect.top     = 0;

      if (likely(rt != nullptr)) {
        auto rtSize = rt->GetSurfaceExtent();
        viewport.Width  = rtSize.width;
        viewport.Height = rtSize.height;
        scissorRect.right  = rtSize.width;
        scissorRect.bottom = rtSize.height;
      } else {
        viewport.Width  = 0;
        viewport.Height = 0;
        scissorRect.right  = 0;
        scissorRect.bottom = 0;
      }

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
    ConsiderFlush(RenderTargetIndex == 0
      ? GpuFlushType::ImplicitStrongHint
      : GpuFlushType::ImplicitWeakHint);
    m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

    m_state.renderTargets[RenderTargetIndex] = rt;

    // Update feedback loop tracking bitmasks
    UpdateActiveRTs(RenderTargetIndex);

    // Update render target alpha swizzle bitmask if we need to fix up the alpha channel
    // for XRGB formats
    uint32_t originalAlphaSwizzleRTs = m_alphaSwizzleRTs;

    m_alphaSwizzleRTs &= ~(1 << RenderTargetIndex);

    if (rt != nullptr) {
      if (texInfo->GetMapping().Swizzle.a == VK_COMPONENT_SWIZZLE_ONE)
        m_alphaSwizzleRTs |= 1 << RenderTargetIndex;

      if (texInfo->IsAutomaticMip())
        texInfo->SetNeedsMipGen(true);
    }

    if (originalAlphaSwizzleRTs != m_alphaSwizzleRTs)
      m_flags.set(D3D9DeviceFlag::DirtyBlendState);

    if (RenderTargetIndex == 0) {
      if (likely(texInfo != nullptr)) {
        if (IsAlphaTestEnabled()) {
          // Need to recalculate the precision.
          m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);
        }

        bool validSampleMask = texInfo->Desc()->MultiSample > D3DMULTISAMPLE_NONMASKABLE;

        if (validSampleMask != m_flags.test(D3D9DeviceFlag::ValidSampleMask)) {
          m_flags.clr(D3D9DeviceFlag::ValidSampleMask);
          if (validSampleMask)
            m_flags.set(D3D9DeviceFlag::ValidSampleMask);

          m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);
        }
      } else {
        m_flags.clr(D3D9DeviceFlag::ValidSampleMask);
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

    ConsiderFlush(GpuFlushType::ImplicitWeakHint);
    m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

    // Update depth bias if necessary
    if (ds != nullptr && m_depthBiasRepresentation.depthBiasRepresentation != VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT) {
      const int32_t vendorId = m_dxvkDevice->adapter()->deviceProperties().vendorID;
      const bool exact = m_depthBiasRepresentation.depthBiasExact;
      const bool forceUnorm = m_depthBiasRepresentation.depthBiasRepresentation == VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT;
      const float rValue = GetDepthBufferRValue(ds->GetCommonTexture()->GetFormatMapping().FormatColor, vendorId, exact, forceUnorm);
      if (m_depthBiasScale != rValue) {
        m_depthBiasScale = rValue;
        m_flags.set(D3D9DeviceFlag::DirtyDepthBias);
      }
    }

    m_state.depthStencil = ds;

    UpdateActiveHazardsDS(std::numeric_limits<uint32_t>::max());

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
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_flags.test(D3D9DeviceFlag::InScene)))
      return D3DERR_INVALIDCALL;

    m_flags.set(D3D9DeviceFlag::InScene);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::EndScene() {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(!m_flags.test(D3D9DeviceFlag::InScene)))
      return D3DERR_INVALIDCALL;

    ConsiderFlush(GpuFlushType::ImplicitStrongHint);

    m_flags.clr(D3D9DeviceFlag::InScene);

    // D3D9 resets the internally bound vertex buffers and index buffer in EndScene if they were unbound in the meantime.
    // We have to ignore unbinding those buffers because of Operation Flashpoint Red River,
    // so we should also clear the bindings here, to avoid leaks.
    if (m_state.indices == nullptr) {
      EmitCs([](DxvkContext* ctx) {
        ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);
      });
    }

    for (uint32_t i : bit::BitMask(~m_activeVertexBuffers & ((1 << 16) - 1))) {
      if (m_state.vertexBuffers[i].vertexBuffer == nullptr) {
        EmitCs([cIndex = i](DxvkContext* ctx) {
          ctx->bindVertexBuffer(cIndex, DxvkBufferSlice(), 0);
        });
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::Clear(
          DWORD    Count,
    const D3DRECT* pRects,
          DWORD    Flags,
          D3DCOLOR Color,
          float    Z,
          DWORD    Stencil) {
    if (unlikely(!Count && pRects))
      return D3D_OK;

    D3D9DeviceLock lock = LockDevice();

    // D3DCLEAR_ZBUFFER and D3DCLEAR_STENCIL are invalid flags
    // if there is no currently bound DS (which can be the autoDS)
    if (unlikely(m_state.depthStencil == nullptr
            && (Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL))))
      return D3DERR_INVALIDCALL;

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

    VkImageAspectFlags depthAspectMask = 0;
    if (m_state.depthStencil != nullptr) {
      if (Flags & D3DCLEAR_ZBUFFER)
        depthAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

      if (Flags & D3DCLEAR_STENCIL)
        depthAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

      depthAspectMask &= lookupFormatInfo(m_state.depthStencil->GetCommonTexture()->GetFormatMapping().FormatColor)->aspectMask;
    }

    auto ClearImageView = [this](
      uint32_t                 alignment,
      VkOffset3D               offset,
      VkExtent3D               extent,
      const Rc<DxvkImageView>& imageView,
      VkImageAspectFlags       aspectMask,
      VkClearValue             clearValue) {

      VkExtent3D imageExtent = imageView->mipLevelExtent(0);
      extent.width = std::min(imageExtent.width, extent.width);
      extent.height = std::min(imageExtent.height, extent.height);

      if (unlikely(uint32_t(offset.x) >= imageExtent.width || uint32_t(offset.y) >= imageExtent.height))
        return;

      const bool fullClear = align(extent.width, alignment) == align(imageExtent.width, alignment)
        && align(extent.height, alignment) == align(imageExtent.height, alignment)
        && offset.x == 0
        && offset.y == 0;

      if (fullClear) {
        EmitCs([
          cClearValue = clearValue,
          cAspectMask = aspectMask,
          cImageView  = imageView
        ] (DxvkContext* ctx) {
          ctx->clearRenderTarget(cImageView,
            cAspectMask, cClearValue, 0u);
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
      uint32_t           alignment,
      VkOffset3D         offset,
      VkExtent3D         extent) {
      // Clear depth if we need to.
      if (depthAspectMask != 0)
        ClearImageView(alignment, offset, extent, m_state.depthStencil->GetDepthStencilView(), depthAspectMask, clearValueDepth);

      // Clear render targets if we need to.
      if (Flags & D3DCLEAR_TARGET) {
        for (uint32_t rt = 0u; rt < m_state.renderTargets.size(); rt++) {
          if (!HasRenderTargetBound(rt))
            continue;
          const auto& rts = m_state.renderTargets[rt];
          const auto& rtv = rts->GetRenderTargetView(srgb);

          if (likely(rtv != nullptr)) {
            ClearImageView(alignment, offset, extent, rtv, VK_IMAGE_ASPECT_COLOR_BIT, clearValueColor);

            D3D9CommonTexture* dstTexture = rts->GetCommonTexture();

            if (dstTexture->IsAutomaticMip())
              MarkTextureMipsDirty(dstTexture);
          }
        }
      }
    };

    // A Hat in Time and other UE3 games only gets partial clears here
    // because of an oversized rt height due to their weird alignment...
    // This works around that.
    uint32_t alignment = m_d3d9Options.lenientClear ? 8 : 1;

    if (extent.width == 0 || extent.height == 0) {
      return D3D_OK;
    }

    if (!Count) {
      // Clear our viewport & scissor minified region in this rendertarget.
      ClearViewRect(alignment, offset, extent);
    }
    else {
      // Clear the application provided rects.
      for (uint32_t i = 0; i < Count; i++) {
        VkOffset3D rectOffset = {
          std::max<int32_t>(pRects[i].x1, offset.x),
          std::max<int32_t>(pRects[i].y1, offset.y),
          0
        };

        if (std::min<int32_t>(pRects[i].x2, offset.x + extent.width) <= rectOffset.x
          || std::min<int32_t>(pRects[i].y2, offset.y + extent.height) <= rectOffset.y) {
          continue;
        }

        VkExtent3D rectExtent = {
          std::min<uint32_t>(pRects[i].x2, offset.x + extent.width)  - rectOffset.x,
          std::min<uint32_t>(pRects[i].y2, offset.y + extent.height) - rectOffset.y,
          1u
        };

        ClearViewRect(alignment, rectOffset, rectExtent);
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

    const uint32_t idx = GetTransformIndex(TransformState);

    m_state.transforms[idx] = m_state.transforms[idx] * ConvertMatrix(pMatrix);

    m_flags.set(D3D9DeviceFlag::DirtyFFVertexData);

    if (idx == GetTransformIndex(D3DTS_VIEW) || idx >= GetTransformIndex(D3DTS_WORLD))
      m_flags.set(D3D9DeviceFlag::DirtyFFVertexBlend);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetViewport(const D3DVIEWPORT9* pViewport) {
    D3D9DeviceLock lock = LockDevice();

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
      m_recorder->SetLight(Index, pLight);
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

    if (unlikely(ShouldRecord())) {
      m_recorder->LightEnable(Index, Enable);
      return D3D_OK;
    }

    if (unlikely(Index >= m_state.lights.size()))
      m_state.lights.resize(Index + 1);

    if (unlikely(!m_state.lights[Index]))
      m_state.lights[Index] = DefaultLight;

    if (m_state.IsLightEnabled(Index) == !!Enable)
      return D3D_OK;

    uint32_t searchIndex = std::numeric_limits<uint32_t>::max();
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

    if (unlikely(!pPlane))
      return D3DERR_INVALIDCALL;

    // Higher indexes will be capped to the last valid index
    if (unlikely(Index >= caps::MaxClipPlanes))
      Index = caps::MaxClipPlanes - 1;

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

    if (unlikely(!pPlane))
      return D3DERR_INVALIDCALL;

    // Higher indexes will be capped to the last valid index
    if (unlikely(Index >= caps::MaxClipPlanes))
      Index = caps::MaxClipPlanes - 1;

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
    DWORD old = states[State];

    bool changed = old != Value;

    if (likely(changed)) {
      const uint32_t vendorId            = m_adapter->GetVendorId();
      const bool     isNvidia            = vendorId == uint32_t(DxvkGpuVendor::Nvidia);
      const bool     isAmd               = vendorId == uint32_t(DxvkGpuVendor::Amd);
      const bool     isIntel             = vendorId == uint32_t(DxvkGpuVendor::Intel);

      const bool     oldClipPlaneEnabled = IsClipPlaneEnabled();
      const bool     oldDepthBiasEnabled = IsDepthBiasEnabled();
      const bool     oldATOC             = !m_isD3D8Compatible ? IsAlphaToCoverageEnabled() : false;
      const bool     oldNVDB             = !m_isD3D8Compatible ? states[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB) : false;
      const bool     oldAlphaTest        = IsAlphaTestEnabled();

      states[State] = Value;

      // AMD's driver hack for ATOC, RESZ, INST and CENT (also supported on Nvidia)
      if (unlikely(State == D3DRS_POINTSIZE &&
                   !m_isD3D8Compatible && !isIntel)) {
        // ATOC (AMD specific)
        constexpr uint32_t AlphaToCoverageEnable  = uint32_t(D3D9Format::A2M1);
        constexpr uint32_t AlphaToCoverageDisable = uint32_t(D3D9Format::A2M0);

        if ((Value == AlphaToCoverageEnable
          || Value == AlphaToCoverageDisable) && isAmd) {
          m_amdATOC = Value == AlphaToCoverageEnable;

          bool newATOC = IsAlphaToCoverageEnabled();
          bool newAlphaTest = IsAlphaTestEnabled();

          if (oldATOC != newATOC)
            m_flags.set(D3D9DeviceFlag::DirtyMultiSampleState);

          if (oldAlphaTest != newAlphaTest)
            m_flags.set(D3D9DeviceFlag::DirtyAlphaTestState);

          return D3D_OK;
        }

        // RESZ (AMD specific - once supported by Intel
        // as well, however modern drivers do not expose it)
        constexpr uint32_t RESZ = 0x7fa05000;
        if (Value == RESZ && isAmd) {
          ResolveZ();
          return D3D_OK;
        }

        // INST (AMD specific)
        if (unlikely(Value == uint32_t(D3D9Format::INST) && isAmd)) {
          // Geometry instancing is supported by SM3, but ATI/AMD
          // exposed this hack to retroactively enable it on their
          // SM2-capable hardware. It's esentially a no-op.
          return D3D_OK;
        }

        // CENT (AMD & Nvidia)
        if (unlikely(Value == uint32_t(D3D9Format::CENT))) {
          // Centroid (alternate pixel center) hack.
          // Taken into account anyway, so yet another no-op.
          return D3D_OK;
        }
      }

      // Nvidia's driver hack for ATOC (also supported on Intel), COPM and SSAA
      if (unlikely(State == D3DRS_ADAPTIVETESS_Y &&
                   !m_isD3D8Compatible && !isAmd)) {
        // ATOC (Nvidia & Intel)
        constexpr uint32_t AlphaToCoverageEnable  = uint32_t(D3D9Format::ATOC);
        // Disabling both ATOC and SSAA is done using D3DFMT_UNKNOWN (0)
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

        // COPM (Nvidia specific)
        if (unlikely(Value == uint32_t(D3D9Format::COPM) && isNvidia)) {
          // UE3 calls this MinimalNVIDIADriverShaderOptimization
          Logger::info("D3D9DeviceEx::SetRenderState: MinimalNVIDIADriverShaderOptimization is unsupported");
          return D3D_OK;
        }

        // SSAA (Nvidia specific)
        if (unlikely(Value == uint32_t(D3D9Format::SSAA) && isNvidia)) {
          Logger::warn("D3D9DeviceEx::SetRenderState: Transparency supersampling is unsupported");
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
          if (likely(!old != !Value))
            UpdateAnyColorWrites<0>();
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE1:
          if (likely(!old != !Value))
            UpdateAnyColorWrites<1>();
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE2:
          if (likely(!old != !Value))
            UpdateAnyColorWrites<2>();
          m_flags.set(D3D9DeviceFlag::DirtyBlendState);
          break;
        case D3DRS_COLORWRITEENABLE3:
          if (likely(!old != !Value))
            UpdateAnyColorWrites<3>();
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
          if (likely(!old != !Value))
            UpdateActiveHazardsDS(std::numeric_limits<uint32_t>::max());
        [[fallthrough]];
        case D3DRS_STENCILENABLE:
        case D3DRS_ZENABLE:
          if (likely(m_state.depthStencil != nullptr))
            m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

          m_flags.set(D3D9DeviceFlag::DirtyDepthStencilState);
          break;

        case D3DRS_ZFUNC:
        case D3DRS_TWOSIDEDSTENCILMODE:
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
          m_flags.set(D3D9DeviceFlag::DirtyRasterizerState);
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

        case D3DRS_ADAPTIVETESS_Y:
          break;

        case D3DRS_ADAPTIVETESS_X:
        case D3DRS_ADAPTIVETESS_Z:
        case D3DRS_ADAPTIVETESS_W:
          // Nvidia specific depth bounds test hack
          if (!m_isD3D8Compatible &&
              (states[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB) || oldNVDB) &&
              isNvidia) {
            m_flags.set(D3D9DeviceFlag::DirtyDepthBounds);

            if (m_state.depthStencil != nullptr && m_state.renderStates[D3DRS_ZENABLE])
              m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
          }
          break;

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

    // A state block can not be created while another is being recorded.
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppSB);

    if (unlikely(ppSB == nullptr))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9StateBlock> sb = new D3D9StateBlock(this, ConvertStateBlockType(Type));
      *ppSB = sb.ref();
      if (!m_isD3D8Compatible)
        m_losableResourceCounter++;

      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::BeginStateBlock() {
    D3D9DeviceLock lock = LockDevice();

    // Only one state block can be recorded at a given time.
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    m_recorder = new D3D9StateBlock(this, D3D9StateBlockType::None);

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    D3D9DeviceLock lock = LockDevice();

    // Recording a state block can't end if recording hasn't been started.
    if (unlikely(ppSB == nullptr || !ShouldRecord()))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppSB);

    *ppSB = m_recorder.ref();
    if (!m_isD3D8Compatible)
      m_losableResourceCounter++;
    m_recorder = nullptr;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    if (unlikely(pClipStatus == nullptr))
      return D3DERR_INVALIDCALL;

    m_state.clipStatus = *pClipStatus;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    if (unlikely(pClipStatus == nullptr))
      return D3DERR_INVALIDCALL;

    *pClipStatus = m_state.clipStatus;

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

    Stage = std::min(Stage, DWORD(caps::TextureStageCount - 1));
    dxvkType = std::min(dxvkType, D3D9TextureStageStateTypes(DXVK_TSS_COUNT - 1));

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
    if (unlikely(InvalidSampler(Sampler)))
      return D3D_OK;

    uint32_t stateSampler = RemapSamplerState(Sampler);

    return SetStateSamplerState(stateSampler, Type, Value);
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::ValidateDevice(DWORD* pNumPasses) {
    D3D9DeviceLock lock = LockDevice();

    if (pNumPasses != nullptr)
      *pNumPasses = 1;

    return IsDeviceLost() ? D3DERR_DEVICELOST : D3D_OK;
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

    if (!bSoftware && (m_behaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING))
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
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_state.vertexDecl == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(!PrimitiveCount))
      return S_OK;

    bool dynamicSysmemVBOs;
    uint32_t firstIndex     = 0;
    int32_t baseVertexIndex = 0;
    uint32_t vertexCount    = GetVertexCount(PrimitiveType, PrimitiveCount);
    UploadPerDrawData(
      StartVertex,
      vertexCount,
      firstIndex,
      0,
      baseVertexIndex,
      &dynamicSysmemVBOs,
      nullptr
    );

    PrepareDraw(PrimitiveType, !dynamicSysmemVBOs, false);

    EmitCs([this,
      cPrimType    = PrimitiveType,
      cPrimCount   = PrimitiveCount,
      cStartVertex = StartVertex
    ](DxvkContext* ctx) {
      uint32_t vertexCount = GetVertexCount(cPrimType, cPrimCount);

      ApplyPrimitiveType(ctx, cPrimType);

      // Tests on Windows show that D3D9 does not do non-indexed instanced draws.

      VkDrawIndirectCommand draw = { };
      draw.vertexCount   = vertexCount;
      draw.instanceCount = 1u;
      draw.firstVertex   = cStartVertex;

      ctx->draw(1u, &draw);
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
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_state.vertexDecl == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(!PrimitiveCount))
      return S_OK;

    bool dynamicSysmemVBOs;
    bool dynamicSysmemIBO;
    uint32_t indexCount = GetVertexCount(PrimitiveType, PrimitiveCount);
    UploadPerDrawData(
      MinVertexIndex,
      NumVertices,
      StartIndex,
      indexCount,
      BaseVertexIndex,
      &dynamicSysmemVBOs,
      &dynamicSysmemIBO
    );

    PrepareDraw(PrimitiveType, !dynamicSysmemVBOs, !dynamicSysmemIBO);

    EmitCs([this,
      cPrimType        = PrimitiveType,
      cPrimCount       = PrimitiveCount,
      cStartIndex      = StartIndex,
      cBaseVertexIndex = BaseVertexIndex,
      cInstanceCount   = GetInstanceCount()
    ](DxvkContext* ctx) {
      auto drawInfo = GenerateDrawInfo(cPrimType, cPrimCount, cInstanceCount);

      ApplyPrimitiveType(ctx, cPrimType);

      VkDrawIndexedIndirectCommand draw = { };
      draw.indexCount    = drawInfo.vertexCount;
      draw.instanceCount = drawInfo.instanceCount;
      draw.firstIndex    = cStartIndex;
      draw.vertexOffset  = cBaseVertexIndex;

      ctx->drawIndexed(1u, &draw);
    });

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::DrawPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_state.vertexDecl == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(!PrimitiveCount))
      return S_OK;

    PrepareDraw(PrimitiveType, false, false);

    uint32_t vertexCount = GetVertexCount(PrimitiveType, PrimitiveCount);

    const uint32_t dataSize = GetUPDataSize(vertexCount, VertexStreamZeroStride);
    const uint32_t bufferSize = GetUPBufferSize(vertexCount, VertexStreamZeroStride);

    auto upSlice = AllocUPBuffer(bufferSize);
    FillUPVertexBuffer(upSlice.mapPtr, pVertexStreamZeroData, dataSize, bufferSize);

    EmitCs([this,
      cBufferSlice  = std::move(upSlice.slice),
      cPrimType     = PrimitiveType,
      cStride       = VertexStreamZeroStride,
      cVertexCount  = vertexCount
    ](DxvkContext* ctx) mutable {
      ApplyPrimitiveType(ctx, cPrimType);

      // Tests on Windows show that D3D9 does not do non-indexed instanced draws.
      VkDrawIndirectCommand draw = { };
      draw.vertexCount = cVertexCount;
      draw.instanceCount = 1u;

      ctx->bindVertexBuffer(0, std::move(cBufferSlice), cStride);
      ctx->draw(1u, &draw);
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
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(m_state.vertexDecl == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(!PrimitiveCount))
      return S_OK;

    PrepareDraw(PrimitiveType, false, false);

    uint32_t vertexCount = GetVertexCount(PrimitiveType, PrimitiveCount);

    const uint32_t vertexDataSize = GetUPDataSize(MinVertexIndex + NumVertices, VertexStreamZeroStride);
    const uint32_t vertexBufferSize = GetUPBufferSize(MinVertexIndex + NumVertices, VertexStreamZeroStride);

    const uint32_t indexSize = IndexDataFormat == D3DFMT_INDEX16 ? 2 : 4;
    const uint32_t indicesSize = vertexCount * indexSize;

    const uint32_t upSize = vertexBufferSize + indicesSize;

    auto upSlice = AllocUPBuffer(upSize);
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

      VkDrawIndexedIndirectCommand draw = { };
      draw.indexCount    = drawInfo.vertexCount;
      draw.instanceCount = drawInfo.instanceCount;

      ctx->bindVertexBuffer(0, cBufferSlice.subSlice(0, cVertexSize), cStride);
      ctx->bindIndexBuffer(cBufferSlice.subSlice(cVertexSize, cBufferSlice.length() - cVertexSize), cIndexType);
      ctx->drawIndexed(1u, &draw);
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

    if (unlikely(pDestBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    // When vertex shader 3.0 or above is set as the current vertex shader,
    // the output vertex declaration must be present.
    if (UseProgrammableVS()) {
      const auto& programInfo = GetCommonShader(m_state.vertexShader)->GetInfo();

      if (unlikely(programInfo.majorVersion() >= 3) && (pVertexDecl == nullptr))
        return D3DERR_INVALIDCALL;
    }

    if (!SupportsSWVP()) {
      static bool s_errorShown = false;

      if (!std::exchange(s_errorShown, true))
        Logger::err("D3D9DeviceEx::ProcessVertices: SWVP emu unsupported (vertexPipelineStoresAndAtomics)");

      return D3D_OK;
    }

    if (!VertexCount)
      return D3D_OK;

    D3D9CommonBuffer* dst  = static_cast<D3D9VertexBuffer*>(pDestBuffer)->GetCommonBuffer();
    D3D9VertexDecl*   decl = static_cast<D3D9VertexDecl*>  (pVertexDecl);

    bool dynamicSysmemVBOs;
    uint32_t firstIndex     = 0;
    int32_t baseVertexIndex = 0;
    UploadPerDrawData(
      SrcStartIndex,
      VertexCount,
      firstIndex,
      0,
      baseVertexIndex,
      &dynamicSysmemVBOs,
      nullptr
    );

    PrepareDraw(D3DPT_FORCE_DWORD, !dynamicSysmemVBOs, false);

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

    uint32_t offset = DestIndex * decl->GetSize(0);

    auto slice = dst->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>();
         slice = slice.subSlice(offset, slice.length() - offset);

    D3D9CompactVertexElements elements;
    for (const D3DVERTEXELEMENT9& element : decl->GetElements()) {
      elements.emplace_back(element);
    }

    EmitCs([this,
      cVertexElements = std::move(elements),
      cVertexCount    = VertexCount,
      cStartIndex     = SrcStartIndex,
      cInstanceCount  = GetInstanceCount(),
      cBufferSlice    = slice
    ](DxvkContext* ctx) mutable {
      Rc<DxvkShader> shader = m_swvpEmulator.GetShaderModule(this, std::move(cVertexElements));

      auto drawInfo = GenerateDrawInfo(D3DPT_POINTLIST, cVertexCount, cInstanceCount);

      if (drawInfo.instanceCount != 1) {
        drawInfo.instanceCount = 1;

        Logger::warn("D3D9DeviceEx::ProcessVertices: instancing unsupported");
      }

      ApplyPrimitiveType(ctx, D3DPT_POINTLIST);

      // Unbind the pixel shader, we aren't drawing
      // to avoid val errors / UB.
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(nullptr);

      VkDrawIndirectCommand draw = { };
      draw.vertexCount   = drawInfo.vertexCount;
      draw.instanceCount = drawInfo.instanceCount;
      draw.firstVertex   = cStartIndex;

      ctx->bindShader<VK_SHADER_STAGE_GEOMETRY_BIT>(std::move(shader));
      ctx->bindUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT, getSWVPBufferSlot(), std::move(cBufferSlice));
      ctx->draw(1u, &draw);
      ctx->bindUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT, getSWVPBufferSlot(), DxvkBufferSlice());
      ctx->bindShader<VK_SHADER_STAGE_GEOMETRY_BIT>(nullptr);
    });

    // We unbound the pixel shader before,
    // let's make sure that gets rebound.
    m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

    if (m_state.pixelShader != nullptr) {
      BindShader<DxsoProgramTypes::PixelShader>(
        GetCommonShader(m_state.pixelShader));
    }

    if (dst->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER) {
      uint32_t copySize = VertexCount * decl->GetSize(0);

      EmitCs([
        cSrcBuffer = dst->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>(),
        cDstBuffer = dst->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>(),
        cOffset    = offset,
        cCopySize  = copySize
      ](DxvkContext* ctx) {
        ctx->copyBuffer(cDstBuffer, cOffset, cSrcBuffer, cOffset, cCopySize);
      });
    }

    dst->SetNeedsReadback(true);
    TrackBufferMappingBufferSequenceNumber(dst);

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
      dirtyFFShader |= decl->GetFlags()        != m_state.vertexDecl->GetFlags()
                    || decl->GetTexcoordMask() != m_state.vertexDecl->GetTexcoordMask();

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

    const uint32_t majorVersion = D3DSHADER_VERSION_MAJOR(pFunction[0]);
    const uint32_t minorVersion = D3DSHADER_VERSION_MINOR(pFunction[0]);

    // Late fixed-function capable hardware exposed support for VS 1.1
    const uint32_t shaderModelVS = m_isD3D8Compatible ? 1u : std::max(1u, m_d3d9Options.shaderModel);

    if (unlikely(majorVersion > shaderModelVS
             || (majorVersion == 1 && minorVersion > 1)
             // Skip checking the SM2 minor version, as it has a 2_x mode apparently
             || (majorVersion == 3 && minorVersion != 0))) {
      Logger::err(str::format("D3D9DeviceEx::CreateVertexShader: Unsupported VS version ", majorVersion, ".", minorVersion));
      return D3DERR_INVALIDCALL;
    }

    DxsoModuleInfo moduleInfo;
    moduleInfo.options = m_dxsoOptions;

    D3D9CommonShader module;
    uint32_t bytecodeLength;

    if (FAILED(this->CreateShaderModule(&module,
      &bytecodeLength,
      VK_SHADER_STAGE_VERTEX_BIT,
      pFunction,
      &moduleInfo)))
      return D3DERR_INVALIDCALL;


    if (m_isD3D8Compatible && !m_isSWVP) {
      const uint32_t maxVSConstantIndex = module.GetMaxDefinedConstant();
      // D3D8 enforces the value advertised in pCaps->MaxVertexShaderConst for HWVP
      if (unlikely(maxVSConstantIndex > caps::MaxFloatConstantsVS - 1)) {
        Logger::err(str::format("D3D9DeviceEx::CreateVertexShader: Invalid constant index ", maxVSConstantIndex));
        return D3DERR_INVALIDCALL;
      }
    }

    *ppShader = ref(new D3D9VertexShader(this,
      &m_shaderAllocator,
      module,
      pFunction,
      bytecodeLength));

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

      BindShader<DxsoProgramTypes::VertexShader>(GetCommonShader(shader));
      m_vsShaderMasks = newShader->GetShaderMask();

      UpdateTextureTypeMismatchesForShader(newShader, m_vsShaderMasks.samplerMask, FirstVSSamplerSlot);
    }
    else {
      m_vsShaderMasks = D3D9ShaderMasks();

      // Fixed function vertex shaders don't support sampling textures.
      m_dirtyTextures |= m_vsShaderMasks.samplerMask & m_mismatchingTextureTypes;
      m_mismatchingTextureTypes &= ~m_vsShaderMasks.samplerMask;
    }

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

    const uint32_t bit = 1u << StreamNumber;
    m_activeVertexBuffers &= ~bit;
    m_activeVertexBuffersToUploadPerDraw &= ~bit;
    m_activeVertexBuffersToUpload &= ~bit;

    if (buffer != nullptr) {
      needsUpdate |= vbo.offset != OffsetInBytes
                  || vbo.stride != Stride;

      vbo.offset = OffsetInBytes;
      vbo.stride = Stride;

      const D3D9CommonBuffer* commonBuffer = GetCommonBuffer(buffer);
      m_activeVertexBuffers |= bit;
      if (commonBuffer->DoPerDrawUpload() || CanOnlySWVP())
        m_activeVertexBuffersToUploadPerDraw |= bit;
      if (commonBuffer->NeedsUpload()) {
        m_activeVertexBuffersToUpload |= bit;
      }
    } else {
      // D3D9 doesn't actually unbind any vertex buffer when passing null.
      // Operation Flashpoint: Red River relies on this behavior.
      needsUpdate = false;
      vbo.offset = 0;
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

    // Don't unbind the buffer if the game sets a nullptr here.
    // Operation Flashpoint Red River breaks if we do that.
    // EndScene will clean it up if necessary.
    if (buffer != nullptr)
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

    const uint32_t majorVersion = D3DSHADER_VERSION_MAJOR(pFunction[0]);
    const uint32_t minorVersion = D3DSHADER_VERSION_MINOR(pFunction[0]);

    const uint32_t shaderModelPS = m_isD3D8Compatible ? std::min(1u, m_d3d9Options.shaderModel) : m_d3d9Options.shaderModel;

    if (unlikely(majorVersion > shaderModelPS
             || (majorVersion == 1 && minorVersion > 4)
             // Skip checking the SM2 minor version, as it has a 2_x mode apparently
             || (majorVersion == 3 && minorVersion != 0))) {
      Logger::err(str::format("D3D9DeviceEx::CreatePixelShader: Unsupported PS version ", majorVersion, ".", minorVersion));
      return D3DERR_INVALIDCALL;
    }

    DxsoModuleInfo moduleInfo;
    moduleInfo.options = m_dxsoOptions;

    D3D9CommonShader module;
    uint32_t bytecodeLength;

    if (FAILED(this->CreateShaderModule(&module,
      &bytecodeLength,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      pFunction,
      &moduleInfo)))
      return D3DERR_INVALIDCALL;

    *ppShader = ref(new D3D9PixelShader(this,
      &m_shaderAllocator,
      module,
      pFunction,
      bytecodeLength));

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

    D3D9ShaderMasks newShaderMasks;

    if (shader != nullptr) {
      m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);

      BindShader<DxsoProgramTypes::PixelShader>(newShader);
      newShaderMasks = newShader->GetShaderMask();

      UpdateTextureTypeMismatchesForShader(newShader, newShaderMasks.samplerMask, 0);
    }
    else {
      // TODO: What fixed function textures are in use?
      // Currently we are making all 8 of them as in use here.

      // The RT output is always 0 for fixed function.
      newShaderMasks = FixedFunctionMask;

      // Fixed function always uses spec constants to decide the texture type.
      m_dirtyTextures |= newShaderMasks.samplerMask & m_mismatchingTextureTypes;
      m_mismatchingTextureTypes &= ~newShaderMasks.samplerMask;
    }

    // If we have any RTs we would have bound to the the FB
    // not in the new shader mask, mark the framebuffer as dirty
    // so we unbind them.
    uint32_t boundMask = 0u;
    uint32_t anyColorWriteMask = 0u;
    for (uint32_t i = 0u; i < m_state.renderTargets.size(); i++) {
      boundMask |= HasRenderTargetBound(i) << i;
      anyColorWriteMask |= (m_state.renderStates[ColorWriteIndex(i)] != 0) << i;
    }

    uint32_t oldUseMask = boundMask & anyColorWriteMask & m_psShaderMasks.rtMask;
    uint32_t newUseMask = boundMask & anyColorWriteMask & newShaderMasks.rtMask;
    if (oldUseMask != newUseMask)
      m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);

    if (m_psShaderMasks.samplerMask != newShaderMasks.samplerMask ||
        m_psShaderMasks.rtMask != newShaderMasks.rtMask) {
      m_psShaderMasks = newShaderMasks;
      UpdateActiveHazardsRT(std::numeric_limits<uint32_t>::max());
      UpdateActiveHazardsDS(std::numeric_limits<uint32_t>::max());
    }

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
      return D3DERR_NOTAVAILABLE;
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

    if (m_cursor.IsSoftwareCursor()) {
      D3D9_SOFTWARE_CURSOR* pSoftwareCursor = m_cursor.GetSoftwareCursor();

      UINT cursorWidth  = pSoftwareCursor->DrawCursor ? pSoftwareCursor->Width : 0;
      UINT cursorHeight = pSoftwareCursor->DrawCursor ? pSoftwareCursor->Height : 0;

      m_implicitSwapchain->SetCursorPosition(pSoftwareCursor->X - pSoftwareCursor->XHotSpot,
                                             pSoftwareCursor->Y - pSoftwareCursor->YHotSpot,
                                             cursorWidth, cursorHeight);

      // Once a hardware cursor has been set or the device has been reset,
      // we need to ensure that we render a 0-sized rectangle first, and
      // only then fully clear the software cursor.
      if (unlikely(pSoftwareCursor->ResetCursor)) {
        pSoftwareCursor->Width = 0;
        pSoftwareCursor->Height = 0;
        pSoftwareCursor->XHotSpot = 0;
        pSoftwareCursor->YHotSpot = 0;
        pSoftwareCursor->ResetCursor = false;
      }
    }

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

    if (unlikely(MultiSample > D3DMULTISAMPLE_16_SAMPLES))
      return D3DERR_INVALIDCALL;

    uint32_t sampleCount = std::max<uint32_t>(MultiSample, 1u);

    // Check if this is a power of two...
    if (sampleCount & (sampleCount - 1))
      return D3DERR_NOTAVAILABLE;

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
    desc.IsLockable         = Lockable;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_SURFACE, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, IsExtended(), nullptr, pSharedHandle);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      m_losableResourceCounter++;

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
    desc.IsAttachmentOnly   = TRUE;
    // Docs: Off-screen plain surfaces are always lockable, regardless of their pool types.
    desc.IsLockable         = TRUE;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_SURFACE, &desc)))
      return D3DERR_INVALIDCALL;

    if (pSharedHandle != nullptr && Pool != D3DPOOL_DEFAULT)
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, IsExtended(), nullptr, pSharedHandle);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      
      if (desc.Pool == D3DPOOL_DEFAULT)
        m_losableResourceCounter++;

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
    desc.IsLockable         = IsLockableDepthStencilFormat(desc.Format);

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_SURFACE, &desc)))
      return D3DERR_INVALIDCALL;

    try {
      const Com<D3D9Surface> surface = new D3D9Surface(this, &desc, IsExtended(), nullptr, pSharedHandle);
      m_initializer->InitTexture(surface->GetCommonTexture());
      *ppSurface = surface.ref();
      m_losableResourceCounter++;

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

    if (unlikely(IsDeviceLost())) {
      return D3DERR_DEVICELOST;
    }

    m_implicitSwapchain->Invalidate(pPresentationParameters->hDeviceWindow);

    try {
      auto* swapchain = new D3D9SwapChainEx(this, pPresentationParameters, pFullscreenDisplayMode, false);
      *ppSwapChain = ref(swapchain);
      m_losableResourceCounter++;
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

    if (state[StateSampler][Type] == Value)
      return D3D_OK;

    state[StateSampler][Type] = Value;

    const uint32_t samplerBit = 1u << StateSampler;

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
      m_dirtySamplerStates |= samplerBit;
    else if (Type == D3DSAMP_SRGBTEXTURE && (m_activeTextures & samplerBit))
      m_dirtyTextures |= samplerBit;

    constexpr DWORD Fetch4Enabled  = MAKEFOURCC('G', 'E', 'T', '4');
    constexpr DWORD Fetch4Disabled = MAKEFOURCC('G', 'E', 'T', '1');

    if (unlikely(Type == D3DSAMP_MIPMAPLODBIAS)) {
      if (unlikely(Value == Fetch4Enabled))
        m_fetch4Enabled |= samplerBit;
      else if (unlikely(Value == Fetch4Disabled))
        m_fetch4Enabled &= ~samplerBit;

      UpdateActiveFetch4(StateSampler);
    }

    if (unlikely(Type == D3DSAMP_MAGFILTER && (m_fetch4Enabled & samplerBit)))
      UpdateActiveFetch4(StateSampler);

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::SetStateTexture(DWORD StateSampler, IDirect3DBaseTexture9* pTexture) {
    D3D9DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStateTexture(StateSampler, pTexture);

    if (m_state.textures[StateSampler] == pTexture)
      return D3D_OK;

    auto oldTexture = GetCommonTexture(m_state.textures[StateSampler]);
    auto newTexture = GetCommonTexture(pTexture);

    // We need to check our ops and disable respective stages.
    // Given we have transition from a null resource to
    // a valid resource or vice versa.
    const bool isPSSampler = StateSampler < caps::MaxTexturesPS;
    if (isPSSampler) {
      const uint32_t textureType = newTexture != nullptr
        ? uint32_t(newTexture->GetType() - D3DRTYPE_TEXTURE)
        : 0;
      // There are 3 texture types, so we need 2 bits.
      const uint32_t offset = StateSampler * 2;
      const uint32_t textureBitMask = 0b11u       << offset;
      const uint32_t textureBits    = textureType << offset;

      // In fixed function shaders and SM < 2 we put the type mask
      // into a spec constant to select the used sampler type.
      m_textureTypes &= ~textureBitMask;
      m_textureTypes |=  textureBits;

      // If we either bind a new texture or unbind the old one,
      // we need to update the fixed function shader
      // because we generate a different shader based on whether each texture is bound.
      if (newTexture == nullptr || oldTexture == nullptr)
        m_flags.set(D3D9DeviceFlag::DirtyFFPixelShader);
    }

    DWORD oldUsage = oldTexture != nullptr ? oldTexture->Desc()->Usage : 0;
    DWORD newUsage = newTexture != nullptr ? newTexture->Desc()->Usage : 0;
    DWORD combinedUsage = oldUsage | newUsage;
    TextureChangePrivate(m_state.textures[StateSampler], pTexture);
    m_dirtyTextures |= 1u << StateSampler;
    UpdateTextureBitmasks(StateSampler, combinedUsage);

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
    
    // Clamp values instead of checking and returning INVALID_CALL
    // Matches tests + Dawn of Magic 2 relies on it.
    Stage = std::min(Stage, DWORD(caps::TextureStageCount - 1));
    Type = std::min(Type, D3D9TextureStageStateTypes(DXVK_TSS_COUNT - 1));

    D3D9DeviceLock lock = LockDevice();

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
    return m_dxvkDevice->features().core.features.vertexPipelineStoresAndAtomics && m_dxvkDevice->features().vk12.shaderInt8;
  }


  bool D3D9DeviceEx::SupportsVCacheQuery() const {
    return m_adapter->GetVendorId() == uint32_t(DxvkGpuVendor::Nvidia);
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

    enabled.vk12.samplerMirrorClampToEdge = VK_TRUE;

    enabled.vk13.shaderDemoteToHelperInvocation = VK_TRUE;

    enabled.extMemoryPriority.memoryPriority = supported.extMemoryPriority.memoryPriority;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;

    // ProcessVertices
    enabled.core.features.vertexPipelineStoresAndAtomics = supported.core.features.vertexPipelineStoresAndAtomics;
    enabled.vk12.shaderInt8 = supported.vk12.shaderInt8;

    // DXVK Meta
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

    if (supported.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout)
      enabled.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout = VK_TRUE;

    enabled.extNonSeamlessCubeMap.nonSeamlessCubeMap = supported.extNonSeamlessCubeMap.nonSeamlessCubeMap;

    enabled.extDepthBiasControl.depthBiasControl = supported.extDepthBiasControl.depthBiasControl;
    enabled.extDepthBiasControl.depthBiasExact = supported.extDepthBiasControl.depthBiasExact;
    if (supported.extDepthBiasControl.floatRepresentation)
      enabled.extDepthBiasControl.floatRepresentation = VK_TRUE;
    else if (supported.extDepthBiasControl.leastRepresentableValueForceUnormRepresentation)
      enabled.extDepthBiasControl.leastRepresentableValueForceUnormRepresentation = VK_TRUE;

    return enabled;
  }


  void D3D9DeviceEx::DetermineConstantLayouts(bool canSWVP) {
    D3D9ConstantSets& vsConstSet    = m_consts[DxsoProgramType::VertexShader];
    vsConstSet.layout.floatCount    = canSWVP ? caps::MaxFloatConstantsSoftware : caps::MaxFloatConstantsVS;
    vsConstSet.layout.intCount      = canSWVP ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
    vsConstSet.layout.boolCount     = canSWVP ? caps::MaxOtherConstantsSoftware : caps::MaxOtherConstants;
    vsConstSet.layout.bitmaskCount  = align(vsConstSet.layout.boolCount, 32) / 32;

    D3D9ConstantSets& psConstSet   = m_consts[DxsoProgramType::PixelShader];
    psConstSet.layout.floatCount   = caps::MaxFloatConstantsPS;
    psConstSet.layout.intCount     = caps::MaxOtherConstants;
    psConstSet.layout.boolCount    = caps::MaxOtherConstants;
    psConstSet.layout.bitmaskCount = align(psConstSet.layout.boolCount, 32) / 32;
  }


  D3D9BufferSlice D3D9DeviceEx::AllocUPBuffer(VkDeviceSize size) {
    constexpr VkDeviceSize UPBufferSize = 1 << 20;

    if (unlikely(m_upBuffer == nullptr || size > UPBufferSize)) {
      VkMemoryPropertyFlags memoryFlags
        = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      DxvkBufferCreateInfo info;
      info.size   = std::max(UPBufferSize, size);
      info.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                  | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                  | VK_ACCESS_INDEX_READ_BIT;
      info.stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.debugName = "UP buffer";

      Rc<DxvkBuffer> buffer = m_dxvkDevice->createBuffer(info, memoryFlags);
      void* mapPtr = buffer->mapPtr(0);

      if (size <= UPBufferSize) {
        m_upBuffer = std::move(buffer);
        m_upBufferMapPtr = mapPtr;
      } else {
        // Temporary buffer
        D3D9BufferSlice result;
        result.slice = DxvkBufferSlice(std::move(buffer), 0, size);
        result.mapPtr = mapPtr;
        return result;
      }
    }

    VkDeviceSize alignedSize = align(size, CACHE_LINE_SIZE);

    if (unlikely(m_upBufferOffset + alignedSize > UPBufferSize)) {
      auto slice = m_upBuffer->allocateStorage();

      m_upBufferOffset = 0;
      m_upBufferMapPtr = slice->mapPtr();

      EmitCs([
        cBuffer = m_upBuffer,
        cSlice  = std::move(slice)
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cSlice));
      });
    }

    D3D9BufferSlice result;
    result.slice = DxvkBufferSlice(m_upBuffer, m_upBufferOffset, size);
    result.mapPtr = reinterpret_cast<char*>(m_upBufferMapPtr) + m_upBufferOffset;

    m_upBufferOffset += alignedSize;
    return result;
  }


  D3D9BufferSlice D3D9DeviceEx::AllocStagingBuffer(VkDeviceSize size) {
    D3D9BufferSlice result;
    result.slice = m_stagingBuffer.alloc(size);
    result.mapPtr = result.slice.mapPtr(0);
    return result;
  }


  void D3D9DeviceEx::WaitStagingBuffer() {
    // Treshold for staging memory in flight. Since the staging buffer granularity
    // is somewhat coars, it is possible for one additional allocation to be in use,
    // but otherwise this is a hard upper bound.
    constexpr VkDeviceSize MaxStagingMemoryInFlight = env::is32BitHostPlatform()
      ? StagingBufferSize * 4
      : StagingBufferSize * 16;

    // Threshold at which to submit eagerly. This is useful to ensure
    // that staging buffer memory gets recycled relatively soon.
    constexpr VkDeviceSize MaxStagingMemoryPerSubmission = MaxStagingMemoryInFlight / 3u;

    VkDeviceSize stagingBufferAllocated = m_stagingBuffer.getStatistics().allocatedTotal;

    if (stagingBufferAllocated > m_stagingMemorySignaled + MaxStagingMemoryPerSubmission) {
      // Perform submission. If the amount of staging memory allocated since the
      // last submission exceeds the hard limit, we need to submit to guarantee
      // forward progress. Ideally, this should not happen very often.
      GpuFlushType flushType = stagingBufferAllocated <= m_stagingMemorySignaled + MaxStagingMemoryInFlight
        ? GpuFlushType::ImplicitSynchronization
        : GpuFlushType::ExplicitFlush;

      ConsiderFlush(flushType);
    }

    // Wait for staging memory to get recycled.
    if (stagingBufferAllocated > MaxStagingMemoryInFlight)
      m_dxvkDevice->waitForFence(*m_stagingBufferFence, stagingBufferAllocated - MaxStagingMemoryInFlight);
  }


  D3D9_VK_FORMAT_MAPPING D3D9DeviceEx::LookupFormat(
    D3D9Format            Format) const {
    return m_adapter->GetFormatMapping(Format);
  }


  const DxvkFormatInfo* D3D9DeviceEx::UnsupportedFormatInfo(
    D3D9Format            Format) const {
    return m_adapter->GetUnsupportedFormatInfo(Format);
  }


  bool D3D9DeviceEx::WaitForResource(
    const DxvkPagedResource&                Resource,
          uint64_t                          SequenceNumber,
          DWORD                             MapFlags) {
    // Wait for the any pending D3D9 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.

    // Determine access type to wait for based on map mode
    DxvkAccess access = (MapFlags & D3DLOCK_READONLY)
      ? DxvkAccess::Write
      : DxvkAccess::Read;

    if (!Resource.isInUse(access))
      SynchronizeCsThread(SequenceNumber);

    if (Resource.isInUse(access)) {
      if (MapFlags & D3DLOCK_DONOTWAIT) {
        // We don't have to wait, but misbehaving games may
        // still try to spin on `Map` until the resource is
        // idle, so we should flush pending commands
        ConsiderFlush(GpuFlushType::ImplicitWeakHint);
        return false;
      }
      else {
        // Make sure pending commands using the resource get
        // executed on the the GPU if we have to wait for it
        Flush();
        SynchronizeCsThread(SequenceNumber);

        m_dxvkDevice->waitForResource(Resource, access);
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
      VkExtent3D blockSize = FormatInfo->blockSize;
      if (unlikely(FormatInfo->flags.test(DxvkFormatFlag::MultiPlane))) {
        elementSize = FormatInfo->planes[0].elementSize;
        blockSize = { FormatInfo->planes[0].blockSize.width, FormatInfo->planes[0].blockSize.height, 1u };
      }

      offsets[0] = offsets[0] / blockSize.depth;
      offsets[1] = offsets[1] / blockSize.height;
      offsets[2] = offsets[2] / blockSize.width;
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

    // We only ever wait for textures that were used with GetRenderTargetData or GetFrontBufferData anyway.
    // Games like Beyond Good and Evil break if this doesn't succeed.
    Flags &= ~D3DLOCK_DONOTWAIT;

    if (unlikely((Flags & (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)) == (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)))
      Flags &= ~D3DLOCK_DISCARD;

    // Tests show that D3D9 drivers ignore DISCARD when the device is lost.
    if (unlikely(m_deviceLostState != D3D9DeviceLostState::Ok))
      Flags &= ~D3DLOCK_DISCARD;

    auto& desc = *(pResource->Desc());

    if (unlikely(!desc.IsLockable))
      return D3DERR_INVALIDCALL;

    if (unlikely(pBox != nullptr)) {
      D3DRESOURCETYPE type = pResource->GetType();
      D3D9_FORMAT_BLOCK_SIZE blockSize = GetFormatAlignedBlockSize(desc.Format);

      bool isBlockAlignedFormat = blockSize.Width > 0 && blockSize.Height > 0;
      bool isNotLeftAligned   = pBox->Left   && (pBox->Left   & (blockSize.Width  - 1));
      bool isNotTopAligned    = pBox->Top    && (pBox->Top    & (blockSize.Height - 1));
      bool isNotRightAligned  = pBox->Right  && (pBox->Right  & (blockSize.Width  - 1));
      bool isNotBottomAligned = pBox->Bottom && (pBox->Bottom & (blockSize.Height - 1));

      // LockImage calls on D3DPOOL_DEFAULT surfaces and volume textures with formats
      // which need to be block aligned, must be validated for mip level 0.
      if (MipLevel == 0 && isBlockAlignedFormat
        && (type == D3DRTYPE_VOLUMETEXTURE ||
            desc.Pool == D3DPOOL_DEFAULT)
        && (isNotLeftAligned  || isNotTopAligned ||
            isNotRightAligned || isNotBottomAligned))
        return D3DERR_INVALIDCALL;
    }

    auto& formatMapping = pResource->GetFormatMapping();
    const DxvkFormatInfo* formatInfo = formatMapping.IsValid()
      ? lookupFormatInfo(formatMapping.FormatColor) : UnsupportedFormatInfo(pResource->Desc()->Format);

    auto subresource = pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);

    VkExtent3D levelExtent = pResource->GetExtentMip(MipLevel);
    VkExtent3D blockCount  = util::computeBlockCount(levelExtent, formatInfo->blockSize);

    bool fullResource = pBox == nullptr;
    if (unlikely(!fullResource)) {
      // Check whether the box passed as argument matches or exceeds the entire texture.
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
    // if we are not locking the full resource.

    // DISCARD is also ignored for MANAGED and SYSTEMEM.
    // DISCARD is not ignored for non-DYNAMIC unlike what the docs say.

    if (!fullResource || desc.Pool != D3DPOOL_DEFAULT)
      Flags &= ~D3DLOCK_DISCARD;

    if (desc.Usage & D3DUSAGE_WRITEONLY)
      Flags &= ~D3DLOCK_READONLY;

    // If we recently wrote to the texture on the gpu,
    // then we need to copy -> buffer
    // We are also always dirty if we are a render target,
    // a depth stencil, or auto generate mipmaps.
    bool renderable = desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL);
    bool needsReadback = pResource->NeedsReadback(Subresource) || renderable;

    // Skip readback if we discard is specified. We can only do this for textures that have an associated Vulkan image.
    // Any other texture might write to the Vulkan staging buffer directly. (GetBackbufferData for example)
    needsReadback &= pResource->GetImage() != nullptr || !(Flags & D3DLOCK_DISCARD);
    pResource->SetNeedsReadback(Subresource, false);

    if (unlikely(pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED || needsReadback)) {
      // Create mapping buffer if it doesn't exist yet. (POOL_DEFAULT)
      pResource->CreateBuffer(!needsReadback);
    }

    // Don't use MapTexture here to keep the mapped list small while the resource is still locked.
    void* mapPtr = pResource->GetData(Subresource);

    if (unlikely(needsReadback)) {
      // The texture was written to on the GPU.
      // This can be either the image (for D3DPOOL_DEFAULT)
      // or the buffer directly (for D3DPOOL_SYSTEMMEM).

      DxvkBufferSlice mappedBufferSlice = pResource->GetBufferSlice(Subresource);
      const Rc<DxvkBuffer> mappedBuffer = pResource->GetBuffer();

      if (unlikely(pResource->GetFormatMapping().ConversionFormatInfo.FormatType != D3D9ConversionFormat_None)) {
        Logger::err(str::format("Reading back format", pResource->Desc()->Format, " is not supported. It is uploaded using the fomrat converter."));
      }

      if (pResource->GetImage() != nullptr) {
        Rc<DxvkImage> resourceImage = pResource->GetImage();

        Rc<DxvkImage> mappedImage;
        if (resourceImage->info().sampleCount != 1) {
            mappedImage = pResource->GetResolveImage();
        } else {
            mappedImage = std::move(resourceImage);
        }

        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        auto subresourceLayers = vk::makeSubresourceLayers(subresource);

        // We need to resolve this, some games
        // lock MSAA render targets even though
        // that's entirely illegal and they explicitly
        // tell us that they do NOT want to lock them...
        //
        // resourceImage is null because the image reference was moved to mappedImage
        // for images that need to be resolved.
        if (resourceImage != nullptr) {
          EmitCs([
            cMainImage    = resourceImage,
            cResolveImage = mappedImage,
            cSubresource  = subresourceLayers
          ] (DxvkContext* ctx) {
            VkFormat format = cMainImage->info().format;

            VkImageResolve region;
            region.srcSubresource = cSubresource;
            region.srcOffset      = VkOffset3D { 0, 0, 0 };
            region.dstSubresource = cSubresource;
            region.dstOffset      = VkOffset3D { 0, 0, 0 };
            region.extent         = cMainImage->mipLevelExtent(cSubresource.mipLevel);

            ctx->resolveImage(cResolveImage, cMainImage, region, format,
              getDefaultResolveMode(format), VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
          });
        }

        // if packedFormat is VK_FORMAT_UNDEFINED
        // DxvkContext::copyImageToBuffer will automatically take the format from the image
        VkFormat packedFormat = GetPackedDepthStencilFormat(desc.Format);

        EmitCs([
          cImageBufferSlice = std::move(mappedBufferSlice),
          cImage            = std::move(mappedImage),
          cSubresources     = subresourceLayers,
          cLevelExtent      = levelExtent,
          cPackedFormat     = packedFormat
        ] (DxvkContext* ctx) {
          ctx->copyImageToBuffer(cImageBufferSlice.buffer(),
            cImageBufferSlice.offset(), 4, 0, cPackedFormat,
            cImage, cSubresources, VkOffset3D { 0, 0, 0 },
            cLevelExtent);
        });
        TrackTextureMappingBufferSequenceNumber(pResource, Subresource);
      }

      // Wait until the buffer is idle which may include the copy (and resolve) we just issued.
      if (!WaitForResource(*mappedBuffer, pResource->GetMappingBufferSequenceNumber(Subresource), Flags))
        return D3DERR_WASSTILLDRAWING;
    }

    const bool atiHack = desc.Format == D3D9Format::ATI1 || desc.Format == D3D9Format::ATI2;
    // Set up map pointer.
    if (atiHack) {
      // The API didn't treat this as a block compressed format here.
      // So we need to lie here. The game is expected to use this info and do a workaround.
      // It's stupid. I know.
      pLockedBox->RowPitch   = align(std::max(desc.Width >> MipLevel, 1u), 4);
      pLockedBox->SlicePitch = pLockedBox->RowPitch * std::max(desc.Height >> MipLevel, 1u);
    }
    else if (likely(!formatInfo->flags.test(DxvkFormatFlag::MultiPlane))) {
      pLockedBox->RowPitch   = align(formatInfo->elementSize * blockCount.width, 4);
      pLockedBox->SlicePitch = pLockedBox->RowPitch * blockCount.height;
    } else {
      auto plane = &formatInfo->planes[0];
      uint32_t planeElementSize = plane->elementSize;
      VkExtent3D planeBlockSize = { plane->blockSize.width, plane->blockSize.height, 1u };
      VkExtent3D blockCount  = util::computeBlockCount(levelExtent, planeBlockSize);
      pLockedBox->RowPitch   = align(planeElementSize * blockCount.width, 4);
      pLockedBox->SlicePitch = pLockedBox->RowPitch * blockCount.height;
    }

    pResource->SetLocked(Subresource, true);

    // Make sure the amount of mapped texture memory stays below the threshold.
    UnmapTextures();

    const bool readOnly = Flags & D3DLOCK_READONLY;
    const bool noDirtyUpdate = Flags & D3DLOCK_NO_DIRTY_UPDATE;
    if ((desc.Pool == D3DPOOL_DEFAULT || !noDirtyUpdate) && !readOnly) {
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

    if (IsPoolManaged(desc.Pool) && !readOnly) {
      // Managed textures are uploaded at draw time.
      pResource->SetNeedsUpload(Subresource, true);

      for (uint32_t i : bit::BitMask(m_activeTextures)) {
        // Guaranteed to not be nullptr...
        auto texInfo = GetCommonTexture(m_state.textures[i]);

        if (texInfo == pResource) {
          m_activeTexturesToUpload |= 1 << i;
        }
      }
    }

    const uint32_t offset = CalcImageLockOffset(
      pLockedBox->SlicePitch,
      pLockedBox->RowPitch,
      (!atiHack) ? formatInfo : nullptr,
      pBox);

    uint8_t* data = reinterpret_cast<uint8_t*>(mapPtr);
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

    // Don't allow multiple unlockings, except for D3DRTYPE_TEXTURE
    if (unlikely(!pResource->GetLocked(Subresource))) {
      if (pResource->GetType() == D3DRTYPE_TEXTURE)
        return D3D_OK;
      else
        return D3DERR_INVALIDCALL;
    }

    MapTexture(pResource, Subresource); // Add it to the list of mapped resources
    pResource->SetLocked(Subresource, false);

    // Flush image contents from staging if we aren't read only
    // and we aren't deferring for managed.
    const D3DBOX& box = pResource->GetDirtyBox(Face);
    bool shouldFlush  = pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
         shouldFlush &= box.Left < box.Right && box.Top < box.Bottom && box.Front < box.Back;
         shouldFlush &= !pResource->IsManaged();

    if (shouldFlush) {
        this->FlushImage(pResource, Subresource);
        if (!pResource->IsAnySubresourceLocked())
          pResource->ClearDirtyBoxes();
    }

    // Toss our staging buffer if we're not dynamic
    // and we aren't managed (for sysmem copy.)
    bool shouldToss  = pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
         shouldToss &= !pResource->IsDynamic();
         shouldToss &= !pResource->IsManaged();
         shouldToss &= !pResource->IsAnySubresourceLocked();

    // The texture converter cannot handle converting back. So just keep textures in memory as a workaround.
    shouldToss &= pResource->GetFormatMapping().ConversionFormatInfo.FormatType == D3D9ConversionFormat_None;

    if (shouldToss)
      pResource->DestroyBuffer();

    UnmapTextures();
    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::FlushImage(
        D3D9CommonTexture*      pResource,
        UINT                    Subresource) {

    const Rc<DxvkImage> image = pResource->GetImage();
    auto formatInfo  = lookupFormatInfo(image->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
      formatInfo->aspectMask, Subresource);

    const D3DBOX& box = pResource->GetDirtyBox(subresource.arrayLayer);

    // The dirty box is only tracked for mip 0. Scale it for the mip level we're gonna upload.
    VkExtent3D mip0Extent = { box.Right - box.Left, box.Bottom - box.Top, box.Back - box.Front };
    VkExtent3D extent = util::computeMipLevelExtent(mip0Extent, subresource.mipLevel);
    VkOffset3D mip0Offset = { int32_t(box.Left), int32_t(box.Top), int32_t(box.Front) };
    VkOffset3D offset = util::computeMipLevelOffset(mip0Offset, subresource.mipLevel);

    UpdateTextureFromBuffer(pResource, pResource, Subresource, Subresource, offset, extent, offset);

    if (pResource->IsAutomaticMip())
      MarkTextureMipsDirty(pResource);

    return D3D_OK;
  }

  void D3D9DeviceEx::UpdateTextureFromBuffer(
    D3D9CommonTexture* pDestTexture,
    D3D9CommonTexture* pSrcTexture,
    UINT DestSubresource,
    UINT SrcSubresource,
    VkOffset3D SrcOffset,
    VkExtent3D SrcExtent,
    VkOffset3D DestOffset) {
    // Wait until the amount of used staging memory is under a certain threshold to avoid using
    // too much memory and even more so to avoid using too much address space.
    WaitStagingBuffer();

    const Rc<DxvkImage> image = pDestTexture->GetImage();

    // Now that data has been written into the buffer,
    // we need to copy its contents into the image

    auto formatInfo = lookupFormatInfo(pDestTexture->GetFormatMapping().FormatColor);
    auto srcSubresource = pSrcTexture->GetSubresourceFromIndex(
      formatInfo->aspectMask, SrcSubresource);

    auto dstSubresource = pDestTexture->GetSubresourceFromIndex(
      formatInfo->aspectMask, DestSubresource);
    VkImageSubresourceLayers dstLayers = { dstSubresource.aspectMask, dstSubresource.mipLevel, dstSubresource.arrayLayer, 1 };

    VkExtent3D dstTexLevelExtent = image->mipLevelExtent(dstSubresource.mipLevel);
    VkExtent3D srcTexLevelExtent = util::computeMipLevelExtent(pSrcTexture->GetExtent(), srcSubresource.mipLevel);

    auto convertFormat = pDestTexture->GetFormatMapping().ConversionFormatInfo;

    if (unlikely(pSrcTexture->NeedsReadback(SrcSubresource))) {
      // The src texutre has to be in POOL_SYSTEMEM, so it cannot use AUTOMIPGEN.
      // That means that NeedsReadback is only true if the texture has been used with GetRTData or GetFrontbufferData before.
      // Those functions create a buffer, so the buffer always exists here.
      const Rc<DxvkBuffer>& buffer = pSrcTexture->GetBuffer();
      WaitForResource(*buffer, pSrcTexture->GetMappingBufferSequenceNumber(SrcSubresource), 0);
      pSrcTexture->SetNeedsReadback(SrcSubresource, false);
    }

    if (likely(convertFormat.FormatType == D3D9ConversionFormat_None)) {
      // The texture does not use a format that needs to be converted in a compute shader.
      // So we just need to make sure the passed size and offset are not out of range and properly aligned,
      // copy the data to a staging buffer and then copy that on the GPU to the actual image.
      VkOffset3D alignedDestOffset = {
        int32_t(alignDown(DestOffset.x, formatInfo->blockSize.width)),
        int32_t(alignDown(DestOffset.y, formatInfo->blockSize.height)),
        int32_t(alignDown(DestOffset.z, formatInfo->blockSize.depth))
      };
      VkOffset3D alignedSrcOffset = {
        int32_t(alignDown(SrcOffset.x, formatInfo->blockSize.width)),
        int32_t(alignDown(SrcOffset.y, formatInfo->blockSize.height)),
        int32_t(alignDown(SrcOffset.z, formatInfo->blockSize.depth))
      };
      SrcExtent.width += SrcOffset.x - alignedSrcOffset.x;
      SrcExtent.height += SrcOffset.y - alignedSrcOffset.y;
      SrcExtent.depth += SrcOffset.z - alignedSrcOffset.z;
      VkExtent3D extentBlockCount = util::computeBlockCount(SrcExtent, formatInfo->blockSize);
      VkExtent3D alignedExtent = util::computeBlockExtent(extentBlockCount, formatInfo->blockSize);

      alignedExtent = util::snapExtent3D(alignedDestOffset, alignedExtent, dstTexLevelExtent);
      alignedExtent = util::snapExtent3D(alignedSrcOffset, alignedExtent, srcTexLevelExtent);

      VkOffset3D srcOffsetBlockCount = util::computeBlockOffset(alignedSrcOffset, formatInfo->blockSize);
      VkExtent3D srcTexLevelExtentBlockCount = util::computeBlockCount(srcTexLevelExtent, formatInfo->blockSize);
      VkDeviceSize pitch = align(srcTexLevelExtentBlockCount.width * formatInfo->elementSize, 4);
      VkDeviceSize copySrcOffset = srcOffsetBlockCount.z * srcTexLevelExtentBlockCount.height * pitch
          + srcOffsetBlockCount.y * pitch
          + srcOffsetBlockCount.x * formatInfo->elementSize;

      // Get the mapping pointer from MapTexture to map the texture and keep track of that
      // in case it is unmappable.
      const void* mapPtr = MapTexture(pSrcTexture, SrcSubresource);
      VkDeviceSize dirtySize = extentBlockCount.width * extentBlockCount.height * extentBlockCount.depth * formatInfo->elementSize;
      D3D9BufferSlice slice = AllocStagingBuffer(dirtySize);
      const void* srcData = reinterpret_cast<const uint8_t*>(mapPtr) + copySrcOffset;
      util::packImageData(
        slice.mapPtr, srcData, extentBlockCount, formatInfo->elementSize,
        pitch, pitch * srcTexLevelExtentBlockCount.height);

      VkFormat packedDSFormat = GetPackedDepthStencilFormat(pDestTexture->Desc()->Format);

      EmitCs([
        cSrcSlice       = slice.slice,
        cDstImage       = image,
        cDstLayers      = dstLayers,
        cDstLevelExtent = alignedExtent,
        cOffset         = alignedDestOffset,
        cPackedDSFormat = packedDSFormat
      ] (DxvkContext* ctx) {
        ctx->copyBufferToImage(
          cDstImage,  cDstLayers,
          cOffset, cDstLevelExtent,
          cSrcSlice.buffer(), cSrcSlice.offset(),
          0, 0, cPackedDSFormat);
      });

      TrackTextureMappingBufferSequenceNumber(pSrcTexture, SrcSubresource);
    }
    else {
      // The texture uses a format which gets converted by a compute shader.
      const void* mapPtr = MapTexture(pSrcTexture, SrcSubresource);

      // The compute shader does not support only converting a subrect of the texture
      if (unlikely(SrcOffset.x != 0 || SrcOffset.y != 0 || SrcOffset.z != 0
        || DestOffset.x != 0 || DestOffset.y != 0 || DestOffset.z != 0
        || SrcExtent != srcTexLevelExtent)) {
        Logger::warn("Offset and rect not supported with the texture converter.");
      }

      if (unlikely(srcTexLevelExtent != dstTexLevelExtent)) {
        Logger::err("Different extents are not supported with the texture converter.");
        return;
      }

      uint32_t formatElementSize = formatInfo->elementSize;
      VkExtent3D srcBlockSize = formatInfo->blockSize;
      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
        formatElementSize = formatInfo->planes[0].elementSize;
        srcBlockSize = { formatInfo->planes[0].blockSize.width, formatInfo->planes[0].blockSize.height, 1u };
      }
      VkExtent3D srcBlockCount = util::computeBlockCount(srcTexLevelExtent, srcBlockSize);
      srcBlockCount.height *= std::min(pSrcTexture->GetPlaneCount(), 2u);

      // the converter can not handle the 4 aligned pitch so we always repack into a staging buffer
      D3D9BufferSlice slice = AllocStagingBuffer(pSrcTexture->GetMipSize(SrcSubresource));
      VkDeviceSize pitch = align(srcBlockCount.width * formatElementSize, 4);

      const DxvkFormatInfo* convertedFormatInfo = lookupFormatInfo(convertFormat.FormatColor);
      VkImageSubresourceLayers convertedDstLayers = { convertedFormatInfo->aspectMask, dstSubresource.mipLevel, dstSubresource.arrayLayer, 1 };

      util::packImageData(
        slice.mapPtr, mapPtr, srcBlockCount, formatElementSize,
        pitch, std::min(pSrcTexture->GetPlaneCount(), 2u) * pitch * srcBlockCount.height);

      EmitCs([this,
        cConvertFormat    = convertFormat,
        cDstImage         = std::move(image),
        cDstLayers        = convertedDstLayers,
        cSrcSlice         = std::move(slice.slice)
      ] (DxvkContext* ctx) {
        auto contextObjects = ctx->beginExternalRendering();

        m_converter->ConvertFormat(contextObjects,
          cConvertFormat, cDstImage, cDstLayers, cSrcSlice);
      });
    }
    UnmapTextures();
    ConsiderFlush(GpuFlushType::ImplicitWeakHint);
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

    auto& desc = *pResource->Desc();

    // Ignore DISCARD if NOOVERWRITE is set
    if (unlikely((Flags & (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)) == (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)))
      Flags &= ~D3DLOCK_DISCARD;

    // Ignore DISCARD and NOOVERWRITE if the buffer is not DEFAULT pool (tests + Halo 2)
    // The docs say DISCARD and NOOVERWRITE are ignored if the buffer is not DYNAMIC
    // but tests say otherwise!
    if (desc.Pool != D3DPOOL_DEFAULT || CanOnlySWVP())
      Flags &= ~(D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE);

    // Ignore DONOTWAIT if we are DYNAMIC
    // Yes... D3D9 is a good API.
    if (desc.Usage & D3DUSAGE_DYNAMIC)
      Flags &= ~D3DLOCK_DONOTWAIT;

    // Tests show that D3D9 drivers ignore DISCARD when the device is lost.
    if (unlikely(m_deviceLostState != D3D9DeviceLostState::Ok))
      Flags &= ~D3DLOCK_DISCARD;

    // In SWVP mode, we always use the per-draw upload path.
    // So the buffer will never be in use on the device.
    // FVF Buffers are the exception. Those can be used as a destination for ProcessVertices.
    if (unlikely(CanOnlySWVP() && !pResource->NeedsReadback()))
      Flags |= D3DLOCK_NOOVERWRITE;

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

    bool updateDirtyRange = (desc.Pool == D3DPOOL_DEFAULT || !(Flags & D3DLOCK_NO_DIRTY_UPDATE)) && !(Flags & D3DLOCK_READONLY);
    if (updateDirtyRange) {
      pResource->DirtyRange().Conjoin(lockRange);

      for (uint32_t i : bit::BitMask(m_activeVertexBuffers)) {
        auto commonBuffer = GetCommonBuffer(m_state.vertexBuffers[i].vertexBuffer);
        if (commonBuffer == pResource) {
          m_activeVertexBuffersToUpload |= 1 << i;
        }
      }
    }

    const bool directMapping = pResource->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_DIRECT;
    const bool needsReadback = pResource->NeedsReadback();

    uint8_t* data = nullptr;

    if ((Flags & D3DLOCK_DISCARD) && (directMapping || needsReadback)) {
      // If we're not directly mapped and don't need readback,
      // the buffer is not currently getting used anyway
      // so there's no reason to waste memory by discarding.

      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.
      Rc<DxvkBuffer> mappingBuffer = pResource->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>();
      auto bufferSlice = pResource->DiscardMapSlice();
      data = reinterpret_cast<uint8_t*>(bufferSlice->mapPtr());

      EmitCs([
        cBuffer      = std::move(mappingBuffer),
        cBufferSlice = std::move(bufferSlice)
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cBufferSlice));
      });

      pResource->SetNeedsReadback(false);
    }
    else {
      // The application either didn't specify DISCARD or the buffer is guaranteed to be idle anyway.

      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3DLOCK_NOOVERWRITE.
      data = reinterpret_cast<uint8_t*>(pResource->GetMappedSlice()->mapPtr());

      const bool readOnly = Flags & D3DLOCK_READONLY;
      // NOOVERWRITE promises that they will not write in a currently used area.
      const bool noOverwrite = Flags & D3DLOCK_NOOVERWRITE;
      const bool directMapping = pResource->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_DIRECT;

      // If we're not directly mapped, we can rely on needsReadback to tell us if a sync is required.
      const bool skipWait = (!needsReadback && (readOnly || !directMapping)) || noOverwrite;

      if (!skipWait) {
        const Rc<DxvkBuffer> mappingBuffer = pResource->GetBuffer<D3D9_COMMON_BUFFER_TYPE_MAPPING>();
        if (!WaitForResource(*mappingBuffer, pResource->GetMappingBufferSequenceNumber(), Flags))
          return D3DERR_WASSTILLDRAWING;

        pResource->SetNeedsReadback(false);
      }
    }

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

    // We just mapped a buffer which may have come with an address space cost.
    // Unmap textures if the amount of mapped texture memory is exceeding the threshold.
    UnmapTextures();

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::FlushBuffer(
        D3D9CommonBuffer*       pResource) {
    // Wait until the amount of used staging memory is under a certain threshold to avoid using
    // too much memory and even more so to avoid using too much address space.
    WaitStagingBuffer();

    auto dstBuffer = pResource->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>();
    auto srcSlice = pResource->GetMappedSlice();

    D3D9Range& range = pResource->DirtyRange();

    D3D9BufferSlice slice = AllocStagingBuffer(range.max - range.min);
    void* srcData = reinterpret_cast<uint8_t*>(srcSlice->mapPtr()) + range.min;
    memcpy(slice.mapPtr, srcData, range.max - range.min);

    EmitCs([
      cDstSlice  = dstBuffer,
      cSrcSlice  = slice.slice,
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

    pResource->DirtyRange().Clear();
    TrackBufferMappingBufferSequenceNumber(pResource);

    UnmapTextures();
    ConsiderFlush(GpuFlushType::ImplicitWeakHint);
    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::UnlockBuffer(
        D3D9CommonBuffer*       pResource) {
    D3D9DeviceLock lock = LockDevice();

    if (pResource->DecrementLockCount() != 0)
      return D3D_OK;

    // Nothing else to do for directly mapped buffers. Those were already written.
    if (pResource->GetMapMode() != D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      return D3D_OK;

    // There is no part of the buffer that hasn't been uploaded yet.
    // This shouldn't happen.
    if (pResource->DirtyRange().IsDegenerate())
      return D3D_OK;

    pResource->SetMapFlags(0);

    // Only D3DPOOL_DEFAULT buffers get uploaded in UnlockBuffer.
    // D3DPOOL_SYSTEMMEM and D3DPOOL_MANAGED get uploaded at draw time.
    if (pResource->Desc()->Pool != D3DPOOL_DEFAULT)
      return D3D_OK;

    FlushBuffer(pResource);

    return D3D_OK;
  }



  void D3D9DeviceEx::UploadPerDrawData(
          UINT&                   FirstVertexIndex,
          UINT                    NumVertices,
          UINT&                   FirstIndex,
          UINT                    NumIndices,
          INT&                    BaseVertexIndex,
          bool*                   pDynamicVBOs,
          bool*                   pDynamicIBO
  ) {
    const uint32_t usedBuffersMask = (m_state.vertexDecl != nullptr ? m_state.vertexDecl->GetStreamMask() : ~0u) & m_activeVertexBuffers;
    bool dynamicSysmemVBOs = usedBuffersMask == m_activeVertexBuffersToUploadPerDraw;

    D3D9CommonBuffer* ibo = GetCommonBuffer(m_state.indices);
    bool dynamicSysmemIBO = NumIndices != 0 && ibo != nullptr && (ibo->DoPerDrawUpload() || CanOnlySWVP());

    *pDynamicVBOs = dynamicSysmemVBOs;

    if (unlikely(pDynamicIBO))
      *pDynamicIBO = dynamicSysmemIBO;

    if (likely(!dynamicSysmemVBOs && !dynamicSysmemIBO))
      return;

    uint32_t vertexBuffersToUpload;
    if (likely(dynamicSysmemVBOs))
      vertexBuffersToUpload = m_activeVertexBuffersToUploadPerDraw & usedBuffersMask;
    else
      vertexBuffersToUpload = 0;

    // The UP buffer allocator will invalidate,
    // so we can only use 1 UP buffer slice per draw.
    // First we calculate the size of that UP buffer slice
    // and store all sizes and offsets into it.

    struct VBOCopy {
      uint32_t srcOffset;
      uint32_t dstOffset;
      uint32_t copyBufferLength;
      uint32_t copyElementCount;
      uint32_t copyElementSize;
      uint32_t copyElementStride;
    };
    uint32_t totalUpBufferSize = 0;
    std::array<VBOCopy, caps::MaxStreams> vboCopies = {};

    for (uint32_t i : bit::BitMask(vertexBuffersToUpload)) {
      auto* vbo = GetCommonBuffer(m_state.vertexBuffers[i].vertexBuffer);
      if (likely(vbo == nullptr)) {
        continue;
      }

      if (unlikely(vbo->NeedsReadback())) {
        // There's only one way the GPU might write new data to a vertex buffer:
        // - Write to the primary buffer using ProcessVertices which gets copied over to the staging buffer at the end.
        //   So it could end up writing to the buffer on the GPU while the same buffer gets read here on the CPU.
        //   That is why we need to ensure the staging buffer is idle here.
        WaitForResource(*vbo->GetBuffer<D3D9_COMMON_BUFFER_TYPE_STAGING>(), vbo->GetMappingBufferSequenceNumber(), D3DLOCK_READONLY);
      }

      const uint32_t vertexSize = m_state.vertexDecl->GetSize(i);
      const uint32_t vertexStride = m_state.vertexBuffers[i].stride;
      const uint32_t srcStride = vertexStride;
      const uint32_t dstStride = std::min(vertexStride, vertexSize);

      uint32_t elementCount = NumVertices;
      if (m_state.streamFreq[i] & D3DSTREAMSOURCE_INSTANCEDATA) {
        elementCount = GetInstanceCount();
      }
      const uint32_t vboOffset = m_state.vertexBuffers[i].offset;
      const uint32_t vertexOffset = (FirstVertexIndex + BaseVertexIndex) * srcStride;
      const uint32_t vertexBufferSize = vbo->Desc()->Size;
      const uint32_t srcOffset = vboOffset + vertexOffset;

      if (unlikely(srcOffset > vertexBufferSize)) {
        // All vertices are out of bounds
        vboCopies[i].copyBufferLength = 0;
      } else if (unlikely(srcOffset + elementCount * srcStride > vertexBufferSize)) {
        // Some vertices are (partially) out of bounds
        uint32_t boundVertexBufferRange = vertexBufferSize - vboOffset;
        elementCount = boundVertexBufferRange / srcStride;
        // Copy all complete vertices
        vboCopies[i].copyBufferLength = elementCount * dstStride;
        // Copy the remaining partial vertex
        vboCopies[i].copyBufferLength += std::min(dstStride, boundVertexBufferRange % srcStride);
      } else {
        // No vertices are out of bounds
        vboCopies[i].copyBufferLength = elementCount * dstStride;
      }

      vboCopies[i].copyElementCount = elementCount;
      vboCopies[i].copyElementStride = srcStride;
      vboCopies[i].copyElementSize = dstStride;
      vboCopies[i].srcOffset = srcOffset;
      vboCopies[i].dstOffset = totalUpBufferSize;
      totalUpBufferSize += vboCopies[i].copyBufferLength;
    }

    uint32_t iboUPBufferSize = 0;
    uint32_t iboUPBufferOffset = 0;
    if (dynamicSysmemIBO) {
      auto* ibo = GetCommonBuffer(m_state.indices);
      if (likely(ibo != nullptr)) {
        uint32_t indexStride = ibo->Desc()->Format == D3D9Format::INDEX16 ? 2 : 4;
        uint32_t offset = indexStride * FirstIndex;
        uint32_t indexBufferSize = ibo->Desc()->Size;
        if (offset < indexBufferSize) {
          iboUPBufferSize = std::min(NumIndices * indexStride, indexBufferSize - offset);
          iboUPBufferOffset = totalUpBufferSize;
          totalUpBufferSize += iboUPBufferSize;
        }
      }
    }

    if (unlikely(totalUpBufferSize == 0)) {
      *pDynamicVBOs = false;
      if (pDynamicIBO)
        *pDynamicIBO = false;

      return;
    }

    auto upSlice = AllocUPBuffer(totalUpBufferSize);

    // Now copy the actual data and bind it.
    if (dynamicSysmemVBOs) {
      for (uint32_t i : bit::BitMask(vertexBuffersToUpload)) {
        const VBOCopy& copy = vboCopies[i];

        if (likely(copy.copyBufferLength != 0)) {
          const auto* vbo = GetCommonBuffer(m_state.vertexBuffers[i].vertexBuffer);
          uint8_t* data = reinterpret_cast<uint8_t*>(upSlice.mapPtr) + copy.dstOffset;
          const uint8_t* src = reinterpret_cast<uint8_t*>(vbo->GetMappedSlice()->mapPtr()) + copy.srcOffset;

          if (likely(copy.copyElementStride == copy.copyElementSize)) {
            std::memcpy(data, src, copy.copyBufferLength);
          } else {
            for (uint32_t j = 0; j < copy.copyElementCount; j++) {
              std::memcpy(data + j * copy.copyElementSize, src + j * copy.copyElementStride, copy.copyElementSize);
            }
            if (unlikely(copy.copyBufferLength > copy.copyElementCount * copy.copyElementSize)) {
              // Partial vertex at the end
              std::memcpy(
                data + copy.copyElementCount * copy.copyElementSize,
                src + copy.copyElementCount * copy.copyElementStride,
                copy.copyBufferLength - copy.copyElementCount * copy.copyElementSize);
            }
          }
        }

        auto vboSlice = upSlice.slice.subSlice(copy.dstOffset, copy.copyBufferLength);
        EmitCs([
          cStream      = i,
          cBufferSlice = std::move(vboSlice),
          cStride      = copy.copyElementSize
        ](DxvkContext* ctx) mutable {
          ctx->bindVertexBuffer(cStream, std::move(cBufferSlice), cStride);
        });
        m_flags.set(D3D9DeviceFlag::DirtyVertexBuffers);
      }

      // Change the draw call parameters to reflect the changed vertex buffers
      if (NumIndices != 0) {
        BaseVertexIndex = -FirstVertexIndex;
      } else {
        FirstVertexIndex = 0;
      }
    }

    if (dynamicSysmemIBO) {
      if (unlikely(iboUPBufferSize == 0)) {
        EmitCs([](DxvkContext* ctx) {
          ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);
        });
        m_flags.set(D3D9DeviceFlag::DirtyIndexBuffer);
      } else {
        auto* ibo = GetCommonBuffer(m_state.indices);
        uint32_t indexStride = ibo->Desc()->Format == D3D9Format::INDEX16 ? 2 : 4;
        VkIndexType indexType = DecodeIndexType(ibo->Desc()->Format);
        uint32_t offset = indexStride * FirstIndex;
        uint8_t* data = reinterpret_cast<uint8_t*>(upSlice.mapPtr) + iboUPBufferOffset;
        uint8_t* src = reinterpret_cast<uint8_t*>(ibo->GetMappedSlice()->mapPtr()) + offset;
        std::memcpy(data, src, iboUPBufferSize);

        auto iboSlice = upSlice.slice.subSlice(iboUPBufferOffset, iboUPBufferSize);
        EmitCs([
          cBufferSlice = std::move(iboSlice),
          cIndexType = indexType
        ](DxvkContext* ctx) mutable {
          ctx->bindIndexBuffer(std::move(cBufferSlice), cIndexType);
        });
        m_flags.set(D3D9DeviceFlag::DirtyIndexBuffer);
      }

      // Change the draw call parameters to reflect the changed index buffer
      FirstIndex = 0;
    }
  }


  void D3D9DeviceEx::InjectCsChunk(
          DxvkCsChunkRef&&            Chunk,
          bool                        Synchronize) {
    m_csThread.injectChunk(DxvkCsQueue::HighPriority, std::move(Chunk), Synchronize);
  }


  void D3D9DeviceEx::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    // Flush init commands so that the CS thread
    // can processe them before the first use.
    m_initializer->FlushCsChunk();

    m_csSeqNum = m_csThread.dispatchChunk(std::move(chunk));
  }


  void D3D9DeviceEx::ConsiderFlush(GpuFlushType FlushType) {
    uint64_t chunkId = GetCurrentSequenceNumber();
    uint64_t submissionId = m_submissionFence->value();

    if (m_flushTracker.considerFlush(FlushType, chunkId, submissionId))
      Flush();
  }


  void D3D9DeviceEx::SynchronizeCsThread(uint64_t SequenceNumber) {
    D3D9DeviceLock lock = LockDevice();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    if (SequenceNumber > m_csSeqNum)
      FlushCsChunk();

    m_csThread.synchronize(SequenceNumber);
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
#elif (defined(__GNUC__) || defined(__MINGW32__)) && (defined(__i386__) || (defined(__x86_64__) && !defined(__arm64ec__)) || defined(__ia64))
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


  void D3D9DeviceEx::CreateConstantBuffers() {
    constexpr VkDeviceSize DefaultConstantBufferSize  = 1024ull << 10;
    constexpr VkDeviceSize SmallConstantBufferSize    =   64ull << 10;

    m_consts[DxsoProgramTypes::VertexShader].buffer = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSConstantBuffer,
      DefaultConstantBufferSize);

    m_consts[DxsoProgramTypes::VertexShader].swvp.intBuffer = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSIntConstantBuffer,
      SmallConstantBufferSize);

    m_consts[DxsoProgramTypes::VertexShader].swvp.boolBuffer = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSBoolConstantBuffer,
      SmallConstantBufferSize);

    m_consts[DxsoProgramTypes::PixelShader].buffer = D3D9ConstantBuffer(this,
      DxsoProgramType::PixelShader,
      DxsoConstantBuffers::PSConstantBuffer,
      DefaultConstantBufferSize);

    m_vsClipPlanes = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSClipPlanes,
      caps::MaxClipPlanes * sizeof(D3D9ClipPlane));

    m_vsFixedFunction = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSFixedFunction,
      sizeof(D3D9FixedFunctionVS));

    m_psFixedFunction = D3D9ConstantBuffer(this,
      DxsoProgramType::PixelShader,
      DxsoConstantBuffers::PSFixedFunction,
      sizeof(D3D9FixedFunctionPS));

    m_psShared = D3D9ConstantBuffer(this,
      DxsoProgramType::PixelShader,
      DxsoConstantBuffers::PSShared,
      sizeof(D3D9SharedPS));

    m_vsVertexBlend = D3D9ConstantBuffer(this,
      DxsoProgramType::VertexShader,
      DxsoConstantBuffers::VSVertexBlendData,
      CanSWVP()
        ? sizeof(D3D9FixedFunctionVertexBlendDataSW)
        : sizeof(D3D9FixedFunctionVertexBlendDataHW));

    // Allocate constant buffer for values that would otherwise get passed as spec constants for fast-linked pipelines to use.
    if (m_usingGraphicsPipelines) {
      m_specBuffer = D3D9ConstantBuffer(this,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        getSpecConstantBufferSlot(),
        D3D9SpecializationInfo::UBOSize);
    }
  }


  inline void D3D9DeviceEx::UploadSoftwareConstantSet(const D3D9ShaderConstantsVSSoftware& Src, const D3D9ConstantLayout& Layout) {
    /*
     * SWVP raises the amount of constants by a lot.
     * To avoid copying huge amounts of data for every draw call,
     * we track the highest set constant and only use a buffer big enough
     * to fit that. We rely on robustness to return 0 for OOB reads.
    */

    D3D9ConstantSets& constSet = m_consts[DxsoProgramType::VertexShader];

    if (!constSet.dirty)
      return;

    constSet.dirty = false;

    uint32_t floatCount = constSet.maxChangedConstF;
    if (constSet.meta.needsConstantCopies) {
      // If the shader requires us to preserve shader defined constants,
      // we copy those over. We need to adjust the amount of used floats accordingly.
      auto shader = GetCommonShader(m_state.vertexShader);
      floatCount = std::max(floatCount, shader->GetMaxDefinedConstant() + 1);
    }
    // If we statically know which is the last float constant accessed by the shader, we don't need to copy the rest.
    floatCount = std::min(floatCount, constSet.meta.maxConstIndexF);

    // Calculate data sizes for each constant type.
    const uint32_t floatDataSize = floatCount * sizeof(Vector4);
    const uint32_t intDataSize   = std::min(constSet.meta.maxConstIndexI, constSet.maxChangedConstI) * sizeof(Vector4i);
    const uint32_t boolDataSize  = divCeil(std::min(constSet.meta.maxConstIndexB, constSet.maxChangedConstB), 32u) * uint32_t(sizeof(uint32_t));

    // Max copy source size is 8192 * 16 => always aligned to any plausible value
    // => we won't copy out of bounds
    if (likely(constSet.meta.maxConstIndexF != 0)) {
      auto mapPtr = CopySoftwareConstants(constSet.buffer, Src.fConsts, floatDataSize);

      if (constSet.meta.needsConstantCopies) {
        // Copy shader defined constants over so they can be accessed
        // with relative addressing.
        Vector4* data = reinterpret_cast<Vector4*>(mapPtr);

        auto& shaderConsts = GetCommonShader(m_state.vertexShader)->GetConstants();

        for (const auto& constant : shaderConsts) {
          if (constant.uboIdx < constSet.meta.maxConstIndexF)
            data[constant.uboIdx] = *reinterpret_cast<const Vector4*>(constant.float32);
        }
      }
    }

    // Max copy source size is 2048 * 16 => always aligned to any plausible value
    // => we won't copy out of bounds
    if (likely(constSet.meta.maxConstIndexI != 0))
      CopySoftwareConstants(constSet.swvp.intBuffer, Src.iConsts, intDataSize);

    if (likely(constSet.meta.maxConstIndexB != 0))
      CopySoftwareConstants(constSet.swvp.boolBuffer, Src.bConsts, boolDataSize);
  }


  inline void* D3D9DeviceEx::CopySoftwareConstants(D3D9ConstantBuffer& dstBuffer, const void* src, uint32_t size) {
    uint32_t alignment = dstBuffer.GetAlignment();
    size = std::max(size, alignment);
    size = align(size, alignment);

    auto mapPtr = dstBuffer.Alloc(size);
    std::memcpy(mapPtr, src, size);
    return mapPtr;
  }


  template <DxsoProgramType ShaderStage, typename HardwareLayoutType, typename SoftwareLayoutType, typename ShaderType>
  inline void D3D9DeviceEx::UploadConstantSet(const SoftwareLayoutType& Src, const D3D9ConstantLayout& Layout, const ShaderType& Shader) {
    /*
     * We just copy the float constants that have been set by the application and rely on robustness
     * to return 0 on OOB reads.
    */
    D3D9ConstantSets& constSet = m_consts[ShaderStage];

    if (!constSet.dirty)
      return;

    constSet.dirty = false;

    uint32_t floatCount = constSet.maxChangedConstF;
    if (constSet.meta.needsConstantCopies) {
      // If the shader requires us to preserve shader defined constants,
      // we copy those over. We need to adjust the amount of used floats accordingly.
      auto shader = GetCommonShader(Shader);
      floatCount = std::max(floatCount, shader->GetMaxDefinedConstant() + 1);
    }
    // If we statically know which is the last float constant accessed by the shader, we don't need to copy the rest.
    floatCount = std::min(constSet.meta.maxConstIndexF, floatCount);

    // There are very few int constants, so we put those into the same buffer at the start.
    // We always allocate memory for all possible int constants to make sure alignment works out.
    const uint32_t intRange = caps::MaxOtherConstants * sizeof(Vector4i);
    uint32_t floatDataSize = floatCount * sizeof(Vector4);
    // Determine amount of floats and buffer size based on highest used float constant and alignment
    const uint32_t alignment = constSet.buffer.GetAlignment();
    const uint32_t bufferSize = align(std::max(floatDataSize + intRange, alignment), alignment);
    floatDataSize = bufferSize - intRange;

    void* mapPtr = constSet.buffer.Alloc(bufferSize);
    auto* dst = reinterpret_cast<HardwareLayoutType*>(mapPtr);

    const uint32_t intDataSize = constSet.meta.maxConstIndexI * sizeof(Vector4i);
    if (constSet.meta.maxConstIndexI != 0)
      std::memcpy(dst->iConsts, Src.iConsts, intDataSize);
    if (constSet.meta.maxConstIndexF != 0)
      std::memcpy(dst->fConsts, Src.fConsts, floatDataSize);

    if (constSet.meta.needsConstantCopies) {
      // Copy shader defined constants over so they can be accessed
      // with relative addressing.
      Vector4* data = reinterpret_cast<Vector4*>(dst->fConsts);

      auto& shaderConsts = GetCommonShader(Shader)->GetConstants();

      for (const auto& constant : shaderConsts) {
        if (constant.uboIdx < constSet.meta.maxConstIndexF)
          data[constant.uboIdx] = *reinterpret_cast<const Vector4*>(constant.float32);
      }
    }
  }


  template <DxsoProgramType ShaderStage>
  void D3D9DeviceEx::UploadConstants() {
    if constexpr (ShaderStage == DxsoProgramTypes::VertexShader) {
      if (CanSWVP())
        return UploadSoftwareConstantSet(m_state.vsConsts.get(), m_consts[ShaderStage].layout);
      else
        return UploadConstantSet<ShaderStage, D3D9ShaderConstantsVSHardware>(m_state.vsConsts.get(), m_consts[ShaderStage].layout, m_state.vertexShader);
    } else {
      return UploadConstantSet<ShaderStage, D3D9ShaderConstantsPS>          (m_state.psConsts.get(), m_consts[ShaderStage].layout, m_state.pixelShader);
    }
  }


  void D3D9DeviceEx::UpdateClipPlanes() {
    m_flags.clr(D3D9DeviceFlag::DirtyClipPlanes);

    auto mapPtr = m_vsClipPlanes.AllocSlice();
    auto dst = reinterpret_cast<D3D9ClipPlane*>(mapPtr);

    uint32_t clipPlaneCount = 0u;
    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      D3D9ClipPlane clipPlane = (m_state.renderStates[D3DRS_CLIPPLANEENABLE] & (1 << i))
        ? m_state.clipPlanes[i]
        : D3D9ClipPlane();

      if (clipPlane != D3D9ClipPlane())
        dst[clipPlaneCount++] = clipPlane;
    }

    // Write the rest to 0 for GPL.
    for (uint32_t i = clipPlaneCount; i < caps::MaxClipPlanes; i++)
      dst[i] = D3D9ClipPlane();

    if (m_specInfo.set<SpecClipPlaneCount>(clipPlaneCount))
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  template <uint32_t Offset, uint32_t Length>
  void D3D9DeviceEx::UpdatePushConstant(const void* pData) {
    struct ConstantData { uint8_t Data[Length]; };

    const ConstantData* constData = reinterpret_cast<const ConstantData*>(pData);

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
      uint32_t alpha = rs[D3DRS_ALPHAREF] & 0xFF;
      UpdatePushConstant<offsetof(D3D9RenderStateInfo, alphaRef), sizeof(uint32_t)>(&alpha);
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


  template <bool Synchronize9On12>
  void D3D9DeviceEx::ExecuteFlush() {
    D3D9DeviceLock lock = LockDevice();

    if constexpr (Synchronize9On12)
      m_submitStatus.result = VK_NOT_READY;

    // Update signaled staging buffer counter and signal the fence
    m_stagingMemorySignaled = m_stagingBuffer.getStatistics().allocatedTotal;

    // Add commands to flush the threaded
    // context, then flush the command list
    uint64_t submissionId = ++m_submissionId;

    EmitCs<false>([
      cSubmissionFence  = m_submissionFence,
      cSubmissionId     = submissionId,
      cSubmissionStatus = Synchronize9On12 ? &m_submitStatus : nullptr,
      cStagingBufferFence = m_stagingBufferFence,
      cStagingBufferAllocated = m_stagingMemorySignaled
    ] (DxvkContext* ctx) {
      ctx->signal(cSubmissionFence, cSubmissionId);
      ctx->signal(cStagingBufferFence, cStagingBufferAllocated);
      ctx->flushCommandList(nullptr, cSubmissionStatus);
    });

    FlushCsChunk();

    m_flushSeqNum = m_csSeqNum;
    m_flushTracker.notifyFlush(m_flushSeqNum, submissionId);

    // If necessary, block calling thread until the
    // Vulkan queue submission is performed.
    if constexpr (Synchronize9On12)
      m_dxvkDevice->waitForSubmission(&m_submitStatus);

    // Notify the device that the context has been flushed,
    // this resets some resource initialization heuristics.
    m_initializer->NotifyContextFlush();
  }


  void D3D9DeviceEx::Flush() {
    ExecuteFlush<false>();
  }


  void D3D9DeviceEx::FlushAndSync9On12() {
    ExecuteFlush<true>();
  }


  void D3D9DeviceEx::BeginFrame(Rc<DxvkLatencyTracker> LatencyTracker, uint64_t FrameId) {
    D3D9DeviceLock lock = LockDevice();

    EmitCs<false>([
      cTracker = std::move(LatencyTracker),
      cFrameId = FrameId
    ] (DxvkContext* ctx) {
      if (cTracker && cTracker->needsAutoMarkers())
        ctx->beginLatencyTracking(cTracker, cFrameId);
    });
  }


  void D3D9DeviceEx::EndFrame(Rc<DxvkLatencyTracker> LatencyTracker) {
    D3D9DeviceLock lock = LockDevice();

    EmitCs<false>([
      cTracker = std::move(LatencyTracker)
    ] (DxvkContext* ctx) {
      ctx->endFrame();

      if (cTracker && cTracker->needsAutoMarkers())
        ctx->endLatencyTracking(cTracker);
    });
  }


  inline void D3D9DeviceEx::UpdateActiveRTs(uint32_t index) {
    const uint32_t bit = 1 << index;

    m_activeRTsWhichAreTextures &= ~bit;

    if (HasRenderTargetBound(index) &&
        m_state.renderTargets[index]->GetBaseTexture() != nullptr &&
        m_state.renderStates[ColorWriteIndex(index)] != 0)
      m_activeRTsWhichAreTextures |= bit;

    UpdateActiveHazardsRT(bit);
  }

  template <uint32_t Index>
  inline void D3D9DeviceEx::UpdateAnyColorWrites() {
    // The 0th RT is always bound.
    bool bound = HasRenderTargetBound(Index);
    if (Index == 0 || bound) {
      if (bound) {
        m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      }

      UpdateActiveRTs(Index);
    }
  }


  inline void D3D9DeviceEx::UpdateTextureBitmasks(uint32_t index, DWORD combinedUsage) {
    const uint32_t bit = 1 << index;

    m_activeTextureRTs        &= ~bit;
    m_activeTextureDSs        &= ~bit;
    m_activeTextures          &= ~bit;
    m_activeTexturesToUpload  &= ~bit;
    m_activeTexturesToGen     &= ~bit;
    m_mismatchingTextureTypes &= ~bit;

    auto tex = GetCommonTexture(m_state.textures[index]);
    if (tex != nullptr) {
      m_activeTextures |= bit;

      if (unlikely(tex->IsRenderTarget()))
        m_activeTextureRTs |= bit;

      if (unlikely(tex->IsDepthStencil()))
        m_activeTextureDSs |= bit;

      if (unlikely(tex->NeedsAnyUpload()))
        m_activeTexturesToUpload |= bit;

      if (unlikely(tex->NeedsMipGen()))
        m_activeTexturesToGen |= bit;

      // Update shadow sampler mask
      const bool oldDepth = m_depthTextures & bit;
      const bool newDepth = tex->IsShadow();

      if (oldDepth != newDepth) {
        m_depthTextures ^= bit;
        m_dirtySamplerStates |= bit;
      }

      // Update dref clamp mask
      m_drefClamp &= ~bit;
      m_drefClamp |= uint32_t(tex->IsUpgradedToD32f()) << index;

      // Update non-seamless cubemap mask
      const bool oldCube = m_cubeTextures & bit;
      const bool newCube = tex->GetType() == D3DRTYPE_CUBETEXTURE;
      if (oldCube != newCube) {
        m_cubeTextures ^= bit;
        m_dirtySamplerStates |= bit;
      }

      if (unlikely(m_fetch4Enabled & bit))
        UpdateActiveFetch4(index);

      UpdateTextureTypeMismatchesForTexture(index);
    } else {
      if (unlikely(m_fetch4 & bit))
        UpdateActiveFetch4(index);
    }

    if (unlikely(combinedUsage & D3DUSAGE_RENDERTARGET))
      UpdateActiveHazardsRT(bit);

    if (unlikely(combinedUsage & D3DUSAGE_DEPTHSTENCIL))
      UpdateActiveHazardsDS(bit);
  }


  inline void D3D9DeviceEx::UpdateActiveHazardsRT(uint32_t texMask) {
    auto masks = m_psShaderMasks;
    masks.rtMask      &= m_activeRTsWhichAreTextures;
    masks.samplerMask &= m_activeTextureRTs & texMask;

    m_activeHazardsRT = m_activeHazardsRT & (~texMask);
    for (uint32_t rtIdx : bit::BitMask(masks.rtMask)) {
      for (uint32_t samplerIdx : bit::BitMask(masks.samplerMask)) {
        D3D9Surface* rtSurf = m_state.renderTargets[rtIdx].ptr();

        IDirect3DBaseTexture9* rtBase  = rtSurf->GetBaseTexture();
        IDirect3DBaseTexture9* texBase = m_state.textures[samplerIdx];

        // HACK: Don't mark for hazards if we aren't rendering to mip 0!
        // Some games use screenspace passes like this for blurring
        // Sampling from mip 0 (texture) -> mip 1 (rt)
        // and we'd trigger the hazard path otherwise which is unnecessary,
        // and would shove us into GENERAL and emitting readback barriers.
        if (likely(rtSurf->GetMipLevel() != 0 || rtBase != texBase))
          continue;

        m_activeHazardsRT |= 1 << samplerIdx;
      }
    }
  }


  inline void D3D9DeviceEx::UpdateActiveHazardsDS(uint32_t texMask) {
    auto masks = m_psShaderMasks;
    masks.samplerMask &= m_activeTextureDSs & texMask;

    m_activeHazardsDS = m_activeHazardsDS & (~texMask);
    if (m_state.depthStencil != nullptr &&
        m_state.depthStencil->GetBaseTexture() != nullptr) {
      for (uint32_t samplerIdx : bit::BitMask(masks.samplerMask)) {
        IDirect3DBaseTexture9* dsBase  = m_state.depthStencil->GetBaseTexture();
        IDirect3DBaseTexture9* texBase = m_state.textures[samplerIdx];

        if (likely(dsBase != texBase))
          continue;

        m_activeHazardsDS |= 1 << samplerIdx;
      }
    }
  }


  void D3D9DeviceEx::MarkRenderHazards() {
    struct {
      uint8_t RT : 1;
      uint8_t DS : 1;
    } hazardState;
    hazardState.RT = m_activeHazardsRT != 0;
    hazardState.DS = m_activeHazardsDS != 0;

    EmitCs([
      cHazardState = hazardState
    ](DxvkContext* ctx) {
      VkPipelineStageFlags srcStages = 0;
      VkAccessFlags srcAccess = 0;

      if (cHazardState.RT != 0) {
        srcStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        srcAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      }
      if (cHazardState.DS != 0) {
        srcStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        srcAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      }

      ctx->emitGraphicsBarrier(
        srcStages,
        srcAccess,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    });

    for (uint32_t samplerIdx : bit::BitMask(m_activeHazardsRT)) {
      // Guaranteed to not be nullptr...
      auto tex = GetCommonTexture(m_state.textures[samplerIdx]);
      if (unlikely(!tex->MarkTransitionedToHazardLayout())) {
        TransitionImage(tex, m_hazardLayout);
        m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      }
    }

    bool zWriteEnabled = m_state.renderStates[D3DRS_ZWRITEENABLE];
    if (m_activeHazardsDS != 0 && zWriteEnabled) {
      // Guaranteed to not be nullptr...
      auto tex = m_state.depthStencil->GetCommonTexture();
      if (unlikely(!tex->MarkTransitionedToHazardLayout())) {
        TransitionImage(tex, m_hazardLayout);
        m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      }
    }
  }


  void D3D9DeviceEx::UpdateActiveFetch4(uint32_t stateSampler) {
    auto& state = m_state.samplerStates;

    const uint32_t samplerBit = 1u << stateSampler;

    auto texture = GetCommonTexture(m_state.textures[stateSampler]);
    const bool textureSupportsFetch4 = texture != nullptr && texture->SupportsFetch4();

    const bool fetch4Enabled = m_fetch4Enabled & samplerBit;
    const bool pointSampled  = state[stateSampler][D3DSAMP_MAGFILTER] == D3DTEXF_POINT;
    const bool shouldFetch4  = fetch4Enabled && textureSupportsFetch4 && pointSampled;

    if (unlikely(shouldFetch4 != !!(m_fetch4 & samplerBit))) {
      if (shouldFetch4)
        m_fetch4 |= samplerBit;
      else
        m_fetch4 &= ~samplerBit;
    }
  }


  void D3D9DeviceEx::UploadManagedTexture(D3D9CommonTexture* pResource) {
    for (uint32_t subresource = 0; subresource < pResource->CountSubresources(); subresource++) {
      if (!pResource->NeedsUpload(subresource))
        continue;

      this->FlushImage(pResource, subresource);
    }

    pResource->ClearDirtyBoxes();
    pResource->ClearNeedsUpload();
  }


  void D3D9DeviceEx::UploadManagedTextures(uint32_t mask) {
    // Guaranteed to not be nullptr...
    for (uint32_t texIdx : bit::BitMask(mask))
      UploadManagedTexture(GetCommonTexture(m_state.textures[texIdx]));

    m_activeTexturesToUpload &= ~mask;
  }


  void D3D9DeviceEx::UpdateTextureTypeMismatchesForShader(const D3D9CommonShader* shader, uint32_t shaderSamplerMask, uint32_t shaderSamplerOffset) {
    const uint32_t stageCorrectedShaderSamplerMask = shaderSamplerMask << shaderSamplerOffset;
    if (unlikely(shader->GetInfo().majorVersion() < 2 || m_d3d9Options.forceSamplerTypeSpecConstants)) {
      // SM 1 shaders don't define the texture type in the shader.
      // We always use spec constants for those.
      m_dirtyTextures |= stageCorrectedShaderSamplerMask & m_mismatchingTextureTypes;
      m_mismatchingTextureTypes &= ~stageCorrectedShaderSamplerMask;
      return;
    }

    for (const uint32_t i : bit::BitMask(stageCorrectedShaderSamplerMask)) {
      const D3D9CommonTexture* texture = GetCommonTexture(m_state.textures[i]);
      if (unlikely(texture == nullptr)) {
        // Unbound textures are not mismatching texture types
        m_dirtyTextures |= m_mismatchingTextureTypes & (1 << i);
        m_mismatchingTextureTypes &= ~(1 << i);
        continue;
      }

      VkImageViewType boundViewType  = D3D9CommonTexture::GetImageViewTypeFromResourceType(texture->GetType(), D3D9CommonTexture::AllLayers);
      VkImageViewType shaderViewType = shader->GetImageViewType(i - shaderSamplerOffset);
      if (unlikely(boundViewType != shaderViewType)) {
        m_dirtyTextures |= 1 << i;
        m_mismatchingTextureTypes |= 1 << i;
      } else {
        // The texture type is no longer mismatching, make sure we bind the texture now.
        m_dirtyTextures |= m_mismatchingTextureTypes & (1 << i);
        m_mismatchingTextureTypes &= ~(1 << i);
      }
    }
  }


  void D3D9DeviceEx::UpdateTextureTypeMismatchesForTexture(uint32_t stateSampler) {
    uint32_t shaderTextureIndex;
    const D3D9CommonShader* shader;
    if (likely(IsPSSampler(stateSampler))) {
      shader = GetCommonShader(m_state.pixelShader);
      shaderTextureIndex = stateSampler;
    } else if (unlikely(IsVSSampler(stateSampler))) {
      shader = GetCommonShader(m_state.vertexShader);
      shaderTextureIndex = stateSampler - caps::MaxTexturesPS - 1;
    } else {
      // Do not type check the fixed function displacement map texture.
      return;
    }

    if (unlikely(shader == nullptr || shader->GetInfo().majorVersion() < 2 || m_d3d9Options.forceSamplerTypeSpecConstants)) {
      // This function only gets called by UpdateTextureBitmasks
      // which clears the dirty and mismatching bits for the texture before anyway.
      return;
    }

    const D3D9CommonTexture* tex = GetCommonTexture(m_state.textures[stateSampler]);
    VkImageViewType boundViewType  = D3D9CommonTexture::GetImageViewTypeFromResourceType(tex->GetType(), D3D9CommonTexture::AllLayers);
    VkImageViewType shaderViewType = shader->GetImageViewType(shaderTextureIndex);
    // D3D9 does not have 1D textures. The value of VIEW_TYPE_1D is 0
    // which is the default when there is no declaration for the type.
    bool shaderUsesTexture = shaderViewType != VkImageViewType(0);
    if (unlikely(boundViewType != shaderViewType && shaderUsesTexture)) {
      const uint32_t samplerBit = 1u << stateSampler;
      m_mismatchingTextureTypes |= samplerBit;
    }
  }


  void D3D9DeviceEx::GenerateTextureMips(uint32_t mask) {
    for (uint32_t texIdx : bit::BitMask(mask)) {
      // Guaranteed to not be nullptr...
      auto texInfo = GetCommonTexture(m_state.textures[texIdx]);

      if (likely(texInfo->NeedsMipGen())) {
        this->EmitGenerateMips(texInfo);
        if (likely(!IsTextureBoundAsAttachment(texInfo))) {
          texInfo->SetNeedsMipGen(false);
        }
      }
    }

    m_activeTexturesToGen &= ~mask;
  }


  void D3D9DeviceEx::MarkTextureMipsDirty(D3D9CommonTexture* pResource) {
    pResource->SetNeedsMipGen(true);

    for (uint32_t i : bit::BitMask(m_activeTextures)) {
      // Guaranteed to not be nullptr...
      auto texInfo = GetCommonTexture(m_state.textures[i]);

      if (texInfo == pResource) {
        m_activeTexturesToGen |= 1 << i;
        // We can early out here, no need to add another index for this.
        break;
      }
    }
  }


  void D3D9DeviceEx::MarkTextureMipsUnDirty(D3D9CommonTexture* pResource) {
    if (likely(!IsTextureBoundAsAttachment(pResource))) {
      // We need to keep the texture marked as needing mipmap generation because we don't set that when rendering.
      pResource->SetNeedsMipGen(false);

      for (uint32_t i : bit::BitMask(m_activeTextures)) {
        // Guaranteed to not be nullptr...
        auto texInfo = GetCommonTexture(m_state.textures[i]);

        if (unlikely(texInfo == pResource)) {
          m_activeTexturesToGen &= ~(1 << i);
        }
      }
    }
  }


  void D3D9DeviceEx::MarkTextureUploaded(D3D9CommonTexture* pResource) {
    for (uint32_t i : bit::BitMask(m_activeTextures)) {
      // Guaranteed to not be nullptr...
      auto texInfo = GetCommonTexture(m_state.textures[i]);

      if (texInfo == pResource)
        m_activeTexturesToUpload &= ~(1 << i);
    }
  }


  void D3D9DeviceEx::UpdatePointMode(bool pointList) {
    if (!pointList) {
      UpdatePointModeSpec(0);
      return;
    }

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

    UpdatePointModeSpec(mode);
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

        UpdateFogModeSpec(true, mode, D3DFOG_NONE);
      }
    }
    else if (pixelFog) {
      D3DFOGMODE mode = D3DFOGMODE(rs[D3DRS_FOGTABLEMODE]);

      UpdateFogConstants(mode);

      if (m_flags.test(D3D9DeviceFlag::DirtyFogState)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogState);

        UpdateFogModeSpec(true, D3DFOG_NONE, mode);
      }
    }
    else {
      if (fogEnabled)
        UpdateFogConstants(D3DFOG_NONE);

      if (m_flags.test(D3D9DeviceFlag::DirtyFogState)) {
        m_flags.clr(D3D9DeviceFlag::DirtyFogState);

        UpdateFogModeSpec(fogEnabled, D3DFOG_NONE, D3DFOG_NONE);
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

    // Some games break if render targets that get disabled using the color write mask
    // end up shrinking the render area. So we don't bind those.
    // (This impacted Dead Space 1.)
    // But we want to minimize frame buffer changes because those
    // break up the current render pass. So we dont unbind for disabled color write masks
    // if the RT has the same size or is bigger than the smallest active RT.

    uint32_t boundMask = 0u;
    uint32_t anyColorWriteMask = 0u;
    uint32_t limitsRenderAreaMask = 0u;
    VkExtent2D renderArea = { ~0u, ~0u };
    for (uint32_t i = 0u; i < m_state.renderTargets.size(); i++) {
      if (!HasRenderTargetBound(i))
        continue;

      const DxvkImageCreateInfo& rtImageInfo = m_state.renderTargets[i]->GetCommonTexture()->GetImage()->info();

      // Dont bind it if the sample count doesnt match
      if (likely(sampleCount == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM))
        sampleCount = rtImageInfo.sampleCount;
      else if (unlikely(sampleCount != rtImageInfo.sampleCount))
        continue;

      // Dont bind it if the pixel shader doesnt write to it
      if (!(m_psShaderMasks.rtMask & (1 << i)))
        continue;

      boundMask |= 1 << i;

      VkExtent2D rtExtent = m_state.renderTargets[i]->GetSurfaceExtent();
      bool rtLimitsRenderArea = rtExtent.width < renderArea.width || rtExtent.height < renderArea.height;
      limitsRenderAreaMask |= rtLimitsRenderArea << i;

      // It will only get bound if its not smaller than the others.
      // So RTs with a disabled color write mask will never impact the render area.
      if (m_state.renderStates[ColorWriteIndex(i)] == 0)
        continue;

      anyColorWriteMask |= 1 << i;

      if (rtExtent.width < renderArea.width && rtExtent.height < renderArea.height) {
        // It's smaller on both axis, so the previous RTs no longer limit the size
        limitsRenderAreaMask = 1 << i;
      }

      renderArea.width = std::min(renderArea.width, rtExtent.width);
      renderArea.height = std::min(renderArea.height, rtExtent.height);
    }

    bool dsvBound = false;
    if (m_state.depthStencil != nullptr) {
      // We only need to skip binding the DSV if it would shrink the render area
      // despite not being used, otherwise we might end up with unnecessary render pass spills
      bool anyDSStateEnabled = m_state.renderStates[D3DRS_ZENABLE]
        || m_state.renderStates[D3DRS_ZWRITEENABLE]
        || m_state.renderStates[D3DRS_STENCILENABLE]
        || m_state.renderStates[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB);

      VkExtent2D dsvExtent = m_state.depthStencil->GetSurfaceExtent();
      bool dsvLimitsRenderArea = dsvExtent.width < renderArea.width || dsvExtent.height < renderArea.height;

      const DxvkImageCreateInfo& dsImageInfo = m_state.depthStencil->GetCommonTexture()->GetImage()->info();
      const bool sampleCountMatches = sampleCount == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM || sampleCount == dsImageInfo.sampleCount;

      dsvBound = sampleCountMatches && (anyDSStateEnabled || !dsvLimitsRenderArea);
      if (sampleCountMatches && anyDSStateEnabled && dsvExtent.width < renderArea.width && dsvExtent.height < renderArea.height) {
        // It's smaller on both axis, so the previous RTs no longer limit the size
        limitsRenderAreaMask = 0u;
      }
    }

    // We only need to skip binding the RT if it would shrink the render area
    // despite not having color writes enabled,
    // otherwise we might end up with unnecessary render pass spills
    boundMask &= (anyColorWriteMask | ~limitsRenderAreaMask);
    for (uint32_t i : bit::BitMask(boundMask)) {
      attachments.color[i] = {
        m_state.renderTargets[i]->GetRenderTargetView(srgb),
        m_state.renderTargets[i]->GetRenderTargetLayout(m_hazardLayout) };
    }

    if (dsvBound) {
      const bool depthWrite = m_state.renderStates[D3DRS_ZWRITEENABLE];

      attachments.depth = {
        m_state.depthStencil->GetDepthStencilView(),
        m_state.depthStencil->GetDepthStencilLayout(depthWrite, m_activeHazardsDS != 0, m_hazardLayout) };
    }

    VkImageAspectFlags feedbackLoopAspects = 0u;
    if (m_hazardLayout == VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT) {
      if (m_activeHazardsRT != 0)
        feedbackLoopAspects |= VK_IMAGE_ASPECT_COLOR_BIT;
      if (m_activeHazardsDS != 0 && attachments.depth.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        feedbackLoopAspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments         = std::move(attachments),
      cFeedbackLoopAspects = feedbackLoopAspects
    ] (DxvkContext* ctx) mutable {
      ctx->bindRenderTargets(std::move(cAttachments), cFeedbackLoopAspects);
    });
  }


  void D3D9DeviceEx::BindViewportAndScissor() {
    m_flags.clr(D3D9DeviceFlag::DirtyViewportScissor);

    // D3D9's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    const D3DVIEWPORT9& vp = m_state.viewport;

    // Correctness Factor for 1/2 texel offset
    constexpr float cf = 0.5f;

    // How much to bias MinZ by to avoid a depth
    // degenerate viewport.
    // Tests show that the bias is only applied below minZ values of 0.5
    float zBias;
    if (vp.MinZ >= 0.5f) {
      zBias = 0.0f;
    } else {
      zBias = 0.001f;
    }

    DxvkViewport state = { };
    state.viewport = VkViewport{
      float(vp.X)     + cf,    float(vp.Height + vp.Y) + cf,
      float(vp.Width),        -float(vp.Height),
      std::clamp(vp.MinZ,                            0.0f, 1.0f),
      std::clamp(std::max(vp.MaxZ, vp.MinZ + zBias), 0.0f, 1.0f),
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

      state.scissor = VkRect2D{ srPosA, srSize };
    }
    else {
      state.scissor = VkRect2D{
        VkOffset2D { int32_t(vp.X), int32_t(vp.Y) },
        VkExtent2D { vp.Width,      vp.Height     }};
    }

    EmitCs([
      cViewport = state
    ] (DxvkContext* ctx) {
      ctx->setViewports(1, &cViewport);
    });
  }


  bool D3D9DeviceEx::IsAlphaToCoverageEnabled() const {
    const bool alphaTest = m_state.renderStates[D3DRS_ALPHATESTENABLE] != 0;

    const D3D9CommonTexture* rt0 = GetCommonTexture(m_state.renderTargets[0].ptr());
    const bool isMultisampled = rt0 != nullptr && (
      rt0->Desc()->MultiSample >= D3DMULTISAMPLE_2_SAMPLES
      || (rt0->Desc()->MultiSample == D3DMULTISAMPLE_NONMASKABLE && rt0->Desc()->MultisampleQuality > 0)
    );

    return (m_amdATOC || (m_nvATOC && alphaTest)) && rt0 != nullptr && isMultisampled;
  }


  void D3D9DeviceEx::BindMultiSampleState() {
    m_flags.clr(D3D9DeviceFlag::DirtyMultiSampleState);

    DxvkMultisampleState msState = { };
    msState.setSampleMask(m_flags.test(D3D9DeviceFlag::ValidSampleMask)
      ? uint16_t(m_state.renderStates[D3DRS_MULTISAMPLEMASK])
      : uint16_t(0xffffu));
    msState.setAlphaToCoverage(IsAlphaToCoverageEnabled());

    EmitCs([
      cState = msState
    ] (DxvkContext* ctx) {
      ctx->setMultisampleState(cState);
    });
  }


  void D3D9DeviceEx::BindBlendState() {
    m_flags.clr(D3D9DeviceFlag::DirtyBlendState);

    auto& state = m_state.renderStates;

    DxvkBlendMode mode = { };
    mode.setBlendEnable(state[D3DRS_ALPHABLENDENABLE]);

    D3D9BlendState color = { };

    color.Src = D3DBLEND(state[D3DRS_SRCBLEND]);
    color.Dst = D3DBLEND(state[D3DRS_DESTBLEND]);
    color.Op  = D3DBLENDOP(state[D3DRS_BLENDOP]);
    FixupBlendState(color);

    D3D9BlendState alpha = color;

    if (state[D3DRS_SEPARATEALPHABLENDENABLE]) {
      alpha.Src = D3DBLEND(state[D3DRS_SRCBLENDALPHA]);
      alpha.Dst = D3DBLEND(state[D3DRS_DESTBLENDALPHA]);
      alpha.Op  = D3DBLENDOP(state[D3DRS_BLENDOPALPHA]);
      FixupBlendState(alpha);
    }

    mode.setColorOp(DecodeBlendFactor(color.Src, false),
                    DecodeBlendFactor(color.Dst, false),
                    DecodeBlendOp(color.Op));

    mode.setAlphaOp(DecodeBlendFactor(alpha.Src, true),
                    DecodeBlendFactor(alpha.Dst, true),
                    DecodeBlendOp(alpha.Op));

    uint16_t writeMasks = 0;

    for (uint32_t i = 0; i < 4; i++)
      writeMasks |= (state[ColorWriteIndex(i)] & 0xfu) << (4u * i);

    EmitCs([
      cMode       = mode,
      cWriteMasks = writeMasks,
      cAlphaMasks = m_alphaSwizzleRTs
    ](DxvkContext* ctx) {
      for (uint32_t i = 0; i < 4; i++) {
        DxvkBlendMode mode = cMode;
        mode.setWriteMask(cWriteMasks >> (4u * i));

        // Adjust the blend factor based on the render target alpha swizzle bit mask.
        // Specific formats such as the XRGB ones require a ONE swizzle for alpha
        // which cannot be directly applied with the image view of the attachment.
        if (cAlphaMasks & (1 << i)) {
          auto NormalizeFactor = [] (VkBlendFactor Factor) {
            if (Factor == VK_BLEND_FACTOR_DST_ALPHA)
              return VK_BLEND_FACTOR_ONE;
            else if (Factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA)
              return VK_BLEND_FACTOR_ZERO;
            return Factor;
          };

          mode.setColorOp(NormalizeFactor(mode.colorSrcFactor()),
                          NormalizeFactor(mode.colorDstFactor()), mode.colorBlendOp());
          mode.setAlphaOp(NormalizeFactor(mode.alphaSrcFactor()),
                          NormalizeFactor(mode.alphaDstFactor()), mode.alphaBlendOp());
        }

        mode.normalize();

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

    DxvkDepthStencilState state = { };
    state.setDepthTest(rs[D3DRS_ZENABLE]);
    state.setDepthWrite(rs[D3DRS_ZWRITEENABLE]);
    state.setStencilTest(stencil);
    state.setDepthCompareOp(DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ZFUNC])));

    DxvkStencilOp frontOp = { };

    if (stencil) {
      frontOp.setFailOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILFAIL])));
      frontOp.setPassOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILPASS])));
      frontOp.setDepthFailOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_STENCILZFAIL])));
      frontOp.setCompareOp(DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_STENCILFUNC])));
      frontOp.setCompareMask(rs[D3DRS_STENCILMASK]);
      frontOp.setWriteMask(rs[D3DRS_STENCILWRITEMASK]);
    }

    DxvkStencilOp backOp = frontOp;

    if (twoSidedStencil) {
      backOp.setFailOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILFAIL])));
      backOp.setPassOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILPASS])));
      backOp.setDepthFailOp(DecodeStencilOp(D3DSTENCILOP(rs[D3DRS_CCW_STENCILZFAIL])));
      backOp.setCompareOp(DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_CCW_STENCILFUNC])));
      backOp.setCompareMask(rs[D3DRS_STENCILMASK]);
      backOp.setWriteMask(rs[D3DRS_STENCILWRITEMASK]);
    }

    state.setStencilOpFront(frontOp);
    state.setStencilOpBack(backOp);

    EmitCs([
      cState = state
    ] (DxvkContext* ctx) mutable {
      cState.normalize();

      ctx->setDepthStencilState(cState);
    });
  }


  void D3D9DeviceEx::BindRasterizerState() {
    m_flags.clr(D3D9DeviceFlag::DirtyRasterizerState);

    auto& rs = m_state.renderStates;

    DxvkRasterizerState state = { };
    state.setCullMode(DecodeCullMode(D3DCULL(rs[D3DRS_CULLMODE])));
    state.setDepthBias(IsDepthBiasEnabled());
    state.setDepthClip(true);
    state.setFrontFace(VK_FRONT_FACE_CLOCKWISE);
    state.setPolygonMode(DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE])));
    state.setFlatShading(m_state.renderStates[D3DRS_SHADEMODE] == D3DSHADE_FLAT);

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


  uint32_t D3D9DeviceEx::GetAlphaTestPrecision() {
    if (m_state.renderTargets[0] == nullptr)
      return 0;

    D3D9Format format = m_state.renderTargets[0]->GetCommonTexture()->Desc()->Format;

    switch (format) {
      case D3D9Format::A2B10G10R10:
      case D3D9Format::A2R10G10B10:
      case D3D9Format::A2W10V10U10:
      case D3D9Format::A2B10G10R10_XR_BIAS:
        return 0x2; /* 10 bit */

      case D3D9Format::R16F:
      case D3D9Format::G16R16F:
      case D3D9Format::A16B16G16R16F:
        return 0x7; /* 15 bit */

      case D3D9Format::G16R16:
      case D3D9Format::A16B16G16R16:
      case D3D9Format::V16U16:
      case D3D9Format::L16:
      case D3D9Format::Q16W16V16U16:
        return 0x8; /* 16 bit */

      case D3D9Format::R32F:
      case D3D9Format::G32R32F:
      case D3D9Format::A32B32G32R32F:
        return 0xF; /* float */

      default:
        return 0x0; /* 8 bit */
    }
  }


  void D3D9DeviceEx::BindAlphaTestState() {
    m_flags.clr(D3D9DeviceFlag::DirtyAlphaTestState);

    auto& rs = m_state.renderStates;

    VkCompareOp alphaOp = IsAlphaTestEnabled()
      ? DecodeCompareOp(D3DCMPFUNC(rs[D3DRS_ALPHAFUNC]))
      : VK_COMPARE_OP_ALWAYS;

    uint32_t precision = alphaOp != VK_COMPARE_OP_ALWAYS
      ? GetAlphaTestPrecision()
      : 0u;

    UpdateAlphaTestSpec(alphaOp, precision);
  }


  void D3D9DeviceEx::BindDepthStencilRefrence() {
    auto& rs = m_state.renderStates;

    uint32_t ref = uint32_t(rs[D3DRS_STENCILREF]) & 0xff;

    EmitCs([cRef = ref] (DxvkContext* ctx) {
      ctx->setStencilReference(cRef);
    });
  }


  void D3D9DeviceEx::BindSampler(DWORD Sampler) {
    auto samplerInfo = RemapStateSamplerShader(Sampler);

    const uint32_t slot = computeResourceSlotId(
      samplerInfo.first, DxsoBindingType::Image,
      samplerInfo.second);

    m_samplerBindCount++;

    EmitCs([this,
      cSlot     = slot,
      cState    = D3D9SamplerInfo(m_state.samplerStates[Sampler]),
      cIsCube   = bool(m_cubeTextures & (1u << Sampler)),
      cIsDepth  = bool(m_depthTextures & (1u << Sampler)),
      cBindId   = m_samplerBindCount
    ] (DxvkContext* ctx) {
      DxvkSamplerKey key = { };

      key.setFilter(
        DecodeFilter(cState.minFilter),
        DecodeFilter(cState.magFilter),
        DecodeMipFilter(cState.mipFilter));

      if (cIsCube) {
        key.setAddressModes(
          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        key.setLegacyCubeFilter(!m_d3d9Options.seamlessCubes);
      } else {
        key.setAddressModes(
          DecodeAddressMode(cState.addressU),
          DecodeAddressMode(cState.addressV),
          DecodeAddressMode(cState.addressW));
      }

      key.setDepthCompare(cIsDepth, VK_COMPARE_OP_LESS_OR_EQUAL);

      if (cState.mipFilter) {
        // Anisotropic filtering doesn't make any sense with only one mip
        uint32_t anisotropy = cState.maxAnisotropy;

        if (cState.minFilter != D3DTEXF_ANISOTROPIC)
          anisotropy = 0u;

        if (m_d3d9Options.samplerAnisotropy != -1 && cState.minFilter > D3DTEXF_POINT)
          anisotropy = m_d3d9Options.samplerAnisotropy;

        key.setAniso(anisotropy);

        float lodBias = cState.mipLodBias;
        lodBias += m_d3d9Options.samplerLodBias;

        if (m_d3d9Options.clampNegativeLodBias)
          lodBias = std::max(lodBias, 0.0f);

        key.setLodRange(float(cState.maxMipLevel), 16.0f, lodBias);
      }

      if (key.u.p.hasBorder)
        DecodeD3DCOLOR(cState.borderColor, key.borderColor.float32);

      VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      ctx->bindResourceSampler(stage, cSlot, m_dxvkDevice->createSampler(key));

      // Let the main thread know about current sampler stats
      uint64_t liveCount = m_dxvkDevice->getSamplerStats().liveCount;
      m_lastSamplerStats.store(liveCount | (cBindId << SamplerCountBits), std::memory_order_relaxed);
    });
  }


  void D3D9DeviceEx::BindTexture(DWORD StateSampler) {
    auto shaderSampler = RemapStateSamplerShader(StateSampler);

    uint32_t slot = computeResourceSlotId(shaderSampler.first,
      DxsoBindingType::Image, uint32_t(shaderSampler.second));

    const bool srgb =
      m_state.samplerStates[StateSampler][D3DSAMP_SRGBTEXTURE] & 0x1;

    D3D9CommonTexture* commonTex =
      GetCommonTexture(m_state.textures[StateSampler]);

    EmitCs([
      cSlot = slot,
      cImageView = commonTex->GetSampleView(srgb)
    ](DxvkContext* ctx) mutable {
      VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      ctx->bindResourceImageView(stage, cSlot, std::move(cImageView));
    });
  }


  void D3D9DeviceEx::UnbindTextures(uint32_t mask) {
    EmitCs([
      cMask = mask
    ](DxvkContext* ctx) {
      for (uint32_t i : bit::BitMask(cMask)) {
        auto shaderSampler = RemapStateSamplerShader(i);

        uint32_t slot = computeResourceSlotId(shaderSampler.first,
          DxsoBindingType::Image, uint32_t(shaderSampler.second));

        VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ctx->bindResourceImageView(stage, slot, nullptr);
      }
    });
  }


  void D3D9DeviceEx::UndirtySamplers(uint32_t mask) {
    EnsureSamplerLimit();

    for (uint32_t i : bit::BitMask(mask))
      BindSampler(i);

    m_dirtySamplerStates &= ~mask;
  }


  void D3D9DeviceEx::UndirtyTextures(uint32_t usedMask) {
    const uint32_t activeMask   = usedMask &  (m_activeTextures & ~m_mismatchingTextureTypes);
    const uint32_t inactiveMask = usedMask & (~m_activeTextures | m_mismatchingTextureTypes);

    for (uint32_t i : bit::BitMask(activeMask))
      BindTexture(i);

    if (inactiveMask)
      UnbindTextures(inactiveMask);

    m_dirtyTextures &= ~usedMask;
  }

  void D3D9DeviceEx::MarkTextureBindingDirty(IDirect3DBaseTexture9* texture) {
    D3D9DeviceLock lock = LockDevice();

    for (uint32_t i : bit::BitMask(m_activeTextures)) {
      if (m_state.textures[i] == texture)
        m_dirtyTextures |= 1u << i;
    }
  }


  D3D9DrawInfo D3D9DeviceEx::GenerateDrawInfo(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
          UINT             InstanceCount) {
    D3D9DrawInfo drawInfo;
    drawInfo.vertexCount = GetVertexCount(PrimitiveType, PrimitiveCount);
    drawInfo.instanceCount = (m_iaState.streamsInstanced & m_iaState.streamsUsed)
      ? InstanceCount
      : 1u;
    return drawInfo;
  }


  uint32_t D3D9DeviceEx::GetInstanceCount() const {
    return std::max(m_state.streamFreq[0] & 0x7FFFFFu, 1u);
  }


  void D3D9DeviceEx::PrepareDraw(D3DPRIMITIVETYPE PrimitiveType, bool UploadVBOs, bool UploadIBO) {
    if (unlikely(m_activeHazardsRT != 0 || m_activeHazardsDS != 0))
      MarkRenderHazards();

    if (unlikely((!m_lastHazardsDS) != (!m_activeHazardsDS))
     || unlikely((!m_lastHazardsRT) != (!m_activeHazardsRT))) {
      m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
      m_lastHazardsDS = m_activeHazardsDS;
      m_lastHazardsRT = m_activeHazardsRT;
    }

    if (likely(UploadVBOs)) {
      const uint32_t usedBuffersMask = m_state.vertexDecl != nullptr ? m_state.vertexDecl->GetStreamMask() : ~0u;
      const uint32_t buffersToUpload = m_activeVertexBuffersToUpload & usedBuffersMask;
      for (uint32_t bufferIdx : bit::BitMask(buffersToUpload)) {
        auto* vbo = GetCommonBuffer(m_state.vertexBuffers[bufferIdx].vertexBuffer);
        if (likely(vbo != nullptr && vbo->NeedsUpload()))
          FlushBuffer(vbo);
      }
      m_activeVertexBuffersToUpload &= ~buffersToUpload;
    }

    const uint32_t usedSamplerMask = m_psShaderMasks.samplerMask | m_vsShaderMasks.samplerMask;
    const uint32_t usedTextureMask = m_activeTextures & usedSamplerMask;

    const uint32_t texturesToUpload = m_activeTexturesToUpload & usedTextureMask;
    if (unlikely(texturesToUpload != 0))
      UploadManagedTextures(texturesToUpload);

    const uint32_t texturesToGen = m_activeTexturesToGen & usedTextureMask;
    if (unlikely(texturesToGen != 0))
      GenerateTextureMips(texturesToGen);

    auto* ibo = GetCommonBuffer(m_state.indices);
    if (unlikely(UploadIBO && ibo != nullptr && ibo->NeedsUpload()))
      FlushBuffer(ibo);

    UpdateFog();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyFramebuffer)))
      BindFramebuffer();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyViewportScissor)))
      BindViewportAndScissor();

    const uint32_t activeDirtySamplers = m_dirtySamplerStates & usedTextureMask;
    if (unlikely(activeDirtySamplers))
      UndirtySamplers(activeDirtySamplers);

    const uint32_t usedDirtyTextures = m_dirtyTextures & usedSamplerMask;
    if (likely(usedDirtyTextures))
      UndirtyTextures(usedDirtyTextures);

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyBlendState)))
      BindBlendState();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyDepthStencilState)))
      BindDepthStencilState();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyRasterizerState)))
      BindRasterizerState();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyDepthBias)))
      BindDepthBias();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyMultiSampleState)))
      BindMultiSampleState();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyAlphaTestState)))
      BindAlphaTestState();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyClipPlanes)))
      UpdateClipPlanes();

    UpdatePointMode(PrimitiveType == D3DPT_POINTLIST);

    if (likely(UseProgrammableVS())) {
      if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyProgVertexShader))) {
        m_flags.set(D3D9DeviceFlag::DirtyInputLayout);

        BindShader<DxsoProgramType::VertexShader>(
          GetCommonShader(m_state.vertexShader));
      }
      UploadConstants<DxsoProgramTypes::VertexShader>();

      if (likely(!CanSWVP())) {
        UpdateVertexBoolSpec(
          m_state.vsConsts->bConsts[0] &
          m_consts[DxsoProgramType::VertexShader].meta.boolConstantMask);
      } else
        UpdateVertexBoolSpec(0);
    }
    else {
      UpdateVertexBoolSpec(0);
      UpdateFixedFunctionVS();
    }

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyInputLayout)))
      BindInputLayout();

    if (likely(UseProgrammablePS())) {
      UploadConstants<DxsoProgramTypes::PixelShader>();

      const uint32_t psTextureMask = usedTextureMask & ((1u << caps::MaxTexturesPS) - 1u);
      const uint32_t fetch4        = m_fetch4             & psTextureMask;
      const uint32_t projected     = m_projectionBitfield & psTextureMask;

      const auto& programInfo = GetCommonShader(m_state.pixelShader)->GetInfo();

      if (programInfo.majorVersion() >= 2)
        UpdatePixelShaderSamplerSpec(m_d3d9Options.forceSamplerTypeSpecConstants ? m_textureTypes : 0u, 0u, fetch4);
      else
        UpdatePixelShaderSamplerSpec(m_textureTypes, programInfo.minorVersion() >= 4 ? 0u : projected, fetch4); // For implicit samplers...

      UpdatePixelBoolSpec(
        m_state.psConsts->bConsts[0] &
        m_consts[DxsoProgramType::PixelShader].meta.boolConstantMask);
    }
    else {
      UpdatePixelBoolSpec(0);
      UpdatePixelShaderSamplerSpec(0u, 0u, 0u);

      UpdateFixedFunctionPS();
    }

    const uint32_t nullTextureMask = usedSamplerMask & ~usedTextureMask;
    const uint32_t depthTextureMask = m_depthTextures & usedTextureMask;
    const uint32_t drefClampMask = m_drefClamp & depthTextureMask;
    UpdateCommonSamplerSpec(nullTextureMask, depthTextureMask, drefClampMask);

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtySharedPixelShaderData))) {
      m_flags.clr(D3D9DeviceFlag::DirtySharedPixelShaderData);

      auto mapPtr = m_psShared.AllocSlice();
      D3D9SharedPS* data = reinterpret_cast<D3D9SharedPS*>(mapPtr);

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

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyDepthBounds))) {
      m_flags.clr(D3D9DeviceFlag::DirtyDepthBounds);

      DxvkDepthBounds db = { };
      db.enableDepthBounds  = (m_state.renderStates[D3DRS_ADAPTIVETESS_X] == uint32_t(D3D9Format::NVDB));

      if (db.enableDepthBounds) {
        db.minDepthBounds = std::clamp(bit::cast<float>(m_state.renderStates[D3DRS_ADAPTIVETESS_Z]), 0.0f, 1.0f);
        db.maxDepthBounds = std::clamp(bit::cast<float>(m_state.renderStates[D3DRS_ADAPTIVETESS_W]), 0.0f, 1.0f);
      }

      EmitCs([
        cDepthBounds = db
      ] (DxvkContext* ctx) {
        ctx->setDepthBounds(cDepthBounds);
      });
    }

    BindSpecConstants();

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyVertexBuffers) && UploadVBOs)) {
      for (uint32_t i = 0; i < caps::MaxStreams; i++) {
        const D3D9VBO& vbo = m_state.vertexBuffers[i];
        BindVertexBuffer(i, vbo.vertexBuffer.ptr(), vbo.offset, vbo.stride);
      }
      m_flags.clr(D3D9DeviceFlag::DirtyVertexBuffers);
    }

    if (unlikely(m_flags.test(D3D9DeviceFlag::DirtyIndexBuffer) && UploadIBO)) {
      BindIndices();
      m_flags.clr(D3D9DeviceFlag::DirtyIndexBuffer);
    }
  }


  void D3D9DeviceEx::EnsureSamplerLimit() {
    constexpr uint32_t MaxSamplerCount = DxvkSamplerPool::MaxSamplerCount - SamplerCount;

    // Maximum possible number of live samplers we can have
    // since last reading back from the CS thread.
    if (likely(m_lastSamplerLiveCount + m_samplerBindCount - m_lastSamplerBindCount <= MaxSamplerCount))
      return;

    // Update current stats from CS thread and check again. We
    // don't want to do this every time due to potential cache
    // thrashing.
    uint64_t lastStats = m_lastSamplerStats.load(std::memory_order_relaxed);
    m_lastSamplerLiveCount = lastStats & SamplerCountMask;
    m_lastSamplerBindCount = lastStats >> SamplerCountBits;

    if (likely(m_lastSamplerLiveCount + m_samplerBindCount - m_lastSamplerBindCount <= MaxSamplerCount))
      return;

    // If we have a large number of sampler updates in flight, wait for
    // the CS thread to complete some and re-evaluate. We should not hit
    // this path under normal gameplay conditions.
    ConsiderFlush(GpuFlushType::ImplicitSynchronization);

    uint64_t sequenceNumber = m_csThread.lastSequenceNumber();

    while (++sequenceNumber <= GetCurrentSequenceNumber()) {
      SynchronizeCsThread(sequenceNumber);

      uint64_t lastStats = m_lastSamplerStats.load(std::memory_order_relaxed);
      m_lastSamplerLiveCount = lastStats & SamplerCountMask;
      m_lastSamplerBindCount = lastStats >> SamplerCountBits;

      if (m_lastSamplerLiveCount + m_samplerBindCount - m_lastSamplerBindCount <= MaxSamplerCount)
        return;
    }

    // If we end up here, the game somehow managed to queue up so
    // many samplers that we need to wait for the GPU to free some.
    // We should absolutely never hit this path in the real world.
    Logger::warn("Sampler pool exhausted, synchronizing with GPU.");

    Flush();
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);

    uint64_t submissionId = m_submissionFence->value();

    while (++submissionId <= m_submissionId) {
      m_submissionFence->wait(submissionId);

      // Need to manually update sampler stats here since we
      // might otherwise hit this path again the next time
      auto samplerStats = m_dxvkDevice->getSamplerStats();
      m_lastSamplerStats = samplerStats.liveCount | (m_samplerBindCount << SamplerCountBits);

      if (samplerStats.liveCount <= MaxSamplerCount)
        return;
    }

    // If we end up *here*, good luck.
    Logger::warn("Sampler pool exhausted, cannot create any new samplers.");
  }


  template <DxsoProgramType ShaderStage>
  void D3D9DeviceEx::BindShader(
  const D3D9CommonShader*                 pShaderModule) {
    auto shader = pShaderModule->GetShader();

    if (unlikely(shader->needsLibraryCompile()))
      m_dxvkDevice->requestCompileShader(shader);

    EmitCs([
      cShader = std::move(shader)
    ] (DxvkContext* ctx) mutable {
      constexpr VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
      ctx->bindShader<stage>(std::move(cShader));
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

        std::array<DxvkVertexInput, 2 * caps::InputRegisterCount> attrList = { };
        std::array<DxvkVertexInput, 2 * caps::InputRegisterCount> bindList = { };
        std::array<uint32_t, 2 * caps::InputRegisterCount> vertexSizes = { };

        uint32_t attrMask = 0;
        uint32_t bindMask = 0;

        const auto& isgn = cVertexShader != nullptr
          ? GetCommonShader(cVertexShader)->GetIsgn()
          : GetFixedFunctionIsgn();

        for (uint32_t i = 0; i < isgn.elemCount; i++) {
          const auto& decl = isgn.elems[i];

          DxvkVertexAttribute attrib = { };
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

          attrList[i] = DxvkVertexInput(attrib);

          vertexSizes[attrib.binding] = std::max(vertexSizes[attrib.binding],
            uint32_t(attrib.offset + lookupFormatInfo(attrib.format)->elementSize));

          DxvkVertexBinding binding = { };
          binding.binding = attrib.binding;
          binding.extent = vertexSizes[attrib.binding];

          uint32_t instanceData = cStreamFreq[binding.binding % caps::MaxStreams];
          if (instanceData & D3DSTREAMSOURCE_INSTANCEDATA) {
            binding.divisor = instanceData & 0x7FFFFF; // Remove instance packed-in flags in the data.
            binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
          }
          else {
            binding.divisor = 0u;
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
          }

          bindList[binding.binding] = DxvkVertexInput(binding);

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
    ] (DxvkContext* ctx) mutable {
      ctx->bindVertexBuffer(cSlotId, std::move(cBufferSlice), cStride);
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
    ](DxvkContext* ctx) mutable {
      ctx->bindIndexBuffer(std::move(cBufferSlice), cIndexType);
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
        : ConsiderFlush(GpuFlushType::ImplicitStrongHint);
    } else if (pQuery->IsStalling()) {
      ConsiderFlush(GpuFlushType::ImplicitWeakHint);
    }
  }


  void D3D9DeviceEx::SetVertexBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.vsConsts->bConsts[idx] &= ~mask;
    m_state.vsConsts->bConsts[idx] |= bits & mask;

    m_consts[DxsoProgramTypes::VertexShader].dirty = true;
  }


  void D3D9DeviceEx::SetPixelBoolBitfield(uint32_t idx, uint32_t mask, uint32_t bits) {
    m_state.psConsts->bConsts[idx] &= ~mask;
    m_state.psConsts->bConsts[idx] |= bits & mask;

    m_consts[DxsoProgramTypes::PixelShader].dirty = true;
  }


  HRESULT D3D9DeviceEx::CreateShaderModule(
        D3D9CommonShader*     pShaderModule,
        uint32_t*             pLength,
        VkShaderStageFlagBits ShaderStage,
  const DWORD*                pShaderBytecode,
  const DxsoModuleInfo*       pModuleInfo) {
    try {
      m_shaderModules->GetShaderModule(this, pShaderModule,
        pLength, ShaderStage, pModuleInfo, pShaderBytecode);

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

    // Error out in case of StartRegister + Count overflow
    if (unlikely(StartRegister > std::numeric_limits<uint32_t>::max() - Count))
      return D3DERR_INVALIDCALL;

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

    D3D9ConstantSets& constSet = m_consts[ProgramType];

    if constexpr (ConstantType == D3D9ConstantType::Float) {
      constSet.maxChangedConstF = std::max(constSet.maxChangedConstF, StartRegister + Count);
    } else if constexpr (ConstantType == D3D9ConstantType::Int && ProgramType == DxsoProgramType::VertexShader) {
      // We only track changed int constants for vertex shaders (and it's only used when the device uses the SWVP UBO layout).
      // Pixel shaders (and vertex shaders on HWVP devices) always copy all int constants into the same UBO as the float constants
      constSet.maxChangedConstI = std::max(constSet.maxChangedConstI, StartRegister + Count);
    } else  if constexpr (ConstantType == D3D9ConstantType::Bool && ProgramType == DxsoProgramType::VertexShader) {
      // We only track changed bool constants for vertex shaders (and it's only used when the device uses the SWVP UBO layout).
      // Pixel shaders (and vertex shaders on HWVP devices) always put all bool constants into a single spec constant.
      constSet.maxChangedConstB = std::max(constSet.maxChangedConstB, StartRegister + Count);
    }

    if constexpr (ConstantType != D3D9ConstantType::Bool) {
      uint32_t maxCount = ConstantType == D3D9ConstantType::Float
        ? constSet.meta.maxConstIndexF
        : constSet.meta.maxConstIndexI;

      constSet.dirty |= StartRegister < maxCount;
    } else if constexpr (ProgramType == DxsoProgramType::VertexShader) {
      if (unlikely(CanSWVP())) {
        constSet.dirty |= StartRegister < constSet.meta.maxConstIndexB;
      }
    }

    UpdateStateConstants<ProgramType, ConstantType, T>(
      &m_state,
      StartRegister,
      pConstantData,
      Count,
      m_d3d9Options.d3d9FloatEmulation == D3D9FloatEmulation::Enabled);

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
          if (m_state.enabledLightIndices[i] != std::numeric_limits<uint32_t>::max())
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
        key.Data.Contents.Projected       |= ((m_state.textureStages[i][DXVK_TSS_TEXTURETRANSFORMFLAGS] & D3DTTFF_PROJECTED) == D3DTTFF_PROJECTED) << i;
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
        ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(shader.GetShader());
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

      auto mapPtr = m_vsFixedFunction.AllocSlice();

      auto WorldView    = m_state.transforms[GetTransformIndex(D3DTS_VIEW)] * m_state.transforms[GetTransformIndex(D3DTS_WORLD)];
      auto NormalMatrix = inverse(WorldView);

      D3D9FixedFunctionVS* data = reinterpret_cast<D3D9FixedFunctionVS*>(mapPtr);
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
        if (idx == std::numeric_limits<uint32_t>::max())
          continue;

        data->Lights[lightIdx++] = D3D9Light(m_state.lights[idx].value(), m_state.transforms[GetTransformIndex(D3DTS_VIEW)]);
      }

      data->Material = m_state.material;
      data->TweenFactor = bit::cast<float>(m_state.renderStates[D3DRS_TWEENFACTOR]);
    }

    if (m_flags.test(D3D9DeviceFlag::DirtyFFVertexBlend) && vertexBlendMode == D3D9FF_VertexBlendMode_Normal) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFVertexBlend);

      auto mapPtr = m_vsVertexBlend.AllocSlice();
      auto UploadVertexBlendData = [&](auto data) {
        for (uint32_t i = 0; i < std::size(data->WorldView); i++)
          data->WorldView[i] = m_state.transforms[GetTransformIndex(D3DTS_VIEW)] * m_state.transforms[GetTransformIndex(D3DTS_WORLDMATRIX(i))];
      };

      (m_isSWVP && indexedVertexBlend)
        ? UploadVertexBlendData(reinterpret_cast<D3D9FixedFunctionVertexBlendDataSW*>(mapPtr))
        : UploadVertexBlendData(reinterpret_cast<D3D9FixedFunctionVertexBlendDataHW*>(mapPtr));
    }
  }


  void D3D9DeviceEx::UpdateFixedFunctionPS() {
    // Shader...
    if (m_flags.test(D3D9DeviceFlag::DirtyFFPixelShader) || m_lastSamplerTypesFF != m_textureTypes) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFPixelShader);
      m_lastSamplerTypesFF = m_textureTypes;

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

        stage.TextureBound = m_state.textures[idx] != nullptr ? 1 : 0;

        stage.ColorOp = data[DXVK_TSS_COLOROP];
        stage.AlphaOp = data[DXVK_TSS_ALPHAOP];

        stage.ColorArg0 = data[DXVK_TSS_COLORARG0];
        stage.ColorArg1 = data[DXVK_TSS_COLORARG1];
        stage.ColorArg2 = data[DXVK_TSS_COLORARG2];

        stage.AlphaArg0 = data[DXVK_TSS_ALPHAARG0];
        stage.AlphaArg1 = data[DXVK_TSS_ALPHAARG1];
        stage.AlphaArg2 = data[DXVK_TSS_ALPHAARG2];

        const uint32_t samplerOffset = idx * 2;
        stage.Type         = (m_textureTypes >> samplerOffset) & 0xffu;
        stage.ResultIsTemp = data[DXVK_TSS_RESULTARG] == D3DTA_TEMP;

        uint32_t ttff  = data[DXVK_TSS_TEXTURETRANSFORMFLAGS];
        uint32_t count = ttff & ~D3DTTFF_PROJECTED;

        stage.Projected      = (ttff & D3DTTFF_PROJECTED) ? 1      : 0;
        stage.ProjectedCount = (ttff & D3DTTFF_PROJECTED) ? count  : 0;

        stage.SampleDref = (m_depthTextures & (1 << idx)) != 0;
      }

      auto& stage0 = key.Stages[0].Contents;

      if (stage0.ResultIsTemp &&
          stage0.ColorOp != D3DTOP_DISABLE &&
          stage0.AlphaOp == D3DTOP_DISABLE) {
        stage0.AlphaOp   = D3DTOP_SELECTARG1;
        stage0.AlphaArg1 = D3DTA_DIFFUSE;
      }

      stage0.GlobalSpecularEnable = m_state.renderStates[D3DRS_SPECULARENABLE];

      // The last stage *always* writes to current.
      if (idx >= 1)
        key.Stages[idx - 1].Contents.ResultIsTemp = false;

      EmitCs([
        this,
        cKey     = key,
       &cShaders = m_ffModules
      ](DxvkContext* ctx) {
        auto shader = cShaders.GetShaderModule(this, cKey);
        ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(shader.GetShader());
      });
    }

    // Constants

    if (m_flags.test(D3D9DeviceFlag::DirtyFFPixelData)) {
      m_flags.clr(D3D9DeviceFlag::DirtyFFPixelData);

      auto mapPtr = m_psFixedFunction.AllocSlice();
      auto& rs = m_state.renderStates;

      D3D9FixedFunctionPS* data = reinterpret_cast<D3D9FixedFunctionPS*>(mapPtr);
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
    DecodeMultiSampleType(m_dxvkDevice, dstDesc->MultiSample, dstDesc->MultisampleQuality, &dstSampleCount);

    if (unlikely(dstSampleCount != VK_SAMPLE_COUNT_1_BIT)) {
      Logger::warn("D3D9DeviceEx::ResolveZ: dstSampleCount != 1. Discarding.");
      return;
    }

    const D3D9_VK_FORMAT_MAPPING srcFormatInfo = LookupFormat(srcDesc->Format);
    const D3D9_VK_FORMAT_MAPPING dstFormatInfo = LookupFormat(dstDesc->Format);

    VkImageSubresource dstSubresource =
      dstTextureInfo->GetSubresourceFromIndex(
        dstFormatInfo.Aspect, 0);

    VkImageSubresource srcSubresource =
      srcTextureInfo->GetSubresourceFromIndex(
        srcFormatInfo.Aspect, src->GetSubresource());

    if ((dstSubresource.aspectMask & srcSubresource.aspectMask) != 0) {
      // for depthStencil -> depth or depthStencil -> stencil copies, only copy the aspect that both images support
      dstSubresource.aspectMask = dstSubresource.aspectMask & srcSubresource.aspectMask;
      srcSubresource.aspectMask = dstSubresource.aspectMask & srcSubresource.aspectMask;
    } else if (unlikely(dstSubresource.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT && srcSubresource.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT)) {
      Logger::err(str::format("D3D9DeviceEx::ResolveZ: Trying to blit from ",
        srcFormatInfo.FormatColor, " (aspect ", srcSubresource.aspectMask, ")", " to ",
        dstFormatInfo.FormatColor, " (aspect ", dstSubresource.aspectMask, ")"
      ));
      return;
    }

    const VkImageSubresourceLayers dstSubresourceLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };

    const VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    VkSampleCountFlagBits srcSampleCount;
    DecodeMultiSampleType(m_dxvkDevice, srcDesc->MultiSample, srcDesc->MultisampleQuality, &srcSampleCount);

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
        VkImageResolve region;
        region.srcSubresource = cSrcSubres;
        region.srcOffset      = VkOffset3D { 0, 0, 0 };
        region.dstSubresource = cDstSubres;
        region.dstOffset      = VkOffset3D { 0, 0, 0 };
        region.extent         = cDstImage->mipLevelExtent(cDstSubres.mipLevel);

        ctx->resolveImage(cDstImage, cSrcImage, region, cSrcImage->info().format,
          VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
      });
    }

    dstTextureInfo->MarkAllNeedReadback();
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


  void D3D9DeviceEx::ResetState(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    SetDepthStencilSurface(nullptr);

    for (uint32_t i = 0; i < caps::MaxSimultaneousRenderTargets; i++)
      SetRenderTargetInternal(i, nullptr);

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

    const VkPhysicalDeviceLimits& limits = m_dxvkDevice->adapter()->deviceProperties().limits;

    rs[D3DRS_POINTSPRITEENABLE]          = FALSE;
    rs[D3DRS_POINTSCALEENABLE]           = FALSE;
    rs[D3DRS_POINTSCALE_A]               = bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSCALE_B]               = bit::cast<DWORD>(0.0f);
    rs[D3DRS_POINTSCALE_C]               = bit::cast<DWORD>(0.0f);
    rs[D3DRS_POINTSIZE]                  = bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSIZE_MIN]              = m_isD3D8Compatible ? bit::cast<DWORD>(0.0f) : bit::cast<DWORD>(1.0f);
    rs[D3DRS_POINTSIZE_MAX]              = bit::cast<DWORD>(limits.pointSizeRange[1]);
    UpdatePushConstant<D3D9RenderStateItem::PointSize>();
    UpdatePushConstant<D3D9RenderStateItem::PointSizeMin>();
    UpdatePushConstant<D3D9RenderStateItem::PointSizeMax>();
    m_flags.set(D3D9DeviceFlag::DirtyPointScale);
    UpdatePointMode(false);

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

    for (uint32_t i = 0; i < m_state.textures->size(); i++) {
      SetStateTexture(i, nullptr);
    }

    EmitCs([
      cSize = m_state.textures->size()
    ](DxvkContext* ctx) {
      VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      for (uint32_t i = 0; i < cSize; i++) {
        auto samplerInfo = RemapStateSamplerShader(DWORD(i));
        uint32_t slot = computeResourceSlotId(samplerInfo.first, DxsoBindingType::Image, uint32_t(samplerInfo.second));
        ctx->bindResourceImageView(stage, slot, nullptr);
      }
    });

    m_dirtyTextures = 0;
    m_depthTextures = 0;
    m_cubeTextures = 0;

    auto& ss = m_state.samplerStates.get();
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

    UpdatePixelShaderSamplerSpec(0u, 0u, 0u);
    UpdateVertexBoolSpec(0u);
    UpdatePixelBoolSpec(0u);
    UpdateCommonSamplerSpec(0u, 0u, 0u);

    UpdateAnyColorWrites<0>();
    UpdateAnyColorWrites<1>();
    UpdateAnyColorWrites<2>();
    UpdateAnyColorWrites<3>();

    SetIndices(nullptr);
    for (uint32_t i = 0; i < caps::MaxStreams; i++) {
      SetStreamSource(i, nullptr, 0, 0);
    }
  }


  HRESULT D3D9DeviceEx::ResetSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    D3D9Format backBufferFmt = EnumerateFormat(pPresentationParameters->BackBufferFormat);
    bool unlockedFormats = m_implicitSwapchain != nullptr && m_implicitSwapchain->HasFormatsUnlocked();

    Logger::info(str::format(
      "D3D9DeviceEx::ResetSwapChain:\n",
      "  Requested Presentation Parameters\n",
      "    - Width:              ", pPresentationParameters->BackBufferWidth, "\n",
      "    - Height:             ", pPresentationParameters->BackBufferHeight, "\n",
      "    - Format:             ", backBufferFmt, "\n"
      "    - Auto Depth Stencil: ", pPresentationParameters->EnableAutoDepthStencil ? "true" : "false", "\n",
      "                ^ Format: ", EnumerateFormat(pPresentationParameters->AutoDepthStencilFormat), "\n",
      "    - Windowed:           ", pPresentationParameters->Windowed ? "true" : "false", "\n",
      "    - Swap effect:        ", pPresentationParameters->SwapEffect, "\n"));

    // Black Desert creates a D3DDEVTYPE_NULLREF device and
    // expects this validation to not prevent a swapchain reset.
    if (likely(m_deviceType != D3DDEVTYPE_NULLREF) &&
        unlikely(!pPresentationParameters->Windowed &&
                 (pPresentationParameters->BackBufferWidth  == 0
               || pPresentationParameters->BackBufferHeight == 0))) {
      return D3DERR_INVALIDCALL;
    }

    if (backBufferFmt != D3D9Format::Unknown && !unlockedFormats) {
      if (!IsSupportedBackBufferFormat(backBufferFmt)) {
        Logger::err(str::format("D3D9DeviceEx::ResetSwapChain: Unsupported backbuffer format: ",
          EnumerateFormat(pPresentationParameters->BackBufferFormat)));
        return D3DERR_INVALIDCALL;
      }
    }

    if (m_implicitSwapchain != nullptr) {
      HRESULT hr = m_implicitSwapchain->Reset(pPresentationParameters, pFullscreenDisplayMode);
      if (FAILED(hr))
        return hr;
    }
    else {
      m_implicitSwapchain = new D3D9SwapChainEx(this, pPresentationParameters, pFullscreenDisplayMode, true);
      m_mostRecentlyUsedSwapchain = m_implicitSwapchain.ptr();
    }

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
      desc.Discard            = (pPresentationParameters->Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL) != 0;
      desc.MultiSample        = pPresentationParameters->MultiSampleType;
      desc.MultisampleQuality = pPresentationParameters->MultiSampleQuality;
      desc.IsBackBuffer       = FALSE;
      desc.IsAttachmentOnly   = TRUE;
      desc.IsLockable         = IsLockableDepthStencilFormat(desc.Format);

      if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(this, D3DRTYPE_SURFACE, &desc)))
        return D3DERR_NOTAVAILABLE;

      m_autoDepthStencil = new D3D9Surface(this, &desc, IsExtended(), nullptr, nullptr);
      m_initializer->InitTexture(m_autoDepthStencil->GetCommonTexture());
      SetDepthStencilSurface(m_autoDepthStencil.ptr());
      m_losableResourceCounter++;
    }

    SetRenderTarget(0, m_implicitSwapchain->GetBackBuffer(0));

    // Force this if we end up binding the same RT to make scissor change go into effect.
    BindViewportAndScissor();

    return D3D_OK;
  }


  HRESULT D3D9DeviceEx::InitialReset(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    ResetState(pPresentationParameters);

    HRESULT hr = ResetSwapChain(pPresentationParameters, pFullscreenDisplayMode);
    if (FAILED(hr))
      return hr;

    Flush();
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);

    return D3D_OK;
  }

  void D3D9DeviceEx::TrackBufferMappingBufferSequenceNumber(
        D3D9CommonBuffer* pResource) {
    uint64_t sequenceNumber = GetCurrentSequenceNumber();
    pResource->TrackMappingBufferSequenceNumber(sequenceNumber);
  }

  void D3D9DeviceEx::TrackTextureMappingBufferSequenceNumber(
      D3D9CommonTexture* pResource,
      UINT Subresource) {
    uint64_t sequenceNumber = GetCurrentSequenceNumber();
    pResource->TrackMappingBufferSequenceNumber(Subresource, sequenceNumber);
  }

  uint64_t D3D9DeviceEx::GetCurrentSequenceNumber() {
    // We do not flush empty chunks, so if we are tracking a resource
    // immediately after a flush, we need to use the sequence number
    // of the previously submitted chunk to prevent deadlocks.
    return m_csChunk->empty() ? m_csSeqNum : m_csSeqNum + 1;
  }


  void* D3D9DeviceEx::MapTexture(D3D9CommonTexture* pTexture, UINT Subresource) {
    // Will only be called inside the device lock
    void *ptr = pTexture->GetData(Subresource);

#ifdef D3D9_ALLOW_UNMAPPING
    if (likely(pTexture->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE)) {
      m_mappedTextures.insert(pTexture);
    }
#endif

    return ptr;
  }

  void D3D9DeviceEx::TouchMappedTexture(D3D9CommonTexture* pTexture) {
#ifdef D3D9_ALLOW_UNMAPPING
    if (pTexture->GetMapMode() != D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE)
      return;

    D3D9DeviceLock lock = LockDevice();
    m_mappedTextures.touch(pTexture);
#endif
  }

  void D3D9DeviceEx::RemoveMappedTexture(D3D9CommonTexture* pTexture) {
#ifdef D3D9_ALLOW_UNMAPPING
    if (pTexture->GetMapMode() != D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE)
      return;

    D3D9DeviceLock lock = LockDevice();
    m_mappedTextures.remove(pTexture);
#endif
  }

  void D3D9DeviceEx::UnmapTextures() {
    // Will only be called inside the device lock

#ifdef D3D9_ALLOW_UNMAPPING
    uint32_t mappedMemory = m_memoryAllocator.MappedMemory();
    if (likely(mappedMemory < uint32_t(m_d3d9Options.textureMemory)))
      return;

    uint32_t threshold = (m_d3d9Options.textureMemory / 4) * 3;

    auto iter = m_mappedTextures.leastRecentlyUsedIter();
    while (m_memoryAllocator.MappedMemory() >= threshold && iter != m_mappedTextures.leastRecentlyUsedEndIter()) {
      if (unlikely((*iter)->IsAnySubresourceLocked() != 0)) {
        iter++;
        continue;
      }
      (*iter)->UnmapData();

      iter = m_mappedTextures.remove(iter);
    }
#endif
  }

  ////////////////////////////////////
  // D3D9 Device Lost
  ////////////////////////////////////

  void D3D9DeviceEx::NotifyFullscreen(HWND window, bool fullscreen) {
    D3D9DeviceLock lock = LockDevice();

    if (fullscreen) {
      if (unlikely(window != m_fullscreenWindow && m_fullscreenWindow != NULL)) {
        Logger::warn("Multiple fullscreen windows detected.");
      }
      m_fullscreenWindow = window;
    } else {
      if (unlikely(m_fullscreenWindow != window)) {
        Logger::warn("Window was not fullscreen in the first place.");
      } else {
        m_fullscreenWindow = 0;
      }
    }
  }

  void D3D9DeviceEx::NotifyWindowActivated(HWND window, bool activated) {
    D3D9DeviceLock lock = LockDevice();

    if (likely(!m_d3d9Options.deviceLossOnFocusLoss || IsExtended()))
      return;

    if (activated && m_deviceLostState == D3D9DeviceLostState::Lost) {
      Logger::info("Device not reset");
      m_deviceLostState = D3D9DeviceLostState::NotReset;
    } else if (!activated && m_deviceLostState != D3D9DeviceLostState::Lost && m_fullscreenWindow == window) {
      Logger::info("Device lost");
      m_deviceLostState = D3D9DeviceLostState::Lost;
      m_fullscreenWindow = NULL;
    }
  }

  ////////////////////////////////////
  // D3D9 Device Specialization State
  ////////////////////////////////////

  void D3D9DeviceEx::UpdateAlphaTestSpec(VkCompareOp alphaOp, uint32_t precision) {
    bool dirty  = m_specInfo.set<SpecAlphaCompareOp>(uint32_t(alphaOp));
         dirty |= m_specInfo.set<SpecAlphaPrecisionBits>(precision);

    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdateVertexBoolSpec(uint32_t value) {
    if (m_specInfo.set<SpecVertexShaderBools>(value))
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdatePixelBoolSpec(uint32_t value) {
    if (m_specInfo.set<SpecPixelShaderBools>(value))
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdatePixelShaderSamplerSpec(uint32_t types, uint32_t projections, uint32_t fetch4) {
    bool dirty  = m_specInfo.set<SpecSamplerType>(types);
         dirty |= m_specInfo.set<SpecProjectionType>(projections);
         dirty |= m_specInfo.set<SpecFetch4>(fetch4);

    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdateCommonSamplerSpec(uint32_t nullMask, uint32_t depthMask, uint32_t drefMask) {
    bool dirty  = m_specInfo.set<SpecSamplerDepthMode>(depthMask);
         dirty |= m_specInfo.set<SpecSamplerNull>(nullMask);
         dirty |= m_specInfo.set<SpecDrefClamp>(drefMask);

    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdatePointModeSpec(uint32_t mode) {
    if (m_specInfo.set<SpecPointMode>(mode))
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::UpdateFogModeSpec(bool fogEnabled, D3DFOGMODE vertexFogMode, D3DFOGMODE pixelFogMode) {
    bool dirty  = m_specInfo.set<SpecFogEnabled>(fogEnabled);
         dirty |= m_specInfo.set<SpecVertexFogMode>(vertexFogMode);
         dirty |= m_specInfo.set<SpecPixelFogMode>(pixelFogMode);

    if (dirty)
      m_flags.set(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  void D3D9DeviceEx::BindSpecConstants() {
    if (!m_flags.test(D3D9DeviceFlag::DirtySpecializationEntries))
      return;

    EmitCs([cSpecInfo = m_specInfo](DxvkContext* ctx) {
      for (size_t i = 0; i < cSpecInfo.data.size(); i++)
        ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, i, cSpecInfo.data[i]);
    });

    // Write spec constants into buffer for fast-linked pipelines to use it.
    if (m_usingGraphicsPipelines) {
      // TODO: Make uploading specialization information less naive.
      auto mapPtr = m_specBuffer.AllocSlice();
      memcpy(mapPtr, m_specInfo.data.data(), D3D9SpecializationInfo::UBOSize);
    }

    m_flags.clr(D3D9DeviceFlag::DirtySpecializationEntries);
  }


  GpuFlushType D3D9DeviceEx::GetMaxFlushType() const {
    if (m_d3d9Options.reproducibleCommandStream)
      return GpuFlushType::ExplicitFlush;
    else if (m_dxvkDevice->perfHints().preferRenderPassOps)
      return GpuFlushType::ImplicitStrongHint;
    else
      return GpuFlushType::ImplicitWeakHint;
  }

}
