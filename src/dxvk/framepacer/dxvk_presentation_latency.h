#pragma once

#include <stdint.h>
#include <array>
#include <atomic>
#include <assert.h>


namespace dxvk {

  class PresentationLatency {

  public:

    void push( int32_t latency ) {

      assert( latency >= 0 );
      size_t index = latency / 8;
      index = std::min( m_buckets.size()-1, index );

      m_buckets[index]++;
      m_numLatencies++;

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

    constexpr static int32_t maxLatency = 50000;
    std::array< std::atomic<int64_t>, maxLatency / 8 > m_buckets = { };
    std::atomic< uint64_t > m_numLatencies = { 0 };

  };

}
