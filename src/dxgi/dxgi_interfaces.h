#pragma once

#include "../dxvk/dxvk_include.h"

#include "../wsi/wsi_edid.h"

#include "../util/util_time.h"

#include "dxgi_format.h"
#include "dxgi_include.h"

namespace dxvk {
  class DxgiAdapter;
  class DxgiSwapChain;
  class DxvkAdapter;
  class DxvkBuffer;
  class DxvkDevice;
  class DxvkImage;
}

struct IDXGIVkInteropDevice;


/**
 * \brief Per-monitor data
 */
struct DXGI_VK_MONITOR_DATA {
  dxvk::DxgiSwapChain*  pSwapChain;
  DXGI_FRAME_STATISTICS FrameStats;
  DXGI_GAMMA_CONTROL    GammaCurve;
  DXGI_MODE_DESC1       LastMode;
  dxvk::wsi::WsiDisplayMetadata DisplayMetadata;
};


/**
 * \brief HDR metadata struct
 */
struct DXGI_VK_HDR_METADATA {
  DXGI_HDR_METADATA_TYPE    Type;
  union {
    DXGI_HDR_METADATA_HDR10 HDR10;
  };
};


/**
 * \brief Frame statistics
 */
struct DXGI_VK_FRAME_STATISTICS {
  UINT64 PresentCount;
  UINT64 PresentQPCTime;
};


/**
 * \brief Private DXGI surface factory
 */
MIDL_INTERFACE("1e7895a1-1bc3-4f9c-a670-290a4bc9581a")
IDXGIVkSurfaceFactory : public IUnknown {
  virtual VkResult STDMETHODCALLTYPE CreateSurface(
          VkInstance                Instance,
          VkPhysicalDevice          Adapter,
          VkSurfaceKHR*             pSurface) = 0;
};


/**
 * \brief Private DXGI presenter
 * 
 * Presenter interface that allows the DXGI swap
 * chain implementation to remain API-agnostic,
 * so that common code can stay in one class.
 */
MIDL_INTERFACE("e4a9059e-b569-46ab-8de7-501bd2bc7f7a")
IDXGIVkSwapChain : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetDesc(
          DXGI_SWAP_CHAIN_DESC1*    pDesc) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE GetAdapter(
          REFIID                    riid,
          void**                    ppvObject) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE GetDevice(
          REFIID                    riid,
          void**                    ppDevice) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetImage(
          UINT                      BufferId,
          REFIID                    riid,
          void**                    ppBuffer) = 0;

  virtual UINT STDMETHODCALLTYPE GetImageIndex() = 0;

  virtual UINT STDMETHODCALLTYPE GetFrameLatency() = 0;

  virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyEvent() = 0;

  virtual HRESULT STDMETHODCALLTYPE ChangeProperties(
    const DXGI_SWAP_CHAIN_DESC1*    pDesc,
    const UINT*                     pNodeMasks,
          IUnknown* const*          ppPresentQueues) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetPresentRegion(
    const RECT*                     pRegion) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetGammaControl(
          UINT                      NumControlPoints,
    const DXGI_RGB*                 pControlPoints) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetFrameLatency(
          UINT                      MaxLatency) = 0;

  virtual HRESULT STDMETHODCALLTYPE Present(
          UINT                      SyncInterval,
          UINT                      PresentFlags,
    const DXGI_PRESENT_PARAMETERS*  pPresentParameters) = 0;

  virtual UINT STDMETHODCALLTYPE CheckColorSpaceSupport(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetColorSpace(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(
    const DXGI_VK_HDR_METADATA*     pMetaData) = 0;
};


MIDL_INTERFACE("785326d4-b77b-4826-ae70-8d08308ee6d1")
IDXGIVkSwapChain1 : public IDXGIVkSwapChain {
  virtual void STDMETHODCALLTYPE GetLastPresentCount(
          UINT64*                   pLastPresentCount) = 0;

  virtual void STDMETHODCALLTYPE GetFrameStatistics(
          DXGI_VK_FRAME_STATISTICS* pFrameStatistics) = 0;
};


/**
 * \brief Private DXGI presenter factory
 */
MIDL_INTERFACE("e7d6c3ca-23a0-4e08-9f2f-ea5231df6633")
IDXGIVkSwapChainFactory : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
          IDXGIVkSurfaceFactory*    pSurfaceFactory,
    const DXGI_SWAP_CHAIN_DESC1*    pDesc,
          IDXGIVkSwapChain**        ppSwapChain) = 0;
};


