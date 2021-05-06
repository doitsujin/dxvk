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

}
