#pragma once

#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"

/**
 * \brief D3D11 extension
 * 
 * Lists D3D11 extensions supported by DXVK.
 */
enum D3D11_VK_EXTENSION : uint32_t {
  D3D11_VK_EXT_MULTI_DRAW_INDIRECT        = 0,
  D3D11_VK_EXT_MULTI_DRAW_INDIRECT_COUNT  = 1,
  D3D11_VK_EXT_DEPTH_BOUNDS               = 2,
  D3D11_VK_EXT_BARRIER_CONTROL            = 3,
  D3D11_VK_NVX_BINARY_IMPORT              = 4,
  D3D11_VK_NVX_IMAGE_VIEW_HANDLE          = 5,
  D3D11_VK_NV_MULTIVIEW                   = 6,
};


/**
 * \brief Barrier control flags
 */
enum D3D11_VK_BARRIER_CONTROL : uint32_t {
  D3D11_VK_BARRIER_CONTROL_IGNORE_WRITE_AFTER_WRITE   = 1 << 0,

  // Removed:
  // D3D11_VK_BARRIER_CONTROL_IGNORE_GRAPHICS_UAV        = 1 << 1,
};


/**
 * \brief Extended D3D11 device
 * 
 * Introduces a method to check for extension support.
 */
MIDL_INTERFACE("8a6e3c42-f74c-45b7-8265-a231b677ca17")
ID3D11VkExtDevice : public IUnknown {
  /**
   * \brief Checks whether an extension is supported
   * 
   * \param [in] Extension The extension to check
   * \returns \c TRUE if the extension is supported
   */
  virtual BOOL STDMETHODCALLTYPE GetExtensionSupport(
          D3D11_VK_EXTENSION      Extension) = 0;
  
};


/**
 * \brief Extended extended D3D11 device
 * 
 * Introduces methods to get virtual addresses and driver
 * handles for resources, and create and destroy objects
 * for D3D11-Cuda interop.
 */
MIDL_INTERFACE("cfcf64ef-9586-46d0-bca4-97cf2ca61b06")
ID3D11VkExtDevice1 : public ID3D11VkExtDevice {

  virtual bool STDMETHODCALLTYPE GetResourceHandleGPUVirtualAddressAndSizeNVX(
          void*                   hObject,
          uint64_t*               gpuVAStart,
          uint64_t*               gpuVASize) = 0;

  virtual bool STDMETHODCALLTYPE CreateUnorderedAccessViewAndGetDriverHandleNVX(
          ID3D11Resource*         pResource,
          const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
          ID3D11UnorderedAccessView** ppUAV,
          uint32_t*               pDriverHandle) = 0;

  virtual bool STDMETHODCALLTYPE CreateShaderResourceViewAndGetDriverHandleNVX(
          ID3D11Resource*         pResource,
          const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
          ID3D11ShaderResourceView** ppSRV,
          uint32_t*               pDriverHandle) = 0;

  virtual bool STDMETHODCALLTYPE CreateSamplerStateAndGetDriverHandleNVX(
          const D3D11_SAMPLER_DESC* pSamplerDesc,
          ID3D11SamplerState**    ppSamplerState,
          uint32_t*               pDriverHandle) = 0;

  virtual bool STDMETHODCALLTYPE CreateCubinComputeShaderWithNameNVX(
          const void*             pCubin,
          uint32_t                size,
          uint32_t                blockX,
          uint32_t                blockY,
          uint32_t                blockZ,
          const char*             pShaderName,
          IUnknown**              phShader) = 0;

  virtual bool STDMETHODCALLTYPE GetCudaTextureObjectNVX(
          uint32_t                srvDriverHandle,
          uint32_t                samplerDriverHandle,
          uint32_t*               pCudaTextureHandle) = 0;
};


/**
 * \brief Extended D3D11 context
 * 
 * Provides functionality for various D3D11
 * extensions.
 */