/**
 * \brief Private DXGI adapter interface
 * 
 * The implementation of \c IDXGIAdapter holds a
 * \ref DxvkAdapter which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("907bf281-ea3c-43b4-a8e4-9f231107b4ff")
IDXGIDXVKAdapter : public IDXGIAdapter4 {
  virtual dxvk::Rc<dxvk::DxvkAdapter> STDMETHODCALLTYPE GetDXVKAdapter() = 0;

  virtual dxvk::Rc<dxvk::DxvkInstance> STDMETHODCALLTYPE GetDXVKInstance() = 0;

};


/**
 * \brief Private DXGI device interface
 */
MIDL_INTERFACE("92a5d77b-b6e1-420a-b260-fdd701272827")
IDXGIDXVKDevice : public IUnknown {
  virtual void STDMETHODCALLTYPE SetAPIVersion(
            UINT                    Version) = 0;

  virtual UINT STDMETHODCALLTYPE GetAPIVersion() = 0;

};


/**
 * \brief Private DXGI monitor info interface
 * 
 * Can be queried from the DXGI factory to store monitor
 * info globally, with a lifetime that exceeds that of
 * the \c IDXGIOutput or \c IDXGIAdapter objects.
 */
MIDL_INTERFACE("c06a236f-5be3-448a-8943-89c611c0c2c1")
IDXGIVkMonitorInfo : public IUnknown {
  /**
   * \brief Initializes monitor data
   * 
   * Fails if data for the given monitor already exists.
   * \param [in] hMonitor The monitor handle
   * \param [in] pData Initial data
   */
  virtual HRESULT STDMETHODCALLTYPE InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData) = 0;

  /**
   * \brief Retrieves and locks monitor data
   * 
   * Fails if no data for the given monitor exists.
   * \param [in] hMonitor The monitor handle
   * \param [out] Pointer to monitor data
   * \returns S_OK on success
   */
  virtual HRESULT STDMETHODCALLTYPE AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) = 0;
  
  /**
   * \brief Unlocks monitor data
   * 
   * Must be called after each successful
   * call to \ref AcquireMonitorData.
   * \param [in] hMonitor The monitor handle
   */
  virtual void STDMETHODCALLTYPE ReleaseMonitorData() = 0;

  /**
   * \brief Punt global colorspace
   *
   * This exists to satiate a requirement for
   * IDXGISwapChain::SetColorSpace1 punting Windows into
   * the global "HDR mode".
   *
   * This operation is atomic and does not require
   * owning any monitor data.
   *
   * \param [in] ColorSpace The colorspace
   */
  virtual void STDMETHODCALLTYPE PuntColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace) = 0;

  /**
   * \brief Get current global colorspace
   *
   * This operation is atomic and does not require
   * owning any monitor data.
   *
   * \returns Current global colorspace
   */
  virtual DXGI_COLOR_SPACE_TYPE STDMETHODCALLTYPE CurrentColorSpace() const = 0;

};


/**
 * \brief DXGI surface interface for Vulkan interop
 * 
 * Provides access to the backing resource of a
 * DXGI surface, which is typically a D3D texture.
 */
