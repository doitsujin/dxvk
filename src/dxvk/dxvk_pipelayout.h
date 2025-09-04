#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "../util/util_small_vector.h"

#include "dxvk_descriptor_info.h"
#include "dxvk_hash.h"
#include "dxvk_include.h"
#include "dxvk_limits.h"

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
    static constexpr uint32_t StoreValueBits = 12u;

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
      Load      = 0x9u,

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
    // For fast-linked pipelines, use the bare minimum of one set
    // for the fragment shader and one for vertex shader resources.
    static constexpr uint32_t GpIndependentSetCount     = 2u;
    static constexpr uint32_t GpIndependentFsResources  = 0u;
    static constexpr uint32_t GpIndependentVsResources  = 1u;

    // For merged pipelines, use one set containing per unique
    // descriptor class. We can reasonably expect uniform buffers
    // to be rebound more often than views and samplers.
    static constexpr uint32_t GpSetCount                = 2u;
    static constexpr uint32_t GpViews                   = 0u;
    static constexpr uint32_t GpBuffers                 = 1u;

    // For compute shaders, put everything into one set since it is
    // very likely that all types of resources get changed at once.
    static constexpr uint32_t CpResources               = 0u;
    static constexpr uint32_t CpSetCount                = 1u;

    // Maximum number of descriptor sets per layout
    static constexpr uint32_t SetCount                  = 2u;
  };


  /**
   * \brief Descriptor class
   */
  struct DxvkDescriptorClass {
    static constexpr uint32_t Buffer  = 1u << 0u;
    static constexpr uint32_t View    = 1u << 8u;
    static constexpr uint32_t Sampler = 1u << 16u;
    static constexpr uint32_t Va      = 1u << 24u;

    static constexpr uint32_t All = Buffer | View | Sampler | Va;
  };


  /**
   * \brief Dirty descriptor set tracker
   */
  class DxvkDescriptorState {
    constexpr static VkShaderStageFlags AllStageMask =
      VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
  public:

    DxvkDescriptorState() = default;

    void dirtyBuffers(VkShaderStageFlags stages) {
      m_dirtyMask |= computeMask(stages, DxvkDescriptorClass::Buffer | DxvkDescriptorClass::Va);
    }

    void dirtyViews(VkShaderStageFlags stages) {
      m_dirtyMask |= computeMask(stages, DxvkDescriptorClass::View | DxvkDescriptorClass::Va);
    }

    void dirtySamplers(VkShaderStageFlags stages) {
      m_dirtyMask |= computeMask(stages, DxvkDescriptorClass::Sampler);
    }

    void dirtyStages(VkShaderStageFlags stages) {
      m_dirtyMask |= computeMask(stages, DxvkDescriptorClass::All);
    }

    void clearStages(VkShaderStageFlags stages) {
      m_dirtyMask &= ~(computeMask(stages, DxvkDescriptorClass::All));
    }

    bool hasDirtyResources(VkShaderStageFlags stages) const {
      return bool(m_dirtyMask & computeMask(stages, DxvkDescriptorClass::All));
    }

    bool hasDirtySamplers(VkShaderStageFlags stages) {
      return bool(m_dirtyMask & computeMask(stages, DxvkDescriptorClass::Sampler));
    }

    bool hasDirtyVas(VkShaderStageFlags stages) {
      return bool(m_dirtyMask & computeMask(stages, DxvkDescriptorClass::Va));
    }

    bool testDirtyMask(uint32_t mask) const {
      return m_dirtyMask & mask;
    }

    VkShaderStageFlags getDirtyStageMask(uint32_t classes) const {
      VkShaderStageFlags result = 0u;

      for (auto classShift : bit::BitMask(classes))
        result |= m_dirtyMask >> classShift;

      return result & AllStageMask;
    }

    static constexpr uint32_t computeMask(VkShaderStageFlags stages, uint32_t classes) {
      return uint32_t(stages) * classes;
    }

  private:

    uint32_t m_dirtyMask = 0u;

  };


  /**
   * \brief Pipeline layout type
   *
   * Determines whether to use a pipeline layout with stage-separated
   * descriptor sets, or one with merged sets.
   */
  enum class DxvkPipelineLayoutType : uint16_t {
    Independent = 0u, ///< Fragment and pre-raster shaders use separate sets
    Merged      = 1u, ///< Fragment and pre-raster shaders use the same sets
  };


  /**
   * \brief Pipeline layout flag
   *
   * Defines properties of the layout.
   */
  enum class DxvkPipelineLayoutFlag : uint8_t {
    UsesSamplerHeap = 0u, ///< Requires global sampler heap
  };

  using DxvkPipelineLayoutFlags = Flags<DxvkPipelineLayoutFlag>;


  /**
   * \brief Descriptor flags
   */
  enum class DxvkDescriptorFlag : uint8_t {
    /** Resource is a plain (uniform) buffer, not a view */
    UniformBuffer   = 0u,
    /** Image resource may be be multisampled */
    Multisampled    = 1u,
    /** Resource is accessed via push data */
    PushData        = 2u,
  };

  using DxvkDescriptorFlags = Flags<DxvkDescriptorFlag>;


  /**
   * \brief Binding info
   *
   * Stores metadata for a single binding in
   * a given shader, or for the whole pipeline.
   */
  struct DxvkBindingInfo {
    /** Shader-defined descriptor set index */
    uint32_t set = 0u;
    /** Shader-defined binding index */
    uint32_t binding = 0u;
    /** Binding slot for the resource */
    uint32_t resourceIndex = 0u;
    /** Descriptor type */
    VkDescriptorType descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    /** Size of descriptor array */
    uint32_t descriptorCount = 1u;
    /** Image view type */
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    /** Access flags for the resource in the shader */
    VkAccessFlags access = 0u;
    /** Additional binding properties */
    DxvkDescriptorFlags flags = 0u;
    /** Order-invariant access type, if any */
    DxvkAccessOp accessOp = DxvkAccessOp::None;
    /** Byte offset of raw address or descriptor index within
     *  the shader's push data block. This will get remapped
     *  when chaining push constant blocks. */
    uint32_t blockOffset = 0u;
  };


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

    DxvkShaderDescriptor(const DxvkBindingInfo& binding, VkShaderStageFlags stages)
    : m_type      (uint8_t(binding.descriptorType)),
      m_stages    (uint8_t(stages)),
      m_index     (uint16_t(binding.resourceIndex)),
      m_viewType  (uint8_t(binding.viewType)),
      m_access    (uint8_t(binding.access)),
      m_accessOp  (binding.accessOp),
      m_flags     (binding.flags),
      m_set       (uint8_t(binding.set)),
      m_binding   (uint16_t(binding.binding)),
      m_arrayIndex(0u),
      m_arraySize (uint16_t(binding.descriptorCount)),
      m_blockOffset(uint16_t(binding.blockOffset)) { }

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
     * \brief Checks whether the resource uses a descriptor
     *
     * Resources may either be accessed through a descriptor stored in the
     * descriptor set, or through the raw GPU address or a descriptor index
     * that is passed in as a push constant.
     * \returns \c true for any descriptor-backed resource
     */
    bool usesDescriptor() const {
      return !m_flags.test(DxvkDescriptorFlag::PushData);
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
     * \brief Queries offset in push data block
     *
     * For GPU addresses, the size of each element will be 8 bytes.
     * For descripor-backed resources, this carries no meaning.
     * \returns Byte offset into push data block
     */
    uint32_t getBlockOffset() const {
      return m_blockOffset;
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
      uint8_t baseIndex = m_arrayIndex;

      DxvkShaderDescriptor result = *this;
      result.m_index += index - baseIndex;
      result.m_arrayIndex = index - baseIndex;
      result.m_arraySize = 1u;
      result.m_blockOffset += getBlockEntrySize() * uint32_t(index - baseIndex);
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
     * \bief Changes block offset
     *
     * Used when remapping push data to its final memory layout.
     * \param [in] offset New absolute block offset
     */
    void setBlockOffset(uint32_t offset) {
      m_blockOffset = offset;
    }

    /**
     * \brief Queries resource info size within the push data block
     *
     * Only returns info for a single element. To get the full data
     * size, multiply with the descriptor count of the binding.
     *
     * The returned size depends on the descriptor type.
     * \returns Element size in push data block
     */
    uint32_t getBlockEntrySize() const {
      switch (getDescriptorType()) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
          return sizeof(uint16_t);

        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          return sizeof(VkDeviceAddress);

        default:
          return 0u;
      }
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
    uint8_t             m_arrayIndex        = 0u;
    uint8_t             m_arraySize         = 0u;
    uint16_t            m_blockOffset       = 0u;

    uint64_t encodeNumeric() const {
      return uint64_t(m_arrayIndex + m_blockOffset)
          | (uint64_t(m_binding)     << 16u)
          | (uint64_t(m_set)         << 32u)
          | (uint64_t(m_type)        << 40u)
          | (uint64_t(m_stages)      << 48u);
    }

  };


  /**
   * \brief Push data block
   *
   * Maps to a shader-defined push constant range, which may
   * contain user-provided data or raw shader bindings.
   *
   * For graphics pipelines, there are two types of push data
   * blocks: Global data, which is available to all stages in
   * the pipeline, and per-stage data.
   */
  class DxvkPushDataBlock {

  public:

    // One shared block and one per shader stage
    constexpr static uint32_t MaxBlockCount = 6u;

    DxvkPushDataBlock() = default;

    DxvkPushDataBlock(
            VkShaderStageFlags        stages,
            uint32_t                  offset,
            uint32_t                  size,
            uint32_t                  alignment,
            uint64_t                  resourceMask)
    : m_stageMask   (uint16_t(stages)),
      m_alignment   (uint16_t(alignment)),
      m_offset      (uint16_t(offset)),
      m_size        (uint16_t(size)),
      m_resourceMask(uint64_t(resourceMask)) { }

    DxvkPushDataBlock(
            uint32_t                  offset,
            uint32_t                  size,
            uint32_t                  alignment,
            uint64_t                  resourceMask)
    : DxvkPushDataBlock(0u, offset, size, alignment, resourceMask) { }

    /**
     * \brief Queries stage mask
     * \returns Stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return m_stageMask;
    }

    /**
     * \brief Checks whether the block is shared
     *
     * Any push constant block with more than
     * one stage flag is considered shared.
     * \returns \c true if the block is shared
     */
    bool isShared() const {
      return bool(m_stageMask & (m_stageMask - 1u));
    }

    /**
     * \brief Checks whether the block is empty
     * \returns \c true for an empty block
     */
    bool isEmpty() const {
      return !m_size;
    }

    /**
     * \brief Queries block size
     * \returns Data block size
     */
    uint32_t getSize() const {
      return m_size;
    }

    /**
     * \brief Queries required block alignment
     *
     * Will be at least 4 bytes, but may be higher
     * if the block stores 64-bit data types.
     * \returns Required data alignment
     */
    uint32_t getAlignment() const {
      return m_alignment;
    }

    /**
     * \brief Push data offset
     *
     * Depending on the context, this either contains the
     * shader-defined push constant offset, or the real
     * offset as defined in the final push constant layout.
     *
     * When remapping, the offsets of all push constant
     * block members within this range will be changed.
     *
     * The shared push constant block will always be
     * mapped to offset 0.
     * \returns Push data offset
     */
    uint32_t getOffset() const {
      return m_offset;
    }

    /**
     * \brief Queries mask of dwords used for resource data
     *
     * The dword corresponding to each set bit in the mask will
     * not be taken from userdata, but will instead contain a
     * resource index or address.
     *
     * Bit 0 corresponds to the first dword int the block.
     * \returns Resource mask
     */
    uint64_t getResourceDwordMask() const {
      return m_resourceMask;
    }

    /**
     * \brief Merges block with another
     *
     * Useful when dealing with shared push constant block
     * definitions coming in from multiple shaders.
     *
     * If neither block is empty, then the offsets of both
     * blocks must be identical; different shaders using
     * this block must agree on its layout.
     * \param [in] other The block to merge with
     */
    void merge(const DxvkPushDataBlock& other) {
      uint32_t oldOffset = m_offset;
      uint32_t newOffset = other.m_offset;

      m_stageMask    |= other.m_stageMask;
      m_alignment     = std::max(m_alignment, other.m_alignment);
      m_offset        = std::min(newOffset, m_size ? oldOffset : newOffset);
      m_size          = align(std::max(oldOffset + m_size, newOffset + other.m_size) - m_offset, m_alignment);

      // Preserve correct bit location of resource masks
      m_resourceMask <<= (oldOffset / sizeof(uint32_t));
      m_resourceMask |= other.m_resourceMask << (newOffset / sizeof(uint32_t));
      m_resourceMask >>= m_offset / sizeof(uint32_t);
    }

    /**
     * \brief Shifts block to a certain offset
     *
     * Useful when remapping push constant ranges.
     * \param [in] newOffset New block offset
     * \param [in] newSize New block size
     */
    void rebase(uint32_t newOffset, uint32_t newSize) {
      m_offset = newOffset;
      m_size = newSize;
    }

    /**
     * \brief Makes block absolute
     *
     * Changes the block to have an offset of 0 while keeping
     * all data in its place. The resulting block may thus
     * be larger than the original.
     */
    void makeAbsolute() {
      m_resourceMask <<= m_offset;
      m_size += m_offset;

      m_offset = 0u;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Block to compare to
     * \returns \c true if the two blocks are identical
     */
    bool eq(const DxvkPushDataBlock& other) const {
      return m_stageMask    == other.m_stageMask
          && m_alignment    == other.m_alignment
          && m_offset       == other.m_offset
          && m_size         == other.m_size
          && m_resourceMask == other.m_resourceMask;
    }

    /**
     * \brief Computes hash
     * \returns Hash value
     */
    size_t hash() const {
      DxvkHashState hash;
      hash.add(m_stageMask);
      hash.add(m_alignment);
      hash.add(m_offset);
      hash.add(m_size);
      hash.add(m_resourceMask);
      return hash;
    }

    /**
     * \brief Computes push data index for given stage mask
     *
     * If this is a shared or compute block, the index will
     * always be 0, otherwise it depends on the exact stage.
     * \param [in] stageMask Stage mask
     * \returns Push data block index
     */
    static uint32_t computeIndex(VkShaderStageFlags stageMask) {
      if (stageMask & VK_SHADER_STAGE_COMPUTE_BIT)
        return 0u;

      uint32_t remainder = stageMask & (stageMask - 1u);
      return remainder ? 0u : (bit::tzcnt(uint32_t(stageMask)) + 1u);
    }

  private:

    uint16_t  m_stageMask     = 0u;
    uint16_t  m_alignment     = 0u;
    uint16_t  m_offset        = 0u;
    uint16_t  m_size          = 0u;
    uint64_t  m_resourceMask  = 0u;

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
     * \returns Binding index
     */
    uint32_t add(DxvkDescriptorSetLayoutBinding binding);

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
     * \brief Checks whether the set layout is empty
     *
     * Empty set layouts are sometimes needed to create valid
     * pipeline layouts for pipeline libraries.
     * \returns \c true if the set layout contains no descriptors
     */
    bool isEmpty() const {
      return m_empty;
    }

    /**
     * \brief Queries descriptor set layout
     * \returns Descriptor set layout
     */
    VkDescriptorSetLayout getSetLayout() const {
      return m_legacy.layout;
    }

    /**
     * \brief Queries descriptor template
     * \returns Descriptor update template
     */
    VkDescriptorUpdateTemplate getSetUpdateTemplate() const {
      return m_legacy.updateTemplate;
    }

    /**
     * \brief Queries allocation size for the set
     * \returns Space required in descriptor heap
     */
    VkDeviceSize getMemorySize() const {
      return m_heap.memorySize;
    }

    /**
     * \brief Updates descriptor memory
     *
     * Uses the pre-computed update list to write descriptors.
     * \param [in] dst Pointer to descriptor memory
     * \param [in] descriptors Pointer to source descriptor list
     */
    void update(
            void*                   dst,
      const DxvkDescriptor**        descriptors) const {
      m_heap.update.update(dst, descriptors);
    }

  private:

    DxvkDevice*                   m_device;
    bool                          m_empty     = false;

    struct {
      VkDescriptorSetLayout       layout          = VK_NULL_HANDLE;
      VkDescriptorUpdateTemplate  updateTemplate  = VK_NULL_HANDLE;
    } m_legacy;

    struct {
      VkDeviceSize                memorySize = 0u;
      DxvkDescriptorUpdateList    update;
    } m_heap;

    void initSetLayout(const DxvkDescriptorSetLayoutKey& key);

    void initDescriptorBufferUpdate(const DxvkDescriptorSetLayoutKey& key);

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
            DxvkPipelineLayoutType    type,
            DxvkPipelineLayoutFlags   flags)
    : m_type          (type),
      m_flags         (flags) { }

    DxvkPipelineLayoutKey(
            DxvkPipelineLayoutType    type,
            DxvkPipelineLayoutFlags   flags,
            VkShaderStageFlags        stageMask,
            uint32_t                  pushDataBlockCount,
      const DxvkPushDataBlock*        pushDataBlocks,
            uint32_t                  setCount,
      const DxvkDescriptorSetLayout** setLayouts)
    : m_type          (type),
      m_flags         (flags),
      m_stages        (uint8_t(stageMask)) {
      for (uint32_t i = 0u; i < pushDataBlockCount; i++)
        addPushData(pushDataBlocks[i]);

      setDescriptorSetLayouts(setCount, setLayouts);
    }

    /**
     * \brief Queries layout type
     * \returns Layout type
     */
    DxvkPipelineLayoutType getType() const {
      return m_type;
    }

    /**
     * \brief Queries layout flags
     * \returns Layout flags
     */
    DxvkPipelineLayoutFlags getFlags() const {
      return m_flags;
    }

    /**
     * \brief Adds a set of shader stages
     * \param [in] stageMask Shader stages to add
     */
    void addStages(VkShaderStageFlags stageMask) {
      m_stages |= uint8_t(stageMask);
    }

    /**
     * \brief Adds a push data block
     *
     * If a shared block is added and another shared push
     * constant block already exists, they will be merged.
     * \param [in] block Push data block to add
     */
    void addPushData(const DxvkPushDataBlock& block) {
      uint32_t index = DxvkPushDataBlock::computeIndex(block.getStageMask());

      if (!block.isEmpty()) {
        m_pushMask |= 1u << index;
        m_pushData[index].merge(block);
      }
    }

    /**
     * \brief Sets flags
     * \param [in] flags Flags to set
     */
    void setFlags(DxvkPipelineLayoutFlags flags) {
      m_flags.set(flags);
    }

    /**
     * \brief Queries push data block mask
     * \returns Mask of active push data blocks
     */
    uint32_t getPushDataMask() const {
      return m_pushMask;
    }

    /**
     * \brief Queries push data block info
     *
     * \param [in] index Block index
     * \returns Push data block info
     */
    DxvkPushDataBlock getPushDataBlock(uint32_t index) const {
      return m_pushData[index];
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
     * \brief Checks for equality
     *
     * \param [in] other Pipeline layout key to compare to
     * \returns \c true if both layout keys are equal
     */
    bool eq(const DxvkPipelineLayoutKey& other) const {
      bool eq = m_type      == other.m_type
             && m_flags     == other.m_flags
             && m_stages    == other.m_stages
             && m_pushMask  == other.m_pushMask
             && m_setCount  == other.m_setCount;

      for (auto i : bit::BitMask(uint32_t(m_pushMask)))
        eq &= m_pushData[i].eq(other.m_pushData[i]);

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
      hash.add(uint16_t(m_type));
      hash.add(m_flags.raw());
      hash.add(m_stages);
      hash.add(m_setCount);
      hash.add(m_pushMask);

      for (auto i : bit::BitMask(uint32_t(m_pushMask)))
        hash.add(m_pushData[i].hash());

      for (uint32_t i = 0; i < m_setCount; i++)
        hash.add(reinterpret_cast<uintptr_t>(m_sets[i]));

      return hash;
    }

  private:

    DxvkPipelineLayoutType  m_type      = DxvkPipelineLayoutType::Independent;
    DxvkPipelineLayoutFlags m_flags     = 0u;
    uint8_t                 m_stages    = 0u;
    uint8_t                 m_pushMask  = 0u;
    uint8_t                 m_setCount  = 0u;

    std::array<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount> m_pushData = { };

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
     * \brief Queries pipeline bind point
     * \returns Pipeline bind point
     */
    VkPipelineBindPoint getBindPoint() const {
      return m_bindPoint;
    }

    /**
     * \brief Queries Vulkan pipeline layout
     *
     * \param [in] independent Whether to return a pipeline
     *    layout that can be used with pipeline libraries.
     * \returns Pipeline layout handle
     */
    VkPipelineLayout getPipelineLayout() const {
      return m_legacy.layout;
    }

    /**
     * \brief Checks whether the pipeline layout uses the sampler heap
     *
     * Affects the pipeline layout as well as resource binding.
     * \returns \c true if the sampler heap is used
     */
    bool usesSamplerHeap() const {
      return m_flags.test(DxvkPipelineLayoutFlag::UsesSamplerHeap);
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

    /**
     * \brief Queries total descriptor memory size
     *
     * Returns the memory required for all descriptor sets.
     * Use this to determine whether to allocate a new
     * descriptor range from the resource heap.
     */
    VkDeviceSize getDescriptorMemorySize() const {
      return m_heap.setMemorySize;
    }

    /**
     * \brief Queries non-empty push data block mask
     */
    uint32_t getPushDataMask() const {
      return m_pushData.blockMask;
    }

    /**
     * \brief Queries merged push data block
     *
     * This block includes all stages and all bytes.
     */
    DxvkPushDataBlock getPushData() const {
      return m_pushData.mergedBlock;
    }

    /**
     * \brief Queries push data block
     *
     * \param [in] index Block index
     * \returns Push data block
     */
    DxvkPushDataBlock getPushDataBlock(uint32_t index) const {
      return m_pushData.blocks[index];
    }

  private:

    DxvkDevice*             m_device;

    DxvkPipelineLayoutFlags m_flags;
    VkPipelineBindPoint     m_bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::array<const DxvkDescriptorSetLayout*, DxvkPipelineLayoutKey::MaxSets> m_setLayouts = { };

    struct {
      VkPipelineLayout  layout = VK_NULL_HANDLE;
    } m_legacy;

    struct {
      DxvkPushDataBlock mergedBlock = { };
      uint32_t          blockMask   = 0u;
      std::array<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount> blocks = { };
    } m_pushData;

    struct {
      VkDeviceSize      setMemorySize = 0u;
    } m_heap;

    void initMetadata(
      const DxvkPipelineLayoutKey&      key);

    void initPipelineLayout(
      const DxvkPipelineLayoutKey&      key);

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
     * \brief Queries stage mask
     * \returns Stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return VkShaderStageFlags(m_stages);
    }

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
    void addBinding(DxvkShaderBinding srcBinding, DxvkShaderBinding dstBinding);

    /**
     * \brief Adds push data block
     *
     * \param [in] block Push constant block
     * \param [in] offset New offset
     */
    void addPushData(const DxvkPushDataBlock& block, uint32_t offset);

    /**
     * \brief Looks up shader binding
     *
     * \param [in] srcBinding Shader-defined binding
     * \returns Pointer to remapped binding, or \c nullptr
     */
    const DxvkShaderBinding* mapBinding(DxvkShaderBinding srcBinding) const;

    /**
     * \brief Looks up push constant data
     *
     * Finds a push data block containing the given
     * offset and computes the remapped offset.
     * \param [in] stage Shader stage
     * \param [in] offset Push data offset to look up
     * \returns Remapped offset, or -1
     */
    uint32_t mapPushData(VkShaderStageFlags stage, uint32_t offset) const;

  private:

    std::unordered_map<DxvkShaderBinding, DxvkShaderBinding, DxvkHash, DxvkEq> m_bindings;

    small_vector<std::pair<DxvkPushDataBlock, uint32_t>, DxvkPushDataBlock::MaxBlockCount> m_pushData;

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
     * \brief Queries push data block mask
     * \returns Mask of active push data blocks
     */
    uint32_t getPushDataMask() const {
      return m_pushMask;
    }

    /**
     * \brief Queries push data block info
     *
     * \param [in] index Block index
     * \returns Push data block info
     */
    DxvkPushDataBlock getPushDataBlock(uint32_t index) const {
      return m_pushData[index];
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
     * \brief Queries number of sampler heap bindings
     * \returns Sampler heap binding count
     */
    uint32_t getSamplerHeapBindingCount() const {
      return m_samplerHeaps.size();
    }

    /**
     * \brief Queries sampler heap binding info
     *
     * \param [in] index Sampler heap binding index
     * \returns Set and binding for a given shader stage
     */
    DxvkShaderBinding getSamplerHeapBinding(uint32_t index) const {
      return m_samplerHeaps[index];
    }

    /**
     * \brief Adds push data block
     * \param [in] range Push data block
     */
    void addPushData(
            DxvkPushDataBlock         range);

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
     * \brief Adds sampler heap declaration
     *
     * Used so that the sampler binding can be remapped.
     * \param [in] binding Sampler heap binding info
     */
    void addSamplerHeap(
      const DxvkShaderBinding&        binding);

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

    VkShaderStageFlags      m_stageMask     = 0u;

    uint32_t                                                        m_pushMask = 0u;
    std::array<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount> m_pushData = { };

    small_vector<DxvkShaderDescriptor, 32u> m_bindings;
    small_vector<DxvkShaderBinding, 4u> m_samplerHeaps;

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
    constexpr static uint32_t MaxStages = 6u;

    using BindingList = small_vector<DxvkShaderDescriptor, 32>;
  public:

    DxvkPipelineBindings(
            DxvkDevice*                 device,
            DxvkPipelineManager*        manager,
      const DxvkPipelineLayoutBuilder&  builder);

    ~DxvkPipelineBindings();

    /**
     * \brief Queries available pipeline stages
     * \returns All stages with descriptors
     */
    VkShaderStageFlags getNonemptyStageMask() const {
      return m_nonemptyStageMask;
    }

    /**
     * \brief Queries pipeline layout
     * \returns Pipeline layout
     */
    const DxvkPipelineLayout* getLayout(DxvkPipelineLayoutType type) const {
      return m_layouts[uint32_t(type)].layout;
    }

    /**
     * \brief Compute dirty sets for the given descriptor state
     *
     * \param [in] type Pipeline layout type
     * \param [in] state Descriptor state
     * \returns Mask of sets that need updating
     */
    uint32_t getDirtySetMask(
            DxvkPipelineLayoutType  type,
      const DxvkDescriptorState&    state) const {
      const auto& layout = m_layouts[uint32_t(type)];

      uint32_t result = 0u;

      for (uint32_t set = 0u; set < layout.setStateMasks.size(); set++) {
        if (state.testDirtyMask(layout.setStateMasks[set]))
          result |= 1u << set;
      }

      return result;
    }

    /**
     * \brief Gets total number of descriptors in all sets
     *
     * Can be used to determine an upper bound of descriptor infos
     * to allocate when exact information is not yet available.
     * \returns Total descriptor count in all sets
     */
    uint32_t getDescriptorCount() const {
      return m_descriptorCount;
    }

    /**
     * \brief Queries all available bindings in a given set
     *
     * Primarily useful to update dirty descriptor sets.
     * \param [in] type Pipeline layout type
     * \param [in] set Set index
     * \returns List of all bindings in the set
     */
    DxvkPipelineBindingRange getAllDescriptorsInSet(DxvkPipelineLayoutType type, uint32_t set) const {
      return makeBindingRange(m_layouts[uint32_t(type)].setDescriptors[set]);
    }

    /**
     * \brief Queries all shader resources in a given set
     *
     * Includes all resources that are bound via an image or buffer view,
     * even unformatted buffer views (i.e. storage buffers). Does not
     * include pure samplers or buffers that are only bound via a buffer
     * range or address, i.e. uniform buffers.
     * \param [in] type Pipeline layout type
     * \param [in] set Set index
     * \returns List of all resource bindings in the set
     */
    DxvkPipelineBindingRange getResourcesInSet(DxvkPipelineLayoutType type, uint32_t set) const {
      return makeBindingRange(m_layouts[uint32_t(type)].setResources[set]);
    }

    /**
     * \brief Queries all uniform buffers in a given set
     *
     * Includes all uniform buffer resources, i.e. resources that are only
     * bound via a buffer range and do not use views, and use the uniform
     * or storage buffer descriptor type.
     * \param [in] type Pipeline layout type
     * \param [in] set Set index
     * \returns List of all uniform buffer bindings in the set
     */
    DxvkPipelineBindingRange getUniformBuffersInSet(DxvkPipelineLayoutType type, uint32_t set) const {
      return makeBindingRange(m_layouts[uint32_t(type)].setUniformBuffers[set]);
    }

    /**
     * \brief Queries all sampler bindings
     *
     * This includes only pure samplers, not
     * combined image and sampler descriptors.
     * \param [in] type Pipeline layout type
     * \returns List of all sampler bindings in the set
     */
    DxvkPipelineBindingRange getSamplers(DxvkPipelineLayoutType type) const {
      return makeBindingRange(m_layouts[uint32_t(type)].samplers);
    }

    /**
     * \brief Queries all virtual address bindings
     *
     * \param [in] type Pipeline layout type
     * \param [in] set Set index
     * \returns List of all non-descriptor bindings.
     */
    DxvkPipelineBindingRange getVaBindings(DxvkPipelineLayoutType type) const {
      return makeBindingRange(m_layouts[uint32_t(type)].vaBindings);
    }

    /**
     * \brief Queries all read-only resources for a given stage
     *
     * Subset of \c getResourcesInSet that only includes bindings that are
     * read and not written by the shader. Useful for hazard tracking.
     * \returns List of all read-only resources in the set
     */
    DxvkPipelineBindingRange getReadOnlyResourcesForStage(VkShaderStageFlagBits stage) const {
      return makeBindingRange(m_readOnlyResources[bit::bsf(uint32_t(stage))]);
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
     * \brief Queries shader stages with hazardous descriptors
     *
     * If non-zero, back-to-back draws or dispatches using the same
     * set of resources must be synchronized, otherwise it is safe
     * to only check for hazards when bound resources have changed.
     * \returns Stages that have hazardous descriptors
     */
    VkShaderStageFlags getHazardousStageMask() const {
      return m_hazardousStageMask;
    }

    /**
     * \brief Queries global resource barrier
     *
     * Can be used to determine whether the pipeline
     * reads or writes any resources, and which shader
     * stages it uses.
     * \returns Global barrier
     */
    DxvkGlobalPipelineBarrier getGlobalBarrier() const {
      return m_barrier;
    }

    /**
     * \brief Queries binding map
     *
     * The binding map is primarily useful for shader patching.
     * \param [in] type Pipeline layout type
     * \returns Pointer to binding map
     */
    const DxvkShaderBindingMap* getBindingMap(DxvkPipelineLayoutType type) const {
      return &m_layouts[uint32_t(type)].bindingMap;
    }

  private:

    struct PerLayoutInfo {
      std::array<BindingList, MaxSets>  setDescriptors       = { };
      std::array<BindingList, MaxSets>  setResources         = { };
      std::array<BindingList, MaxSets>  setUniformBuffers    = { };

      std::array<uint32_t, MaxSets>     setStateMasks = { };

      BindingList                       samplers = { };
      BindingList                       vaBindings = { };
      DxvkShaderBindingMap              bindingMap = { };

      const DxvkPipelineLayout*         layout = nullptr;
    };

    struct SetInfos {
      uint32_t mask   = { };
      uint32_t count  = { };
      std::array<uint8_t, MaxSets> map = { };
    };

    VkShaderStageFlags        m_nonemptyStageMask = 0u;
    VkShaderStageFlags        m_hazardousStageMask = 0u;

    DxvkGlobalPipelineBarrier m_barrier = { };
    uint32_t                  m_descriptorCount = 0u;

    std::array<PerLayoutInfo, 2u> m_layouts = { };

    std::array<BindingList, MaxStages>  m_readOnlyResources = { };
    BindingList                         m_readWriteResources = { };

    void buildPipelineLayout(
            DxvkPipelineLayoutType      type,
            DxvkDevice*                 device,
      const DxvkPipelineLayoutBuilder&  builder,
            DxvkPipelineManager*        manager);

    small_vector<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount>
    buildPushDataBlocks(
            DxvkPipelineLayoutType      type,
            DxvkDevice*                 device,
      const DxvkPipelineLayoutBuilder&  builder,
            DxvkPipelineManager*        manager);

    small_vector<const DxvkDescriptorSetLayout*, MaxSets>
    buildDescriptorSetLayouts(
            DxvkPipelineLayoutType      type,
            DxvkPipelineLayoutFlags     flags,
      const DxvkPipelineLayoutBuilder&  builder,
            DxvkPipelineManager*        manager);

    void buildMetadata(
      const DxvkPipelineLayoutBuilder&  builder);

    DxvkPipelineLayoutFlags getPipelineLayoutFlags(
            DxvkPipelineLayoutType      type,
      const DxvkPipelineLayoutBuilder&  builder);

    static uint32_t computeStateMask(
      const DxvkShaderDescriptor&     binding);

    static uint32_t computeSetForBinding(
            DxvkPipelineLayoutType    type,
      const DxvkShaderDescriptor&     binding);

    static SetInfos computeSetMaskAndCount(
            DxvkPipelineLayoutType    type,
            VkShaderStageFlags        stageMask,
            DxvkPipelineBindingRange  bindings);

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

}