MIDL_INTERFACE("fd0bca13-5cb6-4c3a-987e-4750de2ca791")
ID3D11VkExtContext : public IUnknown {
  virtual void STDMETHODCALLTYPE MultiDrawIndirect(
          UINT                    DrawCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) = 0;
  
  virtual void STDMETHODCALLTYPE MultiDrawIndexedIndirect(
          UINT                    DrawCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) = 0;
  
  virtual void STDMETHODCALLTYPE MultiDrawIndirectCount(
          UINT                    MaxDrawCount,
          ID3D11Buffer*           pBufferForCount,
          UINT                    ByteOffsetForCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) = 0;
  
  virtual void STDMETHODCALLTYPE MultiDrawIndexedIndirectCount(
          UINT                    MaxDrawCount,
          ID3D11Buffer*           pBufferForCount,
          UINT                    ByteOffsetForCount,
          ID3D11Buffer*           pBufferForArgs,
          UINT                    ByteOffsetForArgs,
          UINT                    ByteStrideForArgs) = 0;
  
  virtual void STDMETHODCALLTYPE SetDepthBoundsTest(
          BOOL                    Enable,
          FLOAT                   MinDepthBounds,
          FLOAT                   MaxDepthBounds) = 0;
  
  virtual void STDMETHODCALLTYPE SetBarrierControl(
          UINT                    ControlFlags) = 0;
};


/**
 * \brief Extended extended D3D11 context
 * 
 * Provides functionality to launch a Cuda kernel
 */
MIDL_INTERFACE("874b09b2-ae0b-41d8-8476-5f3b7a0e879d")
ID3D11VkExtContext1 : public ID3D11VkExtContext {

  virtual bool STDMETHODCALLTYPE LaunchCubinShaderNVX(
          IUnknown*               hShader,
          uint32_t                gridX,
          uint32_t                gridY,
          uint32_t                gridZ,
          const void*             pParams,
          uint32_t                paramSize,
          void* const*            pReadResources,
          uint32_t                numReadResources,
          void* const*            pWriteResources,
          uint32_t                numWriteResources) = 0;
};


/**
 * \brief NVAPI custom shader semantic (SMP / multi-view)
 *
 * Mirrors the information from NVAPI's NV_CUSTOM_SEMANTIC without
 * depending on NVIDIA headers. Type values match NV_CUSTOM_SEMANTIC_TYPE
 * (2 = viewport mask, 4 = viewport mask 2, 5 = per-view position).
 *
 * Name[256] matches NVIDIA's own NVCustomSemanticNameString buffer size
 * exactly. Every semantic name observed fits comfortably in far less,
 * but matching NVIDIA's maximum removes any truncation risk and lets the
 * compiler prove the strncpy calls below cannot truncate, which silences
 * -Wstringop-truncation properly rather than suppressing it.
 */
struct D3D11_VK_NV_CUSTOM_SEMANTIC {
  uint32_t Type;
  char     Name[256];
  BOOL     RegisterSpecified;
  uint32_t RegisterNum;
  uint32_t RegisterMask;
};

/**
 * \brief Extended D3D11 device, revision 2
 *
 * Adds NVAPI-style extended shader creation for
 * SMP / multi-view rendering (dxvk-nvapi interop).
 */
MIDL_INTERFACE("1d5c6a10-9f4b-4e0a-b6a4-2c8e13d70f51")
ID3D11VkExtDevice2 : public ID3D11VkExtDevice1 {

  virtual HRESULT STDMETHODCALLTYPE CreateVertexShaderNvSemantics(
    const void*                     pShaderBytecode,
          SIZE_T                    BytecodeLength,
          ID3D11ClassLinkage*       pClassLinkage,
    const D3D11_VK_NV_CUSTOM_SEMANTIC* pSemantics,
          uint32_t                  NumSemantics,
          ID3D11VertexShader**      ppVertexShader) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderNvSemantics(
    const void*                     pShaderBytecode,
          SIZE_T                    BytecodeLength,
          ID3D11ClassLinkage*       pClassLinkage,
    const D3D11_VK_NV_CUSTOM_SEMANTIC* pSemantics,
          uint32_t                  NumSemantics,
          BOOL                      UseViewportMask,
          ID3D11GeometryShader**    ppGeometryShader) = 0;
};

