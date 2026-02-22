#pragma once

#include "dxvk_format.h"
#include "dxvk_include.h"
#include "dxvk_pipelayout.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Clear pipeline info
   *
   * Primarily used to clear storage image or buffer views.
   */
  struct DxvkMetaClear {
    /** Shader arguments for clear pipeline */
    struct Args {
      VkClearColorValue clearValue = { };
      VkOffset3D offset = { };
      VkExtent3D extent = { };
    };

    /** Look-up info for clear pipeline. Set view type to
     *  MAX_ENUM in order to create a buffer pipeline. */
    struct Key {
      VkImageViewType viewType  = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat        format    = VK_FORMAT_UNDEFINED;

      bool eq(const Key& other) const {
        return viewType == other.viewType
            && format   == other.format;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(viewType));
        hash.add(uint32_t(format));
        return hash;
      }
    };

    /** Pipeline properties */
    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkExtent3D workgroupSize = { };
  };

  
  /**
   * \brief Clear shaders and related objects
   * 
   * Creates the shaders, pipeline layouts, and
   * compute pipelines that are going to be used
   * for clear operations.
   */
  class DxvkMetaClearObjects {
    
  public:
    
    DxvkMetaClearObjects(DxvkDevice* device);
    ~DxvkMetaClearObjects();
    
    /**
     * \brief Creates or retrieves clear pipeline
     * \param [in] key Required pipeline properties
     */
    DxvkMetaClear getPipeline(const DxvkMetaClear::Key& key);
    
  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<DxvkMetaClear::Key, DxvkMetaClear, DxvkHash, DxvkEq> m_pipelines;

    VkExtent3D determineWorkgroupSize(const DxvkMetaClear::Key& key) const;

    std::vector<uint32_t> createShader(const DxvkMetaClear::Key& key, const DxvkPipelineLayout* layout);

    DxvkMetaClear createPipeline(const DxvkMetaClear::Key& key);

    static std::string getName(const DxvkMetaClear::Key& key);

  };
  
}
