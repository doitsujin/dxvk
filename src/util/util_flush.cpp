#include "util_flush.h"

namespace dxvk {

  bool GpuFlushTracker::considerFlush(
          GpuFlushType          flushType,
          uint64_t              chunkId,
          uint32_t              lastCompleteSubmissionId) {
    constexpr uint32_t minPendingSubmissions = 2;

    constexpr uint32_t minChunkCount =  3u;
    constexpr uint32_t maxChunkCount = 20u;

    // Do not flush if there is nothing to flush
    uint32_t chunkCount = uint32_t(chunkId - m_lastFlushChunkId);

    if (!chunkCount)
      return false;

    // Take any earlier missed flush with a stronger hint into account, so
    // that we still flush those as soon as possible. Ignore synchronization
    // commands since they will either perform a flush or not need it at all.
    flushType = std::min(flushType, m_lastMissedType);

    if (flushType != GpuFlushType::ImplicitSynchronization)
      m_lastMissedType = flushType;

    switch (flushType) {
      case GpuFlushType::ExplicitFlush: {
        // This shouldn't really be called for explicit flushes,
        // but handle them anyway for the sake of completeness
        return true;
      }

      case GpuFlushType::ImplicitStrongHint: {
        // Flush aggressively with a strong hint to reduce readback latency.
        return chunkCount >= minChunkCount;
      }

      case GpuFlushType::ImplicitWeakHint: {
        // Aim for a higher number of chunks per submission with
        // a weak hint in order to avoid submitting too often.
        if (chunkCount < 2 * minChunkCount)
          return false;

        // Actual heuristic is shared with synchronization commands
      } [[fallthrough]];

      case GpuFlushType::ImplicitSynchronization: {
        // If the GPU is about to go idle, flush aggressively. This may be
        // required if the application is spinning on a query or resource.
        uint32_t pendingSubmissions = uint32_t(m_lastFlushSubmissionId - lastCompleteSubmissionId);

        if (pendingSubmissions < minPendingSubmissions)
          return true;

        // Use the number of pending submissions to decide whether to flush. Other
        // than ignoring the minimum chunk count condition, we should treat this
        // the same as weak hints to avoid unnecessary synchronization.
        uint32_t threshold = std::min(maxChunkCount, pendingSubmissions * minChunkCount);
        return chunkCount >= threshold;
      }
    }

    // Should be unreachable
    return false;
  }


  void GpuFlushTracker::notifyFlush(
          uint64_t              chunkId,
          uint64_t              submissionId) {
    m_lastMissedType = GpuFlushType::ImplicitWeakHint;

    m_lastFlushChunkId = chunkId;
    m_lastFlushSubmissionId = submissionId;
  }

}
