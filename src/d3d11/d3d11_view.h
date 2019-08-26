#pragma once

#include "d3d11_include.h"

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
  inline bool CheckViewOverlap(const D3D11_VK_VIEW_INFO& a, const D3D11_VK_VIEW_INFO b) {
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

}