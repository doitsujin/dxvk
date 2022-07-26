#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "../util/thread.h"
#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkGraphicsPipeline;
  class DxvkGraphicsPipelineStateInfo;

  /**
   * \brief Pipeline compiler
   *
   * Asynchronous pipeline compiler
   */
  class DxvkPipelineCompiler : public RcObject {

  public:

    DxvkPipelineCompiler(const DxvkDevice* device);
    ~DxvkPipelineCompiler();

    /**
     * \brief Compiles a pipeline asynchronously
     *
     * This should be used to compile graphics
     * pipeline instances asynchronously.
     * \param [in] pipeline The pipeline object
     * \param [in] state The pipeline state info object
     * \param [in] renderPass
     */
    void queueCompilation(
      DxvkGraphicsPipeline*                   pipeline,
      const DxvkGraphicsPipelineStateInfo&    state,
      const DxvkRenderPass*                   renderPass);

  private:

    struct PipelineEntry {
      DxvkGraphicsPipeline*                   pipeline = nullptr;
      DxvkGraphicsPipelineStateInfo           state;
      const DxvkRenderPass*                   renderPass = nullptr;
    };

    std::atomic<bool>           m_compilerStop = { false };
    std::mutex                  m_compilerLock;
    std::condition_variable     m_compilerCond;
    std::queue<PipelineEntry>   m_compilerQueue;
    std::vector<dxvk::thread>   m_compilerThreads;

    void runCompilerThread();

  };

}
