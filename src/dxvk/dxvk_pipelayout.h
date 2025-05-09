#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkPipelineManager;

  /**
   * \brief Order-invariant atomic access operation
   *
   * Information used to optimize barriers when a resource
   * is accessed exlusively via order-invariant stores.
   */
  struct DxvkAccessOp {
    enum OpType : uint16_t {
      None      = 0x0u,
      Or        = 0x1u,
      And       = 0x2u,
      Xor       = 0x3u,
      Add       = 0x4u,
      IMin      = 0x5u,
      IMax      = 0x6u,
      UMin      = 0x7u,
      UMax      = 0x8u,

      StoreF    = 0xdu,
      StoreUi   = 0xeu,
      StoreSi   = 0xfu,
    };

    DxvkAccessOp() = default;
    DxvkAccessOp(OpType t)
    : op(uint16_t(t)) { }

    DxvkAccessOp(OpType t, uint16_t constant)
    : op(uint16_t(t) | (constant << 4u)) { }

    uint16_t op = 0u;

    bool operator == (const DxvkAccessOp& t) const { return op == t.op; }
    bool operator != (const DxvkAccessOp& t) const { return op != t.op; }

    template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    explicit operator T() const { return op; }
  };

  static_assert(sizeof(DxvkAccessOp) == sizeof(uint16_t));


  /**
   * \brief Descriptor set indices
   */
  struct DxvkDescriptorSets {
    static constexpr uint32_t FsViews     = 0;
    static constexpr uint32_t FsBuffers   = 1;
    static constexpr uint32_t VsAll       = 2;
    static constexpr uint32_t SetCount    = 3;

    static constexpr uint32_t CsAll       = 0;
    static constexpr uint32_t CsSetCount  = 1;
  };

  /**
   * \brief Binding info
   *
   * Stores metadata for a single binding in
   * a given shader, or for the whole pipeline.
   */
  struct DxvkBindingInfo {
    VkDescriptorType      descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;        ///< Vulkan descriptor type
    uint32_t              resourceBinding = 0u;                                 ///< API binding slot for the resource
    VkImageViewType       viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;        ///< Image view type
    VkShaderStageFlagBits stage           = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM; ///< Shader stage
    VkAccessFlags         access          = 0u;                                 ///< Access mask for the resource
    DxvkAccessOp          accessOp        = DxvkAccessOp::None;                 ///< Order-invariant store type, if any
    bool                  uboSet          = false;                              ///< Whether to include this in the UBO set
    bool                  isMultisampled  = false;                              ///< Multisampled binding

    /**
     * \brief Computes descriptor set index for the given binding
     *
     * This is determines based on the shader stages that use the binding.
     * \returns Descriptor set index
     */
    uint32_t computeSetIndex() const;

    /**
     * \brief Numeric value of the binding
     *
     * Used when sorting bindings.
     * \returns Numeric value
     */
    uint32_t value() const;

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
   * \brief Descriptor flags
   */
  enum class DxvkDescriptorFlag : uint8_t {
    UniformBuffer   = 0u, ///< Resource is a uniform buffer, not a view
    Multisampled    = 1u, ///< Image view must be multisampled
  };

  using DxvkDescriptorFlags = Flags<DxvkDescriptorFlag>;


  /**
   * \brief Descriptor metadata
   *
   * Stores all the information required to map a bound resource
   * to an actual shader descriptor. Note that this refers to one
   * single descriptor rather than a descriptor array.
   */
  class DxvkShaderDescriptor {

  public:

    DxvkShaderDescriptor() = default;

    DxvkShaderDescriptor(const DxvkBindingInfo& binding)
    : m_type      (uint8_t(binding.descriptorType)),
      m_stages    (uint8_t(binding.stage)),
      m_index     (uint16_t(binding.resourceBinding)),
      m_viewType  (uint8_t(binding.viewType)),
      m_access    (uint8_t(binding.access)),
      m_accessOp  (binding.accessOp),
      m_set       (0u),
      m_binding   (binding.resourceBinding),
      m_arrayIndex(0u),
      m_arraySize (1u) {
      if (binding.uboSet)
        m_flags.set(DxvkDescriptorFlag::UniformBuffer);
      if (binding.isMultisampled)
        m_flags.set(DxvkDescriptorFlag::Multisampled);
    }

    /**
     * \brief Queries descriptor type
     * \returns Descriptor type
     */
    VkDescriptorType getDescriptorType() const {
      return VkDescriptorType(m_type);
    }

    /**
     * \brief Queries descriptor count in array
     * \returns Number of descriptors in descriptor array
     */
    uint32_t getDescriptorCount() const {
      return m_arraySize;
    }

    /**
     * \brief Queries shader stage mask
     * \returns Shader stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return VkShaderStageFlags(m_stages);
    }

    /**
     * \brief Queries resource index
     * \returns Resource index
     */
    uint32_t getResourceIndex() const {
      return m_index;
    }

    /**
     * \brief Queries view type
     * \returns View type
     */
    VkImageViewType getViewType() const {
      return m_viewType < 0 ? VK_IMAGE_VIEW_TYPE_MAX_ENUM : VkImageViewType(m_viewType);
    }

    /**
     * \brief Checks whether a multisampled view is required
     * \returns \c true if a multisampled view is needed
     */
    bool isMultisampled() const {
      return m_flags.test(DxvkDescriptorFlag::Multisampled);
    }

    /**
     * \brief Checks whether the resource is a raw buffer
     *
     * Changes whether the resource is sourced as a raw buffer or from
     * a buffer view object. Relevant for storage buffer descriptors
     * that are used as a uniform buffer.
     * \returns \c true for raw buffer descriptors
     */
    bool isUniformBuffer() const {
      return m_flags.test(DxvkDescriptorFlag::UniformBuffer);
    }

    /**
     * \brief Queries shader access types
     * \returns Access types
     */
    VkAccessFlags getAccess() const {
      return VkAccessFlags(m_access);
    }

    /**
     * \brief Queries access op
     * \returns Access op
     */
    DxvkAccessOp getAccessOp() const {
      return m_accessOp;
    }

    /**
     * \brief Queries descriptor set index
     * \returns Descriptor set index
     */
    uint32_t getSet() const {
      return m_set;
    }

    /**
     * \brief Queries descriptor binding index
     * \returns Descriptor binding index
     */
    uint32_t getBinding() const {
      return m_binding;
    }

    /**
     * \brief Queries descriptor index in array
     * \returns Index into descriptor array
     */
    uint32_t getArrayIndex() const {
      return m_arrayIndex;
    }

    /**
     * \brief Retrives info for a specific array element
     *
     * \param [in] index Array index to extract
     * \returns Descriptor info with adjusted index
     */
    DxvkShaderDescriptor getArrayElement(uint32_t index) const {
      DxvkShaderDescriptor result = *this;
      result.m_index += index;
      result.m_arrayIndex = index;
      result.m_arraySize = 1u;
      return result;
    }

    /**
     * \brief Changes set and binding indices
     *
     * \param [in] set New set index
     * \param [in] binding New binding index
     * \returns Adjusted descriptor info
     */
    DxvkShaderDescriptor remapBinding(uint32_t set, uint32_t binding) const {
      DxvkShaderDescriptor result = *this;
      result.m_set = set;
      result.m_binding = binding;
      return result;
    }

    /**
     * \brief Checks whether a binding is ordered before another
     *
     * Bindings are ordered by stage, descriptor type and array
     * properties in various places in order to keep the number
     * of unique descriptor set and pipeline layouts low.
     * \param [in] other Binding to compare against
     * \returns \c true if this binding is ordered before the other.
     */
    bool lt(const DxvkShaderDescriptor& descriptor) const {
      return encodeNumeric() < descriptor.encodeNumeric();
    }

  private:

    uint8_t             m_type              = 0u;
    uint8_t             m_stages            = 0u;
    uint16_t            m_index             = 0u;
    int8_t              m_viewType          = 0u;
    uint8_t             m_access            = 0u;
    DxvkAccessOp        m_accessOp          = DxvkAccessOp::None;
    DxvkDescriptorFlags m_flags             = 0u;
    uint8_t             m_set               = 0u;
    uint16_t            m_binding           = 0u;
    uint16_t            m_arrayIndex        = 0u;
    uint16_t            m_arraySize         = 0u;

    uint64_t encodeNumeric() const {
      return uint64_t(m_arrayIndex)
          | (uint64_t(m_binding) << 16u)
          | (uint64_t(m_set)     << 32u)
          | (uint64_t(m_type)    << 40u)
          | (uint64_t(m_stages)  << 48u);
    }

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
   * \brief Descriptor set binding info
   *
   * Stores unique info for a single binding. The
   * binding index is implied by its position in
   * the binding list.
   */
  struct DxvkDescriptorSetLayoutBinding {

  public:

    DxvkDescriptorSetLayoutBinding() = default;

    DxvkDescriptorSetLayoutBinding(
            VkDescriptorType        type,
            uint32_t                count,
            VkShaderStageFlags      stages)
    : m_type    (uint8_t(type)),
      m_stages  (uint8_t(stages)),
      m_count   (uint16_t(count)) { }

    DxvkDescriptorSetLayoutBinding(
      const DxvkShaderDescriptor&   descriptor)
    : DxvkDescriptorSetLayoutBinding(
        descriptor.getDescriptorType(),
        descriptor.getDescriptorCount(),
        descriptor.getStageMask()) { }

    /**
     * \brief Queries descriptor type
     * \returns Vulkan descriptor type
     */
    VkDescriptorType getDescriptorType() const {
      return VkDescriptorType(m_type);
    }

    /**
     * \brief Queries descriptor count
     * \returns Size of descriptor array
     */
    uint32_t getDescriptorCount() const {
      return m_count;
    }

    /**
     * \brief Queries stage mask
     * \returns Pipeline stages that can access the descriptor
     */
    VkShaderStageFlags getStageMask() const {
      return VkShaderStageFlags(m_stages);
    }

    /**
     * \brief Checks equality
     *
     * \param [in] other Other binding to check
     * \returns \c true if the two bindings match
     */
    bool eq(const DxvkDescriptorSetLayoutBinding& other) const {
      return m_type   == other.m_type
          && m_stages == other.m_stages
          && m_count  == other.m_count;
    }

    /**
     * \brief Computes hash
     * \returns Binding hash
     */
    size_t hash() const {
      return size_t(m_type)
          | (size_t(m_stages) << 8u)
          | (size_t(m_count) << 16u);
    }

  private:

    uint8_t   m_type    = 0u;
    uint8_t   m_stages  = 0u;
    uint16_t  m_count   = 0u;

  };


  /**
   * \brief Descriptor set layout key
   *
   * Stores relevant information to look
   * up unique descriptor set layouts.
   */
  class DxvkDescriptorSetLayoutKey {

  public:

    DxvkDescriptorSetLayoutKey();

    DxvkDescriptorSetLayoutKey(const DxvkBindingList& list);
    ~DxvkDescriptorSetLayoutKey();

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
    DxvkDescriptorSetLayoutBinding getBinding(uint32_t index) const {
      return m_bindings[index];
    }

    /**
     * \brief Adds a binding to the key
     *
     * Useful to construct set layouts on the fly.
     * \param [in] binding Binding info
     */
    void add(DxvkDescriptorSetLayoutBinding binding);

    /**
     * \brief Checks for equality
     *
     * \param [in] other Binding layout to compare to
     * \returns \c true if both binding layouts are equal
     */
    bool eq(const DxvkDescriptorSetLayoutKey& other) const;

    /**
     * \brief Hashes binding layout
     * \returns Binding layout hash
     */
    size_t hash() const;

  private:

    small_vector<DxvkDescriptorSetLayoutBinding, 32> m_bindings;

  };


  /**
   * \brief Descriptor set layout
   *
   * Manages a Vulkan descriptor set layout
   * object for a given binding list.
   */
  class DxvkDescriptorSetLayout {

  public:

    DxvkDescriptorSetLayout(
            DxvkDevice*                 device,
      const DxvkDescriptorSetLayoutKey& key);

    ~DxvkDescriptorSetLayout();

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
   * \brief Push constant info
   */
  class DxvkPushConstantRange {

  public:

    DxvkPushConstantRange() = default;

    DxvkPushConstantRange(
            VkShaderStageFlags  stages,
            uint32_t            size)
    : m_size  (uint8_t(stages ? (size / sizeof(uint32_t)) : 0u)),
      m_stages(uint8_t(size ? stages : 0u)) { }

    /**
     * \brief Queries shader stage mask
     * \returns Shader stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return VkShaderStageFlags(m_stages);
    }

    /**
     * \brief Queries push constant block size
     * \returns Push constant size, in bytes
     */
    uint32_t getSize() const {
      return uint32_t(m_size) * sizeof(uint32_t);
    }

    /**
     * \brief Converts push constant range to Vulkan struct
     * \returns Vulkan push constant range
     */
    VkPushConstantRange getPushConstantRange() const {
      VkPushConstantRange vk = { };
      vk.size       = getSize();
      vk.stageFlags = getStageMask();
      return vk;
    }

    /**
     * \brief Merges two push constant ranges
     *
     * Takes the shader stage and size from the other rage.
     * \param [in] other Other push constant range
     */
    void merge(const DxvkPushConstantRange& other) {
      m_stages |= other.m_stages;
      m_size = std::max(m_size, other.m_size);
    }

    /**
     * \brief Applies subset of stages
     *
     * Removes any stages not included in the given
     * set of stages, and adjusts the size if needed.
     * \param [in] stages Shader stage mask
     */
    void maskStages(VkShaderStageFlags stages) {
      m_stages &= uint8_t(stages);

      if (!m_stages)
        m_size = 0u;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Push constant range to compare against
     * \returns \c true if the two ranges are equal
     */
    bool eq(const DxvkPushConstantRange& other) const {
      return m_size   == other.m_size
          && m_stages == other.m_stages;
    }

    /**
     * \brief Computes hash
     * \returns Hash
     */
    size_t hash() const {
      return size_t(m_size)
          | (size_t(m_stages) << 8u);
    }

  private:

    uint8_t m_size    = 0u;
    uint8_t m_stages  = 0u;

  };


  /**
   * \brief Pipeline layout key
   *
   * Used to look up pipeline layout objects. Stores a reference to the
   * descriptor sets used in this layout, as well as push constant info.
   * Uses the fact that descriptor set layout objects are persistent and
   * unique, and thus pipeline layouts using different set layouts are
   * unique as well.
   */
  class DxvkPipelineLayoutKey {

  public:

    constexpr static uint32_t MaxSets = uint32_t(DxvkDescriptorSets::SetCount);

    DxvkPipelineLayoutKey() = default;

    DxvkPipelineLayoutKey(
            VkShaderStageFlags        stageMask,
            DxvkPushConstantRange     pushConstants,
            uint32_t                  setCount,
      const DxvkDescriptorSetLayout** setLayouts)
    : m_stages        (uint8_t(stageMask)),
      m_setCount      (uint8_t(setCount)),
      m_pushConstants (pushConstants) {
      for (uint32_t i = 0; i < setCount; i++)
        m_sets[i] = setLayouts[i];
    }

    /**
     * \brief Adds a set of shader stages
     * \param [in] stageMask Shader stages to add
     */
    void addStages(VkShaderStageFlags stageMask) {
      m_stages |= uint8_t(stageMask);
    }

    /**
     * \brief Adds a push constant range
     *
     * Merges stage mask and uses the largest of the available
     * sizes so that all push constants are included in one range.
     * \param [in] range Push constant range to add
     */
    void addPushConstantRange(DxvkPushConstantRange range) {
      m_pushConstants.merge(range);
    }

    /**
     * \brief Sets a descriptor set layouts
     *
     * This always assigns all sets in one go. Sets may be \c nullptr
     * when creating pipeline layouts for pipeline libraries. It is
     * valid to create pipeline layouts with no descriptor sets.
     * \param [in] setCount Number of sets to assign
     * \param [in] setLayouts Descriptor set layout objects
     */
    void setDescriptorSetLayouts(
            uint32_t                  setCount,
      const DxvkDescriptorSetLayout** setLayouts) {
      m_setCount = uint8_t(setCount);

      for (uint32_t i = 0; i < MaxSets; i++)
        m_sets[i] = i < setCount ? setLayouts[i] : nullptr;
    }

    /**
     * \brief Queries shader stage mask
     * \returns All stages covered by this layout
     */
    VkShaderStageFlags getStageMask() const {
      return m_stages;
    }

    /**
     * \brief Queries push constant range
     *
     * \param [in] independent Whether to include stages not included
     *    in the pipeline layout itself, which is necessary to create
     *    valid pipeline layouts for pipeline libraries.
     * \returns Push constant range
     */
    DxvkPushConstantRange getPushConstantRange(bool independent) const {
      DxvkPushConstantRange result = m_pushConstants;

      if (!independent)
        result.maskStages(getStageMask());

      return result;
    }

    /**
     * \brief Queries descriptor set count
     * \returns Number of descriptor sets
     */
    uint32_t getDescriptorSetCount() const {
      return m_setCount;
    }

    /**
     * \brief Computes set mask
     *
     * Sets a bit for each included set that is not \c nullptr.
     * \returns Descriptor set mask
     */
    uint32_t computeDescriptorSetMask() const {
      uint32_t mask = 0u;

      for (uint32_t i = 0u; i < MaxSets; i++)
        mask |= (m_sets[i] ? 1u : 0u) << i;

      return mask;
    }

    /**
     * \brief Queries descriptor set layout
     *
     * \param [in] set Descriptor set index
     * \returns Descriptor set layout object
     */
    const DxvkDescriptorSetLayout* getDescriptorSetLayout(uint32_t setIndex) const {
      return m_sets[setIndex];
    }

    /**
     * \brief Checks whether all sets are defined
     *
     * If all set layouts are valid, a non-independent
     * pipeline layout can be created.
     * \returns \c true if all included sets are not \c nullptr.
     */
    bool isComplete() const {
      bool result = true;

      for (uint32_t i = 0; i < m_setCount; i++)
        result &= m_sets[i] != nullptr;

      return result;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Pipeline layout key to compare to
     * \returns \c true if both layout keys are equal
     */
    bool eq(const DxvkPipelineLayoutKey& other) const {
      bool eq = m_stages    == other.m_stages
             && m_setCount  == other.m_setCount;

      eq &= m_pushConstants.eq(other.m_pushConstants);

      for (uint32_t i = 0; i < m_setCount && eq; i++)
        eq = m_sets[i] == other.m_sets[i];

      return eq;
    }

    /**
     * \brief Computes hash
     * \returns Hash
     */
    size_t hash() const {
      DxvkHashState hash;
      hash.add(m_stages);
      hash.add(m_setCount);
      hash.add(m_pushConstants.hash());

      for (uint32_t i = 0; i < m_setCount; i++)
        hash.add(reinterpret_cast<uintptr_t>(m_sets[i]));

      return hash;
    }

  private:

    uint8_t               m_stages    = 0u;
    uint8_t               m_setCount  = 0u;
    DxvkPushConstantRange m_pushConstants = { };

    std::array<const DxvkDescriptorSetLayout*, MaxSets> m_sets = { };

  };


  /**
   * \brief Pipeline layout
   *
   * Manages Vulkan pipeline layout objects for use with pipeline
   * libraries as well as full pipelines where appropriate.
   */
  class DxvkPipelineLayout {

  public:

    DxvkPipelineLayout(
            DxvkDevice*                 device,
      const DxvkPipelineLayoutKey&      key);

    ~DxvkPipelineLayout();

    /**
     * \brief Queries Vulkan pipeline layout
     *
     * \param [in] independent Whether to return a pipeline
     *    layout that can be used with pipeline libraries.
     * \returns Pipeline layout handle
     */
    VkPipelineLayout getPipelineLayout(bool independent) const {
      return independent
        ? m_layoutIndependent
        : m_layoutComplete;
    }

    /**
     * \brief Queries specific descriptor set layout
     *
     * \param [in] set Set index
     * \returns Set layout
     */
    const DxvkDescriptorSetLayout* getDescriptorSetLayout(uint32_t set) const {
      return m_setLayouts[set];
    }

  private:

    DxvkDevice*       m_device;
    VkPipelineLayout  m_layoutIndependent = VK_NULL_HANDLE;
    VkPipelineLayout  m_layoutComplete    = VK_NULL_HANDLE;

    std::array<const DxvkDescriptorSetLayout*, DxvkPipelineLayoutKey::MaxSets> m_setLayouts = { };

  };


  /**
   * \brief Shader resource binding info
   *
   * Stores the set and binding index for a given binding that is
   * used in a shader. Used to patch binding numbers as necessary.
   */
  class DxvkShaderBinding {

  public:

    DxvkShaderBinding() = default;

    DxvkShaderBinding(
            VkShaderStageFlags    stages,
            uint32_t              set,
            uint32_t              binding)
    : m_stages  (uint8_t(stages)),
      m_set     (uint8_t(set)),
      m_binding (uint16_t(binding)) { }

    /**
     * \brief Queries set index
     * \returns Set index
     */
    uint32_t getSet() const {
      return m_set;
    }

    /**
     * \brief Queries binding index
     * \returns Binding index
     */
    uint32_t getBinding() const {
      return m_binding;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Other binding entry
     * \returns \c true if all properties match
     */
    bool eq(const DxvkShaderBinding& other) const {
      return m_stages   == other.m_stages
          && m_set      == other.m_set
          && m_binding  == other.m_binding;
    }

    /**
     * \brief Computes hash
     * \returns Hash
     */
    size_t hash() const {
      return size_t(m_stages)
          | (size_t(m_set) << 8)
          | (size_t(m_binding) << 16);
    }

  private:

    uint8_t   m_stages   = 0u;
    uint8_t   m_set      = 0u;
    uint16_t  m_binding  = 0u;

  };


  /**
   * \brief Binding map
   *
   * Used to assign correct descriptor bindings to shaders.
   */
  class DxvkShaderBindingMap {

  public:

    DxvkShaderBindingMap();

    ~DxvkShaderBindingMap();

    /**
     * \brief Adds entry
     *
     * \param [in] srcBinding Shader binding
     * \param [in] dstBinding Actual descriptor set binding
     */
    void add(DxvkShaderBinding srcBinding, DxvkShaderBinding dstBinding);

    /**
     * \brief Looks up shader binding
     *
     * \param [in] srcBinding Shader-defined binding
     * \returns Pointer to remapped binding, or \c nullptr
     */
    const DxvkShaderBinding* find(DxvkShaderBinding srcBinding) const;

  private:

    std::unordered_map<DxvkShaderBinding, DxvkShaderBinding, DxvkHash, DxvkEq> m_entries;

  };


  /**
   * \brief Binding range
   *
   * Stores number of bindings in a list, as well as the pointer to
   * the first binding. Set and descriptor numbers are discarded
   * here and must be inferred as necessary.
   */
  struct DxvkPipelineBindingRange {
    size_t                      bindingCount  = 0u;
    const DxvkShaderDescriptor* bindings      = nullptr;
  };


  /**
   * \brief Pipeline layout builder
   *
   * Accumulates bindings and push constant ranges and provides
   * functionality to process them into a structure that can be
   * used to create compatible pipeline layouts and metadata.
   */
  class DxvkPipelineLayoutBuilder {

  public:

    DxvkPipelineLayoutBuilder();

    /**
     * \brief Initializes builder
     *
     * \param [in] stageMask Known shader stages. When building a pipeline layout
     *    for full graphics pipelines and not just libraries, this \e must include
     *    the fragment shader stage even if no fragment shader is used.
     */
    DxvkPipelineLayoutBuilder(VkShaderStageFlags stageMask);

    ~DxvkPipelineLayoutBuilder();

    /**
     * \brief Queries shader stage mask
     * \returns Shader stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return m_stageMask;
    }

    /**
     * \brief Queries push constant range
     * \returns Merged push constant range
     */
    DxvkPushConstantRange getPushConstantRange() const {
      return m_pushConstants;
    }

    /**
     * \brief Queries descriptor bindings
     * \returns Descriptor bindings
     */
    DxvkPipelineBindingRange getBindings() const {
      DxvkPipelineBindingRange range;
      range.bindingCount = m_bindings.size();
      range.bindings = m_bindings.data();
      return range;
    }

    /**
     * \brief Adds push constant range
     * \param [in] range Push constant range
     */
    void addPushConstants(
            DxvkPushConstantRange     range);

    /**
     * \brief Adds bindings
     *
     * Ensures that bindings are properly ordered.
     * \param [in] bindingCount Number of bindings
     * \param [in] bindings Binding infos with regular
     *    shader-defined set and binding indices
     */
    void addBindings(
            uint32_t                  bindingCount,
      const DxvkShaderDescriptor*     bindings);

    /**
     * \brief Merges another layout
     *
     * Adds push constants and bindings from the given
     * layout into the calling object.
     * \param [in] layout The layout to add to this one
     */
    void addLayout(
      const DxvkPipelineLayoutBuilder& layout);

  private:

    VkShaderStageFlags    m_stageMask     = 0u;
    DxvkPushConstantRange m_pushConstants = { };

    std::vector<DxvkShaderDescriptor> m_bindings;

  };


  /**
   * \brief Global resource barrier
   *
   * Stores the way any resources will be
   * accessed by this pipeline.
   */
  struct DxvkGlobalPipelineBarrier {
    VkPipelineStageFlags  stages = 0u;
    VkAccessFlags         access = 0u;
  };


  /**
   * \brief Pipeline layout metadata
   *
   * Stores all sorts of information on shader bindings used in
   * a pipeline, including binding lists to iterate over for
   * various different purposes.
   */
  class DxvkPipelineBindings {
    constexpr static uint32_t MaxSets = DxvkPipelineLayoutKey::MaxSets;

    using BindingList = small_vector<DxvkShaderDescriptor, 32>;
  public:

    DxvkPipelineBindings(
            DxvkPipelineManager*        manager,
      const DxvkPipelineLayoutBuilder&  builder);

    ~DxvkPipelineBindings();

    /**
     * \brief Queries pipeline layout
     * \returns Pipeline layout
     */
    const DxvkPipelineLayout* getLayout() const {
      return m_layout;
    }

    /**
     * \brief Queries mask of non-empty descriptor sets
     * \returns Mask of non-empty sets
     */
    uint32_t getSetMask() const {
      return m_setMask;
    }

    /**
     * \brief Queries push constant range
     */
    DxvkPushConstantRange getPushConstantRange(bool independent) const {
      return independent
        ? m_pushConstantsIndependent
        : m_pushConstantsComplete;
    }

    /**
     * \brief Queries all available bindings in a given set
     *
     * Primarily useful to update dirty descriptor sets.
     * \param [in] set Set index
     * \returns List of all bindings in the set
     */
    DxvkPipelineBindingRange getAllDescriptorsInSet(uint32_t set) const {
      return makeBindingRange(m_setDescriptors[set]);
    }

    /**
     * \brief Queries all sampler bindings in a given set
     *
     * This includes both pure samplers as well as
     * combined image and sampler bindings.
     * \param [in] set Set index
     * \returns List of all sampler bindings in the set
     */
    DxvkPipelineBindingRange getSamplersInSet(uint32_t set) const {
      return makeBindingRange(m_setSamplers[set]);
    }

    /**
     * \brief Queries all shader resources in a given set
     *
     * Includes all resources that are bound via an image or buffer view,
     * even unformatted buffer views (i.e. storage buffers). Does not
     * include pure samplers or buffers that are only bound via a buffer
     * range, i.e. uniform buffers.
     * \param [in] set Set index
     * \returns List of all resource bindings in the set
     */
    DxvkPipelineBindingRange getResourcesInSet(uint32_t set) const {
      return makeBindingRange(m_setResources[set]);
    }

    /**
     * \brief Queries all uniform buffers in a given set
     *
     * Includes all uniform buffer resources, i.e. resources that are only
     * bound via a buffer range and do not use views, and use the uniform
     * or storage buffer descriptor type.
     * \param [in] set Set index
     * \returns List of all uniform buffer bindings in the set
     */
    DxvkPipelineBindingRange getUniformBuffersInSet(uint32_t set) const {
      return makeBindingRange(m_setUniformBuffers[set]);
    }

    /**
     * \brief Queries all read-only resources in a given set
     *
     * Subset of \c getResourcesInSet that only includes bindings that are
     * read and not written by the shader. Useful for hazard tracking.
     * \returns List of all read-only resources in the set
     */
    DxvkPipelineBindingRange getReadOnlyResourcesInSet(uint32_t set) const {
      return makeBindingRange(m_setReadOnlyResources[set]);
    }

    /**
     * \brief Queries all writable resources in the layout
     *
     * Not tied to any particular set, this includes resources that
     * are written by the shader. Useful for hazard tracking.
     * \returns List of all written resources in the layout
     */
    DxvkPipelineBindingRange getReadWriteResources() const {
      return makeBindingRange(m_readWriteResources);
    }

    /**
     * \brief Queries global resource barrier
     *
     * Can be used to determine whether the pipeline
     * reads or writes any resources, and which shader
     * stages it uses.
     * \returns Global barrier
     */
    DxvkGlobalPipelineBarrier getGlobalBarrier() const;

    /**
     * \brief Queries binding map
     *
     * The binding map is primarily useful for shader patching.
     * \returns Pointer to binding map
     */
    const DxvkShaderBindingMap* getBindingMap() const {
      return &m_map;
    }

  private:

    VkShaderStageFlags        m_shaderStageMask           = 0u;

    DxvkPushConstantRange     m_pushConstantsComplete     = { };
    DxvkPushConstantRange     m_pushConstantsIndependent  = { };

    DxvkGlobalPipelineBarrier m_barrier = { };

    uint32_t                  m_setMask = 0u;

    std::array<BindingList, MaxSets> m_setDescriptors       = { };
    std::array<BindingList, MaxSets> m_setSamplers          = { };
    std::array<BindingList, MaxSets> m_setResources         = { };
    std::array<BindingList, MaxSets> m_setUniformBuffers    = { };
    std::array<BindingList, MaxSets> m_setReadOnlyResources = { };

    BindingList               m_readWriteResources = { };

    DxvkShaderBindingMap      m_map;

    const DxvkPipelineLayout* m_layout = nullptr;

    void buildPipelineLayout(DxvkPipelineBindingRange bindings, DxvkPipelineManager* manager);

    uint32_t mapToSet(const DxvkShaderDescriptor& binding) const;

    static uint32_t getSetMaskForStages(VkShaderStageFlags stages);

    static uint32_t getSetCountForStages(VkShaderStageFlags stages);

    static DxvkPipelineBindingRange makeBindingRange(const BindingList& list) {
      DxvkPipelineBindingRange result;
      result.bindingCount = list.size();
      result.bindings = list.data();
      return result;
    }

    static void appendDescriptors(
            BindingList&          list,
      const DxvkShaderDescriptor& binding,
      const DxvkShaderBinding&    mapping) {
      for (uint32_t i = 0; i < binding.getDescriptorCount(); i++)
        list.push_back(binding.remapBinding(mapping.getSet(), mapping.getBinding()).getArrayElement(i));
    }

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

    DxvkBindingLayout(VkShaderStageFlags stages);
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
    VkPushConstantRange getPushConstantRange(bool independent) const {
      VkPushConstantRange result = m_pushConst;

      if (!independent) {
        result.stageFlags &= m_pushConstStages;

        if (!result.stageFlags) {
          result.offset = 0;
          result.size = 0;
        }
      }

      return result;
    }

    /**
     * \brief Queries shader stages
     * \returns Shader stages
     */
    VkShaderStageFlags getStages() const {
      return m_stages;
    }

    /**
     * \brief Queries hazardous sets
     *
     * \returns Mask of sets with storage descriptors
     *    that are not accessed in an order-invariant way.
     */
    uint32_t getHazardousSetMask() const {
      return m_hazards;
    }

    /**
     * \brief Queries defined descriptor set layouts
     *
     * Any set layout not included in this must be null.
     * \returns Bit mask of defined descriptor sets
     */
    uint32_t getSetMask() const;

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
     * \brief Adds a stage that actively uses push constants
     * \param [in] stage Shader stage
     */
    void addPushConstantStage(VkShaderStageFlagBits stage);

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
    VkShaderStageFlags                                        m_pushConstStages;
    VkShaderStageFlags                                        m_stages;
    uint32_t                                                  m_hazards;

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
            DxvkDevice*                 device,
      const DxvkBindingLayout&          layout,
      const DxvkDescriptorSetLayout**   setObjects);

    ~DxvkBindingLayoutObjects();

    /**
     * \brief Binding layout
     * \returns Binding layout
     */
    const DxvkBindingLayout& layout() const {
      return m_layout;
    }

    /**
     * \brief Queries total number of bindings
     * \returns Binding count in all sets
     */
    uint32_t getBindingCount() const {
      return m_bindingCount;
    }

    /**
     * \brief Queries active descriptor set mask
     * \returns Bit mask of non-empty descriptor sets
     */
    uint32_t getSetMask() const {
      return m_setMask;
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
     *
     * \param [in] independent Request INDEPENDENT_SETS_BIT
     * \returns Pipeline layout handle
     */
    VkPipelineLayout getPipelineLayout(bool independent) const {
      return independent
        ? m_independentLayout
        : m_completeLayout;
    }

    /**
     * \brief Queries global resource barrier
     *
     * Can be used to determine whether the pipeline
     * reads or writes any resources, and which shader
     * stages it uses.
     * \returns Global barrier
     */
    DxvkGlobalPipelineBarrier getGlobalBarrier() const;

    /**
     * \brief Queries descriptor mapping
     * \returns Descriptor mapping
     */
    const DxvkShaderBindingMap& map() const {
      return m_map;
    }

  private:

    DxvkDevice*         m_device;
    DxvkBindingLayout   m_layout;
    VkPipelineLayout    m_completeLayout    = VK_NULL_HANDLE;
    VkPipelineLayout    m_independentLayout = VK_NULL_HANDLE;

    uint32_t            m_bindingCount      = 0;
    uint32_t            m_setMask           = 0;

    DxvkShaderBindingMap m_map;

    std::array<const DxvkDescriptorSetLayout*, DxvkDescriptorSets::SetCount> m_bindingObjects = { };

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
        result |= (1u << DxvkDescriptorSets::FsViews);
      if ((m_dirtyBuffers | m_dirtyViews) & (VK_SHADER_STAGE_ALL_GRAPHICS & ~VK_SHADER_STAGE_FRAGMENT_BIT))
        result |= (1u << DxvkDescriptorSets::VsAll);
      return result;
    }

    uint32_t getDirtyComputeSets() const {
      uint32_t result = 0;
      if ((m_dirtyBuffers | m_dirtyViews) & VK_SHADER_STAGE_COMPUTE_BIT)
        result |= (1u << DxvkDescriptorSets::CsAll);
      return result;
    }

  private:

    VkShaderStageFlags m_dirtyBuffers   = 0;
    VkShaderStageFlags m_dirtyViews     = 0;

  };
  
}
