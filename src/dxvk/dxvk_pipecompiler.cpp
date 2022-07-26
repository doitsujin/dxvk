#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipecompiler.h"

namespace dxvk {

  DxvkPipelineCompiler::DxvkPipelineCompiler(const DxvkDevice* device) {
    uint32_t numCpuCores = dxvk::thread::hardware_concurrency();
    uint32_t numWorkers  = ((std::max(1u, numCpuCores) - 1) * 5) / 7;

    if (numWorkers <  1) numWorkers =  1;
    if (numWorkers > 32) numWorkers = 32;

    if (device->config().numAsyncThreads > 0)
      numWorkers = device->config().numAsyncThreads;

    Logger::info(str::format("DXVK: Using ", numWorkers, " async compiler threads"));

    // Start the compiler threads
    m_compilerThreads.resize(numWorkers);

    for (uint32_t i = 0; i < numWorkers; i++) {
      m_compilerThreads.at(i) = dxvk::thread(
        [this] { this->runCompilerThread(); });
    }
  }


  DxvkPipelineCompiler::~DxvkPipelineCompiler() {
    { std::lock_guard<std::mutex> lock(m_compilerLock);
      m_compilerStop.store(true);
    }

    m_compilerCond.notify_all();
    for (auto& thread : m_compilerThreads)
      thread.join();
  }


  void DxvkPipelineCompiler::queueCompilation(
    DxvkGraphicsPipeline*                   pipeline,
    const DxvkGraphicsPipelineStateInfo&    state,
    const DxvkRenderPass*                   renderPass) {
    std::lock_guard<std::mutex> lock(m_compilerLock);
    m_compilerQueue.push({ pipeline, state, renderPass });
    m_compilerCond.notify_one();
  }


  void DxvkPipelineCompiler::runCompilerThread() {
    env::setThreadName("dxvk-pcompiler");

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

      if (entry.pipeline != nullptr && entry.renderPass != nullptr &&
          entry.pipeline->compilePipeline(entry.state, entry.renderPass)) {
          entry.pipeline->writePipelineStateToCache(entry.state, entry.renderPass->format());
      }
    }
  }

}
