#pragma once

#include <vector>

#include "dxvk_image.h"

#include "../util/util_small_vector.h"

namespace dxvk {

  struct DxvkImplicitResolveView {
    Rc<DxvkImageView> inputView   = nullptr;
    Rc<DxvkImageView> resolveView = nullptr;
    bool              resolveDone = false;
  };


  struct DxvkImplicitResolveOp {
    Rc<DxvkImage>             inputImage    = nullptr;
    Rc<DxvkImage>             resolveImage  = nullptr;
    VkImageResolve            resolveRegion = { };
    VkFormat                  resolveFormat = VK_FORMAT_UNDEFINED;
  };


  class DxvkDevice;

  class DxvkImplicitResolveTracker {

  public:

    DxvkImplicitResolveTracker(Rc<DxvkDevice> device);

    ~DxvkImplicitResolveTracker();

    /**
     * \brief Checks whether there are pending resolves
     *
     * \returns \c true if any there are any resolves that must
     *    be executed prior to submitting the current draw.
     */
    bool hasPendingResolves() const {
      return !m_resolveOps.empty();
    }

    /**
     * \brief Retrieves resolve image view for a given input view
     *
     * \param [in] view Multisampled view bound to the context
     * \returns Non-multisampled view to replace the bound view with
     */
    Rc<DxvkImageView> getResolveView(
            DxvkImageView&              view,
            uint64_t                    trackingId);

    /**
     * \brief Extracts a resolve operation to execute
     *
     * \param [out] resolve Extracted resolve parameters
     * \returns \c true if a resolve was extracted, \c false
     *    if all resolves have already been processed.
     */
    bool extractResolve(
            DxvkImplicitResolveOp&      resolve);

    /**
     * \brief Invalidates resolve cache for a given set of image subresources
     *
     * Must be called any time the given set of subresources of this
     * resource is written, so that the corresponding resolve image
     * can get updated the next time it is read. Must not be called
     * for any subresource that is only being read, since that may
     * cause problems with read-only depth-stencil access.
     * \param [in] image The multisampled image
     * \param [in] subresources Image subresources written
     */
    void invalidate(
      const DxvkImage&                  image,
      const VkImageSubresourceRange&    subresources);

    /**
     * \brief Cleans up resolve image cache
     *
     * Destroys resolve images that have not been used in a while
     * in order to reduce memory wasted on unused images.
     * \param [in] trackingId Current context command list ID
     */
    void cleanup(
            uint64_t                    trackingId);

  private:

    Rc<DxvkDevice> m_device;

    std::vector<DxvkImplicitResolveView>  m_resolveViews;
    std::vector<DxvkImplicitResolveOp>    m_resolveOps;

    void addResolveOp(
            DxvkImplicitResolveView&    view);

    void cleanup(
            VkDeviceSize                allocationSize,
            uint64_t                    trackingId);

  };

}
