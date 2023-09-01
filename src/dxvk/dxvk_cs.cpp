#include "dxvk_cs.h"

namespace dxvk {
  
  DxvkCsChunk::DxvkCsChunk() {
    
  }
  
  
  DxvkCsChunk::~DxvkCsChunk() {
    this->reset();
  }
  
  
  void DxvkCsChunk::init(DxvkCsChunkFlags flags) {
    m_flags = flags;
  }


  void DxvkCsChunk::executeAll(DxvkContext* ctx) {
    auto cmd = m_head;
    
    if (m_flags.test(DxvkCsChunkFlag::SingleUse)) {
      m_commandOffset = 0;
      
      while (cmd != nullptr) {
        auto next = cmd->next();
        cmd->exec(ctx);
        cmd->~DxvkCsCmd();
        cmd = next;
      }

      m_head = nullptr;
      m_tail = nullptr;
    } else {
      while (cmd != nullptr) {
        cmd->exec(ctx);
        cmd = cmd->next();
      }
    }
  }
  
  
  void DxvkCsChunk::reset() {
    auto cmd = m_head;

    while (cmd != nullptr) {
      auto next = cmd->next();
      cmd->~DxvkCsCmd();
      cmd = next;
    }
    
    m_head = nullptr;
    m_tail = nullptr;

    m_commandOffset = 0;
  }
  
  
  DxvkCsChunkPool::DxvkCsChunkPool() {
    
  }
  
  
  DxvkCsChunkPool::~DxvkCsChunkPool() {
    for (DxvkCsChunk* chunk : m_chunks)
      delete chunk;
  }
  
  
  DxvkCsChunk* DxvkCsChunkPool::allocChunk(DxvkCsChunkFlags flags) {
    DxvkCsChunk* chunk = nullptr;

    { std::lock_guard<dxvk::mutex> lock(m_mutex);
      
      if (m_chunks.size() != 0) {
        chunk = m_chunks.back();
        m_chunks.pop_back();
      }
    }
    
    if (!chunk)
      chunk = new DxvkCsChunk();
    
    chunk->init(flags);
    return chunk;
  }
  
  
  void DxvkCsChunkPool::freeChunk(DxvkCsChunk* chunk) {
    chunk->reset();
    
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_chunks.push_back(chunk);
  }
  
  
  DxvkCsThread::DxvkCsThread(
    const Rc<DxvkDevice>&   device,
    const Rc<DxvkContext>&  context)
  : m_device(device), m_context(context),
    m_thread([this] { threadFunc(); }) {
    
  }
  
  
  DxvkCsThread::~DxvkCsThread() {
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_stopped.store(true);
    }
    
    m_condOnAdd.notify_one();
    m_thread.join();
  }
  
  
  uint64_t DxvkCsThread::dispatchChunk(DxvkCsChunkRef&& chunk) {
    uint64_t seq;

    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      seq = ++m_chunksDispatched;
      m_chunksQueued.push_back(std::move(chunk));
    }
    
    m_condOnAdd.notify_one();
    return seq;
  }
  
  
  void DxvkCsThread::synchronize(uint64_t seq) {
    // Avoid locking if we know the sync is a no-op, may
    // reduce overhead if this is being called frequently
    if (seq > m_chunksExecuted.load(std::memory_order_acquire)) {
      // We don't need to lock the queue here, if synchronization
      // happens while another thread is submitting then there is
      // an inherent race anyway
      if (seq == SynchronizeAll)
        seq = m_chunksDispatched.load();

      auto t0 = dxvk::high_resolution_clock::now();

      { std::unique_lock<dxvk::mutex> lock(m_counterMutex);
        m_condOnSync.wait(lock, [this, seq] {
          return m_chunksExecuted.load() >= seq;
        });
      }

      auto t1 = dxvk::high_resolution_clock::now();
      auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

      m_device->addStatCtr(DxvkStatCounter::CsSyncCount, 1);
      m_device->addStatCtr(DxvkStatCounter::CsSyncTicks, ticks.count());
    }
  }
  
  
  void DxvkCsThread::threadFunc() {
    env::setThreadName("dxvk-cs");

    // Local chunk queue, we use two queues and swap between
    // them in order to potentially reduce lock contention.
    std::vector<DxvkCsChunkRef> chunks;

    try {
      while (!m_stopped.load()) {
        { std::unique_lock<dxvk::mutex> lock(m_mutex);

          m_condOnAdd.wait(lock, [this] {
            return (!m_chunksQueued.empty())
                || (m_stopped.load());
          });

          std::swap(chunks, m_chunksQueued);
        }

        for (auto& chunk : chunks) {
          m_context->addStatCtr(DxvkStatCounter::CsChunkCount, 1);

          chunk->executeAll(m_context.ptr());

          // Use a separate mutex for the chunk counter, this
          // will only ever be contested if synchronization is
          // actually necessary.
          { std::unique_lock<dxvk::mutex> lock(m_counterMutex);
            m_chunksExecuted += 1;
            m_condOnSync.notify_one();
          }

          // Explicitly free chunk here to release
          // references to any resources held by it
          chunk = DxvkCsChunkRef();
        }

        chunks.clear();
      }
    } catch (const DxvkError& e) {
      Logger::err("Exception on CS thread!");
      Logger::err(e.message());
    }
  }
  
}