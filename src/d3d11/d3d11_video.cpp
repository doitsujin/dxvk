#include <algorithm>

#include "d3d11_context.h"
#include "d3d11_context_imm.h"
#include "d3d11_video.h"

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

    for (uint32_t i = 0; aspectMask && i < m_views.size(); i++) {
      viewInfo.aspect = vk::getNextAspect(aspectMask);

      if (viewInfo.aspect != VK_IMAGE_ASPECT_COLOR_BIT)
        viewInfo.format = formatFamily.Formats[i];

      m_views[i] = pDevice->GetDXVKDevice()->createImageView(
        GetCommonTexture(pResource)->GetImage(), viewInfo);
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
          D3D11ImmediateContext*   pContext)
  : m_ctx(pContext) {

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
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputTargetRect: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            YCbCr,
    const D3D11_VIDEO_COLOR*              pColor) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputBackgroundColor: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputColorSpace: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputStereoMode: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamFrameFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamColorSpace: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamSourceRect: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamDestRect: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamAutoProcessingMode: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamRotation: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputTargetRect: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pYCbCr,
          D3D11_VIDEO_COLOR*              pColor) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputBackgroundColor: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputColorSpace: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputStereoMode: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamFrameFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamColorSpace: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamSourceRect: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamDestRect: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamAutoProcessingMode: Stub");
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
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamRotation: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorBlt(
          ID3D11VideoProcessor*           pVideoProcessor,
          ID3D11VideoProcessorOutputView* pOutputView,
          UINT                            FrameIdx,
          UINT                            StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStreams) {
    Logger::err("D3D11VideoContext::VideoProcessorBlt: Stub");
    return E_NOTIMPL;
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

}
