#pragma once

#include "d3d11_device.h"

namespace dxvk {

  static constexpr uint32_t D3D11_VK_VIDEO_STREAM_COUNT = 8;

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


  struct D3D11VideoProcessorStreamState {
    BOOL autoProcessingEnabled  = TRUE;
    BOOL dstRectEnabled         = FALSE;
    BOOL srcRectEnabled         = FALSE;
    BOOL rotationEnabled        = FALSE;
    RECT dstRect                = RECT();
    RECT srcRect                = RECT();
    D3D11_VIDEO_FRAME_FORMAT frameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    D3D11_VIDEO_PROCESSOR_ROTATION rotation = D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY;
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE colorSpace = D3D11_VIDEO_PROCESSOR_COLOR_SPACE();
  };

  struct D3D11VideoProcessorState {
    BOOL outputStereoModeEnabled                       = FALSE;
    BOOL outputBackgroundColorIsYCbCr                  = FALSE;
    BOOL outputTargetRectEnabled                       = FALSE;
    RECT outputTargetRect                              = RECT();
    D3D11_VIDEO_COLOR outputBackgroundColor            = D3D11_VIDEO_COLOR();
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace = D3D11_VIDEO_PROCESSOR_COLOR_SPACE();
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

    D3D11VideoProcessorState* GetState() {
      return &m_state;
    }

    D3D11VideoProcessorStreamState* GetStreamState(UINT StreamIndex) {
      return StreamIndex < D3D11_VK_VIDEO_STREAM_COUNT
        ? &m_streams[StreamIndex]
        : nullptr;
    }

  private:

    D3D11VideoProcessorEnumerator* m_enumerator;
    uint32_t                       m_rateConversionIndex;
    D3D11VideoProcessorState       m_state;
    D3D11VideoProcessorStreamState m_streams[D3D11_VK_VIDEO_STREAM_COUNT];

  };



  class D3D11VideoProcessorInputView : public D3D11DeviceChild<ID3D11VideoProcessorInputView> {

  public:

    D3D11VideoProcessorInputView(
            D3D11Device*            pDevice,
            ID3D11Resource*         pResource,
      const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC& Desc);

    ~D3D11VideoProcessorInputView();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    void STDMETHODCALLTYPE GetResource(
            ID3D11Resource**        ppResource);

    void STDMETHODCALLTYPE GetDesc(
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* pDesc);

    const bool IsYCbCr() const {
      return m_isYCbCr;
    }

    const bool NeedsCopy() const {
      return m_copy != nullptr;
    }

    Rc<DxvkImage> GetImage() const {
      return GetCommonTexture(m_resource.ptr())->GetImage();
    }

    VkImageSubresourceLayers GetImageSubresources() const {
      return m_subresources;
    }

    Rc<DxvkImage> GetShadowCopy() const {
      return m_copy;
    }

    std::array<Rc<DxvkImageView>, 2> GetViews() const {
      return m_views;
    }

  private:

    Com<ID3D11Resource>                   m_resource;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC m_desc;
    VkImageSubresourceLayers              m_subresources;
    Rc<DxvkImage>                         m_copy;
    std::array<Rc<DxvkImageView>, 2>      m_views;
    bool                                  m_isYCbCr = false;

    static bool IsYCbCrFormat(DXGI_FORMAT Format);

  };



  class D3D11VideoProcessorOutputView : public D3D11DeviceChild<ID3D11VideoProcessorOutputView> {

  public:

    D3D11VideoProcessorOutputView(
            D3D11Device*            pDevice,
            ID3D11Resource*         pResource,
      const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC& Desc);

    ~D3D11VideoProcessorOutputView();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    void STDMETHODCALLTYPE GetResource(
            ID3D11Resource**        ppResource);

    void STDMETHODCALLTYPE GetDesc(
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* pDesc);

    Rc<DxvkImageView> GetView() const {
      return m_view;
    }

  private:

