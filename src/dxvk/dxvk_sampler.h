#pragma once

#include <map>
#include <memory>
#include <unordered_map>

#include "../util/util_bit.h"
#include "../util/thread.h"

#include "dxvk_descriptor.h"
#include "dxvk_descriptor_heap.h"
#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

  class DxvkBuffer;
  class DxvkDevice;
  class DxvkSamplerPool;

  /**
   * \brief Sampler key
   *
   * Stores packed sampler properties and in a way that
   * can be reasonably efficiently used with a hash map.
   */
  struct DxvkSamplerKey {
    union {
      struct {
        uint32_t minFilter      :  1;
        uint32_t magFilter      :  1;
        uint32_t mipMode        :  1;
        uint32_t anisotropy     :  5;

        uint32_t addressU       :  3;
        uint32_t addressV       :  3;
        uint32_t addressW       :  3;
        uint32_t hasBorder      :  1;

        uint32_t lodBias        : 14;

        uint32_t minLod         : 12;
        uint32_t maxLod         : 12;

        uint32_t compareEnable  :  1;
        uint32_t compareOp      :  3;
        uint32_t reduction      :  2;
        uint32_t pixelCoord     :  1;
        uint32_t legacyCube     :  1;

        uint32_t viewSwizzleR   :  4;
        uint32_t viewSwizzleG   :  4;
        uint32_t viewSwizzleB   :  4;
        uint32_t viewSwizzleA   :  4;
        uint32_t reserved0      : 16;

        uint32_t viewFormat;
      } p;

      uint32_t properties[4] = { 0u, 0u, 0u, 0u };
    } u;

    VkClearColorValue borderColor = { };

    void setFilter(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip) {
      u.p.minFilter = uint32_t(min);
      u.p.magFilter = uint32_t(mag);
      u.p.mipMode = uint32_t(mip);
    }

    void setAniso(uint32_t anisotropy) {
      u.p.anisotropy = std::min(anisotropy, 16u);
    }

    void setDepthCompare(bool enable, VkCompareOp op) {
      u.p.compareEnable = uint32_t(enable);
      u.p.compareOp = enable ? uint32_t(op) : 0u;
    }

    void setReduction(VkSamplerReductionMode reduction) {
      u.p.reduction = uint32_t(reduction);
    }

    void setUsePixelCoordinates(bool enable) {
      u.p.pixelCoord = uint32_t(enable);
    }

    void setLegacyCubeFilter(bool enable) {
      u.p.legacyCube = uint32_t(enable);
    }

    void setAddressModes(VkSamplerAddressMode u_, VkSamplerAddressMode v_, VkSamplerAddressMode w_) {
      u.p.addressU = u_;
      u.p.addressV = v_;
      u.p.addressW = w_;
      u.p.hasBorder = uint32_t(u_ == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                            || v_ == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                            || w_ == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    }

    void setLodRange(float min, float max, float bias) {
      u.p.minLod = bit::encodeFixed<uint32_t, 4, 8>(min);
      u.p.maxLod = bit::encodeFixed<uint32_t, 4, 8>(std::max(max, min));
      u.p.lodBias = bit::encodeFixed<int32_t, 6, 8>(bias);
    }

    void setBorderColor(VkClearColorValue color) {
      borderColor = color;
    }

    void setViewProperties(const VkComponentMapping& mapping, VkFormat format) {
      u.p.viewSwizzleR = uint32_t(mapping.r);
      u.p.viewSwizzleG = uint32_t(mapping.g);
      u.p.viewSwizzleB = uint32_t(mapping.b);
      u.p.viewSwizzleA = uint32_t(mapping.a);
      u.p.viewFormat = uint32_t(format);
    }

    bool eq(const DxvkSamplerKey& other) const {
      bool eq = u.properties[0] == other.u.properties[0]
             && u.properties[1] == other.u.properties[1]
             && u.properties[2] == other.u.properties[2]
             && u.properties[3] == other.u.properties[3];

      if (eq && u.p.hasBorder) {
        eq = borderColor.uint32[0] == other.borderColor.uint32[0]
          && borderColor.uint32[1] == other.borderColor.uint32[1]
          && borderColor.uint32[2] == other.borderColor.uint32[2]
          && borderColor.uint32[3] == other.borderColor.uint32[3];
      }

      return eq;
    }

    size_t hash() const {
      DxvkHashState hash;
      hash.add(u.properties[0]);
      hash.add(u.properties[1]);
      hash.add(u.properties[2]);
      hash.add(u.properties[3]);

      if (u.p.hasBorder) {
        hash.add(borderColor.uint32[0]);
        hash.add(borderColor.uint32[1]);
        hash.add(borderColor.uint32[2]);
        hash.add(borderColor.uint32[3]);
      }

      return hash;
    }

  };

  static_assert(sizeof(DxvkSamplerKey) == 32u);
  
  
  /**
   * \brief Sampler
   * 
   * Manages a sampler object that can be bound to
   * a pipeline. Sampler objects provide parameters
   * for texture lookups within a shader.
   */
  class DxvkSampler {
    friend class DxvkSamplerPool;
  public:
    
    DxvkSampler(
            DxvkSamplerPool*        pool,
      const DxvkSamplerKey&         key,
            uint16_t                index);

    ~DxvkSampler();

    /**
     * \brief Increments reference count
     */
    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements reference count
     *
     * Recycles the sampler once the ref count reaches zero.
     */
    force_inline void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_relaxed) == 1u)
        release();
    }

    /**
     * \brief Updates tracking ID for sampler object
     *
     * Used when tracking submissions.
     * \param [in] trackingID Tracking ID
     * \returns \c true if the tracking ID has been updated,
     *    \c false if the sampler was already tracked with this ID.
     */
    bool trackId(uint64_t trackingId) {
      if (trackingId <= m_trackingId)
        return false;

      m_trackingId = trackingId;
      return true;
    }

    /**
     * \brief Sampler handle
     * \returns Sampler handle
     */
    DxvkSamplerDescriptor getDescriptor() const {
      return m_descriptor;
    }
    
    /**
     * \brief Sampler key
     * \returns Sampler properties
     */
    const DxvkSamplerKey& key() const {
      return m_key;
    }

  private:
    
    std::atomic<uint64_t> m_refCount  = { 0u };
    uint64_t              m_trackingId = 0u;

    DxvkSamplerPool*      m_pool      = nullptr;
    DxvkSamplerKey        m_key       = { };

    DxvkSamplerDescriptor m_descriptor = { };

    void release();

    VkBorderColor determineBorderColorType(const VkSamplerCustomBorderColorCreateInfoEXT& info) const;

    static VkClearColorValue swizzleBorderColor(const VkClearColorValue& color, VkComponentMapping mapping);

    static float mapBorderColorComponent(const VkClearColorValue& color, const VkComponentMapping& mapping, VkComponentSwizzle which);

  };


  /**
   * \brief Border color registration info
   */
  struct DxvkBorderColor {
    VkFormat          format    = VK_FORMAT_UNDEFINED;
    VkClearColorValue color     = { };
    uint32_t          useCount  = 0u;
  };


  /**
   * \brief Global sampler set and layout
   */
  struct DxvkSamplerDescriptorSet {
    VkDescriptorSet       set         = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout      = VK_NULL_HANDLE;
  };


  /**
   * \brief Sampler descriptor pool
   *
   * Manages a global descriptor pool and set for samplers.
   */
  class DxvkSamplerDescriptorHeap {
    constexpr static uint32_t InvalidBorderColor = -1u;
  public:

    DxvkSamplerDescriptorHeap(
            DxvkDevice*               device,
            uint32_t                  size);

    ~DxvkSamplerDescriptorHeap();

    /**
     * \brief Retrieves descriptor set and layout
     * \returns Descriptor set and layout handles
     */
    DxvkSamplerDescriptorSet getDescriptorSetInfo() const;

    /**
     * \brief Retrieves descriptor heap info
     * \returns Sampler heap address and size
     */
    DxvkDescriptorHeapBindingInfo getDescriptorHeapInfo() const;

    /**
     * \brief Writes sampler descriptor to pool
     *
     * \param [in] index Sampler index
     * \param [in] createInfo Sampler create info
     * \returns Sampler descriptor
     */
    DxvkSamplerDescriptor createSampler(
            uint16_t              index,
      const VkSamplerCreateInfo*  createInfo);

    /**
     * \brief Frees a sampler
     * \param [in] sampler Sampler descriptor
     */
    void freeSampler(
            DxvkSamplerDescriptor sampler);

  private:

    DxvkDevice* m_device          = nullptr;
    uint32_t    m_descriptorCount = 0u;

    struct {
      VkDescriptorPool      pool      = VK_NULL_HANDLE;
      VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
      VkDescriptorSet       set       = VK_NULL_HANDLE;

    } m_legacy;

    struct {
      Rc<DxvkBuffer>        buffer    = nullptr;

      VkDeviceSize          descriptorOffset  = 0u;
      VkDeviceSize          descriptorSize    = 0u;

      VkDeviceSize          reservedSize      = 0u;
    } m_heap;

    struct {
      dxvk::mutex                   mutex;
      std::vector<uint32_t>         indexForSampler;
      std::vector<DxvkBorderColor>  infos;
    } m_borderColors;

    void initDescriptorLayout();

    void initDescriptorPool();

    void initDescriptorHeap();

    uint32_t registerBorderColor(
      const VkSamplerCustomBorderColorCreateInfoEXT*  borderColor);

    uint32_t allocBorderColor(
            uint16_t                                  sampler,
      const VkSamplerCustomBorderColorCreateInfoEXT*  borderColor);

    void freeBorderColor(uint16_t sampler);

    static const VkSamplerCustomBorderColorCreateInfoEXT* findBorderColorInfo(const void* s);

  };


  /**
   * \brief Sampler statistics
   */
  struct DxvkSamplerStats {
    /// Number of samplers currently in use
    uint32_t liveCount = 0u;
  };


  /**
   * \brief Sampler pool
   *
   * Manages unique samplers within a device.
   */
  class DxvkSamplerPool {
    friend DxvkSampler;
  public:

    // Lower limit for sampler counts in Vulkan.
    constexpr static uint32_t MaxSamplerCount = 2048u;

    DxvkSamplerPool(DxvkDevice* device);

    ~DxvkSamplerPool();

    /**
     * \brief Creates sampler
     *
     * \param [in] key Sampler key
     * \returns Sampler object
     */
    Rc<DxvkSampler> createSampler(const DxvkSamplerKey& key);

    /**
     * \brief Queries the global sampler descriptor set
     *
     * Required to bind the set, and for pipeline creation.
     * \returns Global sampler descriptor set and layout
     */
    DxvkSamplerDescriptorSet getDescriptorSetInfo() const {
      return m_descriptorHeap.getDescriptorSetInfo();
    }

    /**
     * \brief Retrieves descriptor heap info
     * \returns Sampler heap address and size
     */
    DxvkDescriptorHeapBindingInfo getDescriptorHeapInfo() const {
      return m_descriptorHeap.getDescriptorHeapInfo();
    }

    /**
     * \brief Retrieves sampler statistics
     *
     * Note that these might be out of date immediately.
     * \returns Sampler counts
     */
    DxvkSamplerStats getStats() const {
      DxvkSamplerStats stats = { };
      stats.liveCount = m_samplersLive.load();
      return stats;
    }

  private:

    struct SamplerEntry {
      int32_t lruPrev = -1;
      int32_t lruNext = -1;
      std::optional<DxvkSampler> object;
    };

    DxvkDevice* m_device = nullptr;

    DxvkSamplerDescriptorHeap m_descriptorHeap;

    dxvk::mutex m_mutex;

    std::array<SamplerEntry, MaxSamplerCount> m_samplers;

    std::unordered_map<DxvkSamplerKey, int32_t, DxvkHash, DxvkEq> m_samplerLut;

    int32_t m_lruHead = -1;
    int32_t m_lruTail = -1;

    std::atomic<uint32_t> m_samplersLive = { 0u };

    Rc<DxvkSampler> m_default = nullptr;

    void releaseSampler(int32_t index);

    void appendLru(SamplerEntry& sampler, int32_t index);

    void removeLru(SamplerEntry& sampler, int32_t index);

    bool samplerIsInLruList(SamplerEntry& sampler, int32_t index) const;

  };


}
