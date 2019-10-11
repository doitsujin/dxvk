#pragma once

#include <vector>

#include "dxvk_include.h"

namespace dxvk {

  /**
   * \brief Resource slot
   * 
   * Describes the type of a single resource
   * binding that a shader can access.
   */
  struct DxvkResourceSlot {
    uint32_t           slot;
    VkDescriptorType   type;
    VkImageViewType    view;
    VkAccessFlags      access;
  };
  
  /**
   * \brief Shader interface binding
   * 
   * Corresponds to a single descriptor binding in
   * Vulkan. DXVK does not use descriptor arrays.
   * Instead, each binding stores one descriptor.
   */
  struct DxvkDescriptorSlot {
    uint32_t           slot;    ///< Resource slot index for the context
    VkDescriptorType   type;    ///< Descriptor type (aka resource type)
    VkImageViewType    view;    ///< Compatible image view type
    VkShaderStageFlags stages;  ///< Stages that can use the resource
    VkAccessFlags      access;  ///< Access flags
  };
  
  
  /**
   * \brief Descriptor slot mapping
   * 
   * Convenience class that generates descriptor slot
   * index to binding index mappings. This is required
   * when generating Vulkan pipeline and descriptor set
   * layouts.
   */
  class DxvkDescriptorSlotMapping {
    constexpr static uint32_t InvalidBinding = 0xFFFFFFFFu;
  public:
    
    DxvkDescriptorSlotMapping();
    ~DxvkDescriptorSlotMapping();
    
    /**
     * \brief Number of descriptor bindings
     * \returns Descriptor binding count
     */
    uint32_t bindingCount() const {
      return m_descriptorSlots.size();
    }
    
    /**
     * \brief Descriptor binding infos
     * \returns Descriptor binding infos
     */
    const DxvkDescriptorSlot* bindingInfos() const {
      return m_descriptorSlots.data();
    }

    /**
     * \brief Push constant range
     * \returns Push constant range
     */
    VkPushConstantRange pushConstRange() const {
      return m_pushConstRange;
    }
    
    /**
     * \brief Defines a new slot
     * 
     * Adds a slot to the mapping. If the slot is already
     * defined by another shader stage, this will extend
     * the stage mask by the given stage. Otherwise, an
     * entirely new binding is added.
     * \param [in] stage Shader stage
     * \param [in] desc Slot description
     */
    void defineSlot(
            VkShaderStageFlagBits stage,
      const DxvkResourceSlot&     desc);

    /**
     * \brief Defines new push constant range
     *
     * \param [in] stage Shader stage
     * \param [in] offset Range offset
     * \param [in] size Range size
     */
    void definePushConstRange(
            VkShaderStageFlagBits stage,
            uint32_t              offset,
            uint32_t              size);
    
    /**
     * \brief Gets binding ID for a slot
     * 
     * \param [in] slot Resource slot
     * \returns Binding index, or \c InvalidBinding
     */
    uint32_t getBindingId(
            uint32_t              slot) const;
    
    /**
     * \brief Makes static descriptors dynamic
     * 
     * Replaces static uniform and storage buffer bindings by
     * their dynamic equivalent if the number of bindings of
     * the respective type lies within supported device limits.
     * Using dynamic descriptor types may improve performance.
     * \param [in] uniformBuffers Max number of uniform buffers
     * \param [in] storageBuffers Max number of storage buffers
     */
    void makeDescriptorsDynamic(
            uint32_t              uniformBuffers,
            uint32_t              storageBuffers);
    
  private:
    
    std::vector<DxvkDescriptorSlot> m_descriptorSlots;
    VkPushConstantRange             m_pushConstRange = { };

    uint32_t countDescriptors(
            VkDescriptorType      type) const;

    void replaceDescriptors(
            VkDescriptorType      oldType,
            VkDescriptorType      newType);
    
  };
  
  
  /**
   * \brief Shader interface
   * 
   * Describes shader resource bindings
   * for a graphics or compute pipeline.
   */
  class DxvkPipelineLayout : public RcObject {
    
  public:
    
    DxvkPipelineLayout(
      const Rc<vk::DeviceFn>&   vkd,
      const DxvkDescriptorSlotMapping& slotMapping,
            VkPipelineBindPoint pipelineBindPoint);
    
    ~DxvkPipelineLayout();
    
    /**
     * \brief Number of resource bindings
     * \returns Resource binding count
     */
    uint32_t bindingCount() const {
      return m_bindingSlots.size();
    }
    
    /**
     * \brief Resource binding info
     * 
     * \param [in] id Binding index
     * \returns Resource binding info
     */
    const DxvkDescriptorSlot& binding(uint32_t id) const {
      return m_bindingSlots[id];
    }
    
    /**
     * \brief Resource binding info
     * \returns Resource binding info
     */
    const DxvkDescriptorSlot* bindings() const {
      return m_bindingSlots.data();
    }
    
    /**
     * \brief Push constant range
     * \returns Push constant range
     */
    const VkPushConstantRange& pushConstRange() const {
      return m_pushConstRange;
    }
    
    /**
     * \brief Descriptor set layout handle
     * \returns Descriptor set layout handle
     */
    VkDescriptorSetLayout descriptorSetLayout() const {
      return m_descriptorSetLayout;
    }
    
    /**
     * \brief Pipeline layout handle
     * \returns Pipeline layout handle
     */
    VkPipelineLayout pipelineLayout() const {
      return m_pipelineLayout;
    }
    
    /**
     * \brief Descriptor update template
     * \returns Descriptor update template
     */
    VkDescriptorUpdateTemplateKHR descriptorTemplate() const {
      return m_descriptorTemplate;
    }

    /**
     * \brief Number of dynamic bindings
     * \returns Dynamic binding count
     */
    uint32_t dynamicBindingCount() const {
      return m_dynamicSlots.size();
    }

    /**
     * \brief Returns a dynamic binding
     * 
     * \param [in] id Dynamic binding ID
     * \returns Reference to that binding
     */
    const DxvkDescriptorSlot& dynamicBinding(uint32_t id) const {
      return this->binding(m_dynamicSlots[id]);
    }
    
    /**
     * \brief Checks for static buffer bindings
     * 
     * Returns \c true if there is at least one
     * descriptor of the static uniform buffer
     * type.
     */
    bool hasStaticBufferBindings() const {
      return m_descriptorTypes.test(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
    
    /**
     * \brief Checks whether buffers or images are written to
     * 
     * It is assumed that storage images and buffers
     * will be written to if they are present. Used
     * for synchronization purposes.
     * \param [in] stages Shader stages to check
     */
    VkShaderStageFlags getStorageDescriptorStages() const {
      VkShaderStageFlags stages = 0;

      for (const auto& slot : m_bindingSlots) {
        if (slot.access & VK_ACCESS_SHADER_WRITE_BIT)
          stages |= slot.stages;
      }
      
      return stages;
    }

  private:
    
    Rc<vk::DeviceFn> m_vkd;
    
    VkPushConstantRange             m_pushConstRange      = { };
    VkDescriptorSetLayout           m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout                m_pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorUpdateTemplateKHR   m_descriptorTemplate  = VK_NULL_HANDLE;
    
    std::vector<DxvkDescriptorSlot> m_bindingSlots;
    std::vector<uint32_t>           m_dynamicSlots;

    Flags<VkDescriptorType>         m_descriptorTypes;
    
  };
  
}