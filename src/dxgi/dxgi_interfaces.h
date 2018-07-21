#pragma once

#include "../dxvk/dxvk_include.h"

#include "dxgi_format.h"
#include "dxgi_include.h"

namespace dxvk {
  class DxgiAdapter;
  class DxvkAdapter;
  class DxvkBuffer;
  class DxvkDevice;
  class DxvkImage;
}

struct IDXGIVkInteropDevice;

/**
 * \brief Private DXGI device interface
 * 
 * The implementation of \c IDXGIDevice stores a
 * \ref DxvkDevice which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("7a622cf6-627a-46b2-b52f-360ef3da831c")
IDXGIVkDevice : public IDXGIDevice2 {
  static const GUID guid;
  
  virtual ~IDXGIVkDevice() { }
  
  virtual dxvk::Rc<dxvk::DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice() = 0;
};


/**
 * \brief Private DXGI adapter interface
 * 
 * The implementation of \c IDXGIAdapter holds a
 * \ref DxvkAdapter which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("907bf281-ea3c-43b4-a8e4-9f231107b4ff")
IDXGIVkAdapter : public IDXGIAdapter2 {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkAdapter> STDMETHODCALLTYPE GetDXVKAdapter() = 0;
  
  /**
   * \brief Creates a DXGI device object
   * 
   * \param [in] pAdapter The adapter
   * \param [in] pFeatures Device features to enable
   * \param [out] ppDevice The DXGI device object
   * \returns \c S_OK on success, or an error code
   */
  virtual HRESULT STDMETHODCALLTYPE CreateDevice(
          IDXGIObject*              pContainer,
    const VkPhysicalDeviceFeatures* pFeatures,
          IDXGIVkDevice**           ppDevice) = 0;
  
  /**
   * \brief Maps a DXGI format to a compatible Vulkan format
   * 
   * For color formats, the returned Vulkan format has the
   * same memory layout as the DXGI format so that it can
   * be mapped and copied to buffers. For depth-stencil
   * formats, this is not guaranteed.
   * \param [in] format The DXGI format
   * \param [in] mode Format lookup mode
   * \returns Vulkan format mapping
   */
  virtual dxvk::DXGI_VK_FORMAT_INFO STDMETHODCALLTYPE LookupFormat(
          DXGI_FORMAT               Format,
          dxvk::DXGI_VK_FORMAT_MODE Mode) = 0;
  
  /**
   * \brief Queries the compatibility family of a given format
   * 
   * \param [in] Format The DXGI format
   * \param [in] Mode Format lookup mode
   * \returns Format family
   */
  virtual dxvk::DXGI_VK_FORMAT_FAMILY STDMETHODCALLTYPE LookupFormatFamily(
          DXGI_FORMAT               Format,
          dxvk::DXGI_VK_FORMAT_MODE Mode) = 0;
};


/**
 * \brief Swap chain back buffer interface
 * 
 * Allows the swap chain and presenter to query
 * the underlying image while it is embedded in
 * a texture object specified by the client API.
 */
MIDL_INTERFACE("5679becd-8547-4d93-96a1-e61a1ce7ef37")
IDXGIVkBackBuffer : public IUnknown {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
};


/**
 * \brief Private presentation device interface
 * 
 * Allows a swap chain to communicate with the device
 * in order to flush pending commands or create the
 * back buffer interface.
 */
MIDL_INTERFACE("79352328-16f2-4f81-9746-9c2e2ccd43cf")
IDXGIVkPresenter : public IUnknown {
  static const GUID guid;
  
  /**
   * \brief Creates a swap chain back buffer
   * 
   * \param [in] pSwapChainDesc Swap chain description
   * \param [out] ppBackBuffer The swap chain back buffer
   * \returns \c S_OK on success
   */
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainBackBuffer(
    const DXGI_SWAP_CHAIN_DESC1*      pSwapChainDesc,
          IDXGIVkBackBuffer**         ppBackBuffer) = 0;
  
  /**
   * \brief Flushes the immediate context
   * 
   * Used by the swap chain's \c Present method to
   * ensure that all rendering commands get dispatched
   * before presenting the swap chain's back buffer.
   * \returns \c S_OK on success
   */
  virtual HRESULT STDMETHODCALLTYPE FlushRenderingCommands() = 0;
  
  /**
   * \brief Underlying DXVK device
   * 
   * \param [in] riid Device type
   * \param [in] ppDevice device
   * \returns DXVK device handle
   */
  virtual HRESULT STDMETHODCALLTYPE GetDevice(
          REFGUID     riid,
          void**      ppDevice) = 0;
};


/**
 * \brief DXGI surface interface for Vulkan interop
 * 
 * Provides access to the backing resource of a
 * DXGI surface, which is typically a D3D texture.
 */
