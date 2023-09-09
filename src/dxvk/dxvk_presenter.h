#pragma once

#include <functional>
#include <vector>

#include "../util/log/log.h"

#include "../util/util_error.h"
#include "../util/util_fps_limiter.h"
#include "../util/util_math.h"
#include "../util/util_string.h"

#include "../util/sync/sync_signal.h"

#include "../vulkan/vulkan_loader.h"

#include "dxvk_format.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Presenter description
   * 
   * Contains the desired properties of
   * the swap chain. This is passed as
   * an input during swap chain creation.
   */
  struct PresenterDesc {
    VkExtent2D          imageExtent;
    uint32_t            imageCount;
    uint32_t            numFormats;
    VkSurfaceFormatKHR  formats[4];
    VkFullScreenExclusiveEXT fullScreenExclusive;
  };

  /**
   * \brief Presenter properties
   * 
   * Contains the actual properties
   * of the underlying swap chain.
   */
  struct PresenterInfo {
    VkSurfaceFormatKHR  format;
    VkPresentModeKHR    presentMode;
    VkExtent2D          imageExtent;
    uint32_t            imageCount;
    uint32_t            syncInterval;
  };

  /**
   * \brief Swap image and view
   */
  struct PresenterImage {
    VkImage     image = VK_NULL_HANDLE;
    VkImageView view  = VK_NULL_HANDLE;
  };

  /**
   * \brief Presenter semaphores
   * 
   * Pair of semaphores used for acquire and present
   * operations, including the command buffers used
   * in between. Also stores a fence to signal on
   * image acquisition.
   */
  struct PresenterSync {
    VkSemaphore acquire;
    VkSemaphore present;
  };

  /**
   * \brief Queued frame
   */
  struct PresenterFrame {
    uint64_t          frameId;
    VkPresentModeKHR  mode;
    VkResult          result;
  };

  /**
   * \brief Vulkan presenter
   * 
   * Provides abstractions for some of the
   * more complicated aspects of Vulkan's
   * window system integration.
   */
  class Presenter : public RcObject {

  public:

    Presenter(
      const Rc<DxvkDevice>&   device,
      const Rc<sync::Signal>& signal,
      const PresenterDesc&    desc);
    
    ~Presenter();

    /**
     * \brief Actual presenter info
     * \returns Swap chain properties
     */
    PresenterInfo info() const;

    /**
     * \brief Retrieves image by index
     * 
     * Can be used to create per-image objects.
     * \param [in] index Image index
     * \returns Image handle
     */
    PresenterImage getImage(
            uint32_t        index) const;

    /**
     * \brief Acquires next image
     * 
     * Potentially blocks the calling thread.
     * If this returns an error, the swap chain
     * must be recreated and a new image must
     * be acquired before proceeding.
     * \param [out] sync Synchronization semaphores
     * \param [out] index Acquired image index
     * \returns Status of the operation
     */
    VkResult acquireNextImage(
            PresenterSync&  sync,
            uint32_t&       index);
    
    /**
     * \brief Presents current image
     * 
     * Presents the current image. If this returns
     * an error, the swap chain must be recreated,
     * but do not present before acquiring an image.
     * \param [in] mode Present mode
     * \param [in] frameId Frame number.
     *    Must increase monotonically.
     * \returns Status of the operation
     */
    VkResult presentImage(
            VkPresentModeKHR  mode,
            uint64_t          frameId);

    /**
     * \brief Signals a given frame
     *
     * Waits for the present operation to complete and then signals
     * the presenter signal with the given frame ID. Must not be
     * called before GPU work prior to the present submission has
     * completed in order to maintain consistency.
     * \param [in] result Presentation result
     * \param [in] mode Present mode
     * \param [in] frameId Frame number
     */
    void signalFrame(
            VkResult          result,
            VkPresentModeKHR  mode,
            uint64_t          frameId);

    /**
     * \brief Changes and takes ownership of surface
     *
     * The presenter will destroy the surface as necessary.
     * \param [in] fn Surface create function
     */
    VkResult recreateSurface(
      const std::function<VkResult (VkSurfaceKHR*)>& fn);

    /**
     * \brief Changes presenter properties
     * 
     * Recreates the swap chain immediately. Note that
     * no swap chain resources must be in use by the
     * GPU at the time this is called.
     * \param [in] desc Swap chain description
     * \param [in] surface New Vulkan surface
     */
    VkResult recreateSwapChain(
      const PresenterDesc&  desc);

    /**
     * \brief Changes sync interval
     *
     * If this returns an error, the swap chain must
     * be recreated.
     * \param [in] syncInterval New sync interval
     */
    VkResult setSyncInterval(uint32_t syncInterval);

    /**
     * \brief Changes maximum frame rate
     *
     * \param [in] frameRate Target frame rate. Set
     *    to 0 in order to disable the limiter.
     */
    void setFrameRateLimit(double frameRate);

    /**
     * \brief Checks whether a Vulkan swap chain exists
     *
     * On Windows, there are situations where we cannot create
     * a swap chain as the surface size can reach zero, and no
     * presentation can be performed.
     * \returns \c true if the presenter has a swap chain.
     */
    bool hasSwapChain() const {
      return m_swapchain;
    }

    /**
     * \brief Checks if a presenter supports the colorspace
     *
     * \param [in] colorspace The colorspace to test
     * * \returns \c true if the presenter supports the colorspace
     */
    bool supportsColorSpace(VkColorSpaceKHR colorspace);

    /**
     * \brief Sets HDR metadata
     *
     * \param [in] hdrMetadata HDR Metadata
     */
    void setHdrMetadata(const VkHdrMetadataEXT& hdrMetadata);

  private:

    Rc<DxvkDevice>    m_device;
    Rc<sync::Signal>  m_signal;

    Rc<vk::InstanceFn> m_vki;
    Rc<vk::DeviceFn>  m_vkd;

    PresenterInfo     m_info        = { };

    VkSurfaceKHR      m_surface     = VK_NULL_HANDLE;
    VkSwapchainKHR    m_swapchain   = VK_NULL_HANDLE;

    std::vector<PresenterImage> m_images;
    std::vector<PresenterSync>  m_semaphores;

    std::vector<VkPresentModeKHR> m_dynamicModes;

    uint32_t          m_imageIndex = 0;
    uint32_t          m_frameIndex = 0;

    VkResult          m_acquireStatus = VK_NOT_READY;

    FpsLimiter        m_fpsLimiter;

    dxvk::mutex                 m_frameMutex;
    dxvk::condition_variable    m_frameCond;
    dxvk::thread                m_frameThread;
    std::queue<PresenterFrame>  m_frameQueue;

    std::atomic<uint64_t>       m_lastFrameId = { 0ull };

    VkResult recreateSwapChainInternal(
      const PresenterDesc&  desc);

    VkResult getSupportedFormats(
            std::vector<VkSurfaceFormatKHR>& formats,
            VkFullScreenExclusiveEXT         fullScreenExclusive) const;
    
    VkResult getSupportedPresentModes(
            std::vector<VkPresentModeKHR>& modes,
            VkFullScreenExclusiveEXT       fullScreenExclusive) const;
    
    VkResult getSwapImages(
            std::vector<VkImage>&     images);
    
    VkSurfaceFormatKHR pickFormat(
            uint32_t                  numSupported,
      const VkSurfaceFormatKHR*       pSupported,
            uint32_t                  numDesired,
      const VkSurfaceFormatKHR*       pDesired);

    VkPresentModeKHR pickPresentMode(
            uint32_t                  numSupported,
      const VkPresentModeKHR*         pSupported,
            uint32_t                  syncInterval);

    VkExtent2D pickImageExtent(
      const VkSurfaceCapabilitiesKHR& caps,
            VkExtent2D                desired);

    uint32_t pickImageCount(
            uint32_t                  minImageCount,
            uint32_t                  maxImageCount,
            uint32_t                  desired);

    void destroySwapchain();

    void destroySurface();

    void applyFrameRateLimit(
            VkPresentModeKHR          mode);

    void runFrameThread();

  };

}
