#include "dxso_ctab.h"

namespace dxvk {

  DxsoCtab::DxsoCtab(DxsoReader& reader, uint32_t commentTokenCount) {
    const uint32_t tableSize = (commentTokenCount - 1) * DxsoReader::TokenSize;

    DxbcTag tag = reader.readTag();
    if (tag != DxbcTag("CTAB"))
      throw DxvkError("DxsoCtab: ctab header invalid");

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