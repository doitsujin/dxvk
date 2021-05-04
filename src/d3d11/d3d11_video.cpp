#include "d3d11_context.h"
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

}
