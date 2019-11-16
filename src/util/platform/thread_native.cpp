#include "../thread.h"
#include "../util_likely.h"

#include <atomic>

namespace dxvk::this_thread {

  std::atomic<uint32_t> g_threadCtr = { 0u };
  thread_local uint32_t g_threadId  = 0u;
  
  // This implementation returns thread ids unique to the current instance.
  // Ie. if you use this across multiple .so's then you might get conflicting ids.
  // This isn't an issue for us as we only use it in d3d11, but do check if this changes.
  uint32_t get_id() {
    if (unlikely(!g_threadId))
      g_threadId = ++g_threadCtr;

    return g_threadId;
  }

}