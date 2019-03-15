#include "dxso_ctab.h"

namespace dxvk {

  DxsoCtab::DxsoCtab(DxsoReader& reader, uint32_t commentTokenCount) {
    m_size          = reader.readu32();

    if (m_size != sizeof(DxsoCtab))
      throw DxvkError("DxsoCtab: ctab size invalid");

    m_creator       = reader.readu32();
    m_version       = reader.readu32();
    m_constants     = reader.readu32();
    m_constantInfo  = reader.readu32();
    m_flags         = reader.readu32();
    m_target        = reader.readu32();
  }

}