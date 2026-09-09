#pragma once

#include <mutex>
#include <unordered_set>

#include "d3d11_include.h"

#include "../util/thread.h"

namespace dxvk {

  /**
   * \brief Buffer view info
   * 
   * Stores the byte range covered
   * by a buffer view.
   */
  struct D3D11_VK_BUFFER_VIEW_INFO {
    VkDeviceSize                Offset;
    VkDeviceSize                Length;
  };

  /**
   * \brief Image view info
   * 
   * Stores the subresource range
   * covered by an image view.
   */
  struct D3D11_VK_IMAGE_VIEW_INFO {
    VkImageAspectFlags          Aspects;
    uint32_t                    MinLevel;
    uint32_t                    MinLayer;
    uint32_t                    NumLevels;
    uint32_t                    NumLayers;
  };

  /**
   * \brief Common view info
   *
   * Stores a pointer to the resource as
   * well as the type-specific range that
   * is affected by the view.
   */
  struct D3D11_VK_VIEW_INFO {
    ID3D11Resource*             pResource;
    D3D11_RESOURCE_DIMENSION    Dimension;
    UINT                        BindFlags;
    union {
      D3D11_VK_BUFFER_VIEW_INFO Buffer;
      D3D11_VK_IMAGE_VIEW_INFO  Image;
    };
  };

  /**
   * \brief Checks whether two views overlap
   * 
   * Overlapping views may conflict in case
   * one or both views are used for writing.
   * \param [in] a First view to check
   * \param [in] b Second view to check
   * \returns \c true if the views overlap
   */
  inline bool CheckViewOverlap(const D3D11_VK_VIEW_INFO& a, const D3D11_VK_VIEW_INFO& b) {
    if (likely(a.pResource != b.pResource))
      return false;
    
    if (a.Dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
      // Just check whether the buffer ranges overlap
      return (a.Buffer.Offset < b.Buffer.Offset + b.Buffer.Length)
          && (a.Buffer.Offset + a.Buffer.Length > b.Buffer.Offset);
    } else {
      // Check whether the subresource ranges overlap
      return (a.Image.Aspects & b.Image.Aspects)
          && (a.Image.MinLevel < b.Image.MinLevel + b.Image.NumLevels)
          && (a.Image.MinLayer < b.Image.MinLayer + b.Image.NumLayers)
          && (a.Image.MinLevel + a.Image.NumLevels > b.Image.MinLevel)
          && (a.Image.MinLayer + a.Image.NumLayers > b.Image.MinLayer);
    }
  }

  template<typename T1, typename T2>
  bool CheckViewOverlap(const T1* a, const T2* b) {
    return a && b && CheckViewOverlap(a->GetViewInfo(), b->GetViewInfo());
  }

  /**
   * \brief Resource view liveness guard
   *
   * Some applications are known to bind a shader resource, render
   * target, unordered access or depth-stencil view to the pipeline
   * after having already released their last reference to it, which
   * is a use-after-free bug in the application (see DCS World,
   * https://github.com/doitsujin/dxvk/issues/5856). Since these view
   * objects are plain C++ objects that get \c delete'd as soon as
   * their reference count hits zero, binding a stale pointer causes
   * DXVK to dereference freed memory and crash.
   *
   * This tracks the set of currently live view object addresses so
   * that bind entry points can reject a pointer that no longer refers
   * to a live object instead of dereferencing it. It is only ever
   * consulted when \c D3D11Options::viewUAFGuard is enabled, since
   * both the registration on every view's construction/destruction
   * and the lookup on every bind call add measurable overhead; the
   * option defaults to off and is only enabled for known-broken apps.
   *
   * Note that this only guards against the "release, then later bind
   * the same stale pointer" pattern, not against a concurrent Release()
   * racing a bind on another thread targeting the same object - doing
   * so would require holding the registry lock across the bind itself,
   * which isn't worth the extra contention for the guard's one actual
   * use case.
   */
  class D3D11ViewUAFGuard {

  public:

    static void registerView(const void* view) {
      std::lock_guard<dxvk::mutex> lock(s_mutex);
      s_liveViews.insert(view);
    }

    static void unregisterView(const void* view) {
      std::lock_guard<dxvk::mutex> lock(s_mutex);
      s_liveViews.erase(view);
    }

    static bool isViewLive(const void* view) {
      std::lock_guard<dxvk::mutex> lock(s_mutex);
      return s_liveViews.find(view) != s_liveViews.end();
    }

  private:

    static inline dxvk::mutex                     s_mutex;
    static inline std::unordered_set<const void*> s_liveViews;

  };

}
