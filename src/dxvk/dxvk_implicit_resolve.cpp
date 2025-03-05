#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_implicit_resolve.h"

namespace dxvk {

  DxvkImplicitResolveTracker::DxvkImplicitResolveTracker(Rc<DxvkDevice> device)
  : m_device(std::move(device)) {

  }


  DxvkImplicitResolveTracker::~DxvkImplicitResolveTracker() {

  }


  Rc<DxvkImageView> DxvkImplicitResolveTracker::getResolveView(
            DxvkImageView&              view,
            uint64_t                    trackingId) {
    // We generally only expect to have one or two views at most in games
    // that hit this path at all, so iterating over the arras is fine
    for (auto& v : m_resolveViews) {
      if (v.inputView == &view) {
        addResolveOp(v);
        return v.resolveView;
      }
    }

    // Create a new resolve image with only the array layers covered by the
    // input view. We expect resolve images to be somewhat short-lived.
    DxvkImageCreateInfo imageInfo = view.image()->info();

    DxvkImageCreateInfo resolveInfo = { };
    resolveInfo.type = imageInfo.type;
    resolveInfo.format = view.info().format;
    resolveInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    resolveInfo.extent = imageInfo.extent;
    resolveInfo.numLayers = view.info().layerCount;
    resolveInfo.mipLevels = 1u;
    resolveInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    resolveInfo.stages = m_device->getShaderPipelineStages();
    resolveInfo.access = VK_ACCESS_SHADER_READ_BIT;
    resolveInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    resolveInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    resolveInfo.transient = VK_TRUE;
    resolveInfo.debugName = "Resolve image";

    if (view.info().aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      resolveInfo.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      resolveInfo.stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      resolveInfo.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    } else {
      resolveInfo.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      resolveInfo.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      resolveInfo.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    Rc<DxvkImage> image = m_device->createImage(resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    cleanup(image->getMemoryInfo().size, trackingId);

    DxvkImageViewKey viewKey = view.info();
    viewKey.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewKey.layerIndex = 0u;

    auto& resolveView = m_resolveViews.emplace_back();
    resolveView.inputView = &view;
    resolveView.resolveView = image->createView(viewKey);

    addResolveOp(resolveView);

    return resolveView.resolveView;
  }


  bool DxvkImplicitResolveTracker::extractResolve(
          DxvkImplicitResolveOp&      resolve) {
    if (m_resolveOps.empty()) {
      resolve = DxvkImplicitResolveOp();
      return false;
    }

    resolve = std::move(m_resolveOps.back());
    m_resolveOps.pop_back();
    return true;
  }


  void DxvkImplicitResolveTracker::invalidate(
    const DxvkImage&                  image,
    const VkImageSubresourceRange&    subresources) {
    for (auto& v : m_resolveViews) {
      if (v.resolveDone && v.inputView->image() == &image) {
        auto viewSubresource = v.inputView->imageSubresources();

        if ((subresources.aspectMask & viewSubresource.aspectMask)
         && vk::checkSubresourceRangeOverlap(viewSubresource, subresources))
          v.resolveDone = false;
      }
    }
  }


  void DxvkImplicitResolveTracker::cleanup(
        uint64_t                    trackingId) {
    cleanup(0u, trackingId);
  }


  void DxvkImplicitResolveTracker::addResolveOp(
          DxvkImplicitResolveView&    view) {
    if (view.resolveDone)
      return;

    // Determine resolve parameters based on the view format rather than the
    // image format, since this will more likely represent what the app is
    // trying to do
    auto format = view.inputView->formatInfo();

    auto& op = m_resolveOps.emplace_back();
    op.inputImage = view.inputView->image();
    op.resolveImage = view.resolveView->image();
    op.resolveRegion.srcSubresource = vk::pickSubresourceLayers(view.inputView->imageSubresources(), 0u);
    op.resolveRegion.srcSubresource.aspectMask = format->aspectMask;
    op.resolveRegion.dstSubresource = vk::pickSubresourceLayers(view.resolveView->imageSubresources(), 0u);
    op.resolveRegion.dstSubresource.aspectMask = format->aspectMask;
    op.resolveRegion.dstSubresource.baseArrayLayer = 0u;
    op.resolveRegion.extent = view.resolveView->mipLevelExtent(0u);
    op.resolveFormat = view.inputView->info().format;

    view.resolveDone = true;
  }


  void DxvkImplicitResolveTracker::cleanup(
          VkDeviceSize                allocationSize,
          uint64_t                    trackingId) {
    constexpr VkDeviceSize MaxMemory = 64ull << 20u;

    constexpr uint64_t MaxLifetime = 256u;
    constexpr uint64_t MinLifetime =  16u;

    // Eliminate images that haven't been used in a long time
    for (auto i = m_resolveViews.begin(); i != m_resolveViews.end(); ) {
      if (i->resolveView->image()->getTrackId() + MaxLifetime < trackingId) {
        i = m_resolveViews.erase(i);
      } else {
        allocationSize += i->resolveView->image()->getMemoryInfo().size;
        i++;
      }
    }

    // If we're using a large amount of memory for resolve images, eliminate
    // the least recently used resolve images until we drop below the size
    // threshold again.
    while (allocationSize > MaxMemory) {
      auto lr = m_resolveViews.end();

      for (auto i = m_resolveViews.begin(); i != m_resolveViews.end(); i++) {
        if (i->resolveView->image()->getTrackId() + MinLifetime < trackingId) {
          if (lr == m_resolveViews.end()
           || lr->resolveView->image()->getTrackId() > i->resolveView->image()->getTrackId())
            lr = i;
        }
      }

      if (lr == m_resolveViews.end())
        break;

      allocationSize -= lr->resolveView->image()->getMemoryInfo().size;
      m_resolveViews.erase(lr);
    }
  }

}
