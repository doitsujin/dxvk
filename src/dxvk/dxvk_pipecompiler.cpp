#include "dxvk_graphics.h"
#include "dxvk_pipecompiler.h"

namespace dxvk {
  
  DxvkPipelineCompiler::DxvkPipelineCompiler() {
    // Use ~half the CPU cores for pipeline compilation
    const uint32_t threadCount = std::max<uint32_t>(
      1u, std::thread::hardware_concurrency() / 2);
    
    Logger::debug(str::format(
      "DxvkPipelineCompiler: Using ", threadCount, " threads"));
    
    // Start the compiler threads
    m_compilerThreads.resize(threadCount);
    
    for (uint32_t i = 0; i < threadCount; i++) {
      m_compilerThreads.at(i) = std::thread(
        [this] { this->runCompilerThread(); });
    }
  }
  
  
  DxvkPipelineCompiler::~DxvkPipelineCompiler() {
    { std::unique_lock<std::mutex> lock(m_compilerLock);
      m_compilerStop.store(true);
      m_compilerCond.notify_all();
    }
    
    for (auto& thread : m_compilerThreads)
      thread.join();
  }
  
  
  void DxvkPipelineCompiler::queueCompilation(
    const Rc<DxvkGraphicsPipeline>&         pipeline,
    const Rc<DxvkGraphicsPipelineInstance>& instance) {
    std::unique_lock<std::mutex> lock(m_compilerLock);
    m_compilerQueue.push({ pipeline, instance });
    m_compilerCond.notify_one();
  }
  
  
  void DxvkPipelineCompiler::runCompilerThread() {
    while (!m_compilerStop.load()) {
      PipelineEntry entry;
      
      { std::unique_lock<std::mutex> lock(m_compilerLock);
        
        m_compilerCond.wait(lock, [this] {
          return m_compilerStop.load()
              || m_compilerQueue.size() != 0;
        });
        
        if (m_compilerQueue.size() != 0) {
          entry = std::move(m_compilerQueue.front());
          m_compilerQueue.pop();
        }
      }
      
      if (entry.pipeline != nullptr && entry.instance != nullptr)
        entry.pipeline->compileInstance(entry.instance);
    }
  }
  
}