#pragma once

#include <stdint.h>
#include <array>
#include <atomic>
#include <deque>
#include <assert.h>

#include "../../util/util_time.h"


namespace dxvk {

  class PresentationStats {

  public:

    using time_point = high_resolution_clock::time_point;

    void push( time_point t, int32_t latency ) {

      int32_t index = getBucketIndex(latency);

      ++m_buckets[index];
      ++m_numLatencies;

      QueueItem item;
      item.timeStamp = t;
      item.latency   = latency;

      m_queue.push_back(item);

      // remove old items from the queue
      while (!m_queue.empty() && m_queue.front().timeStamp
        < high_resolution_clock::now() - std::chrono::milliseconds(5000) ) {
        index = getBucketIndex(m_queue.front().latency);
        --m_buckets[index];
        --m_numLatencies;
        m_queue.pop_front();
      }

    }


    int32_t getMedian() {

      uint64_t targetCount = m_numLatencies / 2;
      uint64_t count = 0;
      size_t index = 0;
      while (count < targetCount && index < m_buckets.size()) {
        count += m_buckets[index];
        ++index;
      }

      return index * 8;

    }


  private:

    int getBucketIndex( int32_t latency ) {
      assert( latency >= 0 );
      size_t index = latency / 8;
      return std::min( m_buckets.size()-1, index );
    }

    // if presents take longer than 5 ms, we probably have a problem?
    constexpr static int32_t maxLatency = 5000;

    std::array< std::atomic<int64_t>, maxLatency / 8 > m_buckets = { };
    std::atomic< int64_t > m_numLatencies = { 0 };

    struct QueueItem {
      time_point timeStamp;
      int32_t    latency;
    };

    // should only be accessed from one thread
    std::deque< QueueItem > m_queue;

  };

}
