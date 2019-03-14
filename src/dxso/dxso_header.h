#pragma once

#include "dxso_common.h"

#include "dxso_reader.h"

namespace dxvk {

/**
  * \brief DXSO header
  * 
  * Stores meta information about the shader such
  * as the version and the type.
  */
  class DxsoHeader {

  public:

    DxsoHeader(DxsoReader& reader);

    const DxsoProgramInfo& info() const {
      return m_info;
    }

  private:

    DxsoProgramInfo m_info;

  };

}