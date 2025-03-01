#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../util/util_time.h"

#include "../dxvk_gpu_query.h"

#include "dxvk_hud_renderer.h"

namespace dxvk::hud {

  /**
   * \brief HUD item
   *
   * A single named item in the HUD that
   * can be enabled by the user.
   */
  class HudItem : public RcObject {

  public:

    virtual ~HudItem();

    /**
     * \brief Updates the HUD item
     * \param [in] time Current time
     */
    virtual void update(
            dxvk::high_resolution_clock::time_point time);

    /**
     * \brief Renders the HUD
     *
     * \param [in] ctx Raw context objects
     * \param [in] options HUD options
     * \param [in] renderer HUD renderer for text rendering
     * \param [in] position Base offset
     * \returns Base offset for next item
     */
    virtual HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position) = 0;

  };


  /**
   * \brief HUD item set
   *
   * Manages HUD items.
   */
  class HudItemSet {

  public:

    HudItemSet(const Rc<DxvkDevice>& device);

    ~HudItemSet();

    /**
     * \brief Updates the HUD
     * Updates all enabled HUD items.
     */
    void update();

    /**
     * \brief Renders the HUD
     *
     * \param [in] renderer HUD renderer
     * \returns Base offset for next item
     */
    void render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer);

    /**
     * \brief Checks whether the item set is empty
     * \returns \c true if there are no items
     */
    bool empty() const {
      return m_items.empty();
    }

    /**
     * \brief Creates a HUD item if enabled
     *
     * \tparam T The HUD item type
     * \param [in] name HUD item name
     * \param [in] at Position at which to insert the item
     * \param [in] args Constructor arguments
     */
    template<typename T, typename... Args>
    Rc<T> add(const char* name, int32_t at, Args... args) {
      bool enable = m_enableFull;

      if (!enable) {
        auto entry = m_enabled.find(name);
        enable = entry != m_enabled.end();
      }

      if (at < 0 || at > int32_t(m_items.size()))
        at = m_items.size();

      Rc<T> item;

      if (enable) {
        item = new T(std::forward<Args>(args)...);
        m_items.insert(m_items.begin() + at, item);
      }

      return item;
    }

    template<typename T>
    T getOption(const char *option, T fallback) {
      auto entry = m_options.find(option);
      if (entry == m_options.end())
        return fallback;

      T value = fallback;
      parseOption(entry->second, value);
      return value;
    }

  private:

    bool                                          m_enableFull = false;
    std::unordered_set<std::string>               m_enabled;
    std::unordered_map<std::string, std::string>  m_options;
    std::vector<Rc<HudItem>>                      m_items;

    static void parseOption(const std::string& str, float& value);

  };


  /**
   * \brief HUD item to display DXVK version
   */
  class HudVersionItem : public HudItem {

  public:

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  };


  /**
   * \brief HUD item to display the client API
   */
  class HudClientApiItem : public HudItem {

  public:

    HudClientApiItem(std::string api);

    ~HudClientApiItem();

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    sync::Spinlock  m_mutex;
    std::string     m_api;

  };


  /**
   * \brief HUD item to display device info
   */
  class HudDeviceInfoItem : public HudItem {

  public:

    HudDeviceInfoItem(const Rc<DxvkDevice>& device);

    ~HudDeviceInfoItem();

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    std::string m_deviceName;
    std::string m_driverName;
    std::string m_driverVer;

  };


  /**
   * \brief HUD item to display the frame rate
   */
  class HudFpsItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudFpsItem();

    ~HudFpsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    uint32_t                                m_frameCount = 0;
    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

    std::string m_frameRate;

  };


  /**
   * \brief HUD item to display the frame rate
   */
  class HudFrameTimeItem : public HudItem {
    constexpr static size_t NumDataPoints = 420u;
    constexpr static size_t NumTextDraws = 2u;
  public:

    HudFrameTimeItem(
      const Rc<DxvkDevice>&     device,
            HudRenderer*        renderer);

    ~HudFrameTimeItem();

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    struct ComputePushConstants {
      float msPerTick;
      uint32_t dataPoint;
      int16_t textPosMinX;
      int16_t textPosMinY;
      int16_t textPosMaxX;
      int16_t textPosMaxY;
    };

    struct RenderPushConstants {
      HudPushConstants hud;
      int16_t x;
      int16_t y;
      int16_t w;
      int16_t h;
      uint32_t frameIndex;
    };

    struct BufferLayout {
      size_t timestampSize;
      size_t drawInfoOffset;
      size_t drawInfoSize;
      size_t drawParamOffset;
      size_t drawParamSize;
      size_t textOffset;
      size_t textSize;
      size_t totalSize;
    };

    Rc<DxvkDevice>            m_device;
    Rc<DxvkBuffer>            m_gpuBuffer;
    Rc<DxvkBufferView>        m_textView;
    Rc<DxvkGpuQuery>          m_query;

    VkDescriptorSetLayout     m_computeSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout          m_computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline                m_computePipeline = VK_NULL_HANDLE;

    HudShaderModule           m_vs;
    HudShaderModule           m_fs;

    VkDescriptorSetLayout     m_gfxSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout          m_gfxPipelineLayout = VK_NULL_HANDLE;

    std::unordered_map<HudPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_gfxPipelines;

    uint32_t                  m_nextDataPoint = 0u;

    void processFrameTimes(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
            HudRenderer&        renderer,
            uint32_t            dataPoint,
            HudPos              minPos,
            HudPos              maxPos);

    void drawFrameTimeGraph(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
            HudRenderer&        renderer,
            uint32_t            dataPoint,
            HudPos              graphPos,
            HudPos              graphSize);

    void createResources(
      const DxvkContextObjects& ctx);

    void createComputePipeline(
          HudRenderer&          renderer);

    VkDescriptorSetLayout createDescriptorSetLayout();

    VkPipelineLayout createPipelineLayout();

    VkPipeline getPipeline(
            HudRenderer&        renderer,
      const HudPipelineKey&     key);

    VkPipeline createPipeline(
            HudRenderer&        renderer,
      const HudPipelineKey&     key);

    static BufferLayout computeBufferLayout();

  };


  /**
   * \brief HUD item to display queue statistics
   */
  class HudSubmissionStatsItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudSubmissionStatsItem(const Rc<DxvkDevice>& device);

    ~HudSubmissionStatsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice>  m_device;

    uint64_t        m_prevSubmitCount = 0;
    uint64_t        m_prevSyncCount   = 0;
    uint64_t        m_prevSyncTicks   = 0;

    uint64_t        m_maxSubmitCount  = 0;
    uint64_t        m_maxSyncCount    = 0;
    uint64_t        m_maxSyncTicks    = 0;

    std::string     m_submitString;
    std::string     m_syncString;

    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

  };


  /**
   * \brief HUD item to display draw call counts
   */
  class HudDrawCallStatsItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudDrawCallStatsItem(const Rc<DxvkDevice>& device);

    ~HudDrawCallStatsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice>    m_device;

    DxvkStatCounters  m_prevCounters;

    uint64_t          m_drawCallCount   = 0;
    uint64_t          m_drawCount       = 0;
    uint64_t          m_dispatchCount   = 0;
    uint64_t          m_renderPassCount = 0;
    uint64_t          m_barrierCount    = 0;

    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

  };


  /**
   * \brief HUD item to display pipeline counts
   */
  class HudPipelineStatsItem : public HudItem {

  public:

    HudPipelineStatsItem(const Rc<DxvkDevice>& device);

    ~HudPipelineStatsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice> m_device;

    uint64_t m_graphicsPipelines  = 0;
    uint64_t m_graphicsLibraries  = 0;
    uint64_t m_computePipelines   = 0;

  };


  /**
   * \brief HUD item to display descriptor stats
   */
  class HudDescriptorStatsItem : public HudItem {

  public:

    HudDescriptorStatsItem(const Rc<DxvkDevice>& device);

    ~HudDescriptorStatsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice> m_device;

    uint64_t m_descriptorPoolCount = 0;
    uint64_t m_descriptorSetCount  = 0;

  };


  /**
   * \brief HUD item to display memory usage
   */
  class HudMemoryStatsItem : public HudItem {

  public:

    HudMemoryStatsItem(const Rc<DxvkDevice>& device);

    ~HudMemoryStatsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice>                    m_device;
    VkPhysicalDeviceMemoryProperties  m_memory;
    DxvkMemoryStats                   m_heaps[VK_MAX_MEMORY_HEAPS];

  };


  /**
   * \brief HUD item to display detailed memory allocation info
   */
  class HudMemoryDetailsItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudMemoryDetailsItem(
      const Rc<DxvkDevice>&     device,
            HudRenderer*        renderer);

    ~HudMemoryDetailsItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    struct DrawInfo {
      int16_t x;
      int16_t y;
      int16_t w;
      int16_t h;
      uint16_t pageMask;
      uint16_t pageCountAndActiveBit;
      uint32_t color;
    };

    struct PipelinePair {
      VkPipeline background = VK_NULL_HANDLE;
      VkPipeline visualize = VK_NULL_HANDLE;
    };

    Rc<DxvkDevice>                    m_device;
    DxvkMemoryAllocationStats         m_stats;
    DxvkSharedAllocationCacheStats    m_cacheStats;

    high_resolution_clock::time_point m_lastUpdate = { };

    bool                      m_displayCacheStats = false;

    Rc<DxvkBuffer>            m_dataBuffer;
    std::vector<DrawInfo>     m_drawInfos;

    HudShaderModule           m_vsBackground;
    HudShaderModule           m_fsBackground;

    HudShaderModule           m_vsVisualize;
    HudShaderModule           m_fsVisualize;

    VkDescriptorSetLayout     m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout          m_pipelineLayout = VK_NULL_HANDLE;

    std::unordered_map<HudPipelineKey,
      PipelinePair, DxvkHash, DxvkEq> m_pipelines;

    void drawChunk(
            HudPos              pos,
            HudPos              size,
            uint32_t            color,
      const DxvkMemoryChunkStats& chunk);

    void flushDraws(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer);

    void updateDataBuffer(
      const DxvkContextObjects& ctx,
            VkDescriptorBufferInfo& drawDescriptor,
            VkDescriptorBufferInfo& dataDescriptor);

    VkDescriptorSetLayout createSetLayout();

    VkPipelineLayout createPipelineLayout();

    PipelinePair createPipeline(
            HudRenderer&        renderer,
      const HudPipelineKey&     key);

    PipelinePair getPipeline(
            HudRenderer&        renderer,
      const HudPipelineKey&     key);

  };


  /**
   * \brief HUD item to display CS thread statistics
   */
  class HudCsThreadItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudCsThreadItem(const Rc<DxvkDevice>& device);

    ~HudCsThreadItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice> m_device;

    uint64_t m_prevCsSyncCount  = 0;
    uint64_t m_prevCsSyncTicks  = 0;
    uint64_t m_prevCsChunks     = 0;
    uint64_t m_prevCsIdleTicks = 0;

    uint64_t m_maxCsSyncCount   = 0;
    uint64_t m_maxCsSyncTicks   = 0;

    uint64_t m_diffCsIdleTicks = 0;

    uint64_t m_updateCount      = 0;

    std::string m_csSyncString;
    std::string m_csChunkString;
    std::string m_csLoadString;

    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

  };


  /**
   * \brief HUD item to display GPU load
   */
  class HudGpuLoadItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;
  public:

    HudGpuLoadItem(const Rc<DxvkDevice>& device);

    ~HudGpuLoadItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice> m_device;

    uint64_t m_prevGpuIdleTicks = 0;
    uint64_t m_diffGpuIdleTicks = 0;

    std::string m_gpuLoadString;

    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

  };


  /**
   * \brief HUD item to display pipeline compiler activity
   */
  class HudCompilerActivityItem : public HudItem {
    constexpr static int64_t MinShowDuration = 1500;
  public:

    HudCompilerActivityItem(const Rc<DxvkDevice>& device);

    ~HudCompilerActivityItem();

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    Rc<DxvkDevice> m_device;

    bool m_show           = false;
    bool m_showPercentage = false;

    uint64_t m_tasksDone    = 0ull;
    uint64_t m_tasksTotal   = 0ull;
    uint64_t m_offset       = 0ull;

    dxvk::high_resolution_clock::time_point m_timeShown = dxvk::high_resolution_clock::now();
    dxvk::high_resolution_clock::time_point m_timeDone = dxvk::high_resolution_clock::now();

    uint32_t computePercentage() const;

  };


  /**
   * \brief Frame latency item
   */
  class HudLatencyItem : public HudItem {
    constexpr static int64_t UpdateInterval = 500'000;

    constexpr static uint32_t MaxInvalidUpdates = 20u;
  public:

    HudLatencyItem();

    ~HudLatencyItem();

    void accumulateStats(const DxvkLatencyStats& stats);

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer,
            HudPos              position);

  private:

    sync::Spinlock      m_mutex;

    DxvkLatencyStats    m_accumStats = { };
    uint32_t            m_accumFrames = 0u;

    uint32_t            m_invalidUpdates = MaxInvalidUpdates;

    std::string         m_latencyString;
    std::string         m_sleepString;

    dxvk::high_resolution_clock::time_point m_lastUpdate
      = dxvk::high_resolution_clock::now();

  };

}
