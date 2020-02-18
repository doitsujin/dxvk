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
};


/**
 * \brief Barrier control flags
 */
enum D3D11_VK_BARRIER_CONTROL : uint32_t {
  D3D11_VK_BARRIER_CONTROL_IGNORE_WRITE_AFTER_WRITE   = 1 << 0,
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

#ifdef _MSC_VER
struct __declspec(uuid("8a6e3c42-f74c-45b7-8265-a231b677ca17")) ID3D11VkExtDevice;
struct __declspec(uuid("fd0bca13-5cb6-4c3a-987e-4750de2ca791")) ID3D11VkExtContext;
#else
__CRT_UUID_DECL(ID3D11VkExtDevice,         0x8a6e3c42,0xf74c,0x45b7,0x82,0x65,0xa2,0x31,0xb6,0x77,0xca,0x17);
__CRT_UUID_DECL(ID3D11VkExtContext,        0xfd0bca13,0x5cb6,0x4c3a,0x98,0x7e,0x47,0x50,0xde,0x2c,0xa7,0x91);
#endif
