#pragma once

#include "dxso_common.h"

#include "dxso_reader.h"

namespace dxvk {

  /**
    * \brief DXSO CTAB
    *
    * Stores meta information about the shader
    */
  class DxsoCtab : public RcObject {

  public:

    DxsoCtab(DxsoReader& reader, uint32_t commentTokenCount);

  private:

    uint32_t m_size;
    uint32_t m_creator;
    uint32_t m_version;
    uint32_t m_constants;
    uint32_t m_constantInfo;
    uint32_t m_flags;
    uint32_t m_target;

  };

}