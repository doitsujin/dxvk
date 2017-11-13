#include "dxbc_gen_vertex.h"

namespace dxvk {
  
  DxbcVsCodeGen::DxbcVsCodeGen() {
    m_outPerVertex = m_module.newVar(
      m_module.defPointerType(this->defPerVertexBlock(), spv::StorageClassOutput),
      spv::StorageClassOutput);
    m_module.setDebugName(m_outPerVertex, "vs_out");
  }
  
  
  DxbcVsCodeGen::~DxbcVsCodeGen() {
    
  }
  
  
  void DxbcVsCodeGen::dclInterfaceVar(
          DxbcOperandType   regType,
          uint32_t          regId,
          uint32_t          regDim,
          DxbcComponentMask regMask,
          DxbcSystemValue   sv) {
    
  }
  
  
  void DxbcVsCodeGen::ptrInterfaceVar(
          DxbcOperandType   regType,
          uint32_t          regId) {
    
  }
  
  
  void DxbcVsCodeGen::ptrInterfaceVarIndexed(
          DxbcOperandType   regType,
          uint32_t          regId,
    const DxbcValue&        index) {
    throw DxvkError(str::format(
      "DxbcVsCodeGen::ptrInterfaceVarIndexed:\n",
      "Vertex shaders do not support indexed interface variables"));
  }
  
  
  Rc<DxvkShader> DxbcVsCodeGen::finalize() {
    m_module.addEntryPoint(m_entryPointId,
      spv::ExecutionModelVertex, "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());
    m_module.setDebugName(m_entryPointId, "main");
    
    return new DxvkShader(VK_SHADER_STAGE_VERTEX_BIT,
      m_module.compile(), 0, nullptr);
  }
  
}