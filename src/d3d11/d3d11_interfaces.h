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
};


/**
 * \brief Barrier control flags
 */
enum D3D11_VK_BARRIER_CONTROL : uint32_t {
  D3D11_VK_BARRIER_CONTROL_IGNORE_WRITE_AFTER_WRITE   = 1 << 0,
  D3D11_VK_BARRIER_CONTROL_IGNORE_GRAPHICS_UAV        = 1 << 1,
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


#ifdef _MSC_VER
struct __declspec(uuid("8a6e3c42-f74c-45b7-8265-a231b677ca17")) ID3D11VkExtDevice;
struct __declspec(uuid("cfcf64ef-9586-46d0-bca4-97cf2ca61b06")) ID3D11VkExtDevice1;
struct __declspec(uuid("fd0bca13-5cb6-4c3a-987e-4750de2ca791")) ID3D11VkExtContext;
struct __declspec(uuid("874b09b2-ae0b-41d8-8476-5f3b7a0e879d")) ID3D11VkExtContext1;
#else
__CRT_UUID_DECL(ID3D11VkExtDevice,         0x8a6e3c42,0xf74c,0x45b7,0x82,0x65,0xa2,0x31,0xb6,0x77,0xca,0x17);
__CRT_UUID_DECL(ID3D11VkExtDevice1,        0xcfcf64ef,0x9586,0x46d0,0xbc,0xa4,0x97,0xcf,0x2c,0xa6,0x1b,0x06);
__CRT_UUID_DECL(ID3D11VkExtContext,        0xfd0bca13,0x5cb6,0x4c3a,0x98,0x7e,0x47,0x50,0xde,0x2c,0xa7,0x91);
__CRT_UUID_DECL(ID3D11VkExtContext1,       0x874b09b2,0xae0b,0x41d8,0x84,0x76,0x5f,0x3b,0x7a,0x0e,0x87,0x9d);
#endif
