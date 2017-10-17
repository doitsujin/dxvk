#pragma once

#include "dxbc_include.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V entry point info
   * 
   * Accumulates information about the entry
   * point of the generated shader module,
   * including execution mode info.
   */
  class DxbcEntryPoint {
    
  public:
    
    DxbcEntryPoint();
    ~DxbcEntryPoint();
    
    /**
     * \brief Generates SPIR-V code
     * \returns SPIR-V code buffer
     */
    DxvkSpirvCodeBuffer code() const;
    
    /**
     * \brief Sets memory model
     * 
     * Generates an \c OpMemoryModel instruction.
     * Only ever call this once, or otherwise the
     * resulting shader will become undefined.
     * \param [in] addressModel Address model
     * \param [in] memoryModel Memory model
     */
    void setMemoryModel(
            spv::AddressingModel  addressModel,
            spv::MemoryModel      memoryModel);
    
    /**
     * \brief Adds an entry point
     * 
     * Currently, DXVK expects there to be a single entry point
     * with the name \c main. Do not create additional entry points.
     * \param [in] functionId Entry point function ID
     * \param [in] execModel Execution model for the function
     * \param [in] name Entry point name that is used by Vulkan
     * \param [in] interfaceCount Number of additional interface IDs
     * \param [in] interfaceIds List of additional interface IDs
     */
    void addEntryPoint(
            uint32_t              functionId,
            spv::ExecutionModel   execModel,
      const char*                 name,
            uint32_t              interfaceCount,
      const uint32_t*             interfaceIds);
    
    /**
     * \brief Sets local work group size for a compute shader
     * 
     * Adds a \c OpExecutionMode instruction that sets
     * the local work group size for a compute shader.
     * \param [in] functionId Entry point ID
     * \param [in] x Number of threads in X direction
     * \param [in] y Number of threads in Y direction
     * \param [in] z Number of threads in Z direction
     */
    void setLocalSize(
            uint32_t              functionId,
            uint32_t              x,
            uint32_t              y,
            uint32_t              z);
    
  private:
    
    DxvkSpirvCodeBuffer m_memoryModel;
    DxvkSpirvCodeBuffer m_entryPoints;
    DxvkSpirvCodeBuffer m_execModeInfo;
    
  };
  
}