MIDL_INTERFACE("5546cf8c-77e7-4341-b05d-8d4d5000e77d")
IDXGIVkInteropSurface : public IUnknown {
  static const GUID guid;
  
  /**
   * \brief Retrieves device interop interfaceSlots
   * 
   * Queries the device that owns the surface for
   * the \ref IDXGIVkInteropDevice interface.
   * \param [out] ppDevice The device interface
   * \returns \c S_OK on success
   */
  virtual HRESULT STDMETHODCALLTYPE GetDevice(
          IDXGIVkInteropDevice**  ppDevice) = 0;
  
  /**
   * \brief Retrieves Vulkan image info
   * 
   * Retrieves both the image handle as well as the image's
   * properties. Any of the given pointers may be \c nullptr.
   * 
   * If \c pInfo is not \c nullptr, the following rules apply:
   * - \c pInfo->sType \e must be \c VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
   * - \c pInfo->pNext \e must be \c nullptr or point to a supported
   *   extension-specific structure (currently none)
   * - \c pInfo->queueFamilyIndexCount must be the length of the
   *   \c pInfo->pQueueFamilyIndices array, in \c uint32_t units.
   * - \c pInfo->pQueueFamilyIndices must point to a pre-allocated
   *   array of \c uint32_t of size \c pInfo->pQueueFamilyIndices.
   * 
   * \note As of now, the sharing mode will always be
   *       \c VK_SHARING_MODE_EXCLUSIVE and no queue
   *       family indices will be written to the array.
   * 
   * After the call, the structure pointed to by \c pInfo can
   * be used to create an image with identical properties.
   * 
   * If \c pLayout is not \c nullptr, it will receive the
   * layout that the image will be in after flushing any
   * outstanding commands on the device.
   * \param [out] pHandle The image handle
   * \param [out] pLayout Image layout
   * \param [out] pInfo Image properties
   * \returns \c S_OK on success, or \c E_INVALIDARG
   */
  virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
          VkImage*              pHandle,
          VkImageLayout*        pLayout,
          VkImageCreateInfo*    pInfo) = 0;
};


/**
 * \brief DXGI device interface for Vulkan interop
 * 
 * Provides access to the device and instance handles
 * as well as the queue that is used for rendering.
 */
MIDL_INTERFACE("e2ef5fa5-dc21-4af7-90c4-f67ef6a09323")
IDXGIVkInteropDevice : public IUnknown {
  static const GUID guid;
  
  /**
   * \brief Queries Vulkan handles used by DXVK
   * 
   * \param [out] pInstance The Vulkan instance
   * \param [out] pPhysDev The physical device
   * \param [out] pDevide The device handle
   */
  virtual void STDMETHODCALLTYPE GetVulkanHandles(
          VkInstance*           pInstance,
          VkPhysicalDevice*     pPhysDev,
          VkDevice*             pDevice) = 0;
  
  /**
   * \brief Queries the rendering queue used by DXVK
   * 
   * \param [out] pQueue The Vulkan queue handle
   * \param [out] pQueueFamilyIndex Queue family index
   */
  virtual void STDMETHODCALLTYPE GetSubmissionQueue(
          VkQueue*              pQueue,
          uint32_t*             pQueueFamilyIndex) = 0;
  
  /**
   * \brief Transitions a surface to a given layout
   * 
   * Executes an explicit image layout transition on the
   * D3D device. Note that the image subresources \e must
   * be transitioned back to its original layout before
   * using it again from D3D11.
   * \param [in] pSurface The image to transform
   * \param [in] pSubresources Subresources to transform
   * \param [in] OldLayout Current image layout
   * \param [in] NewLayout Desired image layout
   */
  virtual void STDMETHODCALLTYPE TransitionSurfaceLayout(
          IDXGIVkInteropSurface*    pSurface,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) = 0;
  
  /**
   * \brief Flushes outstanding D3D rendering commands
   * 
   * Must be called before submitting Vulkan commands
   * to the rendering queue if those commands use the
   * backing resource of a D3D11 object.
   */
  virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
  
  /**
   * \brief Locks submission queue
   * 
   * Should be called immediately before submitting
   * Vulkan commands to the rendering queue in order
   * to prevent DXVK from using the queue.
   * 
   * While the submission queue is locked, no D3D11
   * methods must be called from the locking thread,
   * or otherwise a deadlock might occur.
   */
  virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
  
  /**
   * \brief Releases submission queue
   * 
   * Should be called immediately after submitting
   * Vulkan commands to the rendering queue in order
   * to allow DXVK to submit new commands.
   */
  virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
};


#ifdef _MSC_VER
struct __declspec(uuid("907bf281-ea3c-43b4-a8e4-9f231107b4ff")) IDXGIVkAdapter;
struct __declspec(uuid("7a622cf6-627a-46b2-b52f-360ef3da831c")) IDXGIVkDevice;
struct __declspec(uuid("5679becd-8547-4d93-96a1-e61a1ce7ef37")) IDXGIVkBackBuffer;
struct __declspec(uuid("79352328-16f2-4f81-9746-9c2e2ccd43cf")) IDXGIVkPresenter;
struct __declspec(uuid("e2ef5fa5-dc21-4af7-90c4-f67ef6a09323")) IDXGIVkInteropDevice;
struct __declspec(uuid("5546cf8c-77e7-4341-b05d-8d4d5000e77d")) IDXGIVkInteropSurface;
#else
DXVK_DEFINE_GUID(IDXGIVkAdapter);
DXVK_DEFINE_GUID(IDXGIVkDevice);
DXVK_DEFINE_GUID(IDXGIVkBackBuffer);
DXVK_DEFINE_GUID(IDXGIVkPresenter);
DXVK_DEFINE_GUID(IDXGIVkInteropDevice);
DXVK_DEFINE_GUID(IDXGIVkInteropSurface);
#endif