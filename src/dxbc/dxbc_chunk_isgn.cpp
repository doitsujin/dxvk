#include "dxbc_chunk_isgn.h"

namespace dxvk {
  
  DxbcIsgn::DxbcIsgn(DxbcReader reader) {
    uint32_t elementCount = reader.readu32();
    reader.skip(sizeof(uint32_t));
    
    std::array<DxbcScalarType, 4> componentTypes = {
      DxbcScalarType::Uint32, DxbcScalarType::Uint32,
      DxbcScalarType::Sint32, DxbcScalarType::Float32,
    };
    
    for (uint32_t i = 0; i < elementCount; i++) {
      DxbcSgnEntry entry;
      entry.semanticName    = reader.clone(reader.readu32()).readString();
      entry.semanticIndex   = reader.readu32();
      entry.systemValue     = static_cast<DxbcSystemValue>(reader.readu32());
      entry.componentType   = componentTypes.at(reader.readu32());
      entry.registerId      = reader.readu32();
      entry.componentMask   = bit::extract(reader.readu32(), 0, 3);
      
      Logger::info(str::format(
        entry.semanticName, ",",
        entry.semanticIndex, ",",
        entry.systemValue, ",",
//         entry.componentType, ",",
        entry.registerId));
      
      m_entries.push_back(entry);
    }
  }
  
  
  DxbcIsgn::~DxbcIsgn() {
    
  }
  
}