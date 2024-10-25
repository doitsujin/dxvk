#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "./sync/sync_spinlock.h"

#include "util_flags.h"
#include "util_sleep.h"
#include "util_time.h"

namespace dxvk {

  /**
   * \brief Internal latency marker
   */
  enum class DxvkLatencyMarker : uint32_t {
    CpuFrameStart   = 0u,
    CpuFirstSubmit  = 1u,
    CpuPresent      = 2u,
    GpuFrameStart   = 3u,
    GpuFrameEnd     = 4u,
    GpuPresentEnd   = 5u,

    Count
  };

  using DxvkLatencyMarkerFlags = Flags<DxvkLatencyMarker>;


  /**
   * \brief Latency control helper
   *
   * 
   */
  class DxvkLatencyControl {

  public:

    DxvkLatencyControl();

    ~DxvkLatencyControl();

    /**
     * \brief Increments reference count
     */
    void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements reference count
     *
     * Frees the object as necessary.
     */
    void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Sets latency marker
     *
     * Sets the time stamp for the given marker to the current time.
     * \param [in] frameId Current frame ID
     * \param [in] marker Marker to set
     */
    void setMarker(
            uint64_t                    frameId,
            DxvkLatencyMarker           marker) {
      auto& frame = m_frames[frameId % m_frames.size()];
      frame.timestamps[uint32_t(marker)] = high_resolution_clock::now();

      if (marker == DxvkLatencyMarker::CpuFrameStart)
        frame.markerMask.store(1u << uint32_t(marker), std::memory_order_release);
      else
        frame.markerMask.fetch_or(1u << uint32_t(marker), std::memory_order_release);
    }

    /**
     * \brief Stalls the calling thread to reduce latency
     *
     * Uses markers from the current and previous frames to determine
     * when to give control back to the application in order to reduce
     * overall frame latency without starving the GPU.
     * \param [in] frameId Current frame ID. All CPU timeline
     *    markers for this frame must be up to date.
     * \param [in] frameRate Target frame rate
     */
    void sleep(
          uint64_t                      frameId,
          double                        frameRate);

    /**
     * \brief Queries last sleep duration
     * \returns Last sleep duration
     */
    auto getLastSleepDuration() {
      std::lock_guard lock(m_statLock);
      return m_statSleepDuration;
    }

  private:

    struct FrameEntry {
      std::atomic<uint32_t> markerMask = { ~0u };
      std::array<high_resolution_clock::time_point,
        uint32_t(DxvkLatencyMarker::Count)> timestamps = { };
    };

    std::atomic<uint32_t>       m_refCount = { 0u };
    std::array<FrameEntry, 8u>  m_frames   = { };

    double m_frameRateLimit = 0.0;

    sync::Spinlock            m_statLock;
    std::chrono::nanoseconds  m_statSleepDuration = { };

  };

}
