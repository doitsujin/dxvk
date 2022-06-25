#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Descriptor set indices
   */
  struct DxvkDescriptorSets {
    static constexpr uint32_t FsViews     = 0;
    static constexpr uint32_t FsBuffers   = 1;
    static constexpr uint32_t VsAll       = 2;
    static constexpr uint32_t SetCount    = 3;
  };

  /**
   * \brief Binding info
   *
   * Stores metadata for a single binding in
   * a given shader, or for the whole pipeline.
   */
  struct DxvkBindingInfo {
    VkDescriptorType    descriptorType;   ///< Vulkan descriptor type
    uint32_t            resourceBinding;  ///< API binding slot for the resource
    VkImageViewType     viewType;         ///< Image view type
    VkShaderStageFlags  stages;           ///< Shader stage mask
    VkAccessFlags       access;           ///< Access mask for the resource

    /**
     * \brief Computes descriptor set index for the given binding
     *
     * This is determines based on the shader stages that use the binding.
     * \returns Descriptor set index
     */
    uint32_t computeSetIndex() const;

    /**
     * \brief Checks whether bindings can be merged
     *
     * Bindings can be merged if they access the same resource with
     * the same view and descriptor type and are part of the same
     * descriptor set.
     * \param [in] binding The binding to probe
     * \returns \c true if the bindings can be merged
     */
    bool canMerge(const DxvkBindingInfo& binding) const;

    /**
     * \brief Merges bindings
     *
     * Merges the stage and access flags of two
     * otherwise identical binding declarations.
     * \param [in] binding The binding to merge
     */
    void merge(const DxvkBindingInfo& binding);

    /**
     * \brief Checks for equality
     *
     * \param [in] other Binding to compare to
     * \returns \c true if both bindings are equal
     */
    bool eq(const DxvkBindingInfo& other) const;

    /**
     * \brief Hashes binding info
     * \returns Binding hash
     */
    size_t hash() const;

  };

  /**
   * \brief Binding list
   *
   * Linear structure that can be used to look
   * up descriptor set objects.
   */
  class DxvkBindingList {

  public:

    DxvkBindingList();
    ~DxvkBindingList();

    /**
     * \brief Number of Vulkan bindings
     * \returns Binding count
     */
    uint32_t getBindingCount() const {
      return uint32_t(m_bindings.size());
    }

    /**
     * \brief Retrieves binding info
     *
     * \param [in] idx Binding index
     * \returns Binding info
     */
    const DxvkBindingInfo& getBinding(uint32_t index) const {
      return m_bindings[index];
    }

    /**
     * \brief Adds a binding to the list
     * \param [in] binding Binding info
     */
    void addBinding(const DxvkBindingInfo& binding);

    /**
     * \brief Merges binding lists
     *
     * Adds bindings from another list to the current list. Useful
     * when creating descriptor set layouts for pipelines consisting
     * of multiple shader stages.
     * \param [in] layout Binding list to merge
     */
    void merge(const DxvkBindingList& list);

    /**
     * \brief Checks for equality
     *
     * \param [in] other Binding layout to compare to
     * \returns \c true if both binding layouts are equal
     */
    bool eq(const DxvkBindingList& other) const;

    /**
     * \brief Hashes binding layout
     * \returns Binding layout hash
     */
    size_t hash() const;

  private:

    std::vector<DxvkBindingInfo> m_bindings;

  };


  /**
   * \brief Binding set layout key entry
   *
   * Stores unique info for a single binding.
   */
  struct DxvkBindingSetLayoutKeyEntry {
    VkDescriptorType    descriptorType;
    VkShaderStageFlags  stages;
  };


  /**
   * \brief Binding set layout key
   *
   * Stores relevant information to look
   * up unique descriptor set layouts.
   */
  class DxvkBindingSetLayoutKey {

  public:

    DxvkBindingSetLayoutKey(const DxvkBindingList& list);
    ~DxvkBindingSetLayoutKey();

    /**
     * \brief Retrieves binding count
     * \returns Binding count
     */
    uint32_t getBindingCount() const {
      return uint32_t(m_bindings.size());
    }

    /**
     * \brief Retrieves binding info
     *
     * \param [in] index Binding index
     * \returns Binding info
     */
    DxvkBindingSetLayoutKeyEntry getBinding(uint32_t index) const {
      return m_bindings[index];
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Binding layout to compare to
     * \returns \c true if both binding layouts are equal
     */
    bool eq(const DxvkBindingSetLayoutKey& other) const;

    /**
     * \brief Hashes binding layout
     * \returns Binding layout hash
     */
    size_t hash() const;

  private:

    std::vector<DxvkBindingSetLayoutKeyEntry> m_bindings;

  };


  /**
   * \brief Binding list objects
   *
   * Manages a Vulkan descriptor set layout
   * object for a given binding list.
   */
  class DxvkBindingSetLayout {

  public:

    DxvkBindingSetLayout(
            DxvkDevice*           device,
      const DxvkBindingSetLayoutKey& key);

    ~DxvkBindingSetLayout();

    /**
     * \brief Queries descriptor set layout
     * \returns Descriptor set layout
     */
    VkDescriptorSetLayout getSetLayout() const {
      return m_layout;
    }

    /**
     * \brief Queries descriptor template
     * \returns Descriptor update template
     */
    VkDescriptorUpdateTemplate getSetUpdateTemplate() const {
      return m_template;
    }

  private:

    DxvkDevice*                   m_device;
    VkDescriptorSetLayout         m_layout    = VK_NULL_HANDLE;
    VkDescriptorUpdateTemplate    m_template  = VK_NULL_HANDLE;

  };


  /**
   * \brief Binding layout
   *
   * Convenience class to map out shader bindings for use in
   * descriptor set layouts and pipeline layouts. If possible,
   * bindings that only differ in stage will be merged.
   */
  class DxvkBindingLayout {

  public:

    DxvkBindingLayout();
    ~DxvkBindingLayout();

    /**
     * \brief Number of Vulkan bindings per set
     *
     * \param [in] set Descriptor set index
     * \returns Binding count for the given set
     */
    uint32_t getBindingCount(uint32_t set) const {
      return m_bindings[set].getBindingCount();
    }

    /**
     * \brief Retrieves binding info
     *
     * \param [in] set Descriptor set index
     * \param [in] idx Binding index
     * \returns Binding info
     */
    const DxvkBindingInfo& getBinding(uint32_t set, uint32_t idx) const {
      return m_bindings[set].getBinding(idx);
    }

    /**
     * \brief Retrieves binding list for a given set
     *
     * Use convenience methods above to gather info about
     * individual descriptors. This is intended to be used
     * for descriptor set lookup primarily.
     */
    const DxvkBindingList& getBindingList(uint32_t set) const {
      return m_bindings[set];
    }

    /**
     * \brief Retrieves push constant range
     * \returns Push constant range
     */
    VkPushConstantRange getPushConstantRange() const {
      return m_pushConst;
    }

    /**
     * \brief Adds a binding to the layout
     * \param [in] binding Binding info
     */
    void addBinding(const DxvkBindingInfo& binding);

    /**
     * \brief Adds push constant range
     * \param [in] range Push constant range
     */
    void addPushConstantRange(VkPushConstantRange range);

    /**
     * \brief Merges binding layouts
     *
     * Adds bindings and push constant range from another layout to
     * the current layout. Useful when creating pipeline layouts and
     * descriptor set layouts for pipelines consisting of multiple
     * shader stages.
     * \param [in] layout Binding layout to merge
     */
    void merge(const DxvkBindingLayout& layout);

    /**
     * \brief Checks for equality
     *
     * \param [in] other Binding layout to compare to
     * \returns \c true if both binding layouts are equal
     */
    bool eq(const DxvkBindingLayout& other) const;

    /**
     * \brief Hashes binding layout
     * \returns Binding layout hash
     */
    size_t hash() const;

  private:

    std::array<DxvkBindingList, DxvkDescriptorSets::SetCount> m_bindings;
    VkPushConstantRange                                       m_pushConst;

  };

  /**
   * \brief Descriptor set and binding number
   */
  struct DxvkBindingMapping {
    uint32_t set;
    uint32_t binding;
    uint32_t constId;
  };

  /**
   * \brief Pipeline and descriptor set layouts for a given binding layout
   *
   * Creates the following Vulkan objects for a given binding layout:
   * - A descriptor set layout for each required descriptor set
   * - A descriptor update template for each set with non-zero binding count
   * - A pipeline layout referencing all descriptor sets and the push constant ranges
   */
  class DxvkBindingLayoutObjects {

  public:

    DxvkBindingLayoutObjects(
            DxvkDevice*             device,
      const DxvkBindingLayout&      layout,
      const DxvkBindingSetLayout**  setObjects);

    ~DxvkBindingLayoutObjects();

    /**
     * \brief Binding layout
     * \returns Binding layout
     */
    const DxvkBindingLayout& layout() const {
      return m_layout;
    }

    /**
     * \brief Queries active descriptor set mask
     * \returns Bit mask of non-empty descriptor sets
     */
    uint32_t getSetMask() const {
      return m_setMask;
    }

    /**
     * \brief Queries first binding number for a given set
     *
     * This is relevant for generating binding masks.
     * \param [in] set Descriptor set index
     * \returns First binding in the given set
     */
    uint32_t getFirstBinding(uint32_t set) const {
      return m_bindingOffsets[set];
    }

    /**
     * \brief Retrieves descriptor set layout for a given set
     *
     * \param [in] set Descriptor set index
     * \returns Vulkan descriptor set layout
     */
    VkDescriptorSetLayout getSetLayout(uint32_t set) const {
      return m_bindingObjects[set]->getSetLayout();
    }

    /**
     * \brief Retrieves descriptor update template for a given set
     *
     * \param [in] set Descriptor set index
     * \returns Vulkan descriptor update template
     */
    VkDescriptorUpdateTemplate getSetUpdateTemplate(uint32_t set) const {
      return m_bindingObjects[set]->getSetUpdateTemplate();
    }

    /**
     * \brief Retrieves pipeline layout
     * \returns Pipeline layout
     */
    VkPipelineLayout getPipelineLayout() const {
      return m_pipelineLayout;
    }

    /**
     * \brief Looks up set and binding number by resource binding
     *
     * \param [in] index Resource binding index
     * \returns Descriptor set and binding number
     */
    std::optional<DxvkBindingMapping> lookupBinding(uint32_t index) const {
      auto entry = m_mapping.find(index);

      if (entry != m_mapping.end())
        return entry->second;

      return std::nullopt;
    }

    /**
     * \brief Queries accumulated resource access flags
     *
     * Can be used to determine whether the pipeline
     * reads or writes any resources.
     */
    VkAccessFlags getAccessFlags() const;

  private:

    DxvkDevice*         m_device;
    DxvkBindingLayout   m_layout;
    VkPipelineLayout    m_pipelineLayout  = VK_NULL_HANDLE;

    uint32_t            m_setMask         = 0;

    std::array<const DxvkBindingSetLayout*, DxvkDescriptorSets::SetCount> m_bindingObjects = { };
    std::array<uint32_t,                    DxvkDescriptorSets::SetCount> m_bindingOffsets = { };

    std::unordered_map<uint32_t, DxvkBindingMapping> m_mapping;

  };


  /**
   * \brief Dirty descriptor set state
   */
  class DxvkDescriptorState {

  public:

    void dirtyBuffers(VkShaderStageFlags stages) {
      m_dirtyBuffers  |= stages;
    }

    void dirtyViews(VkShaderStageFlags stages) {
      m_dirtyViews    |= stages;
    }

    void dirtyStages(VkShaderStageFlags stages) {
      m_dirtyBuffers  |= stages;
      m_dirtyViews    |= stages;
    }

    void clearStages(VkShaderStageFlags stages) {
      m_dirtyBuffers  &= ~stages;
      m_dirtyViews    &= ~stages;
    }

    bool hasDirtyGraphicsSets() const {
      return (m_dirtyBuffers | m_dirtyViews) & (VK_SHADER_STAGE_ALL_GRAPHICS);
    }

    bool hasDirtyComputeSets() const {
      return (m_dirtyBuffers | m_dirtyViews) & (VK_SHADER_STAGE_COMPUTE_BIT);
    }

    uint32_t getDirtyGraphicsSets() const {
      uint32_t result = 0;
      if (m_dirtyBuffers & VK_SHADER_STAGE_FRAGMENT_BIT)
        result |= (1u << DxvkDescriptorSets::FsBuffers);
      if (m_dirtyViews & VK_SHADER_STAGE_FRAGMENT_BIT)
        result |= (1u << DxvkDescriptorSets::FsViews) | (1u << DxvkDescriptorSets::FsBuffers);
      if ((m_dirtyBuffers | m_dirtyViews) & (VK_SHADER_STAGE_ALL_GRAPHICS & ~VK_SHADER_STAGE_FRAGMENT_BIT))
        result |= (1u << DxvkDescriptorSets::VsAll);
      return result;
    }

    uint32_t getDirtyComputeSets() const {
      uint32_t result = 0;
      if (m_dirtyBuffers & VK_SHADER_STAGE_COMPUTE_BIT)
        result |= (1u << DxvkDescriptorSets::FsBuffers);
      if (m_dirtyViews & VK_SHADER_STAGE_COMPUTE_BIT)
        result |= (1u << DxvkDescriptorSets::FsViews) | (1u << DxvkDescriptorSets::FsBuffers);
      return result;
    }

  private:

    VkShaderStageFlags m_dirtyBuffers   = 0;
    VkShaderStageFlags m_dirtyViews     = 0;

  };
  
}