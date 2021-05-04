#pragma once

#include "d3d11_device.h"

namespace dxvk {

  class D3D11VideoProcessorEnumerator : public D3D11DeviceChild<ID3D11VideoProcessorEnumerator> {

  public:

    D3D11VideoProcessorEnumerator(
            D3D11Device*            pDevice,
      const D3D11_VIDEO_PROCESSOR_CONTENT_DESC& Desc);

    ~D3D11VideoProcessorEnumerator();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE GetVideoProcessorContentDesc(
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC* pContentDesc);

    HRESULT STDMETHODCALLTYPE CheckVideoProcessorFormat(
            DXGI_FORMAT             Format,
            UINT*                   pFlags);

    HRESULT STDMETHODCALLTYPE GetVideoProcessorCaps(
            D3D11_VIDEO_PROCESSOR_CAPS* pCaps);

    HRESULT STDMETHODCALLTYPE GetVideoProcessorRateConversionCaps(
            UINT                    TypeIndex,
            D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS* pCaps);

    HRESULT STDMETHODCALLTYPE GetVideoProcessorCustomRate(
            UINT                    TypeIndex,
            UINT                    CustomRateIndex,
            D3D11_VIDEO_PROCESSOR_CUSTOM_RATE* pRate);

    HRESULT STDMETHODCALLTYPE GetVideoProcessorFilterRange(
            D3D11_VIDEO_PROCESSOR_FILTER        Filter,
            D3D11_VIDEO_PROCESSOR_FILTER_RANGE* pRange);

  private:

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC  m_desc;

  };



  class D3D11VideoProcessor : public D3D11DeviceChild<ID3D11VideoProcessor> {

  public:

    D3D11VideoProcessor(
            D3D11Device*                    pDevice,
            D3D11VideoProcessorEnumerator*  pEnumerator,
            UINT                            RateConversionIndex);

    ~D3D11VideoProcessor();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    void STDMETHODCALLTYPE GetContentDesc(
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc);

    void STDMETHODCALLTYPE GetRateConversionCaps(
            D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps);

  private:

    D3D11VideoProcessorEnumerator* m_enumerator;
    uint32_t                       m_rateConversionIndex;

  };

}
