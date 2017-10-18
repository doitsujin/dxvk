#include "spirv_gen_entrypoint.h"

namespace dxvk {
  
  SpirvEntryPoint:: SpirvEntryPoint() { }
  SpirvEntryPoint::~SpirvEntryPoint() { }
  
  
  SpirvCodeBuffer SpirvEntryPoint::code() const {
    SpirvCodeBuffer code;
    code.append(m_memoryModel);
    code.append(m_entryPoints);
    code.append(m_execModeInfo);
    return code;
  }
  
  
  void SpirvEntryPoint::setMemoryModel(
          spv::AddressingModel  addressModel,
          spv::MemoryModel      memoryModel) {
    m_memoryModel.putIns  (spv::OpMemoryModel, 3);
    m_memoryModel.putWord (addressModel);
    m_memoryModel.putWord (memoryModel);
  }
  
  
  void SpirvEntryPoint::addEntryPoint(
          uint32_t              functionId,
          spv::ExecutionModel   execModel,
    const char*                 name,
          uint32_t              interfaceCount,
    const uint32_t*             interfaceIds) {
    m_entryPoints.putIns  (spv::OpEntryPoint, 3 + m_entryPoints.strLen(name) + interfaceCount);
    m_entryPoints.putWord (execModel);
    m_entryPoints.putWord (functionId);
    m_entryPoints.putStr  (name);
    
    for (uint32_t i = 0; i < interfaceCount; i++)
      m_entryPoints.putWord(interfaceIds[i]);
  }
  
  
  void SpirvEntryPoint::setLocalSize(
          uint32_t              functionId,
          uint32_t              x,
          uint32_t              y,
          uint32_t              z) {
    m_execModeInfo.putIns (spv::OpExecutionMode, 6);
    m_execModeInfo.putWord(functionId);
    m_execModeInfo.putWord(spv::ExecutionModeLocalSize);
    m_execModeInfo.putWord(x);
    m_execModeInfo.putWord(y);
    m_execModeInfo.putWord(z);
  }
  
}