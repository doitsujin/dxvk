#pragma once

#include <unordered_map>

#include "../util/util_bit.h"
#include "../util/thread.h"

#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

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
      } p;

      uint32_t properties[2] = { 0u, 0u };
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

    bool eq(const DxvkSamplerKey& other) const {
      bool eq = u.properties[0] == other.u.properties[0]
             && u.properties[1] == other.u.properties[1];

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

      if (u.p.hasBorder) {
        hash.add(borderColor.uint32[0]);
        hash.add(borderColor.uint32[1]);
        hash.add(borderColor.uint32[2]);
        hash.add(borderColor.uint32[3]);
      }

      return hash;
    }

  };

  static_assert(sizeof(DxvkSamplerKey) == 24u);
  
  
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
      const DxvkSamplerKey&         key);

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
    VkSampler handle() const {
      return m_sampler;
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

    VkSampler             m_sampler   = VK_NULL_HANDLE;

    DxvkSampler*          m_lruPrev   = nullptr;
    DxvkSampler*          m_lruNext   = nullptr;

    void release();

    VkBorderColor determineBorderColorType() const;

  };


  /**
   * \brief Sampler statistics
   */
  struct DxvkSamplerStats {
    /// Number of sampler objects created
    uint32_t totalCount = 0u;
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
    constexpr static uint32_t MaxSamplerCount = 4000u;

    // Minimum number of samplers to keep alive.
    constexpr static uint32_t MinSamplerCount = 1024u;

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
     * \brief Retrieves sampler statistics
     *
     * Note that these might be out of date immediately.
     * \returns Sampler counts
     */
    DxvkSamplerStats getStats() const {
      DxvkSamplerStats stats = { };
      stats.totalCount = m_samplersTotal.load();
      stats.liveCount = m_samplersLive.load();
      return stats;
    }

  private:

    DxvkDevice* m_device;

    dxvk::mutex m_mutex;
    std::unordered_map<DxvkSamplerKey,
      DxvkSampler, DxvkHash, DxvkEq> m_samplers;

    DxvkSampler* m_lruHead = nullptr;
    DxvkSampler* m_lruTail = nullptr;

    std::atomic<uint32_t> m_samplersLive = { 0u };
    std::atomic<uint32_t> m_samplersTotal = { 0u };

    void releaseSampler(DxvkSampler* sampler);

    void destroyLeastRecentlyUsedSampler();

  };


}
