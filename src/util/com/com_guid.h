#pragma once

#include <ostream>
#include <iomanip>

#include "com_include.h"

namespace dxvk {

  /**
   * \brief Checks whether an unknown GUID should be logged
   *
   * \param [in] objectGuid GUID of the object that QueryInterface is called on
   * \param [in] requestGuid Requested unsupported GUID
   * \returns \c true if the error should be logged
   */
  bool logQueryInterfaceError(REFIID objectGuid, REFIID requestedGuid);

};

std::ostream& operator << (std::ostream& os, REFIID guid);
