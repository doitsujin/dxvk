#include "dxvk_cs.h"

namespace dxvk {
  
  DxvkCsChunk::DxvkCsChunk() {
    
  }
  
  
  DxvkCsChunk::~DxvkCsChunk() {
    for (size_t i = 0; i < m_commandCount; i++)
      m_commandList[i]->~DxvkCsCmd();
  }
  
  
  void DxvkCsChunk::executeAll(DxvkContext* ctx) {
    for (size_t i = 0; i < m_commandCount; i++) {
      m_commandList[i]->exec(ctx);
      m_commandList[i]->~DxvkCsCmd();
    }
    
    m_commandCount  = 0;
    m_commandOffset = 0;
  }
  
  
  DxvkCsThread::DxvkCsThread(const Rc<DxvkContext>& context)
  : m_context(context), m_thread([this] { threadFunc(); }) {
    
  }
  
  
  DxvkCsThread::~DxvkCsThread() {
    { std::unique_lock<std::mutex> lock(m_mutex);
      m_stopped.store(true);
    }
    
    m_condOnAdd.notify_one();
    m_thread.join();
  }
  
  
  void DxvkCsThread::dispatchChunk(Rc<DxvkCsChunk>&& chunk) {
    { std::unique_lock<std::mutex> lock(m_mutex);
      m_chunks.push(std::move(chunk));
      m_chunksPending += 1;
      
      m_condOnSync.wait(lock, [this] {
        return m_stopped.load() || (m_chunksPending < MaxChunksInFlight);
      });
    }
    
    m_condOnAdd.notify_one();
  }
  
  
  void DxvkCsThread::synchronize() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    m_condOnSync.wait(lock, [this] {
      return m_chunksPending == 0;
    });
  }
  
  
  void DxvkCsThread::threadFunc() {
    while (!m_stopped.load()) {
      Rc<DxvkCsChunk> chunk;
      
      { std::unique_lock<std::mutex> lock(m_mutex);
        
        m_condOnAdd.wait(lock, [this] {
          return m_stopped.load() || (m_chunks.size() != 0);
        });
        
        if (m_chunks.size() != 0) {
          chunk = std::move(m_chunks.front());
          m_chunks.pop();
        }
      }
      
      if (chunk != nullptr) {
        chunk->executeAll(m_context.ptr());
        
        { std::unique_lock<std::mutex> lock(m_mutex);
          m_chunksPending -= 1;
        }
        
        m_condOnSync.notify_one();
      }
    }
  }
  
}