MIDL_INTERFACE("5546cf8c-77e7-4341-b05d-8d4d5000e77d")
IDXGIVkInteropSurface : public IUnknown {
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

struct D3D11_TEXTURE2D_DESC1;
struct ID3D11Texture2D;

/**
 * \brief See IDXGIVkInteropDevice.
 */
MIDL_INTERFACE("e2ef5fa5-dc21-4af7-90c4-f67ef6a09324")
IDXGIVkInteropDevice1 : public IDXGIVkInteropDevice {
  /**
   * \brief Queries the rendering queue used by DXVK
   * 
   * \param [out] pQueue The Vulkan queue handle
   * \param [out] pQueueIndex Queue index
   * \param [out] pQueueFamilyIndex Queue family index
   */
  virtual void STDMETHODCALLTYPE GetSubmissionQueue1(
          VkQueue*              pQueue,
          uint32_t*             pQueueIndex,
          uint32_t*             pQueueFamilyIndex) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateTexture2DFromVkImage(
    const D3D11_TEXTURE2D_DESC1* pDesc,
          VkImage               vkImage,
          ID3D11Texture2D**     ppTexture2D) = 0;
};

/**
 * \brief DXGI adapter interface for Vulkan interop
 *
 * Provides access to the physical device and
 * instance handles for the given DXGI adapter.
 */
MIDL_INTERFACE("3a6d8f2c-b0e8-4ab4-b4dc-4fd24891bfa5")
IDXGIVkInteropAdapter : public IUnknown {
  /**
   * \brief Queries Vulkan handles used by DXVK
   *
   * \param [out] pInstance The Vulkan instance
   * \param [out] pPhysDev The physical device
   */
  virtual void STDMETHODCALLTYPE GetVulkanHandles(
          VkInstance*           pInstance,
          VkPhysicalDevice*     pPhysDev) = 0;
};

/**
 * \brief DXGI factory interface for Vulkan interop
 */
MIDL_INTERFACE("4c5e1b0d-b0c8-4131-bfd8-9b2476f7f408")
IDXGIVkInteropFactory : public IUnknown {
  /**
   * \brief Queries Vulkan instance used by DXVK
   *
   * \param [out] pInstance The Vulkan instance
   * \param [out] ppfnVkGetInstanceProcAddr Vulkan entry point
   */
  virtual void STDMETHODCALLTYPE GetVulkanInstance(
          VkInstance*           pInstance,
          PFN_vkGetInstanceProcAddr* ppfnVkGetInstanceProcAddr) = 0;
};

/**
 * \brief DXGI factory interface for Vulkan interop
 */
MIDL_INTERFACE("2a289dbd-2d0a-4a51-89f7-f2adce465cd6")
IDXGIVkInteropFactory1 : public IDXGIVkInteropFactory {
  virtual HRESULT STDMETHODCALLTYPE GetGlobalHDRState(
          DXGI_COLOR_SPACE_TYPE   *pOutColorSpace,
          DXGI_HDR_METADATA_HDR10 *ppOutMetadata) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetGlobalHDRState(
          DXGI_COLOR_SPACE_TYPE    ColorSpace,
    const DXGI_HDR_METADATA_HDR10 *pMetadata) = 0;
};


#ifndef _MSC_VER
__CRT_UUID_DECL(IDXGIDXVKAdapter,          0x907bf281,0xea3c,0x43b4,0xa8,0xe4,0x9f,0x23,0x11,0x07,0xb4,0xff);
__CRT_UUID_DECL(IDXGIDXVKDevice,           0x92a5d77b,0xb6e1,0x420a,0xb2,0x60,0xfd,0xf7,0x01,0x27,0x28,0x27);
__CRT_UUID_DECL(IDXGIVkMonitorInfo,        0xc06a236f,0x5be3,0x448a,0x89,0x43,0x89,0xc6,0x11,0xc0,0xc2,0xc1);
__CRT_UUID_DECL(IDXGIVkInteropFactory,     0x4c5e1b0d,0xb0c8,0x4131,0xbf,0xd8,0x9b,0x24,0x76,0xf7,0xf4,0x08);
__CRT_UUID_DECL(IDXGIVkInteropFactory1,    0x2a289dbd,0x2d0a,0x4a51,0x89,0xf7,0xf2,0xad,0xce,0x46,0x5c,0xd6);
__CRT_UUID_DECL(IDXGIVkInteropAdapter,     0x3a6d8f2c,0xb0e8,0x4ab4,0xb4,0xdc,0x4f,0xd2,0x48,0x91,0xbf,0xa5);
__CRT_UUID_DECL(IDXGIVkInteropDevice,      0xe2ef5fa5,0xdc21,0x4af7,0x90,0xc4,0xf6,0x7e,0xf6,0xa0,0x93,0x23);
__CRT_UUID_DECL(IDXGIVkInteropDevice1,     0xe2ef5fa5,0xdc21,0x4af7,0x90,0xc4,0xf6,0x7e,0xf6,0xa0,0x93,0x24);
__CRT_UUID_DECL(IDXGIVkInteropSurface,     0x5546cf8c,0x77e7,0x4341,0xb0,0x5d,0x8d,0x4d,0x50,0x00,0xe7,0x7d);
__CRT_UUID_DECL(IDXGIVkSurfaceFactory,     0x1e7895a1,0x1bc3,0x4f9c,0xa6,0x70,0x29,0x0a,0x4b,0xc9,0x58,0x1a);
__CRT_UUID_DECL(IDXGIVkSwapChain,          0xe4a9059e,0xb569,0x46ab,0x8d,0xe7,0x50,0x1b,0xd2,0xbc,0x7f,0x7a);
__CRT_UUID_DECL(IDXGIVkSwapChain1,         0x785326d4,0xb77b,0x4826,0xae,0x70,0x8d,0x08,0x30,0x8e,0xe6,0xd1);
__CRT_UUID_DECL(IDXGIVkSwapChainFactory,   0xe7d6c3ca,0x23a0,0x4e08,0x9f,0x2f,0xea,0x52,0x31,0xdf,0x66,0x33);
#endif
