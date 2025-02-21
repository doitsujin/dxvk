#pragma once

#include <atomic>
#include <dxgi.h>
#include <vector>
#include <array>
#include <assert.h>
#include "../../util/util_sleep.h"
#include "../../util/log/log.h"
#include "../../util/util_string.h"


namespace dxvk {

  class FramePacer;
  class LatencyMarkersStorage;


  struct LatencyMarkers {

    using time_point = high_resolution_clock::time_point;

    time_point start;
    time_point end;

    int32_t csStart;
    int32_t csFinished;
    int32_t cpuFinished;
    int32_t gpuStart;
    int32_t gpuFinished;
    int32_t presentFinished;

    std::vector<time_point> gpuReady;
    std::vector<time_point> gpuSubmit;
    std::vector<time_point> gpuQueueSubmit;

  };


  /*
   * stores which information is accessible for which frame
   */
  struct LatencyMarkersTimeline {

    std::atomic<uint64_t> cpuFinished   = { DXGI_MAX_SWAP_CHAIN_BUFFERS };
    std::atomic<uint64_t> gpuStart      = { DXGI_MAX_SWAP_CHAIN_BUFFERS };
    std::atomic<uint64_t> gpuFinished   = { DXGI_MAX_SWAP_CHAIN_BUFFERS };
    std::atomic<uint64_t> frameFinished = { DXGI_MAX_SWAP_CHAIN_BUFFERS };

  };


  class LatencyMarkersReader {

  public:

    LatencyMarkersReader( const LatencyMarkersStorage* storage, uint32_t numEntries );
    bool getNext( const LatencyMarkers*& result );

  private:

    const LatencyMarkersStorage* m_storage;
    uint64_t m_index;

  };


  class LatencyMarkersStorage {
    friend class LatencyMarkersReader;
    friend class FramePacer;
  public:

    LatencyMarkersStorage() { }
    ~LatencyMarkersStorage() { }

    LatencyMarkersReader getReader( uint32_t numEntries ) const {
      return LatencyMarkersReader(this, numEntries);
    }

    void registerFrameStart( uint64_t frameId ) {
      if (frameId <= m_timeline.frameFinished.load()) {
        Logger::warn( str::format("internal error during registerFrameStart: expected frameId=",
          m_timeline.frameFinished.load()+1, ", got: ", frameId) );
      }
      auto now = high_resolution_clock::now();

      LatencyMarkers* markers = getMarkers(frameId);
      markers->start = now;
    }

    void registerFrameEnd( uint64_t frameId ) {
      if (frameId <= m_timeline.frameFinished.load()) {
        Logger::warn( str::format("internal error during registerFrameEnd: expected frameId=",
          m_timeline.frameFinished.load()+1, ", got: ", frameId) );
      }
      auto now = high_resolution_clock::now();

      LatencyMarkers* markers = getMarkers(frameId);
      markers->presentFinished = std::chrono::duration_cast<std::chrono::microseconds>(
        now - markers->start).count();
      markers->end = now;

      m_timeline.frameFinished.store(frameId);
    }

    const LatencyMarkersTimeline* getTimeline() const {
      return &m_timeline;
    }

    const LatencyMarkers* getConstMarkers( uint64_t frameId ) const {
      return &m_markers[frameId % m_numMarkers];
    }


  private:

    LatencyMarkers* getMarkers( uint64_t frameId ) {
      return &m_markers[frameId % m_numMarkers];
    }

    // simple modulo hash mapping is used for frameIds. They are expected to monotonically increase by one.
    // select the size large enough, so we never come into a situation where the reader cannot keep up with the producer
    static constexpr uint16_t m_numMarkers = 128;
    std::array<LatencyMarkers, m_numMarkers> m_markers = { };
    LatencyMarkersTimeline m_timeline;

  };



  inline LatencyMarkersReader::LatencyMarkersReader( const LatencyMarkersStorage* storage, uint32_t numEntries )
  : m_storage(storage) {
    m_index = 0;
    if (m_storage->m_timeline.frameFinished.load() > numEntries + DXGI_MAX_SWAP_CHAIN_BUFFERS + 2)
      m_index = m_storage->m_timeline.frameFinished.load() - numEntries;
  }


  inline bool LatencyMarkersReader::getNext( const LatencyMarkers*& result ) {
    if (m_index == 0 || m_index > m_storage->m_timeline.frameFinished.load())
      return false;

    result = &m_storage->m_markers[m_index % m_storage->m_numMarkers];
    m_index++;
    return true;
  }

}
