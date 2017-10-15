#pragma once

#include <vector>

#include "dxvk_include.h"

#include "./spirv/dxvk_spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Resource access mode
   * 
   * Defines whether a resource will be
   * used for reading, writing, or both.
   */
  enum class DxvkResourceModeBit : uint32_t {
    Read  = 0,
    Write = 1,
  };
  
  using DxvkResourceMode = Flags<DxvkResourceModeBit>;
  
  /**
   * \brief Shader resource type
   * 
   * Enumerates the types of resources
   * that can be accessed by shaders.
   */
  enum class DxvkResourceType : uint32_t {
    UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    ImageSampler  = VK_DESCRIPTOR_TYPE_SAMPLER,
    SampledImage  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    StorageImage  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
  };
  
  
  /**
   * \brief Resource slot
   */
  struct DxvkResourceSlot{
    DxvkResourceMode mode;
    DxvkResourceType type;
    uint32_t         slot;
  };
  
  
  /**
   * \brief Shader module
   * 
   * Manages a Vulkan shader module. This will not
   * perform any sort of shader compilation. Instead,
   * the context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShader : public RcObject {
    
  public:
    
    DxvkShader(
            VkShaderStageFlagBits stage,
            DxvkSpirvCodeBuffer&& code,
            uint32_t              numResourceSlots,
      const DxvkResourceSlot*     resourceSlots);
    ~DxvkShader();
    
    /**
     * \brief Retrieves shader code
     * 
     * Since the exact binding IDs are not known by the
     * time the shader is created, we need to offset them
     * by the first binding index reserved for the shader
     * stage that this shader belongs to.
     * \param [in] bindingOffset First binding ID
     * \returns Modified code buffer
     */
    DxvkSpirvCodeBuffer code(
      uint32_t bindingOffset) const;
    
    /**
     * \brief Number of resource slots
     * \returns Number of enabled slots
     */
    uint32_t slotCount() const;
    
    /**
     * \brief Retrieves resource slot properties
     * 
     * Resource slots store which resources that are bound
     * to a DXVK context are used by the shader. The slot
     * ID corresponds to the binding index relative to the
     * first binding index within the shader.
     * \param [in] slotId Slot index
     * \returns The resource slot
     */
    DxvkResourceSlot slot(
      uint32_t slotId) const;
    
    /**
     * \brief Descriptor set layout binding
     * 
     * Creates Vulkan-compatible binding information for
     * a single resource slot. Each resource slot used
     * by the shader corresponds to one binding in Vulkan.
     * \param [in] slotId Shader binding slot ID
     * \param [in] bindingOffset Binding index offset
     * \returns Binding info
     */
    VkDescriptorSetLayoutBinding slotBinding(
      uint32_t slotId, uint32_t bindingOffset) const;
    
  private:
    
    VkShaderStageFlagBits m_stage;
    DxvkSpirvCodeBuffer   m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    
  };
  
}