    Com<ID3D11Resource>                     m_resource;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC  m_desc;
    Rc<DxvkImageView>                       m_view;

  };



  class D3D11VideoContext : public ID3D11VideoContext {

  public:

    D3D11VideoContext(
            D3D11ImmediateContext*  pContext,
      const Rc<DxvkDevice>&         Device);

    ~D3D11VideoContext();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID                 Name,
            UINT*                   pDataSize,
            void*                   pData);

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                 Name,
            UINT                    DataSize,
      const void*                   pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                 Name,
      const IUnknown*               pUnknown);

    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device**          ppDevice);

    HRESULT STDMETHODCALLTYPE GetDecoderBuffer(
            ID3D11VideoDecoder*             pDecoder,
            D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
            UINT*                           BufferSize,
            void**                          ppBuffer);

    HRESULT STDMETHODCALLTYPE ReleaseDecoderBuffer(
            ID3D11VideoDecoder*             pDecoder,
            D3D11_VIDEO_DECODER_BUFFER_TYPE Type);

    HRESULT STDMETHODCALLTYPE DecoderBeginFrame(
            ID3D11VideoDecoder*             pDecoder,
            ID3D11VideoDecoderOutputView*   pView,
            UINT                            KeySize,
      const void*                           pKey);

    HRESULT STDMETHODCALLTYPE DecoderEndFrame(
            ID3D11VideoDecoder*             pDecoder);

    HRESULT STDMETHODCALLTYPE SubmitDecoderBuffers(
            ID3D11VideoDecoder*             pDecoder,
            UINT                            BufferCount,
      const D3D11_VIDEO_DECODER_BUFFER_DESC* pBufferDescs);

    HRESULT STDMETHODCALLTYPE DecoderExtension(
            ID3D11VideoDecoder*             pDecoder,
      const D3D11_VIDEO_DECODER_EXTENSION*  pExtension);

    void STDMETHODCALLTYPE VideoProcessorSetOutputTargetRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL                            Enable,
      const RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorSetOutputBackgroundColor(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL                            YCbCr,
      const D3D11_VIDEO_COLOR*              pColor);

    void STDMETHODCALLTYPE VideoProcessorSetOutputColorSpace(
            ID3D11VideoProcessor*           pVideoProcessor,
      const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

    void STDMETHODCALLTYPE VideoProcessorSetOutputAlphaFillMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
            UINT                            StreamIndex);

    void STDMETHODCALLTYPE VideoProcessorSetOutputConstriction(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL                            Enable,
            SIZE                            Size);

    void STDMETHODCALLTYPE VideoProcessorSetOutputStereoMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL                            Enable);

    HRESULT STDMETHODCALLTYPE VideoProcessorSetOutputExtension(
            ID3D11VideoProcessor*           pVideoProcessor,
      const GUID*                           pExtensionGuid,
            UINT                            DataSize,
            void*                           pData);

    void STDMETHODCALLTYPE VideoProcessorSetStreamFrameFormat(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_FRAME_FORMAT        Format);

    void STDMETHODCALLTYPE VideoProcessorSetStreamColorSpace(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
      const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

    void STDMETHODCALLTYPE VideoProcessorSetStreamOutputRate(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE Rate,
            BOOL                            Repeat,
      const DXGI_RATIONAL*                  CustomRate);

    void STDMETHODCALLTYPE VideoProcessorSetStreamSourceRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
      const RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorSetStreamDestRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
      const RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorSetStreamAlpha(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
            FLOAT                           Alpha);

    void STDMETHODCALLTYPE VideoProcessorSetStreamPalette(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            UINT                            EntryCount,
      const UINT*                           pEntries);

    void STDMETHODCALLTYPE VideoProcessorSetStreamPixelAspectRatio(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
      const DXGI_RATIONAL*                  pSrcAspectRatio,
      const DXGI_RATIONAL*                  pDstAspectRatio);

    void STDMETHODCALLTYPE VideoProcessorSetStreamLumaKey(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
            FLOAT                           Lower,
            FLOAT                           Upper);

    void STDMETHODCALLTYPE VideoProcessorSetStreamStereoFormat(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
            BOOL                            LeftViewFrame0,
            BOOL                            BaseViewFrame0,
            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
            int                             MonoOffset);

    void STDMETHODCALLTYPE VideoProcessorSetStreamAutoProcessingMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable);

    void STDMETHODCALLTYPE VideoProcessorSetStreamFilter(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_PROCESSOR_FILTER    Filter,
            BOOL                            Enable,
            int                             Level);

    HRESULT STDMETHODCALLTYPE VideoProcessorSetStreamExtension(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
      const GUID*                           pExtensionGuid,
            UINT                            DataSize,
            void*                           pData);

    void STDMETHODCALLTYPE VideoProcessorSetStreamRotation(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL                            Enable,
            D3D11_VIDEO_PROCESSOR_ROTATION  Rotation);

    void STDMETHODCALLTYPE VideoProcessorGetOutputTargetRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL*                           pEnabled,
            RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorGetOutputBackgroundColor(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL*                           pYCbCr,
            D3D11_VIDEO_COLOR*              pColor);

    void STDMETHODCALLTYPE VideoProcessorGetOutputColorSpace(
            ID3D11VideoProcessor*           pVideoProcessor,
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace);

    void STDMETHODCALLTYPE VideoProcessorGetOutputAlphaFillMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE* pAlphaFillMode,
            UINT*                           pStreamIndex);

    void STDMETHODCALLTYPE VideoProcessorGetOutputConstriction(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL*                           pEnabled,
            SIZE*                           pSize);

    void STDMETHODCALLTYPE VideoProcessorGetOutputStereoMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            BOOL*                           pEnabled);

    HRESULT STDMETHODCALLTYPE VideoProcessorGetOutputExtension(
            ID3D11VideoProcessor*           pVideoProcessor,
      const GUID*                           pExtensionGuid,
            UINT                            DataSize,
            void*                           pData);

    void STDMETHODCALLTYPE VideoProcessorGetStreamFrameFormat(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_FRAME_FORMAT*       pFormat);

    void STDMETHODCALLTYPE VideoProcessorGetStreamColorSpace(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace);

    void STDMETHODCALLTYPE VideoProcessorGetStreamOutputRate(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pRate,
            BOOL*                           pRepeat,
            DXGI_RATIONAL*                  pCustomRate);

    void STDMETHODCALLTYPE VideoProcessorGetStreamSourceRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorGetStreamDestRect(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            RECT*                           pRect);

    void STDMETHODCALLTYPE VideoProcessorGetStreamAlpha(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            FLOAT*                          pAlpha);

    void STDMETHODCALLTYPE VideoProcessorGetStreamPalette(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            UINT                            EntryCount,
            UINT*                           pEntries);

    void STDMETHODCALLTYPE VideoProcessorGetStreamPixelAspectRatio(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            DXGI_RATIONAL*                  pSrcAspectRatio,
            DXGI_RATIONAL*                  pDstAspectRatio);

    void STDMETHODCALLTYPE VideoProcessorGetStreamLumaKey(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            FLOAT*                          pLower,
            FLOAT*                          pUpper);

    void STDMETHODCALLTYPE VideoProcessorGetStreamStereoFormat(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled,
            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT* pFormat,
            BOOL*                           pLeftViewFrame0,
            BOOL*                           pBaseViewFrame0,
            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE* pFlipMode,
            int*                            pMonoOffset);

    void STDMETHODCALLTYPE VideoProcessorGetStreamAutoProcessingMode(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnabled);

    void STDMETHODCALLTYPE VideoProcessorGetStreamFilter(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            D3D11_VIDEO_PROCESSOR_FILTER    Filter,
            BOOL*                           pEnabled,
            int*                            pLevel);

    HRESULT STDMETHODCALLTYPE VideoProcessorGetStreamExtension(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
      const GUID*                           pExtensionGuid,
            UINT                            DataSize,
            void*                           pData);

    void STDMETHODCALLTYPE VideoProcessorGetStreamRotation(
            ID3D11VideoProcessor*           pVideoProcessor,
            UINT                            StreamIndex,
            BOOL*                           pEnable,
            D3D11_VIDEO_PROCESSOR_ROTATION* pRotation);

    HRESULT STDMETHODCALLTYPE VideoProcessorBlt(
            ID3D11VideoProcessor*           pVideoProcessor,
            ID3D11VideoProcessorOutputView* pOutputView,
            UINT                            FrameIdx,
            UINT                            StreamCount,
      const D3D11_VIDEO_PROCESSOR_STREAM*   pStreams);

    HRESULT STDMETHODCALLTYPE NegotiateCryptoSessionKeyExchange(
            ID3D11CryptoSession*            pSession,
            UINT                            DataSize,
            void*                           pData);

    void STDMETHODCALLTYPE EncryptionBlt(
            ID3D11CryptoSession*            pSession,
            ID3D11Texture2D*                pSrcSurface,
            ID3D11Texture2D*                pDstSurface,
            UINT                            IVSize,
            void*                           pIV);

    void STDMETHODCALLTYPE DecryptionBlt(
            ID3D11CryptoSession*            pSession,
            ID3D11Texture2D*                pSrcSurface,
            ID3D11Texture2D*                pDstSurface,
            D3D11_ENCRYPTED_BLOCK_INFO*     pBlockInfo,
            UINT                            KeySize,
      const void*                           pKey,
            UINT                            IVSize,
            void*                           pIV);

    void STDMETHODCALLTYPE StartSessionKeyRefresh(
            ID3D11CryptoSession*            pSession,
            UINT                            RandomNumberSize,
            void*                           pRandomNumber);

    void STDMETHODCALLTYPE FinishSessionKeyRefresh(
            ID3D11CryptoSession*            pSession);

    HRESULT STDMETHODCALLTYPE GetEncryptionBltKey(
            ID3D11CryptoSession*            pSession,
            UINT                            KeySize,
            void*                           pKey);

    HRESULT STDMETHODCALLTYPE NegotiateAuthenticatedChannelKeyExchange(
            ID3D11AuthenticatedChannel*     pChannel,
            UINT                            DataSize,
            void*                           pData);

    HRESULT STDMETHODCALLTYPE QueryAuthenticatedChannel(
            ID3D11AuthenticatedChannel*     pChannel,
            UINT                            InputSize,
      const void*                           pInput,
            UINT                            OutputSize,
            void*                           pOutput);

    HRESULT STDMETHODCALLTYPE ConfigureAuthenticatedChannel(
            ID3D11AuthenticatedChannel*     pChannel,
            UINT                            InputSize,
      const void*                           pInput,
            D3D11_AUTHENTICATED_CONFIGURE_OUTPUT* pOutput);

  private:

    struct alignas(16) UboData {
      float colorMatrix[3][4];
      float coordMatrix[3][2];
      float yMin, yMax;
    };

    D3D11ImmediateContext* m_ctx;

    Rc<DxvkSampler> m_sampler;
    Rc<DxvkShader> m_vs;
    Rc<DxvkShader> m_fs;
    Rc<DxvkBuffer> m_ubo;

    VkExtent2D m_dstExtent = { 0u, 0u };

    void ApplyColorMatrix(float pDst[3][4], const float pSrc[3][4]);

    void ApplyYCbCrMatrix(float pColorMatrix[3][4], bool UseBt709);

    void BindOutputView(
            ID3D11VideoProcessorOutputView* pOutputView);

    void BlitStream(
      const D3D11VideoProcessorStreamState* pStreamState,
      const D3D11_VIDEO_PROCESSOR_STREAM*   pStream);

  };

}
