#include <algorithm>

#include "d3d11_context.h"
#include "d3d11_context_imm.h"
#include "d3d11_video.h"

#include <d3d11_video_blit_frag.h>
#include <d3d11_video_blit_vert.h>

namespace dxvk {

  D3D11VideoProcessorEnumerator::D3D11VideoProcessorEnumerator(
          D3D11Device*            pDevice,
    const D3D11_VIDEO_PROCESSOR_CONTENT_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorEnumerator>(pDevice),
    m_desc(Desc) {

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

    Logger::warn("D3D11VideoProcessorEnumerator::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
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
    Logger::err("D3D11VideoProcessorEnumerator::CheckVideoProcessorFormat: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCaps(
          D3D11_VIDEO_PROCESSOR_CAPS* pCaps) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorCaps: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps(
          UINT                    TypeIndex,
          D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS* pCaps) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate(
          UINT                    TypeIndex,
          UINT                    CustomRateIndex,
          D3D11_VIDEO_PROCESSOR_CUSTOM_RATE* pRate) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange(
          D3D11_VIDEO_PROCESSOR_FILTER        Filter,
          D3D11_VIDEO_PROCESSOR_FILTER_RANGE* pRange) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange: Stub");
    return E_NOTIMPL;
  }




  D3D11VideoProcessor::D3D11VideoProcessor(
          D3D11Device*                    pDevice,
          D3D11VideoProcessorEnumerator*  pEnumerator,
          UINT                            RateConversionIndex)
  : D3D11DeviceChild<ID3D11VideoProcessor>(pDevice),
    m_enumerator(pEnumerator), m_rateConversionIndex(RateConversionIndex) {

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

    Logger::warn("D3D11VideoProcessor::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
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




  D3D11VideoProcessorInputView::D3D11VideoProcessorInputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorInputView>(pDevice),
    m_resource(pResource), m_desc(Desc) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    Rc<DxvkImage> dxvkImage = GetCommonTexture(pResource)->GetImage();

    if (!(dxvkImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
      DxvkImageCreateInfo info = dxvkImage->info();
      info.flags  = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
      info.usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      info.tiling = VK_IMAGE_TILING_OPTIMAL;
      info.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      info.shared = VK_FALSE;
      dxvkImage = m_copy = pDevice->GetDXVKDevice()->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);
    DXGI_VK_FORMAT_FAMILY formatFamily = pDevice->LookupFamily(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    VkImageAspectFlags aspectMask = imageFormatInfo(formatInfo.Format)->aspectMask;

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = VK_IMAGE_USAGE_SAMPLED_BIT;

    switch (m_desc.ViewDimension) {
      case D3D11_VPIV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = m_desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;

      case D3D11_RTV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    m_subresources.aspectMask = aspectMask;
    m_subresources.baseArrayLayer = viewInfo.minLayer;
    m_subresources.layerCount = viewInfo.numLayers;
    m_subresources.mipLevel = viewInfo.minLevel;

    for (uint32_t i = 0; aspectMask && i < m_views.size(); i++) {
      viewInfo.aspect = vk::getNextAspect(aspectMask);

      if (viewInfo.aspect != VK_IMAGE_ASPECT_COLOR_BIT)
        viewInfo.format = formatFamily.Formats[i];

      m_views[i] = pDevice->GetDXVKDevice()->createImageView(dxvkImage, viewInfo);
    }

    m_isYCbCr = IsYCbCrFormat(resourceDesc.Format);
  }


  D3D11VideoProcessorInputView::~D3D11VideoProcessorInputView() {

  }


  bool D3D11VideoProcessorInputView::IsYCbCrFormat(DXGI_FORMAT Format) {
    static const std::array<DXGI_FORMAT, 3> s_formats = {{
      DXGI_FORMAT_NV12,
      DXGI_FORMAT_YUY2,
      DXGI_FORMAT_AYUV,
    }};

    return std::find(s_formats.begin(), s_formats.end(), Format) != s_formats.end();
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

    Logger::warn("D3D11VideoProcessorInputView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_resource.ref();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }



  D3D11VideoProcessorOutputView::D3D11VideoProcessorOutputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorOutputView>(pDevice),
    m_resource(pResource), m_desc(Desc) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(
      resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.aspect  = imageFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    switch (m_desc.ViewDimension) {
      case D3D11_VPOV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = m_desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;

      case D3D11_VPOV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = m_desc.Texture2DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = m_desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = m_desc.Texture2DArray.ArraySize;
        break;

      case D3D11_RTV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    m_view = pDevice->GetDXVKDevice()->createImageView(
      GetCommonTexture(pResource)->GetImage(), viewInfo);
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

    Logger::warn("D3D11VideoProcessorOutputView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_resource.ref();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }



  D3D11VideoContext::D3D11VideoContext(
          D3D11ImmediateContext*  pContext,
    const Rc<DxvkDevice>&         Device)
  : m_ctx(pContext) {
    const SpirvCodeBuffer vsCode(d3d11_video_blit_vert);
    const SpirvCodeBuffer fsCode(d3d11_video_blit_frag);

    const std::array<DxvkResourceSlot, 4> fsResourceSlots = {{
      { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC               },
      { 1, VK_DESCRIPTOR_TYPE_SAMPLER                              },
      { 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_VIEW_TYPE_2D },
      { 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_VIEW_TYPE_2D },
    }};

    m_vs = Device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0u, 1u },
      vsCode);
    
    m_fs = Device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResourceSlots.size(),
      fsResourceSlots.data(),
      { 1u, 1u, 0u, 0u },
      fsCode);

    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 0.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor     = VkClearColorValue();
    samplerInfo.usePixelCoord   = VK_FALSE;
    m_sampler = Device->createSampler(samplerInfo);

    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size = sizeof(UboData);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    bufferInfo.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_ubo = Device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    Logger::err("D3D11VideoContext::GetDecoderBuffer: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ReleaseDecoderBuffer(
          ID3D11VideoDecoder*             pDecoder,
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type) {
    Logger::err("D3D11VideoContext::ReleaseDecoderBuffer: Stub");
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderBeginFrame(
          ID3D11VideoDecoder*             pDecoder,
          ID3D11VideoDecoderOutputView*   pView,
          UINT                            KeySize,
    const void*                           pKey) {
    Logger::err("D3D11VideoContext::DecoderBeginFrame: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderEndFrame(
          ID3D11VideoDecoder*             pDecoder) {
    Logger::err("D3D11VideoContext::DecoderEndFrame: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SubmitDecoderBuffers(
          ID3D11VideoDecoder*             pDecoder,
          UINT                            BufferCount,
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pBufferDescs) {
    Logger::err("D3D11VideoContext::SubmitDecoderBuffers: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderExtension(
          ID3D11VideoDecoder*             pDecoder,
    const D3D11_VIDEO_DECODER_EXTENSION*  pExtension) {
    Logger::err("D3D11VideoContext::DecoderExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputTargetRectEnabled = Enable;

    if (Enable)
      state->outputTargetRect = *pRect;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetOutputTargetRect: Stub.");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            YCbCr,
    const D3D11_VIDEO_COLOR*              pColor) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputBackgroundColorIsYCbCr = YCbCr;
    state->outputBackgroundColor = *pColor;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetOutputBackgroundColor: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
          SIZE                            Size) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputConstriction: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputExtension: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamOutputRate: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamSourceRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->srcRectEnabled = Enable;

    if (Enable)
      state->srcRect = *pRect;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetStreamSourceRect: Stub.");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
    const UINT*                           pEntries) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const DXGI_RATIONAL*                  pSrcAspectRatio,
    const DXGI_RATIONAL*                  pDstAspectRatio) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          FLOAT                           Lower,
          FLOAT                           Upper) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamLumaKey: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamStereoFormat: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamExtension: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          SIZE*                           pSize) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputConstriction: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputExtension: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamOutputRate: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
          UINT*                           pEntries) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          DXGI_RATIONAL*                  pSrcAspectRatio,
          DXGI_RATIONAL*                  pDstAspectRatio) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          FLOAT*                          pLower,
          FLOAT*                          pUpper) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamLumaKey: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamStereoFormat: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamExtension: Stub");
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

    auto videoProcessor = static_cast<D3D11VideoProcessor*>(pVideoProcessor);
    bool hasStreamsEnabled = false;

    // Resetting and restoring all context state incurs
    // a lot of overhead, so only do it as necessary
    for (uint32_t i = 0; i < StreamCount; i++) {
      auto streamState = videoProcessor->GetStreamState(i);

      if (!pStreams[i].Enable || !streamState)
        continue;

      if (!hasStreamsEnabled) {
        m_ctx->ResetState();
        BindOutputView(pOutputView);
        hasStreamsEnabled = true;
      }

      BlitStream(streamState, &pStreams[i]);
    }

    if (hasStreamsEnabled)
      m_ctx->RestoreState();

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateCryptoSessionKeyExchange(
          ID3D11CryptoSession*            pSession,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::NegotiateCryptoSessionKeyExchange: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::EncryptionBlt(
          ID3D11CryptoSession*            pSession,
          ID3D11Texture2D*                pSrcSurface,
          ID3D11Texture2D*                pDstSurface,
          UINT                            IVSize,
          void*                           pIV) {
    Logger::err("D3D11VideoContext::EncryptionBlt: Stub");
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
    Logger::err("D3D11VideoContext::DecryptionBlt: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::StartSessionKeyRefresh(
          ID3D11CryptoSession*            pSession,
          UINT                            RandomNumberSize,
          void*                           pRandomNumber) {
    Logger::err("D3D11VideoContext::StartSessionKeyRefresh: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::FinishSessionKeyRefresh(
          ID3D11CryptoSession*            pSession) {
    Logger::err("D3D11VideoContext::FinishSessionKeyRefresh: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetEncryptionBltKey(
          ID3D11CryptoSession*            pSession,
          UINT                            KeySize,
          void*                           pKey) {
    Logger::err("D3D11VideoContext::GetEncryptionBltKey: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::QueryAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          UINT                            OutputSize,
          void*                           pOutput) {
    Logger::err("D3D11VideoContext::QueryAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ConfigureAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          D3D11_AUTHENTICATED_CONFIGURE_OUTPUT* pOutput) {
    Logger::err("D3D11VideoContext::ConfigureAuthenticatedChannel: Stub");
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
          ID3D11VideoProcessorOutputView* pOutputView) {
    auto dxvkView = static_cast<D3D11VideoProcessorOutputView*>(pOutputView)->GetView();

    m_ctx->EmitCs([this, cView = dxvkView] (DxvkContext* ctx) {
      DxvkRenderTargets rt;
      rt.color[0].view = cView;
      rt.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      ctx->bindRenderTargets(rt);
      ctx->bindShader(VK_SHADER_STAGE_VERTEX_BIT, m_vs);
      ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_fs);
      ctx->bindResourceBuffer(0, DxvkBufferSlice(m_ubo));

      DxvkInputAssemblyState iaState;
      iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      iaState.primitiveRestart = VK_FALSE;
      iaState.patchVertexCount = 0;
      ctx->setInputAssemblyState(iaState);
    });

    VkExtent3D viewExtent = dxvkView->mipLevelExtent(0);
    m_dstExtent = { viewExtent.width, viewExtent.height };
  }


  void D3D11VideoContext::BlitStream(
    const D3D11VideoProcessorStreamState* pStreamState,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStream) {
    if (pStream->PastFrames || pStream->FutureFrames)
      Logger::err("D3D11VideoContext: Ignoring non-zero PastFrames and FutureFrames");

    if (pStream->OutputIndex)
      Logger::err("D3D11VideoContext: Ignoring non-zero OutputIndex");

    if (pStream->InputFrameOrField)
      Logger::err("D3D11VideoContext: Ignoring non-zero InputFrameOrField");

    auto view = static_cast<D3D11VideoProcessorInputView*>(pStream->pInputSurface);

    if (view->NeedsCopy()) {
      m_ctx->EmitCs([
        cDstImage     = view->GetShadowCopy(),
        cSrcImage     = view->GetImage(),
        cSrcLayers    = view->GetImageSubresources()
      ] (DxvkContext* ctx) {
        VkImageSubresourceLayers cDstLayers;
        cDstLayers.aspectMask = cSrcLayers.aspectMask;
        cDstLayers.baseArrayLayer = 0;
        cDstLayers.layerCount = cSrcLayers.layerCount;
        cDstLayers.mipLevel = cSrcLayers.mipLevel;

        ctx->copyImage(
          cDstImage, cDstLayers, VkOffset3D(),
          cSrcImage, cSrcLayers, VkOffset3D(),
          cDstImage->info().extent);
      });
    }

    m_ctx->EmitCs([this,
      cStreamState  = *pStreamState,
      cViews        = view->GetViews(),
      cIsYCbCr      = view->IsYCbCr()
    ] (DxvkContext* ctx) {
      VkViewport viewport;
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = float(m_dstExtent.width);
      viewport.height   = float(m_dstExtent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor;
      scissor.offset = { 0, 0 };
      scissor.extent = m_dstExtent;

      if (cStreamState.dstRectEnabled) {
        viewport.x      = float(cStreamState.dstRect.left);
        viewport.y      = float(cStreamState.dstRect.top);
        viewport.width  = float(cStreamState.dstRect.right) - viewport.x;
        viewport.height = float(cStreamState.dstRect.bottom) - viewport.y;
      }

      UboData uboData = { };
      uboData.colorMatrix[0][0] = 1.0f;
      uboData.colorMatrix[1][1] = 1.0f;
      uboData.colorMatrix[2][2] = 1.0f;
      uboData.coordMatrix[0][0] = 1.0f;
      uboData.coordMatrix[1][1] = 1.0f;
      uboData.yMin = 0.0f;
      uboData.yMax = 1.0f;

      if (cIsYCbCr)
        ApplyYCbCrMatrix(uboData.colorMatrix, cStreamState.colorSpace.YCbCr_Matrix);

      if (cStreamState.colorSpace.Nominal_Range) {
        uboData.yMin = 0.0627451f;
        uboData.yMax = 0.9215686f;
      }

      DxvkBufferSliceHandle uboSlice = m_ubo->allocSlice();
      memcpy(uboSlice.mapPtr, &uboData, sizeof(uboData));

      ctx->invalidateBuffer(m_ubo, uboSlice);
      ctx->setViewports(1, &viewport, &scissor);
      ctx->bindResourceSampler(1, m_sampler);

      for (uint32_t i = 0; i < cViews.size(); i++)
        ctx->bindResourceView(2 + i, cViews[i], nullptr);

      ctx->draw(3, 1, 0, 0);
    });
  }

}
