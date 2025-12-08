#include <algorithm>

#include "d3d11_context_imm.h"
#include "d3d11_video.h"

#include <d3d11_video_blit_frag.h>
#include <d3d11_video_blit_vert.h>

#include "../dxvk/dxvk_shader_spirv.h"

namespace dxvk {

  D3D11VideoProcessorEnumerator::D3D11VideoProcessorEnumerator(
          D3D11Device*            pDevice,
    const D3D11_VIDEO_PROCESSOR_CONTENT_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorEnumerator>(pDevice),
    m_desc(Desc), m_destructionNotifier(this) {

  }


  D3D11VideoProcessorEnumerator::~D3D11VideoProcessorEnumerator() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11VideoProcessorEnumerator)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorEnumerator), riid)) {
      Logger::warn("D3D11VideoProcessorEnumerator::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorContentDesc(
          D3D11_VIDEO_PROCESSOR_CONTENT_DESC* pContentDesc) {
    *pContentDesc = m_desc;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::CheckVideoProcessorFormat(
          DXGI_FORMAT             Format,
          UINT*                   pFlags) {
    Logger::warn(str::format("D3D11VideoProcessorEnumerator::CheckVideoProcessorFormat: stub, format ", Format));

    if (!pFlags)
      return E_INVALIDARG;

    *pFlags = D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT | D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCaps(
          D3D11_VIDEO_PROCESSOR_CAPS* pCaps) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoProcessorEnumerator::GetVideoProcessorCaps: semi-stub");

    if (!pCaps)
      return E_INVALIDARG;

    *pCaps = {};
    pCaps->RateConversionCapsCount = 1;
    pCaps->MaxInputStreams = 52;
    pCaps->MaxStreamStates = 52;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps(
          UINT                    TypeIndex,
          D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS* pCaps) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps: semi-stub");

    if (!pCaps || TypeIndex)
      return E_INVALIDARG;

    *pCaps = {};
    if (m_desc.InputFrameFormat == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE) {
      pCaps->ProcessorCaps = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION;
    } else {
      pCaps->ProcessorCaps = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB;
      pCaps->PastFrames = 1;
      pCaps->FutureFrames = 1;
    }
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate(
          UINT                    TypeIndex,
          UINT                    CustomRateIndex,
          D3D11_VIDEO_PROCESSOR_CUSTOM_RATE* pRate) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate: Stub");

    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange(
          D3D11_VIDEO_PROCESSOR_FILTER        Filter,
          D3D11_VIDEO_PROCESSOR_FILTER_RANGE* pRange) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange: Stub");

    return E_NOTIMPL;
  }




  D3D11VideoProcessor::D3D11VideoProcessor(
          D3D11Device*                    pDevice,
          D3D11VideoProcessorEnumerator*  pEnumerator,
          UINT                            RateConversionIndex)
  : D3D11DeviceChild<ID3D11VideoProcessor>(pDevice),
    m_enumerator(pEnumerator), m_rateConversionIndex(RateConversionIndex),
    m_destructionNotifier(this) {

  }


  D3D11VideoProcessor::~D3D11VideoProcessor() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessor::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11VideoProcessor)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessor), riid)) {
      Logger::warn("D3D11VideoProcessor::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessor::GetContentDesc(
          D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc) {
    m_enumerator->GetVideoProcessorContentDesc(pDesc);
  }


  void STDMETHODCALLTYPE D3D11VideoProcessor::GetRateConversionCaps(
          D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps) {
    m_enumerator->GetVideoProcessorRateConversionCaps(m_rateConversionIndex, pCaps);
  }




  D3D11VideoProcessorView::D3D11VideoProcessorView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
          DxvkImageViewKey        viewInfo)
  : m_resource(pResource), m_image(GetCommonTexture(pResource)->GetImage()) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);
    DXGI_VK_FORMAT_FAMILY formatFamily = pDevice->LookupFamily(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    VkImageAspectFlags aspectMask = lookupFormatInfo(formatInfo.Format)->aspectMask;

    viewInfo.format = formatInfo.Format;
    viewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(formatInfo.Swizzle);
    viewInfo.aspects = aspectMask;

    m_layers.aspectMask = aspectMask;
    m_layers.baseArrayLayer = viewInfo.layerIndex;
    m_layers.layerCount = viewInfo.layerCount;
    m_layers.mipLevel = viewInfo.mipIndex;

    // Create shadow image if we know that the base image is incompatible
    // with the required usage flags and cannot be relocated.
    if (m_image->info().shared && (m_image->info().usage & viewInfo.usage) != viewInfo.usage) {
      DxvkImageCreateInfo imageInfo = { };
      imageInfo.type = m_image->info().type;
      imageInfo.format = viewInfo.format;
      imageInfo.sampleCount = m_image->info().sampleCount;
      imageInfo.extent = m_image->mipLevelExtent(viewInfo.mipIndex);
      imageInfo.numLayers = viewInfo.layerCount;
      imageInfo.mipLevels = viewInfo.mipCount;
      imageInfo.usage = viewInfo.usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      imageInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      imageInfo.access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
      imageInfo.debugName = "Video shadow image";

      if (viewInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        imageInfo.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageInfo.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        imageInfo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }

      if (viewInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        imageInfo.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        imageInfo.access |= VK_ACCESS_SHADER_READ_BIT;

        if (imageInfo.layout != VK_IMAGE_LAYOUT_GENERAL)
          imageInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }

      if (viewInfo.aspects != VK_IMAGE_ASPECT_COLOR_BIT) {
        imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
                        |  VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
      }

      m_shadow = pDevice->GetDXVKDevice()->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      viewInfo.layerIndex = 0u;
      viewInfo.mipIndex = 0u;
    }

    if (viewInfo.usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      viewInfo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    for (uint32_t i = 0; aspectMask && i < m_views.size(); i++) {
      viewInfo.aspects = vk::getNextAspect(aspectMask);

      if (viewInfo.aspects != VK_IMAGE_ASPECT_COLOR_BIT)
        viewInfo.format = formatFamily.Formats[i];

      m_views[i] = (m_shadow ? m_shadow : m_image)->createView(viewInfo);
    }

    m_isYCbCr = IsYCbCrFormat(resourceDesc.Format);
  }


  D3D11VideoProcessorView::~D3D11VideoProcessorView() {

  }


  bool D3D11VideoProcessorView::IsYCbCrFormat(DXGI_FORMAT Format) {
    static const std::array<DXGI_FORMAT, 3> s_formats = {{
      DXGI_FORMAT_NV12,
      DXGI_FORMAT_YUY2,
      DXGI_FORMAT_AYUV,
    }};

    return std::find(s_formats.begin(), s_formats.end(), Format) != s_formats.end();
  }



  D3D11VideoProcessorInputView::D3D11VideoProcessorInputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorInputView>(pDevice),
    m_common(pDevice, pResource, CreateViewInfo(Desc)),
    m_desc(Desc), m_destructionNotifier(this) {

  }


  D3D11VideoProcessorInputView::~D3D11VideoProcessorInputView() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorInputView::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11VideoProcessorInputView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorInputView), riid)) {
      Logger::warn("D3D11VideoProcessorInputView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_common.GetResource();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }


  DxvkImageViewKey D3D11VideoProcessorInputView::CreateViewInfo(
    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC& Desc) {
    DxvkImageViewKey viewInfo = { };
    viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    switch (Desc.ViewDimension) {
      case D3D11_VPIV_DIMENSION_TEXTURE2D:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.mipIndex   = Desc.Texture2D.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = Desc.Texture2D.ArraySlice;
        viewInfo.layerCount = 1;
        break;

      case D3D11_VPIV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    return viewInfo;
  }


  D3D11VideoProcessorOutputView::D3D11VideoProcessorOutputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorOutputView>(pDevice),
    m_common(pDevice, pResource, CreateViewInfo(Desc)),
    m_desc(Desc), m_destructionNotifier(this) {

  }


  D3D11VideoProcessorOutputView::~D3D11VideoProcessorOutputView() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorOutputView::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11VideoProcessorOutputView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorOutputView), riid)) {
      Logger::warn("D3D11VideoProcessorOutputView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_common.GetResource();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }


  DxvkImageViewKey D3D11VideoProcessorOutputView::CreateViewInfo(
    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC& Desc) {
    DxvkImageViewKey viewInfo = { };
    viewInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    switch (Desc.ViewDimension) {
      case D3D11_VPOV_DIMENSION_TEXTURE2D:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.mipIndex   = Desc.Texture2D.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = 0;
        viewInfo.layerCount = 1;
        break;

      case D3D11_VPOV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.mipIndex   = Desc.Texture2DArray.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = Desc.Texture2DArray.FirstArraySlice;
        viewInfo.layerCount = Desc.Texture2DArray.ArraySize;
        break;

      case D3D11_VPOV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    return viewInfo;
  }




  D3D11VideoContext::D3D11VideoContext(
          D3D11ImmediateContext*  pContext,
    const Rc<DxvkDevice>&         Device)
  : m_ctx(pContext), m_device(Device) {

  }


  D3D11VideoContext::~D3D11VideoContext() {

  }


  ULONG STDMETHODCALLTYPE D3D11VideoContext::AddRef() {
    return m_ctx->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D11VideoContext::Release() {
    return m_ctx->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_ctx->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetPrivateData(
          REFGUID                 Name,
          UINT*                   pDataSize,
          void*                   pData) {
    return m_ctx->GetPrivateData(Name, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SetPrivateData(
          REFGUID                 Name,
          UINT                    DataSize,
    const void*                   pData)  {
    return m_ctx->SetPrivateData(Name, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SetPrivateDataInterface(
          REFGUID                 Name,
    const IUnknown*               pUnknown) {
    return m_ctx->SetPrivateDataInterface(Name, pUnknown);
  }


  void STDMETHODCALLTYPE D3D11VideoContext::GetDevice(
          ID3D11Device**          ppDevice) {
    return m_ctx->GetDevice(ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetDecoderBuffer(
          ID3D11VideoDecoder*             pDecoder,
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
          UINT*                           BufferSize,
          void**                          ppBuffer) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::GetDecoderBuffer: Stub");

    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ReleaseDecoderBuffer(
          ID3D11VideoDecoder*             pDecoder,
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::ReleaseDecoderBuffer: Stub");

    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderBeginFrame(
          ID3D11VideoDecoder*             pDecoder,
          ID3D11VideoDecoderOutputView*   pView,
          UINT                            KeySize,
    const void*                           pKey) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::DecoderBeginFrame: Stub");

    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderEndFrame(
          ID3D11VideoDecoder*             pDecoder) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::DecoderEndFrame: Stub");

    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SubmitDecoderBuffers(
          ID3D11VideoDecoder*             pDecoder,
          UINT                            BufferCount,
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pBufferDescs) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::SubmitDecoderBuffers: Stub");

    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderExtension(
          ID3D11VideoDecoder*             pDecoder,
    const D3D11_VIDEO_DECODER_EXTENSION*  pExtension) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11VideoContext::DecoderExtension: Stub");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
    const RECT*                           pRect) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetOutputTargetRect: Stub.");

    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputTargetRectEnabled = Enable;

    if (Enable)
      state->outputTargetRect = *pRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            YCbCr,
    const D3D11_VIDEO_COLOR*              pColor) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetOutputBackgroundColor: Stub");

    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputBackgroundColorIsYCbCr = YCbCr;
    state->outputBackgroundColor = *pColor;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputColorSpace = *pColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputAlphaFillMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
          UINT                            StreamIndex) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
          SIZE                            Size) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetOutputConstriction: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputStereoMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputStereoModeEnabled = Enable;

    if (Enable)
      Logger::err("D3D11VideoContext: Stereo output not supported");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetOutputExtension: Stub");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamFrameFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_FRAME_FORMAT        Format) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->frameFormat = Format;

    if (Format != D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE)
      Logger::err(str::format("D3D11VideoContext: Unsupported frame format: ", Format));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->colorSpace = *pColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamOutputRate(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_OUTPUT_RATE Rate,
          BOOL                            Repeat,
    const DXGI_RATIONAL*                  CustomRate) {
    Logger::warn(str::format("D3D11VideoContext::VideoProcessorSetStreamOutputRate: Stub, Rate ", Rate));
    if (CustomRate)
      Logger::warn(str::format("CustomRate ", CustomRate->Numerator, "/", CustomRate->Denominator));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamSourceRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamSourceRect: Stub.");

    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->srcRectEnabled = Enable;

    if (Enable)
      state->srcRect = *pRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->dstRectEnabled = Enable;

    if (Enable)
      state->dstRect = *pRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamAlpha(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          FLOAT                           Alpha) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
    const UINT*                           pEntries) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const DXGI_RATIONAL*                  pSrcAspectRatio,
    const DXGI_RATIONAL*                  pDstAspectRatio) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          FLOAT                           Lower,
          FLOAT                           Upper) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamLumaKey: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamStereoFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
          BOOL                            LeftViewFrame0,
          BOOL                            BaseViewFrame0,
          D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
          int                             MonoOffset) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamStereoFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamAutoProcessingMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->autoProcessingEnabled = Enable;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamFilter(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_FILTER    Filter,
          BOOL                            Enable,
          int                             Level) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorSetStreamExtension: Stub");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamRotation(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          D3D11_VIDEO_PROCESSOR_ROTATION  Rotation) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->rotationEnabled = Enable;
    state->rotation = Rotation;

    if (Enable && Rotation != D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY)
      Logger::err(str::format("D3D11VideoContext: Unsupported rotation: ", Rotation));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pEnabled)
      *pEnabled = state->outputTargetRectEnabled;

    if (pRect)
      *pRect = state->outputTargetRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pYCbCr,
          D3D11_VIDEO_COLOR*              pColor) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    
    if (pYCbCr)
      *pYCbCr = state->outputBackgroundColorIsYCbCr;

    if (pColor)
      *pColor = state->outputBackgroundColor;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pColorSpace)
      *pColorSpace = state->outputColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputAlphaFillMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE* pAlphaFillMode,
          UINT*                           pStreamIndex) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          SIZE*                           pSize) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetOutputConstriction: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputStereoMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pEnabled)
      *pEnabled = state->outputStereoModeEnabled;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetOutputExtension: Stub");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamFrameFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_FRAME_FORMAT*       pFormat) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pFormat)
      *pFormat = state->frameFormat;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pColorSpace)
      *pColorSpace = state->colorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamOutputRate(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pRate,
          BOOL*                           pRepeat,
          DXGI_RATIONAL*                  pCustomRate) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamOutputRate: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamSourceRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnabled)
      *pEnabled = state->srcRectEnabled;

    if (pRect)
      *pRect = state->srcRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnabled)
      *pEnabled = state->dstRectEnabled;

    if (pRect)
      *pRect = state->dstRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamAlpha(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          FLOAT*                          pAlpha) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
          UINT*                           pEntries) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          DXGI_RATIONAL*                  pSrcAspectRatio,
          DXGI_RATIONAL*                  pDstAspectRatio) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          FLOAT*                          pLower,
          FLOAT*                          pUpper) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamLumaKey: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamStereoFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          D3D11_VIDEO_PROCESSOR_STEREO_FORMAT* pFormat,
          BOOL*                           pLeftViewFrame0,
          BOOL*                           pBaseViewFrame0,
          D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE* pFlipMode,
          int*                            pMonoOffset) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamStereoFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamAutoProcessingMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    *pEnabled = state->autoProcessingEnabled;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamFilter(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_FILTER    Filter,
          BOOL*                           pEnabled,
          int*                            pLevel) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11VideoContext::VideoProcessorGetStreamExtension: Stub");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamRotation(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnable,
          D3D11_VIDEO_PROCESSOR_ROTATION* pRotation) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnable)
      *pEnable = state->rotationEnabled;

    if (pRotation)
      *pRotation = state->rotation;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorBlt(
          ID3D11VideoProcessor*           pVideoProcessor,
          ID3D11VideoProcessorOutputView* pOutputView,
          UINT                            FrameIdx,
          UINT                            StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStreams) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    m_ctx->EmitCs([] (DxvkContext* ctx) {
      ctx->beginDebugLabel(vk::makeLabel(0x59eaff, "Video blit"));
    });

    auto videoProcessor = static_cast<D3D11VideoProcessor*>(pVideoProcessor);

    auto& outputView = static_cast<D3D11VideoProcessorOutputView*>(pOutputView)->GetCommon();
    auto views = outputView.GetViews();

    bool hasStreamsEnabled = false;

    m_dstIsYCbCr = outputView.IsYCbCr();

    for (uint32_t vi = 0; vi < views.size(); vi++) {
      if (!views[vi])
        continue;

      bool outputBound = false;

      // Resetting and restoring all context state incurs
      // a lot of overhead, so only do it as necessary
      for (uint32_t i = 0; i < StreamCount; i++) {
        auto streamState = videoProcessor->GetStreamState(i);

        if (!pStreams[i].Enable || !streamState)
          continue;

        if (!hasStreamsEnabled) {
          m_ctx->ResetDirtyTracking();
          m_ctx->ResetCommandListState();

          CopyBaseImageToShadow(outputView);

          hasStreamsEnabled = true;
        }

        if (!outputBound) {
          BindOutputView(views[vi], views[0]);
          outputBound = true;
        }

        if (!views[1])
          m_exportMode = ExportRGBA;
        else if (!vi)
          m_exportMode = ExportY;
        else
          m_exportMode = ExportCbCr;

        BlitStream(streamState, &pStreams[i]);
      }
    }

    if (hasStreamsEnabled) {
      CopyShadowToBaseImage(outputView);

      UnbindResources();

      m_ctx->RestoreCommandListState();
    }

    m_ctx->EmitCs([] (DxvkContext* ctx) {
      ctx->endDebugLabel();
    });

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateCryptoSessionKeyExchange(
          ID3D11CryptoSession*            pSession,
          UINT                            DataSize,
          void*                           pData) {
    Logger::warn("D3D11VideoContext::NegotiateCryptoSessionKeyExchange: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::EncryptionBlt(
          ID3D11CryptoSession*            pSession,
          ID3D11Texture2D*                pSrcSurface,
          ID3D11Texture2D*                pDstSurface,
          UINT                            IVSize,
          void*                           pIV) {
    Logger::warn("D3D11VideoContext::EncryptionBlt: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::DecryptionBlt(
          ID3D11CryptoSession*            pSession,
          ID3D11Texture2D*                pSrcSurface,
          ID3D11Texture2D*                pDstSurface,
          D3D11_ENCRYPTED_BLOCK_INFO*     pBlockInfo,
          UINT                            KeySize,
    const void*                           pKey,
          UINT                            IVSize,
          void*                           pIV) {
    Logger::warn("D3D11VideoContext::DecryptionBlt: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::StartSessionKeyRefresh(
          ID3D11CryptoSession*            pSession,
          UINT                            RandomNumberSize,
          void*                           pRandomNumber) {
    Logger::warn("D3D11VideoContext::StartSessionKeyRefresh: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::FinishSessionKeyRefresh(
          ID3D11CryptoSession*            pSession) {
    Logger::warn("D3D11VideoContext::FinishSessionKeyRefresh: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetEncryptionBltKey(
          ID3D11CryptoSession*            pSession,
          UINT                            KeySize,
          void*                           pKey) {
    Logger::warn("D3D11VideoContext::GetEncryptionBltKey: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            DataSize,
          void*                           pData) {
    Logger::warn("D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::QueryAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          UINT                            OutputSize,
          void*                           pOutput) {
    Logger::warn("D3D11VideoContext::QueryAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ConfigureAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          D3D11_AUTHENTICATED_CONFIGURE_OUTPUT* pOutput) {
    Logger::warn("D3D11VideoContext::ConfigureAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  void D3D11VideoContext::ApplyColorMatrix(float pDst[3][4], const float pSrc[3][4]) {
    float result[3][4];

    for (uint32_t i = 0; i < 3; i++) {
      for (uint32_t j = 0; j < 4; j++) {
        result[i][j] = pSrc[i][0] * pDst[0][j]
                     + pSrc[i][1] * pDst[1][j]
                     + pSrc[i][2] * pDst[2][j]
                     + pSrc[i][3] * float(j == 3);
      }
    }

    memcpy(pDst, &result[0][0], sizeof(result));
  }


  void D3D11VideoContext::ApplyYCbCrMatrix(float pColorMatrix[3][4], bool UseBt709) {
    static const float pretransform[3][4] = {
      { 0.0f, 1.0f, 0.0f,  0.0f },
      { 0.0f, 0.0f, 1.0f, -0.5f },
      { 1.0f, 0.0f, 0.0f, -0.5f },
    };

    static const float bt601[3][4] = {
      { 1.0f,  0.000000f,  1.402000f, 0.0f },
      { 1.0f, -0.344136f, -0.714136f, 0.0f },
      { 1.0f,  1.772000f,  0.000000f, 0.0f },
    };

    static const float bt709[3][4] = {
      { 1.0f,  0.000000f,  1.574800f, 0.0f },
      { 1.0f, -0.187324f, -0.468124f, 0.0f },
      { 1.0f,  1.855600f,  0.000000f, 0.0f },
    };

    ApplyColorMatrix(pColorMatrix, pretransform);
    ApplyColorMatrix(pColorMatrix, UseBt709 ? bt709 : bt601);
  }


  void D3D11VideoContext::BindOutputView(
          Rc<DxvkImageView>               View,
          Rc<DxvkImageView>               FirstView) {
    VkExtent3D viewExtent = View->mipLevelExtent(0);
    m_dstExtent = { viewExtent.width, viewExtent.height };

    VkExtent3D firstExtent = FirstView->mipLevelExtent(0);
    m_dstSizeFact[0] = (float) viewExtent.width  / (float) firstExtent.width;
    m_dstSizeFact[1] = (float) viewExtent.height / (float) firstExtent.height;

    m_ctx->EmitCs([
      cView   = std::move(View)
    ] (DxvkContext* ctx) {
      DxvkImageUsageInfo usage = { };
      usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      usage.stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      usage.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      ctx->ensureImageCompatibility(cView->image(), usage);

      DxvkRenderTargets rt;
      rt.color[0].view = cView;

      ctx->bindRenderTargets(std::move(rt), 0u);

      DxvkInputAssemblyState iaState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
      ctx->setInputAssemblyState(iaState);
    });
  }


  void D3D11VideoContext::BlitStream(
    const D3D11VideoProcessorStreamState* pStreamState,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStream) {
    CreateResources();

    if (pStream->PastFrames || pStream->FutureFrames)
      Logger::err("D3D11VideoContext: Ignoring non-zero PastFrames and FutureFrames");

    if (pStream->OutputIndex)
      Logger::err("D3D11VideoContext: Ignoring non-zero OutputIndex");

    if (pStream->InputFrameOrField)
      Logger::err("D3D11VideoContext: Ignoring non-zero InputFrameOrField");

    auto& view = static_cast<D3D11VideoProcessorInputView*>(pStream->pInputSurface)->GetCommon();

    CopyBaseImageToShadow(view);

    m_ctx->EmitCs([this,
      cStreamState  = *pStreamState,
      cImage        = view.GetImage(),
      cViews        = view.GetViews(),
      cSrcIsYCbCr   = view.IsYCbCr(),
      cDstIsYCbCr   = m_dstIsYCbCr,
      cDstExtent    = m_dstExtent,
      cDstSizeFactX = m_dstSizeFact[0],
      cDstSizeFactY = m_dstSizeFact[1],
      cExportMode   = m_exportMode
    ] (DxvkContext* ctx) {
      DxvkImageUsageInfo usage = { };
      usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      usage.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      usage.access = VK_ACCESS_SHADER_READ_BIT;

      ctx->ensureImageCompatibility(cImage, usage);

      VkViewport viewport;
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = float(cDstExtent.width);
      viewport.height   = float(cDstExtent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor;
      scissor.offset = { 0, 0 };
      scissor.extent = cDstExtent;

      if (cStreamState.dstRectEnabled) {
        viewport.x      = cDstSizeFactX * float(cStreamState.dstRect.left);
        viewport.y      = cDstSizeFactY * float(cStreamState.dstRect.top);
        viewport.width  = cDstSizeFactX * float(cStreamState.dstRect.right) - viewport.x;
        viewport.height = cDstSizeFactY * float(cStreamState.dstRect.bottom) - viewport.y;
      }

      VkExtent3D viewExtent = cViews[0]->mipLevelExtent(0);

      VkRect2D srcRect;
      srcRect.offset = { 0, 0 };
      srcRect.extent = { viewExtent.width, viewExtent.height };

      if (cStreamState.srcRectEnabled) {
        srcRect.offset.x      = cStreamState.srcRect.left;
        srcRect.offset.y      = cStreamState.srcRect.top;
        srcRect.extent.width  = cStreamState.srcRect.right - srcRect.offset.x;
        srcRect.extent.height = cStreamState.srcRect.bottom - srcRect.offset.y;
      }

      UboData uboData = { };
      uboData.colorMatrix[0][0] = 1.0f;
      uboData.colorMatrix[1][1] = 1.0f;
      uboData.colorMatrix[2][2] = 1.0f;
      uboData.coordMatrix[0][0] = float(srcRect.extent.width) / float(viewExtent.width);
      uboData.coordMatrix[1][1] = float(srcRect.extent.height) / float(viewExtent.height);
      uboData.coordMatrix[2][0] = float(srcRect.offset.x) / float(viewExtent.width);
      uboData.coordMatrix[2][1] = float(srcRect.offset.y) / float(viewExtent.height);
      uboData.srcRect = srcRect;
      uboData.yMin = 0.0f;
      uboData.yMax = 1.0f;
      uboData.isPlanar = cViews[1] != nullptr;
      uboData.exportMode = cExportMode;

      if (cSrcIsYCbCr && !cDstIsYCbCr)
        ApplyYCbCrMatrix(uboData.colorMatrix, cStreamState.colorSpace.YCbCr_Matrix);

      if (cStreamState.colorSpace.Nominal_Range) {
        uboData.yMin = 0.0627451f;
        uboData.yMax = 0.9215686f;
      }

      Rc<DxvkResourceAllocation> uboSlice = m_ubo->allocateStorage();
      memcpy(uboSlice->mapPtr(), &uboData, sizeof(uboData));

      DxvkViewport vp = { viewport, scissor };

      ctx->invalidateBuffer(m_ubo, std::move(uboSlice));
      ctx->setViewports(1, &vp);

      ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(Rc<DxvkShader>(m_vs));
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_fs));

      ctx->bindUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, DxvkBufferSlice(m_ubo));

      for (uint32_t i = 0; i < cViews.size(); i++)
        ctx->bindResourceImageView(VK_SHADER_STAGE_FRAGMENT_BIT, 1 + i, Rc<DxvkImageView>(cViews[i]));

      VkDrawIndirectCommand draw = { };
      draw.vertexCount   = 3u;
      draw.instanceCount = 1u;

      ctx->draw(1, &draw);

      for (uint32_t i = 0; i < cViews.size(); i++)
        ctx->bindResourceImageView(VK_SHADER_STAGE_FRAGMENT_BIT, 1 + i, nullptr);
    });
  }


  void D3D11VideoContext::CopyBaseImageToShadow(
    const D3D11VideoProcessorView&        View) {
    auto shadow = View.GetShadow();

    if (!shadow)
      return;

    VkImageSubresourceLayers imageLayers = View.GetImageSubresource();

    VkImageSubresourceLayers shadowLayers = { };
    shadowLayers.aspectMask = imageLayers.aspectMask;
    shadowLayers.layerCount = imageLayers.layerCount;

    m_ctx->SyncImage(shadow, shadowLayers, View.GetImage(), imageLayers);
  }


  void D3D11VideoContext::CopyShadowToBaseImage(
    const D3D11VideoProcessorView&        View) {
    auto shadow = View.GetShadow();

    if (!shadow)
      return;

    VkImageSubresourceLayers imageLayers = View.GetImageSubresource();

    VkImageSubresourceLayers shadowLayers = { };
    shadowLayers.aspectMask = imageLayers.aspectMask;
    shadowLayers.layerCount = imageLayers.layerCount;

    m_ctx->SyncImage(View.GetImage(), imageLayers, shadow, shadowLayers);
  }


  void D3D11VideoContext::CreateUniformBuffer() {
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size = sizeof(UboData);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    bufferInfo.access = VK_ACCESS_UNIFORM_READ_BIT;
    bufferInfo.debugName = "Video blit parameters";

    m_ubo = m_device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  void D3D11VideoContext::CreateShaders() {
    const std::array<DxvkBindingInfo, 3> fsBindings = {{
      { 0u, 0u, 0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_ACCESS_UNIFORM_READ_BIT, DxvkDescriptorFlag::UniformBuffer },
      { 0u, 1u, 1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1u, VK_IMAGE_VIEW_TYPE_2D,       VK_ACCESS_SHADER_READ_BIT },
      { 0u, 2u, 2u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1u, VK_IMAGE_VIEW_TYPE_2D,       VK_ACCESS_SHADER_READ_BIT },
    }};

    DxvkSpirvShaderCreateInfo vsInfo = { };
    m_vs = new DxvkSpirvShader(vsInfo, d3d11_video_blit_vert);

    DxvkSpirvShaderCreateInfo fsInfo = { };
    fsInfo.bindingCount = fsBindings.size();
    fsInfo.bindings = fsBindings.data();
    m_fs = new DxvkSpirvShader(fsInfo, d3d11_video_blit_frag);
  }


  void D3D11VideoContext::CreateResources() {
    if (std::exchange(m_resourcesCreated, true))
      return;

    CreateUniformBuffer();
    CreateShaders();
  }


  void D3D11VideoContext::UnbindResources() {
    m_ctx->EmitCs([] (DxvkContext* ctx) {
      ctx->bindRenderTargets(DxvkRenderTargets(), 0u);

      ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(nullptr);

      ctx->bindUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, DxvkBufferSlice());
    });
  }

}