/**
 * \brief Extended D3D11 context, revision 2
 *
 * Adds the SMP multi-view mode toggle (dxvk-nvapi interop).
 */
MIDL_INTERFACE("7e2c1b9f-4d38-4a11-9b6e-0f5a8c42d3e7")
ID3D11VkExtContext2 : public ID3D11VkExtContext1 {

  virtual void STDMETHODCALLTYPE SetMultiviewModeNV(
          uint32_t                  NumViews,
          BOOL                      IndependentViewportMask) = 0;
};


/**
 * \brief Frame reports used for Reflex interop
 */
struct D3D_LOW_LATENCY_FRAME_REPORT
{
    UINT64 frameID;
    UINT64 inputSampleTime;
    UINT64 simStartTime;
    UINT64 simEndTime;
    UINT64 renderSubmitStartTime;
    UINT64 renderSubmitEndTime;
    UINT64 presentStartTime;
    UINT64 presentEndTime;
    UINT64 driverStartTime;
    UINT64 driverEndTime;
    UINT64 osRenderQueueStartTime;
    UINT64 osRenderQueueEndTime;
    UINT64 gpuRenderStartTime;
    UINT64 gpuRenderEndTime;
    UINT32 gpuActiveRenderTimeUs;
    UINT32 gpuFrameTimeUs;
    UINT8 rsvd[120];
};


/**
 * \brief Data structure used for Reflex interop
 */
struct D3D_LOW_LATENCY_RESULTS
{
    UINT32 version;
    D3D_LOW_LATENCY_FRAME_REPORT frameReports[64];
    UINT8 rsvd[32];
};


/**
 * \brief D3D interop interface for Nvidia Reflex
 */
MIDL_INTERFACE("f3112584-41f9-348d-a59b-00b7e1d285d6")
ID3DLowLatencyDevice : public IUnknown {
  virtual BOOL STDMETHODCALLTYPE SupportsLowLatency() = 0;

  virtual HRESULT STDMETHODCALLTYPE LatencySleep() = 0;

  virtual HRESULT STDMETHODCALLTYPE SetLatencySleepMode(
          BOOL                          LowLatencyEnable,
          BOOL                          LowLatencyBoost,
          UINT32                        MinIntervalUs) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetLatencyMarker(
          UINT64                        FrameId,
          UINT32                        MarkerType) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetLatencyInfo(
          D3D_LOW_LATENCY_RESULTS*      pLowLatencyResults) = 0;
};


#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D11VkExtDevice,         0x8a6e3c42,0xf74c,0x45b7,0x82,0x65,0xa2,0x31,0xb6,0x77,0xca,0x17);
__CRT_UUID_DECL(ID3D11VkExtDevice1,        0xcfcf64ef,0x9586,0x46d0,0xbc,0xa4,0x97,0xcf,0x2c,0xa6,0x1b,0x06);
__CRT_UUID_DECL(ID3D11VkExtDevice2,        0x1d5c6a10,0x9f4b,0x4e0a,0xb6,0xa4,0x2c,0x8e,0x13,0xd7,0x0f,0x51);
__CRT_UUID_DECL(ID3D11VkExtContext,        0xfd0bca13,0x5cb6,0x4c3a,0x98,0x7e,0x47,0x50,0xde,0x2c,0xa7,0x91);
__CRT_UUID_DECL(ID3D11VkExtContext1,       0x874b09b2,0xae0b,0x41d8,0x84,0x76,0x5f,0x3b,0x7a,0x0e,0x87,0x9d);
__CRT_UUID_DECL(ID3D11VkExtContext2,       0x7e2c1b9f,0x4d38,0x4a11,0x9b,0x6e,0x0f,0x5a,0x8c,0x42,0xd3,0xe7);
__CRT_UUID_DECL(ID3DLowLatencyDevice,      0xf3112584,0x41f9,0x348d,0xa5,0x9b,0x00,0xb7,0xe1,0xd2,0x85,0xd6);
#endif
