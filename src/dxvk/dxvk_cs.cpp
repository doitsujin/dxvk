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

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
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
    
    std::lock_guard<sync::Spinlock> lock(m_mutex);
    m_chunks.push_back(chunk);
  }
  
  
  DxvkCsThread::DxvkCsThread(const Rc<DxvkContext>& context)
  : m_context(context), m_thread([this] { threadFunc(); }) {
    
  }
  
  
  DxvkCsThread::~DxvkCsThread() {
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_stopped.store(true);
    }
    
    m_condOnAdd.notify_one();
    m_thread.join();
  }
  
  
  void DxvkCsThread::dispatchChunk(DxvkCsChunkRef&& chunk) {
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_chunksQueued.push(std::move(chunk));
      m_chunksPending += 1;
    }
    
    m_condOnAdd.notify_one();
  }
  
  
  void DxvkCsThread::synchronize() {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    
    m_condOnSync.wait(lock, [this] {
      return !m_chunksPending.load();
    });
  }
  
  
  void DxvkCsThread::threadFunc() {
    env::setThreadName("dxvk-cs");

    DxvkCsChunkRef chunk;

    try {
      while (!m_stopped.load()) {
        { std::unique_lock<dxvk::mutex> lock(m_mutex);
          if (chunk) {
            if (--m_chunksPending == 0)
              m_condOnSync.notify_one();
            
            chunk = DxvkCsChunkRef();
          }
          
          if (m_chunksQueued.size() == 0) {
            m_condOnAdd.wait(lock, [this] {
              return (m_chunksQueued.size() != 0)
                  || (m_stopped.load());
            });
          }
          
          if (m_chunksQueued.size() != 0) {
            chunk = std::move(m_chunksQueued.front());
            m_chunksQueued.pop();
          }
        }
        
        if (chunk)
          chunk->executeAll(m_context.ptr());
      }
    } catch (const DxvkError& e) {
      Logger::err("Exception on CS thread!");
      Logger::err(e.message());
    }
  }
  
}