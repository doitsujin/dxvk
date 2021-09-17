#pragma once

#include <mutex>
#include <vector>

#include "../dxbc/dxbc_options.h"

#include "../dxgi/dxgi_object.h"
#include "../dxgi/dxgi_interfaces.h"

#include "../dxvk/dxvk_cs.h"

#include "../d3d10/d3d10_device.h"

#include "../util/com/com_private_data.h"

#include "d3d11_cmdlist.h"
#include "d3d11_cuda.h"
#include "d3d11_initializer.h"
#include "d3d11_interfaces.h"
#include "d3d11_interop.h"
#include "d3d11_options.h"
#include "d3d11_shader.h"
#include "d3d11_state.h"
#include "d3d11_util.h"

namespace dxvk {
  class DxgiAdapter;
  
  class D3D11Buffer;
  class D3D11CommonShader;
  class D3D11CommonTexture;
  class D3D11Counter;
  class D3D11DeviceContext;
  class D3D11DXGIDevice;
  class D3D11ImmediateContext;
  class D3D11Predicate;
  class D3D11Query;
  class D3D11Texture1D;
  class D3D11Texture2D;
  class D3D11Texture3D;
  
  /**
   * \brief D3D11 device implementation
   * 
   * Implements the ID3D11Device interfaces
   * as part of a \ref D3D11DeviceContainer.
   */
  class D3D11Device final : public ID3D11Device5 {
    /// Maximum number of resource init commands per command buffer
    constexpr static uint64_t InitCommandThreshold = 50;
  public:
    
    D3D11Device(
            D3D11DXGIDevice*        pContainer,
            D3D_FEATURE_LEVEL       FeatureLevel,
            UINT                    FeatureFlags);
    
    ~D3D11Device();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateBuffer(
      const D3D11_BUFFER_DESC*      pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Buffer**          ppBuffer);
    
    HRESULT STDMETHODCALLTYPE CreateTexture1D(
      const D3D11_TEXTURE1D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture1D**       ppTexture1D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture2D(
      const D3D11_TEXTURE2D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture2D**       ppTexture2D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture2D1(
      const D3D11_TEXTURE2D_DESC1*  pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture2D1**      ppTexture2D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture3D(
      const D3D11_TEXTURE3D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture3D**       ppTexture3D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture3D1(
      const D3D11_TEXTURE3D_DESC1*  pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture3D1**      ppTexture3D);
    
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
            ID3D11Resource*                   pResource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
            ID3D11ShaderResourceView**        ppSRView);
    
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView1(
            ID3D11Resource*                   pResource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc,
            ID3D11ShaderResourceView1**       ppSRView);
    
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
            ID3D11Resource*                   pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
            ID3D11UnorderedAccessView**       ppUAView);
    
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView1(
            ID3D11Resource*                   pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc,
            ID3D11UnorderedAccessView1**      ppUAView);
    
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
            ID3D11Resource*                   pResource,
      const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
            ID3D11RenderTargetView**          ppRTView);
    
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView1(
            ID3D11Resource*                   pResource,
      const D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc,
            ID3D11RenderTargetView1**         ppRTView);
    
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
            ID3D11Resource*                   pResource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
            ID3D11DepthStencilView**          ppDepthStencilView);
    
    HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
            UINT                        NumElements,
      const void*                       pShaderBytecodeWithInputSignature,
            SIZE_T                      BytecodeLength,
            ID3D11InputLayout**         ppInputLayout);
    
    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11VertexShader**        ppVertexShader);
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader);
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
            UINT                        NumEntries,
      const UINT*                       pBufferStrides,
            UINT                        NumStrides,
            UINT                        RasterizedStream,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader);
    
    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11PixelShader**         ppPixelShader);
    
    HRESULT STDMETHODCALLTYPE CreateHullShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11HullShader**          ppHullShader);
    
    HRESULT STDMETHODCALLTYPE CreateDomainShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11DomainShader**        ppDomainShader);
    
    HRESULT STDMETHODCALLTYPE CreateComputeShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11ComputeShader**       ppComputeShader);
    
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(
            ID3D11ClassLinkage**        ppLinkage);
    
    HRESULT STDMETHODCALLTYPE CreateBlendState(
      const D3D11_BLEND_DESC*           pBlendStateDesc,
            ID3D11BlendState**          ppBlendState);
    
    HRESULT STDMETHODCALLTYPE CreateBlendState1(
      const D3D11_BLEND_DESC1*          pBlendStateDesc,
            ID3D11BlendState1**         ppBlendState);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
            ID3D11DepthStencilState**   ppDepthStencilState);
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(
      const D3D11_RASTERIZER_DESC*      pRasterizerDesc,
            ID3D11RasterizerState**     ppRasterizerState);
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState1(
      const D3D11_RASTERIZER_DESC1*     pRasterizerDesc,
            ID3D11RasterizerState1**    ppRasterizerState);
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState2(
      const D3D11_RASTERIZER_DESC2*     pRasterizerDesc,
            ID3D11RasterizerState2**    ppRasterizerState);
    
    HRESULT STDMETHODCALLTYPE CreateSamplerState(
      const D3D11_SAMPLER_DESC*         pSamplerDesc,
            ID3D11SamplerState**        ppSamplerState);
    
    HRESULT STDMETHODCALLTYPE CreateQuery(
      const D3D11_QUERY_DESC*           pQueryDesc,
            ID3D11Query**               ppQuery);
    
    HRESULT STDMETHODCALLTYPE CreateQuery1(
      const D3D11_QUERY_DESC1*          pQueryDesc,
            ID3D11Query1**              ppQuery);
    
    HRESULT STDMETHODCALLTYPE CreatePredicate(
      const D3D11_QUERY_DESC*           pPredicateDesc,
            ID3D11Predicate**           ppPredicate);
    
    HRESULT STDMETHODCALLTYPE CreateCounter(
      const D3D11_COUNTER_DESC*         pCounterDesc,
            ID3D11Counter**             ppCounter);
    
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(
            UINT                        ContextFlags,
            ID3D11DeviceContext**       ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
            UINT                        ContextFlags,
            ID3D11DeviceContext1**      ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeferredContext2(
            UINT                        ContextFlags,
            ID3D11DeviceContext2**      ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeferredContext3(
            UINT                        ContextFlags,
            ID3D11DeviceContext3**      ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
            UINT                        Flags,
      const D3D_FEATURE_LEVEL*          pFeatureLevels,
            UINT                        FeatureLevels,
            UINT                        SDKVersion,
            REFIID                      EmulatedInterface,
            D3D_FEATURE_LEVEL*          pChosenFeatureLevel,
            ID3DDeviceContextState**    ppContextState);

    HRESULT STDMETHODCALLTYPE CreateFence(
            UINT64                      InitialValue,
            D3D11_FENCE_FLAG            Flags,
            REFIID                      ReturnedInterface,
            void**                      ppFence);

    void STDMETHODCALLTYPE ReadFromSubresource(
            void*                       pDstData,
            UINT                        DstRowPitch,
            UINT                        DstDepthPitch,
            ID3D11Resource*             pSrcResource,
            UINT                        SrcSubresource,
      const D3D11_BOX*                  pSrcBox);

    void STDMETHODCALLTYPE WriteToSubresource(
            ID3D11Resource*             pDstResource,
            UINT                        DstSubresource,
      const D3D11_BOX*                  pDstBox,
      const void*                       pSrcData,
            UINT                        SrcRowPitch,
            UINT                        SrcDepthPitch);

    HRESULT STDMETHODCALLTYPE OpenSharedResource(
            HANDLE      hResource,
            REFIID      ReturnedInterface,
            void**      ppResource);

    HRESULT STDMETHODCALLTYPE OpenSharedResource1(
            HANDLE      hResource,
            REFIID      returnedInterface,
            void**      ppResource);

    HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(
            LPCWSTR     lpName,
            DWORD       dwDesiredAccess,
            REFIID      returnedInterface,
            void**      ppResource);
    
    HRESULT STDMETHODCALLTYPE OpenSharedFence(
            HANDLE      hFence,
            REFIID      ReturnedInterface,
            void**      ppFence);

    HRESULT STDMETHODCALLTYPE CheckFormatSupport(
            DXGI_FORMAT Format,
            UINT*       pFormatSupport);
    
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
            DXGI_FORMAT Format,
            UINT        SampleCount,
            UINT*       pNumQualityLevels);
    
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1(
            DXGI_FORMAT Format,
            UINT        SampleCount,
            UINT        Flags,
            UINT*       pNumQualityLevels);
    
    void STDMETHODCALLTYPE CheckCounterInfo(
            D3D11_COUNTER_INFO* pCounterInfo);
    
    HRESULT STDMETHODCALLTYPE CheckCounter(
      const D3D11_COUNTER_DESC* pDesc,
            D3D11_COUNTER_TYPE* pType,
            UINT*               pActiveCounters,
            LPSTR               szName,
            UINT*               pNameLength,
            LPSTR               szUnits,
            UINT*               pUnitsLength,
            LPSTR               szDescription,
            UINT*               pDescriptionLength);
    
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
            D3D11_FEATURE Feature,
            void*         pFeatureSupportData,
            UINT          FeatureSupportDataSize);
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID Name,
            UINT    *pDataSize,
            void    *pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID Name,
            UINT    DataSize,
      const void    *pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID  Name,
      const IUnknown *pUnknown);
    
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel();
    
    UINT STDMETHODCALLTYPE GetCreationFlags();
    
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason();
    
    void STDMETHODCALLTYPE GetImmediateContext(
            ID3D11DeviceContext** ppImmediateContext);

    void STDMETHODCALLTYPE GetImmediateContext1(
            ID3D11DeviceContext1** ppImmediateContext);
    
    void STDMETHODCALLTYPE GetImmediateContext2(
            ID3D11DeviceContext2** ppImmediateContext);
    
    void STDMETHODCALLTYPE GetImmediateContext3(
            ID3D11DeviceContext3** ppImmediateContext);
    
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags);
    
    UINT STDMETHODCALLTYPE GetExceptionMode();
    
    void STDMETHODCALLTYPE GetResourceTiling(
            ID3D11Resource*           pTiledResource,
            UINT*                     pNumTilesForEntireResource,
            D3D11_PACKED_MIP_DESC*    pPackedMipDesc,
            D3D11_TILE_SHAPE*         pStandardTileShapeForNonPackedMips,
            UINT*                     pNumSubresourceTilings,
            UINT                      FirstSubresourceTilingToGet,
            D3D11_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips);

    HRESULT STDMETHODCALLTYPE RegisterDeviceRemovedEvent(
            HANDLE                    hEvent,
            DWORD*                    pdwCookie);

    void STDMETHODCALLTYPE UnregisterDeviceRemoved(
            DWORD                     dwCookie);

    Rc<DxvkDevice> GetDXVKDevice() {
      return m_dxvkDevice;
    }
    
    void FlushInitContext();
    
    VkPipelineStageFlags GetEnabledShaderStages() const {
      return m_dxvkDevice->getShaderPipelineStages();
    }
    
    DXGI_VK_FORMAT_INFO LookupFormat(
            DXGI_FORMAT           Format,
            DXGI_VK_FORMAT_MODE   Mode) const;
    
    DXGI_VK_FORMAT_INFO LookupPackedFormat(
            DXGI_FORMAT           Format,
            DXGI_VK_FORMAT_MODE   Mode) const;
    
    DXGI_VK_FORMAT_FAMILY LookupFamily(
            DXGI_FORMAT           Format,
            DXGI_VK_FORMAT_MODE   Mode) const;
    
    DxvkCsChunkRef AllocCsChunk(DxvkCsChunkFlags flags) {
      DxvkCsChunk* chunk = m_csChunkPool.allocChunk(flags);
      return DxvkCsChunkRef(chunk, &m_csChunkPool);
    }
    
    const D3D11Options* GetOptions() const {
      return &m_d3d11Options;
    }

    D3D10Device* GetD3D10Interface() const {
      return m_d3d10Device;
    }
    
    static bool CheckFeatureLevelSupport(
      const Rc<DxvkInstance>& instance,
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
    static DxvkDeviceFeatures GetDeviceFeatures(
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
  private:
    
    IDXGIObject*                    m_container;

    D3D_FEATURE_LEVEL               m_featureLevel;
    UINT                            m_featureFlags;
    
    const Rc<DxvkDevice>            m_dxvkDevice;
    const Rc<DxvkAdapter>           m_dxvkAdapter;
    
    const DXGIVkFormatTable         m_d3d11Formats;
    const D3D11Options              m_d3d11Options;
    const DxbcOptions               m_dxbcOptions;
    
    DxvkCsChunkPool                 m_csChunkPool;
    
    D3D11Initializer*               m_initializer = nullptr;
    D3D10Device*                    m_d3d10Device = nullptr;
    Com<D3D11ImmediateContext, false> m_context;

    D3D11StateObjectSet<D3D11BlendState>        m_bsStateObjects;
    D3D11StateObjectSet<D3D11DepthStencilState> m_dsStateObjects;
    D3D11StateObjectSet<D3D11RasterizerState>   m_rsStateObjects;
    D3D11StateObjectSet<D3D11SamplerState>      m_samplerObjects;
    D3D11ShaderModuleSet                        m_shaderModules;
    
    HRESULT CreateShaderModule(
            D3D11CommonShader*      pShaderModule,
            DxvkShaderKey           ShaderKey,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
            ID3D11ClassLinkage*     pClassLinkage,
      const DxbcModuleInfo*         pModuleInfo);
    
    HRESULT GetFormatSupportFlags(
            DXGI_FORMAT Format,
            UINT*       pFlags1,
            UINT*       pFlags2) const;
    
    BOOL GetImageTypeSupport(
            VkFormat    Format,
            VkImageType Type) const;

    uint32_t GetViewPlaneIndex(
            ID3D11Resource*         pResource,
            DXGI_FORMAT             ViewFormat);
    
    template<typename Void>
    void CopySubresourceData(
            Void*                       pData,
            UINT                        RowPitch,
            UINT                        DepthPitch,
            ID3D11Resource*             pResource,
            UINT                        Subresource,
      const D3D11_BOX*                  pBox);
    
    static D3D_FEATURE_LEVEL GetMaxFeatureLevel(
      const Rc<DxvkInstance>&           pInstance);
    
  };
  
  
  /**
   * \brief Extended D3D11 device
   */
  class D3D11DeviceExt : public ID3D11VkExtDevice1 {
    
  public:
    
    D3D11DeviceExt(
            D3D11DXGIDevice*        pContainer,
            D3D11Device*            pDevice);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    BOOL STDMETHODCALLTYPE GetExtensionSupport(
            D3D11_VK_EXTENSION      Extension);
    
    bool STDMETHODCALLTYPE GetCudaTextureObjectNVX(
            uint32_t                srvDriverHandle,
            uint32_t                samplerDriverHandle,
            uint32_t*               pCudaTextureHandle);

    bool STDMETHODCALLTYPE CreateCubinComputeShaderWithNameNVX(
            const void*             pCubin,
            uint32_t                size,
            uint32_t                blockX,
            uint32_t                blockY,
            uint32_t                blockZ,
            const char*             pShaderName,
            IUnknown**              phShader);

    bool STDMETHODCALLTYPE GetResourceHandleGPUVirtualAddressAndSizeNVX(
            void*                   hObject,
            uint64_t*               gpuVAStart,
            uint64_t*               gpuVASize);

     bool STDMETHODCALLTYPE CreateUnorderedAccessViewAndGetDriverHandleNVX(
            ID3D11Resource*                         pResource,
            const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
            ID3D11UnorderedAccessView**             ppUAV,
            uint32_t*                               pDriverHandle);

     bool STDMETHODCALLTYPE CreateShaderResourceViewAndGetDriverHandleNVX(
            ID3D11Resource*                        pResource,
            const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
            ID3D11ShaderResourceView**             ppSRV,
            uint32_t*                              pDriverHandle);

     bool STDMETHODCALLTYPE CreateSamplerStateAndGetDriverHandleNVX(
            const D3D11_SAMPLER_DESC* pSamplerDesc,
            ID3D11SamplerState**      ppSamplerState,
            uint32_t*                 pDriverHandle);
    
  private:
    
    D3D11DXGIDevice* m_container;
    D3D11Device*     m_device;
    
    void AddSamplerAndHandleNVX(
            ID3D11SamplerState*       pSampler,
            uint32_t                  Handle);

    ID3D11SamplerState* HandleToSamplerNVX(
            uint32_t                  Handle);

    void AddSrvAndHandleNVX(
            ID3D11ShaderResourceView* pSrv,
            uint32_t                  Handle);

    ID3D11ShaderResourceView* HandleToSrvNVX(
            uint32_t                  Handle);
    
    dxvk::mutex m_mapLock;
    std::unordered_map<uint32_t, ID3D11SamplerState*> m_samplerHandleToPtr;
    std::unordered_map<uint32_t, ID3D11ShaderResourceView*> m_srvHandleToPtr;
  };


  /**
   * \brief D3D11 video device
   */
  class D3D11VideoDevice : public ID3D11VideoDevice {

  public:

    D3D11VideoDevice(
            D3D11DXGIDevice*        pContainer,
            D3D11Device*            pDevice);

    ~D3D11VideoDevice();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE CreateVideoDecoder(
      const D3D11_VIDEO_DECODER_DESC*                     pVideoDesc,
      const D3D11_VIDEO_DECODER_CONFIG*                   pConfig,
            ID3D11VideoDecoder**                          ppDecoder);

    HRESULT STDMETHODCALLTYPE CreateVideoProcessor(
            ID3D11VideoProcessorEnumerator*               pEnum,
            UINT                                          RateConversionIndex,
            ID3D11VideoProcessor**                        ppVideoProcessor);

    HRESULT STDMETHODCALLTYPE CreateAuthenticatedChannel(
            D3D11_AUTHENTICATED_CHANNEL_TYPE              ChannelType,
            ID3D11AuthenticatedChannel**                  ppAuthenticatedChannel);

    HRESULT STDMETHODCALLTYPE CreateCryptoSession(
      const GUID*                                         pCryptoType,
      const GUID*                                         pDecoderProfile,
      const GUID*                                         pKeyExchangeType,
            ID3D11CryptoSession**                         ppCryptoSession);

    HRESULT STDMETHODCALLTYPE CreateVideoDecoderOutputView(
            ID3D11Resource*                               pResource,
      const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*         pDesc,
            ID3D11VideoDecoderOutputView**                ppVDOVView);

    HRESULT STDMETHODCALLTYPE CreateVideoProcessorInputView(
            ID3D11Resource*                               pResource,
            ID3D11VideoProcessorEnumerator*               pEnum,
      const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC*        pDesc,
            ID3D11VideoProcessorInputView**               ppVPIView);

    HRESULT STDMETHODCALLTYPE CreateVideoProcessorOutputView(
            ID3D11Resource*                               pResource,
            ID3D11VideoProcessorEnumerator*               pEnum,
      const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC*       pDesc,
            ID3D11VideoProcessorOutputView**              ppVPOView);

    HRESULT STDMETHODCALLTYPE CreateVideoProcessorEnumerator(
      const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*           pDesc,
            ID3D11VideoProcessorEnumerator**              ppEnum);

    UINT STDMETHODCALLTYPE GetVideoDecoderProfileCount();

    HRESULT STDMETHODCALLTYPE GetVideoDecoderProfile(
            UINT                                          Index,
            GUID*                                         pDecoderProfile);

    HRESULT STDMETHODCALLTYPE CheckVideoDecoderFormat(
      const GUID*                                         pDecoderProfile,
            DXGI_FORMAT                                   Format,
            BOOL*                                         pSupported);

    HRESULT STDMETHODCALLTYPE GetVideoDecoderConfigCount(
      const D3D11_VIDEO_DECODER_DESC*                     pDesc,
            UINT*                                         pCount);

    HRESULT STDMETHODCALLTYPE GetVideoDecoderConfig(
      const D3D11_VIDEO_DECODER_DESC*                     pDesc,
            UINT                                          Index,
            D3D11_VIDEO_DECODER_CONFIG*                   pConfig);

    HRESULT STDMETHODCALLTYPE GetContentProtectionCaps(
      const GUID*                                         pCryptoType,
      const GUID*                                         pDecoderProfile,
            D3D11_VIDEO_CONTENT_PROTECTION_CAPS*          pCaps);

    HRESULT STDMETHODCALLTYPE CheckCryptoKeyExchange(
      const GUID*                                         pCryptoType,
      const GUID*                                         pDecoderProfile,
            UINT                                          Index,
            GUID*                                         pKeyExchangeType);

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                                       Name,
            UINT                                          DataSize,
      const void*                                         pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                                       Name,
      const IUnknown*                                     pData);

  private:

    D3D11DXGIDevice* m_container;
    D3D11Device*     m_device;

  };


  /**
   * \brief DXGI swap chain factory
   */
  class WineDXGISwapChainFactory : public IWineDXGISwapChainFactory {
    
  public:
    
    WineDXGISwapChainFactory(
            D3D11DXGIDevice*        pContainer,
            D3D11Device*            pDevice);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
            IDXGIFactory*           pFactory,
            HWND                    hWnd,
      const DXGI_SWAP_CHAIN_DESC1*  pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
            IDXGIOutput*            pRestrictToOutput,
            IDXGISwapChain1**       ppSwapChain);
    
  private:
    
    D3D11DXGIDevice* m_container;
    D3D11Device*     m_device;
    
  };
  

  /**
   * \brief D3D11 device metadata shenanigans
   */
  class DXGIDXVKDevice : public IDXGIDXVKDevice {

  public:

    DXGIDXVKDevice(D3D11DXGIDevice* pContainer);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    void STDMETHODCALLTYPE SetAPIVersion(
              UINT                    Version);

    UINT STDMETHODCALLTYPE GetAPIVersion();

  private:

    D3D11DXGIDevice* m_container;
    UINT             m_apiVersion;

  };


  /**
   * \brief D3D11 device container
   * 
   * Stores all the objects that contribute to the D3D11
   * device implementation, including the DXGI device.
   */
  class D3D11DXGIDevice : public DxgiObject<IDXGIDevice4> {
    constexpr static uint32_t DefaultFrameLatency = 3;
  public:
    
    D3D11DXGIDevice(
            IDXGIAdapter*       pAdapter,
      const Rc<DxvkInstance>&   pDxvkInstance,
      const Rc<DxvkAdapter>&    pDxvkAdapter,
            D3D_FEATURE_LEVEL   FeatureLevel,
            UINT                FeatureFlags);
    
    ~D3D11DXGIDevice();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                riid,
            void**                ppParent);
    
    HRESULT STDMETHODCALLTYPE CreateSurface(
      const DXGI_SURFACE_DESC*    pDesc,
            UINT                  NumSurfaces,
            DXGI_USAGE            Usage,
      const DXGI_SHARED_RESOURCE* pSharedResource,
            IDXGISurface**        ppSurface) final;
    
    HRESULT STDMETHODCALLTYPE GetAdapter(
            IDXGIAdapter**        pAdapter) final;
    
    HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(
            INT*                  pPriority) final;
    
    HRESULT STDMETHODCALLTYPE QueryResourceResidency(
            IUnknown* const*      ppResources,
            DXGI_RESIDENCY*       pResidencyStatus,
            UINT                  NumResources) final;
    
    HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(
            INT                   Priority) final;
    
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                 pMaxLatency) final;
    
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                  MaxLatency) final;

    HRESULT STDMETHODCALLTYPE OfferResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            DXGI_OFFER_RESOURCE_PRIORITY  Priority) final;
        
    HRESULT STDMETHODCALLTYPE OfferResources1( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            DXGI_OFFER_RESOURCE_PRIORITY  Priority,
            UINT                          Flags) final;
    
    HRESULT STDMETHODCALLTYPE ReclaimResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            BOOL*                         pDiscarded) final;
    
    HRESULT STDMETHODCALLTYPE ReclaimResources1(
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            DXGI_RECLAIM_RESOURCE_RESULTS* pResults) final;
        
    HRESULT STDMETHODCALLTYPE EnqueueSetEvent( 
            HANDLE                hEvent) final;
    
    void STDMETHODCALLTYPE Trim() final;
    
    Rc<DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice();

  private:

    Com<IDXGIAdapter>   m_dxgiAdapter;

    Rc<DxvkInstance>    m_dxvkInstance;
    Rc<DxvkAdapter>     m_dxvkAdapter;
    Rc<DxvkDevice>      m_dxvkDevice;

    D3D11Device         m_d3d11Device;
    D3D11DeviceExt      m_d3d11DeviceExt;
    D3D11VkInterop      m_d3d11Interop;
    D3D11VideoDevice    m_d3d11Video;
    DXGIDXVKDevice      m_metaDevice;
    
    WineDXGISwapChainFactory m_wineFactory;
    
    uint32_t m_frameLatency = DefaultFrameLatency;

    Rc<DxvkDevice> CreateDevice(D3D_FEATURE_LEVEL FeatureLevel);

  };
  
}
